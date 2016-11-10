#include <HRL/Serialize.hpp>

#define checkReturn(expression) \
    if((expression) != Ontology::VoidSymbol) \
        return false;

struct Deserializer {
    Symbol parentEntry, currentEntry, input, package = Ontology::VoidSymbol;
    Ontology::BlobIndex<true> locals;
    Ontology::BlobVector<true, Symbol> stack, queue;
    NativeNaturalType tokenBegin, tokenEnd, inputEnd, row, column;

    bool tokenBeginsWithString(const char* str) {
        if(strlen(str) > tokenEnd-tokenBegin)
            return false;
        for(NativeNaturalType i = 0; i < strlen(str); ++i)
            if(Storage::Blob(input).readAt<Natural8>(tokenBegin+i) != str[i])
                return false;
        return true;
    }

    Symbol throwException(const char* message) {
        Symbol exception = Storage::createSymbol(),
               rowSymbol = Ontology::createFromData(row),
               columnSymbol = Ontology::createFromData(column);
        Ontology::link({exception, Ontology::RowSymbol, rowSymbol});
        Ontology::link({exception, Ontology::ColumnSymbol, columnSymbol});
        Ontology::link({exception, Ontology::MessageSymbol, Ontology::createFromString(message)});
        return exception;
    }

    Symbol nextSymbol(Symbol stackEntry, Symbol symbol, Symbol package = Ontology::VoidSymbol) {
        if(package != Ontology::VoidSymbol)
            Ontology::link({package, Ontology::HoldsSymbol, symbol});
        if(Ontology::valueCountIs(stackEntry, Ontology::UnnestEntitySymbol, 0)) {
            queue.symbol = stackEntry;
            queue.insert(0, symbol);
            queue.symbol = currentEntry;
        } else {
            Symbol entity, attribute;
            if(!Ontology::getUncertain(stackEntry, Ontology::UnnestEntitySymbol, entity) ||
               !Ontology::getUncertain(stackEntry, Ontology::UnnestAttributeSymbol, attribute))
                return throwException("Unnesting failed");
            Ontology::link({entity, attribute, symbol});
            Ontology::setSolitary({stackEntry, Ontology::UnnestEntitySymbol, Ontology::VoidSymbol});
        }
        return Ontology::VoidSymbol;
    }

    Symbol sliceText() {
        Symbol dstSymbol = Ontology::createFromSlice(input, tokenBegin*8, (tokenEnd-tokenBegin)*8);
        Ontology::link({dstSymbol, Ontology::BlobTypeSymbol, Ontology::UTF8Symbol});
        return dstSymbol;
    }

    Symbol parseToken(bool isText = false) {
        if(tokenEnd > tokenBegin) {
            Storage::Blob srcBlob(input);
            Natural8 src = srcBlob.readAt<Natural8>(tokenBegin);
            Symbol symbol;
            if(isText)
                symbol = sliceText();
            else if(src == '#') {
                symbol = sliceText();
                locals.insertElement(symbol);
            } else if(tokenBeginsWithString(HRLRawBegin)) {
                tokenBegin += strlen(HRLRawBegin);
                NativeNaturalType nibbleCount = tokenEnd-tokenBegin;
                if(nibbleCount == 0)
                    return throwException("Empty raw data");
                if(nibbleCount%2 == 1)
                    return throwException("Count of nibbles must be even");
                symbol = Storage::createSymbol();
                Storage::Blob dstBlob(symbol);
                dstBlob.increaseSize(0, nibbleCount*4);
                Natural8 nibble, byte;
                NativeNaturalType at = 0;
                while(tokenBegin < tokenEnd) {
                    src = srcBlob.readAt<Natural8>(tokenBegin);
                    if(src >= '0' && src <= '9')
                        nibble = src-'0';
                    else if(src >= 'A' && src <= 'F')
                        nibble = src-'A'+0xA;
                    else
                        return throwException("Non hex characters");
                    if(at%2 == 0)
                        byte = nibble;
                    else {
                        dstBlob.writeAt<Natural8>(at/2, byte|(nibble<<4));
                        byte = 0;
                    }
                    ++at;
                    ++tokenBegin;
                }
                Storage::modifiedBlob(symbol);
            } else {
                NativeNaturalType mantissa = 0, devisor = 0, pos = tokenBegin;
                bool isNumber = true, negative = (src == '-');
                if(negative)
                    ++pos;
                // TODO What if too long, precision loss?
                while(pos < tokenEnd) {
                    src = srcBlob.readAt<Natural8>(pos);
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
                    ++pos;
                }
                if(isNumber) {
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
            checkReturn(nextSymbol(currentEntry, symbol, package));
        }
        tokenBegin = tokenEnd+1;
        return Ontology::VoidSymbol;
    }

    Symbol fillInAnonymous(Symbol& entity) {
        if(entity == Ontology::VoidSymbol) {
            entity = Storage::createSymbol();
            Ontology::link({currentEntry, Ontology::EntitySymbol, entity});
            checkReturn(nextSymbol(parentEntry, entity, package));
        }
        return Ontology::VoidSymbol;
    }

    Symbol seperateTokens(bool semicolon) {
        checkReturn(parseToken());
        Symbol entity = Ontology::VoidSymbol;
        Ontology::getUncertain(currentEntry, Ontology::EntitySymbol, entity);
        if(queue.empty()) {
            if(semicolon) {
                if(entity != Ontology::VoidSymbol)
                    return throwException("Pointless semicolon");
                checkReturn(fillInAnonymous(entity));
            }
            return Ontology::VoidSymbol;
        }
        if(semicolon && queue.size() == 1) {
            if(entity == Ontology::VoidSymbol) {
                entity = queue.pop_back();
                Ontology::link({currentEntry, Ontology::EntitySymbol, entity});
                checkReturn(nextSymbol(parentEntry, entity));
            } else
                Ontology::link({entity, queue.pop_back(), entity});
            return Ontology::VoidSymbol;
        }
        checkReturn(fillInAnonymous(entity));
        if(semicolon)
            Ontology::setSolitary({parentEntry, Ontology::UnnestEntitySymbol, Ontology::VoidSymbol});
        else
            Ontology::setSolitary({parentEntry, Ontology::UnnestEntitySymbol, entity}, true);
        Symbol attribute = queue.pop_back();
        Ontology::setSolitary({parentEntry, Ontology::UnnestAttributeSymbol, attribute}, true);
        while(!queue.empty())
            Ontology::link({entity, attribute, queue.pop_back()});
        return Ontology::VoidSymbol;
    }

    Symbol deserialize() {
        if(!Ontology::tripleExists({input, Ontology::BlobTypeSymbol, Ontology::UTF8Symbol}))
            return throwException("Invalid Blob Type");
        currentEntry = Storage::createSymbol();
        stack.push_back(currentEntry);
        queue.symbol = currentEntry;
        row = column = 1;
        Storage::Blob srcBlob(input);
        inputEnd = srcBlob.getSize()/8;
        for(tokenBegin = tokenEnd = 0; tokenEnd < inputEnd; ++tokenEnd) {
            Natural8 src = srcBlob.readAt<Natural8>(tokenEnd);
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
                    tokenBegin = tokenEnd+1;
                    while(true) {
                        if(tokenEnd == inputEnd)
                            return throwException("Unterminated text");
                        bool prev = (src != '\\');
                        ++tokenEnd;
                        src = srcBlob.readAt<Natural8>(tokenEnd);
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
                        if(!Ontology::getUncertain(currentEntry, Ontology::EntitySymbol, entity) ||
                           Ontology::query(Ontology::MVV, {entity, Ontology::VoidSymbol, Ontology::VoidSymbol}) == 0)
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
        return Ontology::VoidSymbol;
    }
};
