#include "Serialize.hpp"

class Deserialize {
    Task& task;
    const char *pos, *end, *tokenBegin;
    ArchitectureType row, column;
    Symbol package;

    void throwException(const char* message) {
        task.throwException(message, {
            {PreDef_Row, task.context->createFromData(row)},
            {PreDef_Column, task.context->createFromData(column)}
        });
    }

    struct StackEntry {
        Symbol entity, unnestEntity, unnestAttribute;
        std::queue<Symbol> queue;
        StackEntry() :entity(PreDef_Void), unnestEntity(PreDef_Void) {}
    } *parentEntry, *currentEntry;
    std::vector<std::unique_ptr<StackEntry>> stack;
    std::map<Context::SymbolObject*, Symbol, Context::BlobIndexCompare> locals;

    void nextSymbol(StackEntry* stackEntry, Symbol symbol) {
        assert(stackEntry);
        if(stackEntry->unnestEntity == PreDef_Void)
            stackEntry->queue.push(symbol);
        else {
            try {
                task.link({stackEntry->unnestEntity, stackEntry->unnestAttribute, symbol});
                stackEntry->unnestEntity = PreDef_Void;
            } catch(Symbol error) {
                throwException("Triple defined twice via unnesting");
            }
        }
    }

    template<bool isText = false>
    void parseToken() {
        if(pos > tokenBegin) {
            Symbol symbol;
            if(isText)
                symbol = task.context->createFromData(tokenBegin, pos-tokenBegin);
            else if(*tokenBegin == '#') {
                symbol = task.context->createFromData(tokenBegin, pos-tokenBegin);
                Context::SymbolObject* symbolObject = task.context->getSymbolObject(symbol);
                auto iter = locals.find(symbolObject);
                if(iter == locals.end())
                    locals.insert(std::make_pair(symbolObject, symbol));
                else {
                    task.destroy(symbol);
                    symbol = (*iter).second;
                }
            } else if(pos-tokenBegin > strlen(HRLRawBegin) && strcmp(tokenBegin, HRLRawBegin) == 0) {
                const char* src = tokenBegin+strlen(HRLRawBegin);
                ArchitectureType nibbleCount = src-pos;
                if(nibbleCount == 0)
                    throwException("Empty raw data");
                symbol = task.context->create();
                Context::SymbolObject* symbolObject = task.context->getSymbolObject(symbol);
                symbolObject->allocateBlob(nibbleCount*4);
                char* dst = reinterpret_cast<char*>(symbolObject->blobData.get()), odd = 0, nibble;
                while(src < pos) {
                    if(*src >= '0' && *src <= '9')
                        nibble = *src-'0';
                    else if(*src >= 'A' && *src <= 'F')
                        nibble = *src-'A'+0xA;
                    else
                        throwException("Non hex characters");
                    if(odd == 0) {
                        *dst = nibble;
                        odd = 1;
                    } else {
                        *(dst++) |= nibble<<4;
                        odd = 0;
                    }
                    ++src;
                }
            } else {
                ArchitectureType mantissa = 0, devisor = 0;
                bool isNumber = true, negative = (*tokenBegin == '-');
                const char* src = tokenBegin+negative;
                // TODO What if too long, precision loss?
                while(src < pos) {
                    devisor *= 10;
                    if(*src >= '0' && *src <= '9') {
                        mantissa *= 10;
                        mantissa += *src-'0';
                    } else if(*src == '.') {
                        if(devisor > 0) {
                            isNumber = false;
                            break;
                        }
                        devisor = 1;
                    } else {
                        isNumber = false;
                        break;
                    }
                    ++src;
                }
                if(isNumber && devisor != 1) {
                    if(devisor > 0) {
                        double value = mantissa;
                        value /= devisor;
                        if(negative) value *= -1;
                        symbol = task.context->createFromData(value);
                    } else if(negative)
                        symbol = task.context->createFromData(-static_cast<int64_t>(mantissa));
                    else
                        symbol = task.context->createFromData(mantissa);
                } else
                    symbol = task.context->createFromData(tokenBegin, pos-tokenBegin);
                symbol = task.indexBlob(symbol);
            }
            task.context->link({package, PreDef_Holds, symbol});
            nextSymbol(currentEntry, symbol);
        }
        tokenBegin = pos+1;
    }

    void fillInAnonymous() {
        if(currentEntry->entity != PreDef_Void)
            return;
        currentEntry->entity = task.context->create();
        task.link({package, PreDef_Holds, currentEntry->entity});
        nextSymbol(parentEntry, currentEntry->entity);
    }

    template<bool semicolon>
    void seperateTokens() {
        parseToken();

        if(currentEntry->queue.size() == 0) {
            if(semicolon) {
                if(currentEntry->entity != PreDef_Void)
                    throwException("Pointless semicolon");
                fillInAnonymous();
            }
            return;
        }

        try {
            if(semicolon && currentEntry->queue.size() == 1) {
                if(currentEntry->entity == PreDef_Void) {
                    currentEntry->entity = currentEntry->queue.front();
                    nextSymbol(parentEntry, currentEntry->entity);
                } else
                    task.link({currentEntry->entity, currentEntry->queue.front(), currentEntry->entity});
                currentEntry->queue.pop();
                return;
            }

            fillInAnonymous();
            parentEntry->unnestEntity = (semicolon) ? PreDef_Void : currentEntry->entity;
            parentEntry->unnestAttribute = currentEntry->queue.front();
            currentEntry->queue.pop();
            while(currentEntry->queue.size()) {
                task.link({currentEntry->entity, parentEntry->unnestAttribute, currentEntry->queue.front()});
                currentEntry->queue.pop();
            }
        } catch(Symbol error) {
            throwException("Triple defined twice");
        }
    }

    public:
    Deserialize(Task& _task) :task(_task) {
        row = column = 1;
        stack.clear();
        locals.clear();
        currentEntry = new StackEntry();
        stack.push_back(std::unique_ptr<StackEntry>(currentEntry));
        package = task.getGuaranteed(task.block, PreDef_Package);
        getSymbolObjectByName(Input)
        checkBlobType(Input, PreDef_Text)
        tokenBegin = pos = reinterpret_cast<const char*>(InputSymbolObject->blobData.get());
        end = pos+InputSymbolObject->blobSize/8;
        while(pos < end) {
            switch(*pos) {
                case '\n':
                    parseToken();
                    column = 0;
                    ++row;
                    break;
                case '\t':
                    column += 3;
                case ' ':
                    parseToken();
                    break;
                case '"':
                    tokenBegin = pos+1;
                    while(true) {
                        if(pos == end)
                            throwException("Unterminated text");
                        bool prev = (*pos != '\\');
                        ++pos;
                        if(prev) {
                            if(*pos == '\\')
                                continue;
                            if(*pos == '"')
                                break;
                        }
                    }
                    parseToken<true>();
                    break;
                case '(':
                    parseToken();
                    parentEntry = currentEntry;
                    currentEntry = new StackEntry();
                    stack.push_back(std::unique_ptr<StackEntry>(currentEntry));
                    break;
                case ';':
                    if(stack.size() == 1)
                        throwException("Semicolon outside of any brackets");
                    seperateTokens<true>();
                    if(currentEntry->unnestEntity != PreDef_Void)
                        throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.size() == 1)
                        throwException("Unmatched closing bracket");
                    seperateTokens<false>();
                    if(stack.size() == 2 && parentEntry->unnestEntity == PreDef_Void) {
                        locals.clear();
                        auto topIter = task.context->topIndex.find(currentEntry->entity);
                        if(topIter != task.context->topIndex.end() && topIter->second->subIndices[EAV].empty())
                            throwException("Nothing declared");
                    }
                    if(currentEntry->unnestEntity != PreDef_Void)
                        throwException("Unnesting failed");
                    stack.pop_back();
                    currentEntry = parentEntry;
                    parentEntry = (stack.size() == 1) ? NULL : (++stack.rbegin())->get();
                }   break;
            }
            ++column;
            ++pos;
        }
        parseToken();

        if(stack.size() > 1)
            throwException("Missing closing bracket");
        if(currentEntry->unnestEntity != PreDef_Void)
            throwException("Unnesting failed");
        if(currentEntry->queue.size() == 0)
            throwException("Empty Input");

        Symbol OutputSymbol;
        if(task.getUncertain(task.block, PreDef_Output, OutputSymbol)) {
            Symbol TargetSymbol = task.popCallStackTargetSymbol();
            task.unlink(TargetSymbol, OutputSymbol);
            while(!currentEntry->queue.empty()) {
                task.link({TargetSymbol, OutputSymbol, currentEntry->queue.front()});
                currentEntry->queue.pop();
            }
        } else
            task.popCallStack();
    }
};
