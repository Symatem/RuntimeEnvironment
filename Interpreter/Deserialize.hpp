#include "Serialize.hpp"

#define getSymbolByName(Name) \
    Symbol Name##Symbol = thread.getGuaranteed(thread.block, PreDef_##Name);

#define checkBlobType(Name, expectedType) \
if(Ontology::query(1, {Name##Symbol, PreDef_BlobType, expectedType}) == 0) \
    thread.throwException("Invalid Blob Type");

#define getUncertainValueByName(Name, DefaultValue) \
    Symbol Name##Symbol; \
    ArchitectureType Name##Value = DefaultValue; \
    if(thread.getUncertain(thread.block, PreDef_##Name, Name##Symbol)) { \
        checkBlobType(Name, PreDef_Natural) \
        Name##Value = thread.accessBlobAs<ArchitectureType>(Name##Symbol); \
    }

class Deserialize {
    Thread& thread;
    const char *pos, *end, *tokenBegin;
    ArchitectureType row, column;
    Symbol package;

    void throwException(const char* message) {
        Symbol data = Ontology::create(),
               rowSymbol = Ontology::createFromData(row),
               columnSymbol = Ontology::createFromData(column);
        thread.link({data, PreDef_Holds, rowSymbol});
        thread.link({data, PreDef_Holds, columnSymbol});
        thread.link({data, PreDef_Row, rowSymbol});
        thread.link({data, PreDef_Column, columnSymbol});
        thread.throwException(message, data);
    }

    Vector<Symbol, false> stack;
    Vector<Symbol, false> queue;
    BlobIndex locals;
    Symbol parentEntry, currentEntry;

    void nextSymbol(Symbol stackEntry, Symbol symbol) {
        if(thread.valueCountIs(stackEntry, PreDef_UnnestEntity, 0)) {
            queue.symbol = stackEntry;
            queue.insert(0, symbol);
            queue.symbol = currentEntry;
        } else {
            Symbol entity = thread.getGuaranteed(stackEntry, PreDef_UnnestEntity),
                   attribute = thread.getGuaranteed(stackEntry, PreDef_UnnestAttribute);
            thread.link({entity, attribute, symbol});
            thread.setSolitary({stackEntry, PreDef_UnnestEntity, PreDef_Void});
        }
    }

    void parseToken(bool isText = false) {
        if(pos > tokenBegin) {
            Symbol symbol;
            if(isText)
                symbol = Ontology::createFromData(tokenBegin, pos-tokenBegin);
            else if(*tokenBegin == '#') {
                symbol = Ontology::createFromData(tokenBegin, pos-tokenBegin);
                locals.insertElement(symbol);
            } else if(pos-tokenBegin > strlen(HRLRawBegin) && memcmp(tokenBegin, HRLRawBegin, 4) == 0) {
                const char* src = tokenBegin+strlen(HRLRawBegin);
                ArchitectureType nibbleCount = pos-src;
                if(nibbleCount == 0)
                    throwException("Empty raw data");
                symbol = Ontology::create();
                Ontology::allocateBlob(symbol, nibbleCount*4);
                char* dst = reinterpret_cast<char*>(Ontology::accessBlobData(symbol)), odd = 0, nibble;
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
                        symbol = Ontology::createFromData(value);
                    } else if(negative)
                        symbol = Ontology::createFromData(-static_cast<int64_t>(mantissa));
                    else
                        symbol = Ontology::createFromData(mantissa);
                } else
                    symbol = Ontology::createFromData(tokenBegin, pos-tokenBegin);
                Ontology::blobIndex.insertElement(symbol);
            }
            Ontology::link({package, PreDef_Holds, symbol});
            nextSymbol(currentEntry, symbol);
        }
        tokenBegin = pos+1;
    }

    void fillInAnonymous(Symbol& entity) {
        if(entity != PreDef_Void)
            return;
        entity = Ontology::create();
        thread.link({currentEntry, PreDef_Entity, entity});
        thread.link({package, PreDef_Holds, entity});
        nextSymbol(parentEntry, entity);
    }

    void seperateTokens(bool semicolon) {
        parseToken();

        Symbol entity = PreDef_Void;
        thread.getUncertain(currentEntry, PreDef_Entity, entity);

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
                thread.link({currentEntry, PreDef_Entity, entity});
                nextSymbol(parentEntry, entity);
            } else
                thread.link({entity, queue.pop_back(), entity});
            return;
        }

        fillInAnonymous(entity);
        if(semicolon)
            thread.setSolitary({parentEntry, PreDef_UnnestEntity, PreDef_Void});
        else
            thread.setSolitary({parentEntry, PreDef_UnnestEntity, entity}, true);
        Symbol attribute = queue.pop_back();
        thread.setSolitary({parentEntry, PreDef_UnnestAttribute, attribute}, true);

        while(!queue.empty())
            thread.link({entity, attribute, queue.pop_back()});
    }

    public:
    Deserialize(Thread& _thread) :thread(_thread) {
        package = thread.getGuaranteed(thread.block, PreDef_Package);
        getSymbolByName(Input)
        checkBlobType(Input, PreDef_Text)

        stack.symbol = Ontology::create();
        thread.link({thread.block, PreDef_Holds, stack.symbol});
        currentEntry = Ontology::create();
        thread.link({thread.block, PreDef_Holds, currentEntry});
        stack.push_back(currentEntry);
        queue.symbol = currentEntry;

        row = column = 1;
        tokenBegin = pos = reinterpret_cast<const char*>(Ontology::accessBlobData(InputSymbol));
        end = pos+Ontology::accessBlobSize(InputSymbol)/8;
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
                    currentEntry = Ontology::create();
                    thread.link({thread.block, PreDef_Holds, currentEntry});
                    stack.push_back(currentEntry);
                    queue.symbol = currentEntry;
                    break;
                case ';':
                    if(stack.size() == 1)
                        throwException("Semicolon outside of any brackets");
                    seperateTokens(true);
                    if(!thread.valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
                        throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.size() == 1)
                        throwException("Unmatched closing bracket");
                    seperateTokens(false);
                    if(stack.size() == 2 && thread.valueCountIs(parentEntry, PreDef_UnnestEntity, 0)) {
                        locals.clear();
                        if(Ontology::query(12, {thread.getGuaranteed(currentEntry, PreDef_Entity), PreDef_Void, PreDef_Void}) == 0)
                            throwException("Nothing declared");
                    }
                    if(!thread.valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
                        throwException("Unnesting failed");
                    Ontology::destroy(currentEntry);
                    stack.pop_back();
                    currentEntry = parentEntry;
                    queue.symbol = currentEntry;
                    parentEntry = (stack.size() < 2) ? PreDef_Void : stack[stack.size()-2];
                }   break;
            }
            ++column;
            ++pos;
        }
        parseToken();

        if(stack.size() != 1)
            throwException("Missing closing bracket");
        if(!thread.valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
            throwException("Unnesting failed");
        if(queue.empty())
            throwException("Empty Input");

        Symbol OutputSymbol;
        if(thread.getUncertain(thread.block, PreDef_Output, OutputSymbol)) {
            Symbol TargetSymbol = thread.getTargetSymbol();
            thread.setSolitary({TargetSymbol, OutputSymbol, PreDef_Void});
            while(!queue.empty())
                thread.link({TargetSymbol, OutputSymbol, queue.pop_back()});
        }
        thread.popCallStack();
    }
};
