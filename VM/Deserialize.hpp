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

    std::vector<Symbol> stack;
    Symbol parentEntry, currentEntry;
    std::map<Context::SymbolObject*, Symbol, Context::BlobIndexCompare> locals;

    bool isStackSize(ArchitectureType size) {
        return stack.size() == size;
    }

    Symbol popQueue() {
        Symbol oldElement = task.getGuaranteed(currentEntry, PreDef_Queue),
               symbol = task.getGuaranteed(oldElement, PreDef_Value),
               queueBegin = oldElement;
        if(task.getUncertain(queueBegin, PreDef_Next, queueBegin))
            task.setSolitary({currentEntry, PreDef_Queue, queueBegin});
        else
            task.unlink(currentEntry, PreDef_Queue);
        task.destroy(oldElement);
        return symbol;
    }

    void nextSymbol(Symbol stackEntry, Symbol symbol) {
        if(task.valueSetCountIs(stackEntry, PreDef_UnnestEntity, 0)) {
            Symbol queueEnd, newElement = task.context->create({{PreDef_Value, symbol}});
            if(task.getUncertain(stackEntry, PreDef_Queue, queueEnd)) {
                while(task.getUncertain(queueEnd, PreDef_Next, queueEnd));
                task.link({queueEnd, PreDef_Next, newElement});
            } else
                task.link({stackEntry, PreDef_Queue, newElement});
        } else {
            Symbol entity = task.getGuaranteed(stackEntry, PreDef_UnnestEntity),
                   attribute = task.getGuaranteed(stackEntry, PreDef_UnnestAttribute);
            if(!task.context->link({entity, attribute, symbol}))
                throwException("Triple defined twice via unnesting");
            task.unlink(stackEntry, PreDef_UnnestEntity);
        }
    }

    void parseToken(bool isText = false) {
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
            } else if(pos-tokenBegin > strlen(HRLRawBegin) && memcmp(tokenBegin, HRLRawBegin, 4) == 0) {
                const char* src = tokenBegin+strlen(HRLRawBegin);
                ArchitectureType nibbleCount = pos-src;
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

    void fillInAnonymous(Symbol& entity) {
        if(entity != PreDef_Void)
            return;
        entity = task.context->create();
        task.context->link({currentEntry, PreDef_Entity, entity});
        task.context->link({package, PreDef_Holds, entity});
        nextSymbol(parentEntry, entity);
    }

    void seperateTokens(bool semicolon) {
        parseToken();

        Symbol entity, queue;
        if(!task.getUncertain(currentEntry, PreDef_Entity, entity))
            entity = PreDef_Void;
        if(!task.getUncertain(currentEntry, PreDef_Queue, queue))
            queue = PreDef_Void;

        if(queue == PreDef_Void) {
            if(semicolon) {
                if(entity != PreDef_Void)
                    throwException("Pointless semicolon");
                fillInAnonymous(entity);
            }
            return;
        }

        if(semicolon && queue != PreDef_Void && task.valueSetCountIs(queue, PreDef_Next, 0)) {
            if(entity == PreDef_Void) {
                entity = popQueue();
                task.context->link({currentEntry, PreDef_Entity, entity});
                nextSymbol(parentEntry, entity);
            } else if(!task.context->link({entity, popQueue(), entity}))
                throwException("Triple defined twice via self reference");
            return;
        }

        fillInAnonymous(entity);
        if(semicolon)
            task.unlink(parentEntry, PreDef_UnnestEntity);
        else
            task.setSolitary({parentEntry, PreDef_UnnestEntity, entity});
        Symbol attribute = popQueue();
        task.setSolitary({parentEntry, PreDef_UnnestAttribute, attribute});

        while(!task.valueSetCountIs(currentEntry, PreDef_Queue, 0))
            if(!task.context->link({entity, attribute, popQueue()}))
                throwException("Triple defined twice");
    }

    public:
    Deserialize(Task& _task) :task(_task) {
        package = task.getGuaranteed(task.block, PreDef_Package);
        getSymbolObjectByName(Input)
        checkBlobType(Input, PreDef_Text)

        currentEntry = task.context->create();
        task.link({package, PreDef_Holds, currentEntry});
        stack.push_back(currentEntry);

        row = column = 1;
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
                    parseToken(true);
                    break;
                case '(':
                    parseToken();
                    parentEntry = currentEntry;
                    currentEntry = task.context->create();
                    task.link({package, PreDef_Holds, currentEntry});
                    stack.push_back(currentEntry);
                    break;
                case ';':
                    if(isStackSize(1))
                        throwException("Semicolon outside of any brackets");
                    seperateTokens(true);
                    if(!task.valueSetCountIs(currentEntry, PreDef_UnnestEntity, 0))
                        throwException("Unnesting failed");
                    break;
                case ')': {
                    if(isStackSize(1))
                        throwException("Unmatched closing bracket");
                    seperateTokens(false);
                    if(isStackSize(2) && task.valueSetCountIs(parentEntry, PreDef_UnnestEntity, 0)) {
                        locals.clear();
                        auto topIter = task.context->topIndex.find(task.getGuaranteed(currentEntry, PreDef_Entity));
                        if(topIter != task.context->topIndex.end() && topIter->second->subIndices[EAV].empty())
                            throwException("Nothing declared");
                    }
                    if(!task.valueSetCountIs(currentEntry, PreDef_UnnestEntity, 0))
                        throwException("Unnesting failed");
                    task.destroy(currentEntry);
                    stack.pop_back();
                    currentEntry = parentEntry;
                    parentEntry = (isStackSize(1)) ? PreDef_Void : *(++stack.rbegin());
                }   break;
            }
            ++column;
            ++pos;
        }
        parseToken();

        if(!isStackSize(1))
            throwException("Missing closing bracket");
        if(!task.valueSetCountIs(currentEntry, PreDef_UnnestEntity, 0))
            throwException("Unnesting failed");
        if(task.valueSetCountIs(currentEntry, PreDef_Queue, 0))
            throwException("Empty Input");

        Symbol OutputSymbol;
        if(task.getUncertain(task.block, PreDef_Output, OutputSymbol)) {
            Symbol TargetSymbol = task.popCallStackTargetSymbol();
            task.unlink(TargetSymbol, OutputSymbol);
            while(!task.valueSetCountIs(currentEntry, PreDef_Queue, 0))
                task.link({TargetSymbol, OutputSymbol, popQueue()});
        } else
            task.popCallStack();
    }
};
