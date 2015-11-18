#include "Task.hpp"

class Deserialize {
    Task& task;
    uint64_t row, column;
    std::string token;
    Symbol package;

    void throwException(const char* message) {
        task.throwException(message, {
            {PreDef_Row, task.symbolFor(row)},
            {PreDef_Column, task.symbolFor(column)}
        });
    }

    struct StackEntry {
        Symbol entity, unnestEntity, unnestAttribute;
        std::queue<Symbol> queue;
        StackEntry() :entity(PreDef_Void), unnestEntity(PreDef_Void) {}
    };
    std::vector<std::unique_ptr<StackEntry>> stack;
    std::map<std::string, Symbol> locals;

    void nextSymbol(StackEntry* stackEntry, Symbol symbol) {
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
        std::istringstream stream(token);
        stream >> value;
        return stream.eof() && !stream.fail();
    }

    template<bool isText = false>
    void parseToken(StackEntry* currentEntry) {
        if(token.size() == 0) return;

        Symbol symbol;
        if(isText)
            symbol = task.symbolFor(token);
        else if(token[0] == '#') {
            auto iter = locals.find(token);
            if(iter == locals.end()) {
                symbol = task.context->create();
                locals.insert(std::make_pair(token, symbol));
            } else
                symbol = (*iter).second;
        } else if(token.compare(0, 4, "raw:") == 0) {
            if(token.size() % 2 == 1)
                throwException("Odd count of characters in raw data");
            Blob blob;
            blob.allocate((token.size()-4)*4);
            uint8_t *ptr = reinterpret_cast<uint8_t*>(blob.data.get()), byte = 0;
            for(size_t i = 4; i < token.size(); ++i) {
                char current = token[i];
                if(current >= '0' && current <= '9')
                    byte |= current-'0';
                else if(current >= 'A' && current <= 'F')
                    byte |= current-'A'+0xA;
                else
                    throwException("Non hex characters");
                if(i % 2 == 1) ptr[(i-4)/2] = byte;
                byte <<= 4;
            }
            symbol = task.context->symbolFor<PreDef_Void>(std::move(blob));
        } else {
            uint64_t uintValue;
            int64_t intValue;
            double floatValue;
            if(parseNumericToken(uintValue))
                symbol = task.symbolFor(uintValue);
            else if(parseNumericToken(intValue))
                symbol = task.symbolFor(intValue);
            else if(parseNumericToken(floatValue))
                symbol = task.symbolFor(floatValue);
            else
                symbol = task.symbolFor(token);
        }

        task.context->link({package, PreDef_Holds, symbol});
        nextSymbol(currentEntry, symbol);
        token = "";
    }

    void fillInAnonymous(StackEntry* parentEntry, StackEntry* currentEntry) {
        if(currentEntry->entity != PreDef_Void) return;
        currentEntry->entity = task.context->create();
        task.link({package, PreDef_Holds, currentEntry->entity});
        nextSymbol(parentEntry, currentEntry->entity);
    }

    template<bool semicolon>
    void seperateTokens(StackEntry* parentEntry, StackEntry* currentEntry) {
        parseToken(currentEntry);

        if(currentEntry->queue.size() == 0) {
            if(semicolon) {
                if(currentEntry->entity != PreDef_Void)
                    throwException("Pointless semicolon");
                fillInAnonymous(parentEntry, currentEntry);
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

            fillInAnonymous(parentEntry, currentEntry);

            parentEntry->unnestEntity = currentEntry->entity;
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
        token = "";
        stack.clear();
        locals.clear();

        StackEntry *parentEntry, *currentEntry = new StackEntry();
        stack.push_back(std::unique_ptr<StackEntry>(currentEntry));

        package = task.getGuaranteed(task.block, PreDef_Package);
        getSymbolAndBlobByName(Input)
        checkBlobType(Input, PreDef_Text)
        const char *pos = reinterpret_cast<const char*>(InputBlob->data.get()), *end = pos+InputBlob->size/8;
        while(pos < end) {
            if(std::isspace(*pos)) {
                parseToken(currentEntry);
                switch(*pos) {
                    case '\n':
                        column = 1;
                        ++row;
                    break;
                    case ' ':
                        ++column;
                    break;
                    case '\t':
                        column += 4;
                    break;
                    default:
                        throwException("Illegal white space");
                }
            } else {
                switch(*pos) {
                    case '"':
                        while(true) {
                            if(pos == end)
                                throwException("Unterminated text");
                            bool prev = (*pos != '\\');
                            ++pos;
                            if(prev) {
                                if(*pos == '\\') continue;
                                if(*pos == '"') break;
                            }
                            token += *pos;
                        }
                        parseToken<true>(currentEntry);
                    break;
                    case '(':
                        parseToken(currentEntry);
                        parentEntry = currentEntry;
                        currentEntry = new StackEntry();
                        stack.push_back(std::unique_ptr<StackEntry>(currentEntry));
                    break;
                    case ';':
                        if(stack.size() == 1)
                            throwException("Semicolon outside of any brackets");
                        seperateTokens<true>(parentEntry, currentEntry);
                        if(currentEntry->unnestEntity != PreDef_Void)
                            throwException("Unnesting failed");
                        parentEntry->unnestEntity = PreDef_Void;
                    break;
                    case ')': {
                        if(stack.size() == 1)
                            throwException("Unmatched closing bracket");
                        seperateTokens<false>(parentEntry, currentEntry);
                        if(stack.size() == 2 && parentEntry->unnestEntity == PreDef_Void) {
                            locals.clear();
                            auto topIter = task.context->topIndex.find(currentEntry->entity);
                            if(topIter != task.context->topIndex.end() && topIter->second->subIndices[EAV].empty())
                                throwException("Nothing declared");
                        }
                        if(currentEntry->unnestEntity != PreDef_Void)
                            throwException("Unnesting failed");
                        if(currentEntry->entity == PreDef_Void)
                            fillInAnonymous(parentEntry, currentEntry);
                        stack.pop_back();
                        currentEntry = parentEntry;
                        parentEntry = (++stack.rbegin())->get();
                    } break;
                    default:
                        token += *pos;
                    break;
                }
                ++column;
            }
            ++pos;
        }
        parseToken(currentEntry);

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
