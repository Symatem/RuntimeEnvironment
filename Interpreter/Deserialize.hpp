#include "Serialize.hpp"

#define getSymbolByName(Name) \
    Symbol Name##Symbol = thread.getGuaranteed(thread.block, PreDef_##Name);

#define checkBlobType(Name, expectedType) \
if(Ontology::query(1, {Name##Symbol, PreDef_BlobType, expectedType}) == 0) \
    thread.throwException("Invalid Blob Type");

#define getUncertainValueByName(Name, DefaultValue) \
    Symbol Name##Symbol; \
    NativeNaturalType Name##Value = DefaultValue; \
    if(Ontology::getUncertain(thread.block, PreDef_##Name, Name##Symbol)) { \
        checkBlobType(Name, PreDef_Natural) \
        Name##Value = thread.readBlob<NativeNaturalType>(Name##Symbol); \
    }

class Deserialize {
    Thread& thread;
    Symbol input, package, parentEntry, currentEntry;
    BlobIndex<false> locals;
    Vector<false, Symbol> stack, queue;
    NativeNaturalType tokenBegin, pos, end, row, column;

    void throwException(const char* message) {
        Symbol data = Storage::createSymbol(),
               rowSymbol = Ontology::createFromData(row),
               columnSymbol = Ontology::createFromData(column);
        thread.link({data, PreDef_Holds, rowSymbol});
        thread.link({data, PreDef_Holds, columnSymbol});
        thread.link({data, PreDef_Row, rowSymbol});
        thread.link({data, PreDef_Column, columnSymbol});
        thread.throwException(message, data);
    }

    void nextSymbol(Symbol stackEntry, Symbol symbol) {
        if(thread.valueCountIs(stackEntry, PreDef_UnnestEntity, 0)) {
            queue.symbol = stackEntry;
            queue.insert(0, symbol);
            queue.symbol = currentEntry;
        } else {
            Symbol entity = thread.getGuaranteed(stackEntry, PreDef_UnnestEntity),
                       attribute = thread.getGuaranteed(stackEntry, PreDef_UnnestAttribute);
            thread.link({entity, attribute, symbol});
            Ontology::setSolitary({stackEntry, PreDef_UnnestEntity, PreDef_Void});
        }
    }

    Symbol sliceText() {
        return Ontology::createFromSlice(input, tokenBegin*8, (pos-tokenBegin)*8);
    }

    bool tokenBeginsWithString(const char* str) {
        if(strlen(str) > pos-tokenBegin)
            return false;
        for(NativeNaturalType i = 0; i < strlen(str); ++i)
            if(Storage::readBlobAt<char>(input, tokenBegin+i) != str[i])
                return false;
        return true;
    }

    void parseToken(bool isText = false) {
        if(pos > tokenBegin) {
            char src = Storage::readBlobAt<char>(input, tokenBegin);
            Symbol symbol;
            if(isText)
                symbol = sliceText();
            else if(src == '#') {
                symbol = sliceText();
                locals.insertElement(symbol);
            } else if(tokenBeginsWithString(HRLRawBegin)) {
                tokenBegin += strlen(HRLRawBegin);
                NativeNaturalType nibbleCount = pos-tokenBegin;
                if(nibbleCount == 0)
                    throwException("Empty raw data");
                symbol = Storage::createSymbol();
                Storage::setBlobSize(symbol, nibbleCount*4);
                char nibble;
                NativeNaturalType at = 0;
                while(tokenBegin < pos) {
                    src = Storage::readBlobAt<char>(input, tokenBegin);
                    if(src >= '0' && src <= '9')
                        nibble = src-'0';
                    else if(src >= 'A' && src <= 'F')
                        nibble = src-'A'+0xA;
                    else
                        throwException("Non hex characters");
                    if(at%2 == 0)
                        Storage::writeBlobAt<char>(symbol, at/2, nibble);
                    else
                        Storage::writeBlobAt<char>(symbol, at/2, Storage::readBlobAt<char>(symbol, at/2)|(nibble<<4));
                    ++at;
                    ++tokenBegin;
                }
                Storage::modifiedBlob(symbol);
            } else {
                NativeNaturalType mantissa = 0, devisor = 0;
                bool isNumber = true, negative = (src == '-');
                if(negative)
                    ++tokenBegin;
                // TODO What if too long, precision loss?
                while(tokenBegin < pos) {
                    src = Storage::readBlobAt<char>(input, tokenBegin);
                    devisor *= 10;
                    if(src >= '0' && src <= '9') {
                        mantissa *= 10;
                        mantissa += src-'0';
                    } else if(src == '.') {
                        if(devisor > 0) {
                            isNumber = false;
                            break;
                        }
                        devisor = 1;
                    } else {
                        isNumber = false;
                        break;
                    }
                    ++tokenBegin;
                }
                if(isNumber && devisor != 1) {
                    if(devisor > 0) {
                        NativeFloatType value = mantissa;
                        value /= devisor;
                        if(negative)
                            value *= -1;
                        symbol = Ontology::createFromData(value);
                    } else if(negative)
                        symbol = Ontology::createFromData(-static_cast<NativeIntegerType>(mantissa));
                    else
                        symbol = Ontology::createFromData(mantissa);
                } else
                    symbol = sliceText();
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
        entity = Storage::createSymbol();
        thread.link({currentEntry, PreDef_Entity, entity});
        thread.link({package, PreDef_Holds, entity});
        nextSymbol(parentEntry, entity);
    }

    void seperateTokens(bool semicolon) {
        parseToken();
        Symbol entity = PreDef_Void;
        Ontology::getUncertain(currentEntry, PreDef_Entity, entity);
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
            Ontology::setSolitary({parentEntry, PreDef_UnnestEntity, PreDef_Void});
        else
            Ontology::setSolitary({parentEntry, PreDef_UnnestEntity, entity}, true);
        Symbol attribute = queue.pop_back();
        Ontology::setSolitary({parentEntry, PreDef_UnnestAttribute, attribute}, true);
        while(!queue.empty())
            thread.link({entity, attribute, queue.pop_back()});
    }

    public:
    Deserialize(Thread& _thread) :thread(_thread) {
        package = thread.getGuaranteed(thread.block, PreDef_Package);
        input = thread.getGuaranteed(thread.block, PreDef_Input);
        if(Ontology::query(1, {input, PreDef_BlobType, PreDef_Text}) == 0)
            thread.throwException("Invalid Blob Type");
        locals.symbol = Storage::createSymbol();
        thread.link({thread.block, PreDef_Holds, locals.symbol});
        stack.symbol = Storage::createSymbol();
        thread.link({thread.block, PreDef_Holds, stack.symbol});
        currentEntry = Storage::createSymbol();
        thread.link({thread.block, PreDef_Holds, currentEntry});
        stack.push_back(currentEntry);
        queue.symbol = currentEntry;
        row = column = 1;
        end = Storage::getBlobSize(input)/8;
        for(tokenBegin = pos = 0; pos < end; ++pos) {
            char src = Storage::readBlobAt<char>(input, pos);
            switch(src) {
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
                        bool prev = (src != '\\');
                        ++pos;
                        src = Storage::readBlobAt<char>(input, pos);
                        if(prev) {
                            if(src == '\\')
                                continue;
                            if(src == '"')
                                break;
                        }
                    }
                    parseToken(true);
                    break;
                case '(':
                    parseToken();
                    parentEntry = currentEntry;
                    currentEntry = Storage::createSymbol();
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
                    Ontology::unlink(currentEntry);
                    stack.pop_back();
                    currentEntry = parentEntry;
                    queue.symbol = currentEntry;
                    parentEntry = (stack.size() < 2) ? PreDef_Void : stack.readElementAt(stack.size()-2);
                }   break;
            }
            ++column;
        }
        parseToken();
        if(stack.size() != 1)
            throwException("Missing closing bracket");
        if(!thread.valueCountIs(currentEntry, PreDef_UnnestEntity, 0))
            throwException("Unnesting failed");
        if(queue.empty())
            throwException("Empty Input");
        Symbol OutputSymbol;
        if(Ontology::getUncertain(thread.block, PreDef_Output, OutputSymbol)) {
            Symbol TargetSymbol = thread.getTargetSymbol();
            Ontology::setSolitary({TargetSymbol, OutputSymbol, PreDef_Void});
            while(!queue.empty())
                thread.link({TargetSymbol, OutputSymbol, queue.pop_back()});
        }
        thread.popCallStack();
    }
};
