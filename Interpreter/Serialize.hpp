#include "Thread.hpp"

const char* HRLRawBegin = "raw:";

struct Serializer {
    Thread& thread;
    Symbol symbol;

    void put(Natural8 src) {
        NativeNaturalType at = Storage::getBlobSize(symbol);
        Storage::increaseBlobSize(symbol, at, 8);
        Storage::writeBlobAt<Natural8>(symbol, at/8, src);
    }

    void puts(const char* src) {
        NativeNaturalType at = Storage::getBlobSize(symbol), length = strlen(src)*8;
        Storage::Blob dstBlob(symbol);
        dstBlob.increaseSize(0, length);
        dstBlob.externalOperate<true>(const_cast<Integer8*>(src), at, length);
        Storage::modifiedBlob(symbol);
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

    Serializer(Thread& _thread, Symbol _symbol) :thread(_thread), symbol(_symbol) {
        Ontology::setSolitary({symbol, Ontology::BlobTypeSymbol, Ontology::TextSymbol});
    }

    Serializer(Thread& _thread) :Serializer(_thread, Storage::createSymbol()) {}

    void serializeBlob(Symbol src) {
        NativeNaturalType len = Storage::getBlobSize(src);
        if(len) {
            Symbol type = Ontology::VoidSymbol;
            Ontology::getUncertain(src, Ontology::BlobTypeSymbol, type);
            switch(type) {
                case Ontology::TextSymbol: {
                    len /= 8;
                    bool spaces = false;
                    for(NativeNaturalType i = 0; i < len; ++i) {
                        Natural8 element = Storage::readBlobAt<Natural8>(src, i);
                        if(element == ' ' || element == '\t' || element == '\n') {
                            spaces = true;
                            break;
                        }
                    }
                    if(spaces)
                        put('"');
                    for(NativeNaturalType i = 0; i < len; ++i)
                        put(Storage::readBlobAt<Natural8>(src, i));
                    if(spaces)
                        put('"');
                }   break;
                case Ontology::NaturalSymbol:
                    serializeNumber(Storage::readBlobAt<NativeNaturalType>(src));
                    break;
                case Ontology::IntegerSymbol:
                    serializeNumber(Storage::readBlobAt<NativeIntegerType>(src));
                    break;
                case Ontology::FloatSymbol:
                    serializeNumber(Storage::readBlobAt<NativeFloatType>(src));
                    break;
                default: {
                    puts(HRLRawBegin);
                    len = (len+3)/4;
                    for(NativeNaturalType i = 0; i < len; ++i) {
                        Natural8 element = Storage::readBlobAt<Natural8>(src, i/2),
                                 nibble = (element>>((i%2)*4))&0x0F;
                        if(nibble < 0xA)
                            put('0'+nibble);
                        else
                            put('A'+nibble-0xA);
                    }
                }   break;
            }
        } else {
            put('#');
            serializeNumber(src);
        }
    }

    void serializeEntity(Symbol entity, Closure<Symbol(Symbol)> followCallback = nullptr) {
        while(true) {
            Symbol followAttribute, followEntity = Ontology::VoidSymbol;
            if(followCallback)
                followAttribute = followCallback(entity);
            puts("(\n\t");
            serializeBlob(entity);
            puts(";\n");
            Ontology::query(21, {entity, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                if(followCallback && result.pos[0] == followAttribute) {
                    Ontology::getUncertain(entity, followAttribute, followEntity);
                    return;
                }
                put('\t');
                serializeBlob(result.pos[0]);
                Ontology::query(9, {entity, result.pos[0], Ontology::VoidSymbol}, [&](Ontology::Triple resultB) {
                    put(' ');
                    serializeBlob(resultB.pos[0]);
                });
                puts(";\n");
            });
            if(!followCallback || followEntity == Ontology::VoidSymbol) {
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
