#include "Serialize.hpp"

struct Deserializer {
    Thread& thread;
    Symbol input, package, parentEntry, currentEntry;
    Ontology::BlobIndex<false> locals;
    Ontology::BlobVector<false, Symbol> stack, queue;
    NativeNaturalType tokenBegin, pos, end, row, column;

    Deserializer(Thread& _thread) :thread(_thread) {}

    bool throwException(const char* message) {
        Symbol data = Storage::createSymbol(),
               rowSymbol = Ontology::createFromData(row),
               columnSymbol = Ontology::createFromData(column);
        Ontology::link({data, Ontology::HoldsSymbol, rowSymbol});
        Ontology::link({data, Ontology::HoldsSymbol, columnSymbol});
        Ontology::link({data, Ontology::RowSymbol, rowSymbol});
        Ontology::link({data, Ontology::ColumnSymbol, columnSymbol});
        return thread.throwException(message, data);
    }

    void nextSymbol(Symbol stackEntry, Symbol symbol) {
        if(Ontology::valueCountIs(stackEntry, Ontology::UnnestEntitySymbol, 0)) {
            queue.symbol = stackEntry;
            queue.insert(0, symbol);
            queue.symbol = currentEntry;
        } else {
            Symbol entity, attribute;
            assert(thread.getGuaranteed(stackEntry, Ontology::UnnestEntitySymbol, entity));
            assert(thread.getGuaranteed(stackEntry, Ontology::UnnestAttributeSymbol, attribute));
            Ontology::link({entity, attribute, symbol});
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
            if(Storage::readBlobAt<Natural8>(input, tokenBegin+i) != str[i])
                return false;
        return true;
    }

    bool parseToken(bool isText = false) {
        if(pos > tokenBegin) {
            Natural8 src = Storage::readBlobAt<Natural8>(input, tokenBegin);
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
                    return throwException("Empty raw data");
                if(nibbleCount%2 == 1)
                    return throwException("Count of nibbles must be even");
                symbol = Storage::createSymbol();
                Storage::increaseBlobSize(symbol, 0, nibbleCount*4);
                Natural8 nibble, byte;
                NativeNaturalType at = 0;
                while(tokenBegin < pos) {
                    src = Storage::readBlobAt<Natural8>(input, tokenBegin);
                    if(src >= '0' && src <= '9')
                        nibble = src-'0';
                    else if(src >= 'A' && src <= 'F')
                        nibble = src-'A'+0xA;
                    else
                        return throwException("Non hex characters");
                    if(at%2 == 0)
                        byte = nibble;
                    else {
                        Storage::writeBlobAt<Natural8>(symbol, at/2, byte|(nibble<<4));
                        byte = 0;
                    }
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
                    src = Storage::readBlobAt<Natural8>(input, tokenBegin);
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
        return true;
    }

    bool fillInAnonymous(Symbol& entity) {
        if(entity != Ontology::VoidSymbol)
            return false;
        entity = Storage::createSymbol();
        Ontology::link({currentEntry, Ontology::EntitySymbol, entity});
        Ontology::link({package, Ontology::HoldsSymbol, entity});
        nextSymbol(parentEntry, entity);
        return true;
    }

    bool seperateTokens(bool semicolon) {
        checkReturn(parseToken());
        Symbol entity = Ontology::VoidSymbol;
        Ontology::getUncertain(currentEntry, Ontology::EntitySymbol, entity);
        if(queue.empty()) {
            if(semicolon) {
                if(entity != Ontology::VoidSymbol)
                    return throwException("Pointless semicolon");
                fillInAnonymous(entity);
            }
            return true;
        }
        if(semicolon && queue.size() == 1) {
            if(entity == Ontology::VoidSymbol) {
                entity = queue.pop_back();
                Ontology::link({currentEntry, Ontology::EntitySymbol, entity});
                nextSymbol(parentEntry, entity);
            } else
                Ontology::link({entity, queue.pop_back(), entity});
            return true;
        }
        fillInAnonymous(entity);
        if(semicolon)
            Ontology::setSolitary({parentEntry, Ontology::UnnestEntitySymbol, Ontology::VoidSymbol});
        else
            Ontology::setSolitary({parentEntry, Ontology::UnnestEntitySymbol, entity}, true);
        Symbol attribute = queue.pop_back();
        Ontology::setSolitary({parentEntry, Ontology::UnnestAttributeSymbol, attribute}, true);
        while(!queue.empty())
            Ontology::link({entity, attribute, queue.pop_back()});
        return true;
    }

    bool deserialize() {
        checkReturn(thread.getGuaranteed(thread.block, Ontology::PackageSymbol, package));
        checkReturn(thread.getGuaranteed(thread.block, Ontology::InputSymbol, input));
        if(Ontology::query(1, {input, Ontology::BlobTypeSymbol, Ontology::TextSymbol}) == 0)
            return thread.throwException("Invalid Blob Type");
        locals.symbol = Storage::createSymbol();
        stack.symbol = Storage::createSymbol();
        currentEntry = Storage::createSymbol();
        Ontology::link({thread.block, Ontology::HoldsSymbol, locals.symbol});
        Ontology::link({thread.block, Ontology::HoldsSymbol, stack.symbol});
        Ontology::link({thread.block, Ontology::HoldsSymbol, currentEntry});
        stack.push_back(currentEntry);
        queue.symbol = currentEntry;
        row = column = 1;
        end = Storage::getBlobSize(input)/8;
        for(tokenBegin = pos = 0; pos < end; ++pos) {
            Natural8 src = Storage::readBlobAt<Natural8>(input, pos);
            switch(src) {
                case '\n':
                    checkReturn(parseToken());
                    column = 0;
                    ++row;
                    break;
                case '\t':
                    column += 3;
                case ' ':
                    checkReturn(parseToken());
                    break;
                case '"':
                    tokenBegin = pos+1;
                    while(true) {
                        if(pos == end)
                            return throwException("Unterminated text");
                        bool prev = (src != '\\');
                        ++pos;
                        src = Storage::readBlobAt<Natural8>(input, pos);
                        if(prev) {
                            if(src == '\\')
                                continue;
                            if(src == '"')
                                break;
                        }
                    }
                    checkReturn(parseToken());
                    break;
                case '(':
                    checkReturn(parseToken());
                    parentEntry = currentEntry;
                    currentEntry = Storage::createSymbol();
                    Ontology::link({thread.block, Ontology::HoldsSymbol, currentEntry});
                    stack.push_back(currentEntry);
                    queue.symbol = currentEntry;
                    break;
                case ';':
                    if(stack.size() == 1)
                        return throwException("Semicolon outside of any brackets");
                    checkReturn(seperateTokens(true));
                    if(!Ontology::valueCountIs(currentEntry, Ontology::UnnestEntitySymbol, 0))
                        return throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.size() == 1)
                        return throwException("Unmatched closing bracket");
                    checkReturn(seperateTokens(false));
                    if(stack.size() == 2 && Ontology::valueCountIs(parentEntry, Ontology::UnnestEntitySymbol, 0)) {
                        locals.clear();
                        Symbol entity;
                        checkReturn(thread.getGuaranteed(currentEntry, Ontology::EntitySymbol, entity));
                        if(Ontology::query(12, {entity, Ontology::VoidSymbol, Ontology::VoidSymbol}) == 0)
                            return throwException("Nothing declared");
                    }
                    if(!Ontology::valueCountIs(currentEntry, Ontology::UnnestEntitySymbol, 0))
                        return throwException("Unnesting failed");
                    Ontology::unlink(currentEntry);
                    stack.pop_back();
                    currentEntry = parentEntry;
                    queue.symbol = currentEntry;
                    parentEntry = (stack.size() < 2) ? Ontology::VoidSymbol : stack.readElementAt(stack.size()-2);
                }   break;
            }
            ++column;
        }
        checkReturn(parseToken());
        if(stack.size() != 1)
            return throwException("Missing closing bracket");
        if(!Ontology::valueCountIs(currentEntry, Ontology::UnnestEntitySymbol, 0))
            return throwException("Unnesting failed");
        if(queue.empty())
            return throwException("Empty Input");
        Symbol output;
        if(Ontology::getUncertain(thread.block, Ontology::OutputSymbol, output)) {
            Symbol target;
            checkReturn(thread.getTargetSymbol(target));
            Ontology::setSolitary({target, output, Ontology::VoidSymbol});
            while(!queue.empty())
                Ontology::link({target, output, queue.pop_back()});
        }
        thread.popCallStack();
        return true;
    }
};
