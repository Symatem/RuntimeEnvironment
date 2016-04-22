#include "Serialize.hpp"

#define getSymbolByName(name, Name) \
    Symbol name = thread.getGuaranteed(thread.block, Ontology::Name##Symbol);

#define checkBlobType(name, expectedType) \
if(Ontology::query(1, {name, Ontology::BlobTypeSymbol, expectedType}) == 0) \
    thread.throwException("Invalid Blob Type");

#define getUncertainValueByName(name, Name, DefaultValue) \
    Symbol name; \
    NativeNaturalType name##Value = DefaultValue; \
    if(Ontology::getUncertain(thread.block, Ontology::Name##Symbol, name)) { \
        checkBlobType(name, Ontology::NaturalSymbol) \
        name##Value = thread.readBlob<NativeNaturalType>(name); \
    }

struct Deserialize {
    Thread& thread;
    Symbol input, package, parentEntry, currentEntry;
    Ontology::BlobIndex<false> locals;
    Ontology::BlobVector<false, Symbol> stack, queue;
    NativeNaturalType tokenBegin, pos, end, row, column;

    void throwException(const char* message) {
        Symbol data = Storage::createSymbol(),
               rowSymbol = Ontology::createFromData(row),
               columnSymbol = Ontology::createFromData(column);
        thread.link({data, Ontology::HoldsSymbol, rowSymbol});
        thread.link({data, Ontology::HoldsSymbol, columnSymbol});
        thread.link({data, Ontology::RowSymbol, rowSymbol});
        thread.link({data, Ontology::ColumnSymbol, columnSymbol});
        thread.throwException(message, data);
    }

    void nextSymbol(Symbol stackEntry, Symbol symbol) {
        if(thread.valueCountIs(stackEntry, Ontology::UnnestEntitySymbol, 0)) {
            queue.symbol = stackEntry;
            queue.insert(0, symbol);
            queue.symbol = currentEntry;
        } else {
            Symbol entity = thread.getGuaranteed(stackEntry, Ontology::UnnestEntitySymbol),
                   attribute = thread.getGuaranteed(stackEntry, Ontology::UnnestAttributeSymbol);
            thread.link({entity, attribute, symbol});
            Ontology::setSolitary({stackEntry, Ontology::UnnestEntitySymbol, Ontology::VoidSymbol});
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
                Storage::increaseBlobSize(symbol, 0, nibbleCount*4);
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
            Ontology::link({package, Ontology::HoldsSymbol, symbol});
            nextSymbol(currentEntry, symbol);
        }
        tokenBegin = pos+1;
    }

    void fillInAnonymous(Symbol& entity) {
        if(entity != Ontology::VoidSymbol)
            return;
        entity = Storage::createSymbol();
        thread.link({currentEntry, Ontology::EntitySymbol, entity});
        thread.link({package, Ontology::HoldsSymbol, entity});
        nextSymbol(parentEntry, entity);
    }

    void seperateTokens(bool semicolon) {
        parseToken();
        Symbol entity = Ontology::VoidSymbol;
        Ontology::getUncertain(currentEntry, Ontology::EntitySymbol, entity);
        if(queue.empty()) {
            if(semicolon) {
                if(entity != Ontology::VoidSymbol)
                    throwException("Pointless semicolon");
                fillInAnonymous(entity);
            }
            return;
        }
        if(semicolon && queue.size() == 1) {
            if(entity == Ontology::VoidSymbol) {
                entity = queue.pop_back();
                thread.link({currentEntry, Ontology::EntitySymbol, entity});
                nextSymbol(parentEntry, entity);
            } else
                thread.link({entity, queue.pop_back(), entity});
            return;
        }
        fillInAnonymous(entity);
        if(semicolon)
            Ontology::setSolitary({parentEntry, Ontology::UnnestEntitySymbol, Ontology::VoidSymbol});
        else
            Ontology::setSolitary({parentEntry, Ontology::UnnestEntitySymbol, entity}, true);
        Symbol attribute = queue.pop_back();
        Ontology::setSolitary({parentEntry, Ontology::UnnestAttributeSymbol, attribute}, true);
        while(!queue.empty())
            thread.link({entity, attribute, queue.pop_back()});
    }

    Deserialize(Thread& _thread) :thread(_thread) {
        package = thread.getGuaranteed(thread.block, Ontology::PackageSymbol);
        input = thread.getGuaranteed(thread.block, Ontology::InputSymbol);
        if(Ontology::query(1, {input, Ontology::BlobTypeSymbol, Ontology::TextSymbol}) == 0)
            thread.throwException("Invalid Blob Type");
        locals.symbol = Storage::createSymbol();
        thread.link({thread.block, Ontology::HoldsSymbol, locals.symbol});
        stack.symbol = Storage::createSymbol();
        thread.link({thread.block, Ontology::HoldsSymbol, stack.symbol});
        currentEntry = Storage::createSymbol();
        thread.link({thread.block, Ontology::HoldsSymbol, currentEntry});
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
                    thread.link({thread.block, Ontology::HoldsSymbol, currentEntry});
                    stack.push_back(currentEntry);
                    queue.symbol = currentEntry;
                    break;
                case ';':
                    if(stack.size() == 1)
                        throwException("Semicolon outside of any brackets");
                    seperateTokens(true);
                    if(!thread.valueCountIs(currentEntry, Ontology::UnnestEntitySymbol, 0))
                        throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.size() == 1)
                        throwException("Unmatched closing bracket");
                    seperateTokens(false);
                    if(stack.size() == 2 && thread.valueCountIs(parentEntry, Ontology::UnnestEntitySymbol, 0)) {
                        locals.clear();
                        if(Ontology::query(12, {thread.getGuaranteed(currentEntry, Ontology::EntitySymbol), Ontology::VoidSymbol, Ontology::VoidSymbol}) == 0)
                            throwException("Nothing declared");
                    }
                    if(!thread.valueCountIs(currentEntry, Ontology::UnnestEntitySymbol, 0))
                        throwException("Unnesting failed");
                    Ontology::unlink(currentEntry);
                    stack.pop_back();
                    currentEntry = parentEntry;
                    queue.symbol = currentEntry;
                    parentEntry = (stack.size() < 2) ? Ontology::VoidSymbol : stack.readElementAt(stack.size()-2);
                }   break;
            }
            ++column;
        }
        parseToken();
        if(stack.size() != 1)
            throwException("Missing closing bracket");
        if(!thread.valueCountIs(currentEntry, Ontology::UnnestEntitySymbol, 0))
            throwException("Unnesting failed");
        if(queue.empty())
            throwException("Empty Input");
        Symbol Output;
        if(Ontology::getUncertain(thread.block, Ontology::OutputSymbol, Output)) {
            Symbol Target = thread.getTargetSymbol();
            Ontology::setSolitary({Target, Output, Ontology::VoidSymbol});
            while(!queue.empty())
                thread.link({Target, Output, queue.pop_back()});
        }
        thread.popCallStack();
    }
};
