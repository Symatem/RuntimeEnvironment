#include "Serialize.hpp"

class Deserialize {
    Task& task;
    uint64_t row, column;
    Serialize token;
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

    template<typename T>
    bool parseNumericToken(T& value) {
        // TODO: Remove useage of C++ StdLib
        std::string str(token.charPtr(), token.symbolObject->blobSize/8);
        std::istringstream stream(str);
        stream >> value;
        return stream.eof() && !stream.fail();
    }

    template<bool isText = false>
    void parseToken() {
        if(token.isEmpty())
            return;

        Symbol symbol;
        if(isText)
            symbol = token.getSymbol(true);
        else if(token.beginsWith("#")) {
            token.getSymbol(); // TODO: Refactor token reset

            auto iter = locals.find(token.symbolObject);
            if(iter == locals.end()) {
                symbol = token.getSymbol(true); // TODO: Refactor token reset
                locals.insert(std::make_pair(task.context->getSymbolObject(symbol), symbol));
            } else
                symbol = (*iter).second;
        } else if(token.beginsWith(HRLRawBegin)) {
            ArchitectureType nibbleCount = token.blobSize/8-strlen(HRLRawBegin);
            if(nibbleCount == 0)
                throwException("Empty raw data");
            symbol = task.context->create();
            Context::SymbolObject* symbolObject = task.context->getSymbolObject(symbol);
            symbolObject->allocateBlob(nibbleCount*4);
            uint8_t *dst = reinterpret_cast<uint8_t*>(symbolObject->blobData.get()), odd = 0, nibble;
            const uint8_t *src = reinterpret_cast<const uint8_t*>(token.symbolObject->blobData.get())+strlen(HRLRawBegin),
                          *end = src+nibbleCount;
            while(src < end) {
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
            uint64_t uintValue;
            int64_t intValue;
            double floatValue;
            if(parseNumericToken(uintValue))
                symbol = task.context->createFromData(uintValue);
            else if(parseNumericToken(intValue))
                symbol = task.context->createFromData(intValue);
            else if(parseNumericToken(floatValue))
                symbol = task.context->createFromData(floatValue);
            else
                symbol = token.getSymbol(true); // TODO: Refactor token reset
            symbol = task.indexBlob(symbol);
        }

        task.context->link({package, PreDef_Holds, symbol});
        nextSymbol(currentEntry, symbol);
        token.reset(); // TODO: Refactor token reset
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
    Deserialize(Task& _task) :task(_task), token(_task) {
        row = column = 1;
        stack.clear();
        locals.clear();
        currentEntry = new StackEntry();
        stack.push_back(std::unique_ptr<StackEntry>(currentEntry));
        package = task.getGuaranteed(task.block, PreDef_Package);
        getSymbolObjectByName(Input)
        checkBlobType(Input, PreDef_Text)
        const char *pos = reinterpret_cast<const char*>(InputSymbolObject->blobData.get()),
                   *end = pos+InputSymbolObject->blobSize/8;
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
                        token.put(*pos);
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
                default:
                    token.put(*pos);
                    break;
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

    ~Deserialize() {
        task.destroy(token.symbol);
    }
};
