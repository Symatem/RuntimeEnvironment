#include "Serialize.hpp"

#define getSymbolByName(Name) \
    Symbol Name##Symbol = Context::getGuaranteed(task.block, PreDef_##Name);

#define getSymbolObjectByName(Name) \
    getSymbolByName(Name) \
    auto Name##SymbolObject = Context::getSymbolObject(Name##Symbol);

#define checkBlobType(Name, expectedType) \
if(Context::query(1, {Name##Symbol, PreDef_BlobType, expectedType}) == 0) \
    throw Exception("Invalid Blob Type");

#define getUncertainValueByName(Name, DefaultValue) \
    Symbol Name##Symbol; \
    ArchitectureType Name##Value = DefaultValue; \
    if(Context::getUncertain(task.block, PreDef_##Name, Name##Symbol)) { \
        checkBlobType(Name, PreDef_Natural) \
        Name##Value = Context::getSymbolObject(Name##Symbol)->accessBlobAt<ArchitectureType>(); \
    }

class Deserialize {
    Task& task;
    const char *pos, *end, *tokenBegin;
    ArchitectureType row, column;
    Symbol package;

    void throwException(const char* message) {
        throw Exception(message, {
            {PreDef_Row, Context::createFromData(row)},
            {PreDef_Column, Context::createFromData(column)}
        });
    }

    Vector<Symbol, true> stack;
    Vector<Symbol, false> queue;
    BlobIndex locals;
    Symbol parentEntry, currentEntry;

    void nextSymbol(Symbol stackEntry, Symbol symbol) {
        if(Context::valueCountIs(stackEntry, PreDef_UnnestEntity, 0)) {
            queue.setSymbol(stackEntry);
            queue.insert(0, symbol);
            queue.setSymbol(currentEntry);
        } else {
            Symbol entity = Context::getGuaranteed(stackEntry, PreDef_UnnestEntity),
                   attribute = Context::getGuaranteed(stackEntry, PreDef_UnnestAttribute);
            Context::link({entity, attribute, symbol});
            Context::setSolitary({stackEntry, PreDef_UnnestEntity, PreDef_Void});
        }
    }

    void parseToken(bool isText = false) {
        if(pos > tokenBegin) {
            Symbol symbol;
            if(isText)
                symbol = Context::createFromData(tokenBegin, pos-tokenBegin);
            else if(*tokenBegin == '#') {
                symbol = Context::createFromData(tokenBegin, pos-tokenBegin);
                locals.insertElement(symbol);
            } else if(pos-tokenBegin > strlen(HRLRawBegin) && memcmp(tokenBegin, HRLRawBegin, 4) == 0) {
                const char* src = tokenBegin+strlen(HRLRawBegin);
                ArchitectureType nibbleCount = pos-src;
                if(nibbleCount == 0)
                    throwException("Empty raw data");
                symbol = Context::create();
                SymbolObject* symbolObject = Context::getSymbolObject(symbol);
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
                        symbol = Context::createFromData(value);
                    } else if(negative)
                        symbol = Context::createFromData(-static_cast<int64_t>(mantissa));
                    else
                        symbol = Context::createFromData(mantissa);
                } else
                    symbol = Context::createFromData(tokenBegin, pos-tokenBegin);
                Context::blobIndex.insertElement(symbol);
            }
            Context::link({package, PreDef_Holds, symbol}, false);
            nextSymbol(currentEntry, symbol);
        }
        tokenBegin = pos+1;
    }

    void fillInAnonymous(Symbol& entity) {
        if(entity != PreDef_Void)
            return;
        entity = Context::create();
        Context::link({currentEntry, PreDef_Entity, entity});
        Context::link({package, PreDef_Holds, entity});
        nextSymbol(parentEntry, entity);
    }

    void seperateTokens(bool semicolon) {
        parseToken();

        Symbol entity = PreDef_Void;
        Context::getUncertain(currentEntry, PreDef_Entity, entity);

        if(queue.empty()) {
            if(semicolon) {
                if(entity != PreDef_Void)
                    throwException("Pointless semicolon");
                fillInAnonymous(entity);
            }
            return;
        }

        if(semicolon && queue.size() == 1) {
            if(entity == PreDef_Void) {
                entity = queue.pop_back();
                Context::link({currentEntry, PreDef_Entity, entity});
                nextSymbol(parentEntry, entity);
            } else
                Context::link({entity, queue.pop_back(), entity});
            return;
        }

        fillInAnonymous(entity);
        if(semicolon)
            Context::setSolitary({parentEntry, PreDef_UnnestEntity, PreDef_Void});
        else
            Context::setSolitary({parentEntry, PreDef_UnnestEntity, entity}, true);
        Symbol attribute = queue.pop_back();
        Context::setSolitary({parentEntry, PreDef_UnnestAttribute, attribute}, true);

        while(!queue.empty())
            Context::link({entity, attribute, queue.pop_back()});
    }

    public:
    Deserialize(Task& _task) :task(_task) {
        package = Context::getGuaranteed(task.block, PreDef_Package);
        getSymbolObjectByName(Input)
        checkBlobType(Input, PreDef_Text)

        currentEntry = Context::create();
        Context::link({task.block, PreDef_Holds, currentEntry});
        stack.push_back(currentEntry);
        queue.setSymbol(currentEntry);

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
                    currentEntry = Context::create();
                    Context::link({task.block, PreDef_Holds, currentEntry});
                    stack.push_back(currentEntry);
                    queue.setSymbol(currentEntry);
                    break;
                case ';':
                    if(stack.size() == 1)
                        throwException("Semicolon outside of any brackets");
                    seperateTokens(true);
                    if(!Context::valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
                        throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.size() == 1)
                        throwException("Unmatched closing bracket");
                    seperateTokens(false);
                    if(stack.size() == 2 && Context::valueCountIs(parentEntry, PreDef_UnnestEntity, 0)) {
                        locals.clear();
                        if(Context::query(12, {Context::getGuaranteed(currentEntry, PreDef_Entity), PreDef_Void, PreDef_Void}) == 0)
                            throwException("Nothing declared");
                    }
                    if(!Context::valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
                        throwException("Unnesting failed");
                    Context::destroy(currentEntry);
                    stack.pop_back();
                    currentEntry = parentEntry;
                    queue.setSymbol(currentEntry);
                    parentEntry = (stack.size() < 2) ? PreDef_Void : stack[stack.size()-2];
                }   break;
            }
            ++column;
            ++pos;
        }
        parseToken();

        if(stack.size() != 1)
            throwException("Missing closing bracket");
        if(!Context::valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
            throwException("Unnesting failed");
        if(queue.empty())
            throwException("Empty Input");

        Symbol OutputSymbol;
        if(Context::getUncertain(task.block, PreDef_Output, OutputSymbol)) {
            Symbol TargetSymbol = task.getTargetSymbol();
            Context::setSolitary({TargetSymbol, OutputSymbol, PreDef_Void});
            while(!queue.empty())
                Context::link({TargetSymbol, OutputSymbol, queue.pop_back()});
        }
        task.popCallStack();
    }
};
