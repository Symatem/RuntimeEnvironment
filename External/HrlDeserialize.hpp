#include <External/HrlSerialize.hpp>

#define checkReturn(expression) { \
    Symbol exception = (expression); \
    if(exception != VoidSymbol) \
        return exception; \
}

struct HrlDeserializer {
    Symbol parentEntry, input, package = VoidSymbol;
    GuardedBitstreamDataStructure<BitstreamContentIndex<>> locals;
    GuardedBitstreamDataStructure<BitstreamVector<Symbol>> stack;
    BitstreamDataStructure<BitstreamVector<Symbol>> queue;
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
            BitstreamDataStructure<BitstreamVector<Symbol>> stackEntrysQueue(stackEntry);
            insertAsFirstElement(stackEntrysQueue, symbol);
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
            checkReturn(nextSymbol(queue.blob.symbol, symbol, package));
        }
        tokenBegin = tokenEnd+1;
        return VoidSymbol;
    }

    Symbol fillInAnonymous(Symbol& entity) {
        if(entity == VoidSymbol) {
            entity = createSymbol();
            link({queue.blob.symbol, EntitySymbol, entity});
            checkReturn(nextSymbol(parentEntry, entity, package));
        }
        return VoidSymbol;
    }

    Symbol seperateTokens(bool semicolon) {
        checkReturn(parseToken());
        Symbol entity = VoidSymbol;
        getUncertain(queue.blob.symbol, EntitySymbol, entity);
        if(queue.isEmpty()) {
            if(semicolon) {
                if(entity != VoidSymbol)
                    return throwException("Pointless semicolon");
                checkReturn(fillInAnonymous(entity));
            }
            return VoidSymbol;
        }
        if(semicolon && queue.getElementCount() == 1) {
            if(entity == VoidSymbol) {
                entity = eraseLastElement(queue);
                link({queue.blob.symbol, EntitySymbol, entity});
                checkReturn(nextSymbol(parentEntry, entity));
            } else
                link({entity, eraseLastElement(queue), entity});
            return VoidSymbol;
        }
        checkReturn(fillInAnonymous(entity));
        if(semicolon)
            setSolitary({parentEntry, UnnestEntitySymbol, VoidSymbol});
        else
            setSolitary({parentEntry, UnnestEntitySymbol, entity}, true);
        Symbol attribute = eraseLastElement(queue);
        setSolitary({parentEntry, UnnestAttributeSymbol, attribute}, true);
        while(!queue.isEmpty())
            link({entity, attribute, eraseLastElement(queue)});
        return VoidSymbol;
    }

    Symbol deserialize() {
        row = column = 1;
        if(!tripleExists({input, BlobTypeSymbol, UTF8Symbol}))
            return throwException("Invalid Blob Type");
        insertAsLastElement(stack, queue.blob.symbol);
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
                    parentEntry = queue.blob.symbol;
                    queue.blob = createSymbol();
                    insertAsLastElement(stack, queue.blob.symbol);
                    break;
                case ';':
                    if(stack.getElementCount() == 1)
                        return throwException("Semicolon outside of any brackets");
                    checkReturn(seperateTokens(true));
                    if(!valueCountIs(queue.blob.symbol, UnnestEntitySymbol, 0))
                        return throwException("Unnesting failed");
                    break;
                case ')': {
                    if(stack.getElementCount() == 1)
                        return throwException("Unmatched closing bracket");
                    checkReturn(seperateTokens(false));
                    if(stack.getElementCount() == 2 && valueCountIs(parentEntry, UnnestEntitySymbol, 0)) {
                        Symbol entity;
                        if(!getUncertain(queue.blob.symbol, EntitySymbol, entity) ||
                           query(MVV, {entity, VoidSymbol, VoidSymbol}) == 0)
                            return throwException("Nothing declared");
                    }
                    if(!valueCountIs(queue.blob.symbol, UnnestEntitySymbol, 0))
                        return throwException("Unnesting failed");
                    unlink(queue.blob.symbol);
                    eraseLastElement(stack);
                    queue.blob = parentEntry;
                    parentEntry = (stack.getElementCount() < 2) ? VoidSymbol : stack.getElementAt(stack.getElementCount()-2);
                }   break;
            }
            ++column;
        }
        checkReturn(parseToken());
        if(stack.getElementCount() != 1)
            return throwException("Missing closing bracket");
        if(!valueCountIs(queue.blob.symbol, UnnestEntitySymbol, 0))
            return throwException("Unnesting failed");
        iterateElements(locals, [](Symbol local) {
            Blob(local).setSize(0);
        });
        return VoidSymbol;
    }
};

#undef checkReturn
