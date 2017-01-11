#include <HRL/Serialize.hpp>

#define checkReturn(expression) \
    if((expression) != VoidSymbol) \
        return false;

struct Deserializer {
    Symbol parentEntry, input, package = VoidSymbol;
    BlobIndex<true> locals;
    BlobVector<true, Symbol> stack;
    BlobVector<false, Symbol> queue;
    NativeNaturalType tokenBegin, tokenEnd, inputEnd, row, column;

    bool tokenBeginsWithString(const char* str) {
        if(strlen(str) > tokenEnd-tokenBegin)
            return false;
        for(NativeNaturalType i = 0; i < strlen(str); ++i)
            if(Blob(input).readAt<Natural8>(tokenBegin+i) != str[i])
                return false;
        return true;
    }

    Symbol throwException(const char* message) {
        Symbol exception = createSymbol(),
               rowSymbol = createFromData(row),
               columnSymbol = createFromData(column);
        link({exception, RowSymbol, rowSymbol});
        link({exception, ColumnSymbol, columnSymbol});
        link({exception, MessageSymbol, createFromString(message)});
        return exception;
    }

    Symbol nextSymbol(Symbol stackEntry, Symbol symbol, Symbol package = VoidSymbol) {
        if(package != VoidSymbol)
            link({package, HoldsSymbol, symbol});
        if(valueCountIs(stackEntry, UnnestEntitySymbol, 0)) {
            BlobVector<false, Symbol> stackEntrysQueue;
            stackEntrysQueue.symbol = stackEntry;
            stackEntrysQueue.insert(0, symbol);
        } else {
            Symbol entity, attribute;
            if(!getUncertain(stackEntry, UnnestEntitySymbol, entity) ||
               !getUncertain(stackEntry, UnnestAttributeSymbol, attribute))
                return throwException("Unnesting failed");
            link({entity, attribute, symbol});
            setSolitary({stackEntry, UnnestEntitySymbol, VoidSymbol});
        }
        return VoidSymbol;
    }

    template<bool local>
    Symbol sliceText() {
        Symbol dstSymbol = createFromSlice(input, tokenBegin*8, (tokenEnd-tokenBegin)*8);
        if(local)
            locals.insertElement(dstSymbol);
        else {
            blobIndex.insertElement(dstSymbol);
            link({dstSymbol, BlobTypeSymbol, UTF8Symbol});
        }
        return dstSymbol;
    }

    Symbol parseToken(bool isText = false) {
        if(tokenEnd > tokenBegin) {
            Blob srcBlob(input);
            Natural8 src = srcBlob.readAt<Natural8>(tokenBegin);
            Symbol symbol;
            if(isText)
                symbol = sliceText<false>();
            else if(src == '#')
                symbol = sliceText<true>();
            else if(tokenBeginsWithString(HRLRawBegin)) {
                tokenBegin += strlen(HRLRawBegin);
                NativeNaturalType nibbleCount = tokenEnd-tokenBegin;
                if(nibbleCount == 0)
                    return throwException("Empty raw data");
                if(nibbleCount%2 == 1)
                    return throwException("Count of nibbles must be even");
                symbol = createSymbol();
                Blob dstBlob(symbol);
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
                modifiedBlob(symbol);
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
                        symbol = createFromData(value);
                    } else if(negative)
                        symbol = createFromData(-static_cast<NativeIntegerType>(mantissa));
                    else
                        symbol = createFromData(mantissa);
                    blobIndex.insertElement(symbol);
                } else
                    symbol = sliceText<false>();
            }
            checkReturn(nextSymbol(queue.symbol, symbol, package));
        }
        tokenBegin = tokenEnd+1;
        return VoidSymbol;
    }

    Symbol fillInAnonymous(Symbol& entity) {
        if(entity == VoidSymbol) {
            entity = createSymbol();
            link({queue.symbol, EntitySymbol, entity});
            checkReturn(nextSymbol(parentEntry, entity, package));
        }
        return VoidSymbol;
    }

    Symbol seperateTokens(bool semicolon) {
        checkReturn(parseToken());
        Symbol entity = VoidSymbol;
        getUncertain(queue.symbol, EntitySymbol, entity);
        if(queue.empty()) {
            if(semicolon) {
                if(entity != VoidSymbol)
                    return throwException("Pointless semicolon");
                checkReturn(fillInAnonymous(entity));
            }
            return VoidSymbol;
        }
        if(semicolon && queue.size() == 1) {
            if(entity == VoidSymbol) {
                entity = queue.pop_back();
                link({queue.symbol, EntitySymbol, entity});
                checkReturn(nextSymbol(parentEntry, entity));
            } else
                link({entity, queue.pop_back(), entity});
            return VoidSymbol;
        }
        checkReturn(fillInAnonymous(entity));
        if(semicolon)
            setSolitary({parentEntry, UnnestEntitySymbol, VoidSymbol});
        else
            setSolitary({parentEntry, UnnestEntitySymbol, entity}, true);
        Symbol attribute = queue.pop_back();
        setSolitary({parentEntry, UnnestAttributeSymbol, attribute}, true);
        while(!queue.empty())
            link({entity, attribute, queue.pop_back()});
        return VoidSymbol;
    }

    Symbol deserialize() {
        row = column = 1;
        if(!tripleExists({input, BlobTypeSymbol, UTF8Symbol}))
            return throwException("Invalid Blob Type");
        stack.push_back(queue.symbol);
        Blob srcBlob(input);
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
                    parentEntry = queue.symbol;
                    queue.symbol = createSymbol();
                    stack.push_back(queue.symbol);
                    break;
                case ';':
                    if(stack.size() == 1)
                        return throwException("Semicolon outside of any brackets");
                    checkReturn(seperateTokens(true));
                    if(!valueCountIs(queue.symbol, UnnestEntitySymbol, 0))
                        return throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.size() == 1)
                        return throwException("Unmatched closing bracket");
                    checkReturn(seperateTokens(false));
                    if(stack.size() == 2 && valueCountIs(parentEntry, UnnestEntitySymbol, 0)) {
                        Symbol entity;
                        if(!getUncertain(queue.symbol, EntitySymbol, entity) ||
                           query(MVV, {entity, VoidSymbol, VoidSymbol}) == 0)
                            return throwException("Nothing declared");
                    }
                    if(!valueCountIs(queue.symbol, UnnestEntitySymbol, 0))
                        return throwException("Unnesting failed");
                    unlink(queue.symbol);
                    stack.pop_back();
                    queue.symbol = parentEntry;
                    parentEntry = (stack.size() < 2) ? VoidSymbol : stack.readElementAt(stack.size()-2);
                }   break;
            }
            ++column;
        }
        checkReturn(parseToken());
        if(stack.size() != 1)
            return throwException("Missing closing bracket");
        if(!valueCountIs(queue.symbol, UnnestEntitySymbol, 0))
            return throwException("Unnesting failed");
        locals.iterate([](Symbol local) {
            Blob(local).setSize(0);
        });
        return VoidSymbol;
    }
};
