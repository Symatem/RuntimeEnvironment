#include "Thread.hpp"

const char* HRLRawBegin = "raw:";

struct Serialize {
    Thread& thread;
    Symbol symbol;

    void put(NativeNaturalType src) {
        Storage::insertIntoBlob(symbol, &src, Storage::getBlobSize(symbol), 8);
    }

    void puts(const char* src) {
        Storage::insertIntoBlob(symbol, reinterpret_cast<const NativeNaturalType*>(src),
                                Storage::getBlobSize(symbol), strlen(src)*8);
    }

    template<typename NumberType>
    void serializeNumber(NumberType number) {
        // TODO Last digits of float numbers are incorrect (rounding error)
        const NumberType base = 10;
        if(number == 0) {
            put('0');
            return;
        }
        if(number < 0) {
            put('-');
            number *= -1;
        }
        NumberType mask = 1;
        while(mask <= number)
            mask *= base;
        while(mask > 0.001) {
            if(mask == 1 && number > 0) // TODO
                put('.');
            mask /= base;
            if(mask == 0)
                break;
            NumberType digit = number/mask;
            digit = static_cast<NativeIntegerType>(digit); // TODO
            number -= digit*mask;
            put('0'+digit);
        }
    }

    Serialize(Thread& _thread, Symbol _symbol) :thread(_thread), symbol(_symbol) {
        Ontology::setSolitary({symbol, PreDef_BlobType, PreDef_Text});
    }

    Serialize(Thread& _thread) :Serialize(_thread, Storage::createSymbol()) {}

    void serializeBlob(Symbol srcSymbol) {
        auto src = reinterpret_cast<const char*>(Storage::accessBlobData(srcSymbol));
        NativeNaturalType len = Storage::getBlobSize(srcSymbol);
        if(len) {
            Symbol type = PreDef_Void;
            Ontology::getUncertain(srcSymbol, PreDef_BlobType, type);
            switch(type) {
                case PreDef_Text: {
                    len /= 8;
                    bool spaces = false;
                    for(NativeNaturalType i = 0; i < len; ++i)
                        if(src[i] == ' ' || src[i] == '\t' || src[i] == '\n') {
                            spaces = true;
                            break;
                        }
                    if(spaces)
                        put('"');
                    for(NativeNaturalType i = 0; i < len; ++i)
                        put(src[i]);
                    if(spaces)
                        put('"');
                }   break;
                case PreDef_Natural:
                    serializeNumber(thread.accessBlobAs<NativeNaturalType>(srcSymbol));
                    break;
                case PreDef_Integer:
                    serializeNumber(thread.accessBlobAs<NativeIntegerType>(srcSymbol));
                    break;
                case PreDef_Float:
                    serializeNumber(thread.accessBlobAs<NativeFloatType>(srcSymbol));
                    break;
                default: {
                    for(NativeNaturalType i = 0; i < strlen(HRLRawBegin); ++i)
                        put(HRLRawBegin[i]);
                    len = (len+3)/4;
                    for(NativeNaturalType i = 0; i < len; ++i) {
                        char nibble = (src[i/2]>>((i%2)*4))&0x0F;
                        if(nibble < 0xA)
                            put('0'+nibble);
                        else
                            put('A'+nibble-0xA);
                    }
                }   break;
            }
        } else {
            put('#');
            serializeNumber(srcSymbol);
        }
    }

    void serializeEntity(Symbol entity, Closure<Symbol(Symbol)> followCallback = nullptr) {
        while(true) {
            Symbol followAttribute, followEntity = PreDef_Void;
            if(followCallback)
                followAttribute = followCallback(entity);
            puts("(\n\t");
            serializeBlob(entity);
            puts(";\n");
            Ontology::query(21, {entity, PreDef_Void, PreDef_Void}, [&](Triple result) {
                if(followCallback && result.pos[0] == followAttribute) {
                    Ontology::getUncertain(entity, followAttribute, followEntity);
                    return;
                }
                put('\t');
                serializeBlob(result.pos[0]);
                Ontology::query(9, {entity, result.pos[0], PreDef_Void}, [&](Triple resultB) {
                    put(' ');
                    serializeBlob(resultB.pos[0]);
                });
                puts(";\n");
            });
            if(!followCallback || followEntity == PreDef_Void) {
                put(')');
                return;
            }
            put('\t');
            serializeBlob(followAttribute);
            entity = followEntity;
            puts("\n) ");
        }
    }
};
