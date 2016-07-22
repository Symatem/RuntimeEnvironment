#include "Thread.hpp"

const char* HRLRawBegin = "raw:";

struct Serializer {
    Thread& thread;
    Symbol symbol;

    void put(Natural8 src) {
        Storage::Blob dstBlob(symbol);
        NativeNaturalType offset = dstBlob.getSize();
        dstBlob.increaseSize(offset, 8);
        dstBlob.writeAt<Natural8>(offset/8, src);
    }

    void puts(const char* src) {
        Storage::Blob dstBlob(symbol);
        NativeNaturalType offset = dstBlob.getSize(), length = strlen(src)*8;
        dstBlob.increaseSize(offset, length);
        dstBlob.externalOperate<true>(const_cast<Integer8*>(src), offset, length);
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
        Storage::Blob srcBlob(src);
        NativeNaturalType len = srcBlob.getSize();
        if(len) {
            Symbol type = Ontology::VoidSymbol;
            Ontology::getUncertain(src, Ontology::BlobTypeSymbol, type);
            switch(type) {
                case Ontology::TextSymbol: {
                    len /= 8;
                    bool spaces = false;
                    for(NativeNaturalType i = 0; i < len; ++i) {
                        Natural8 element = srcBlob.readAt<Natural8>(i);
                        if(element == ' ' || element == '\t' || element == '\n') {
                            spaces = true;
                            break;
                        }
                    }
                    if(spaces)
                        put('"');
                    for(NativeNaturalType i = 0; i < len; ++i)
                        put(srcBlob.readAt<Natural8>(i));
                    if(spaces)
                        put('"');
                }   break;
                case Ontology::NaturalSymbol:
                    serializeNumber(srcBlob.readAt<NativeNaturalType>());
                    break;
                case Ontology::IntegerSymbol:
                    serializeNumber(srcBlob.readAt<NativeIntegerType>());
                    break;
                case Ontology::FloatSymbol:
                    serializeNumber(srcBlob.readAt<NativeFloatType>());
                    break;
                default: {
                    puts(HRLRawBegin);
                    len = (len+3)/4;
                    for(NativeNaturalType i = 0; i < len; ++i) {
                        Natural8 element = srcBlob.readAt<Natural8>(i/2),
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
            Ontology::query(Ontology::MVI, {entity, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                if(followCallback && result.pos[1] == followAttribute) {
                    Ontology::getUncertain(entity, followAttribute, followEntity);
                    return;
                }
                put('\t');
                serializeBlob(result.pos[1]);
                Ontology::query(Ontology::MMV, {entity, result.pos[1], Ontology::VoidSymbol}, [&](Ontology::Triple resultB) {
                    put(' ');
                    serializeBlob(resultB.pos[2]);
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
