#include "Task.hpp"

const char* HRLRawBegin = "raw:";

struct Serialize {
    Task& task;
    Symbol symbol;
    Context::SymbolObject* symbolObject;
    ArchitectureType blobSize;

    bool isEmpty() const {
        return (blobSize == 0);
    }

    void reset() {
        symbolObject->allocateBlob(0);
        blobSize = 0;
    }

    Symbol getSymbol(bool decouple = false) {
        Symbol returnValue = symbol;
        symbolObject->reallocateBlob(blobSize);
        if(decouple) {
            symbol = task.context->create({{PreDef_BlobType, PreDef_Text}});
            symbolObject = task.context->getSymbolObject(symbol);
            blobSize = 0;
        }
        return returnValue;
    }

    void put(uint8_t data) {
        ArchitectureType nextBlobSize = blobSize+8;
        if(nextBlobSize > symbolObject->blobSize)
            symbolObject->reallocateBlob(std::max(nextBlobSize, symbolObject->blobSize*2));
        reinterpret_cast<uint8_t*>(symbolObject->blobData.get())[blobSize/8] = data;
        blobSize = nextBlobSize;
    }

    char* charPtr() {
        return reinterpret_cast<char*>(symbolObject->blobData.get());
    }

    bool beginsWith(const char* str) {
        // TODO: Remove useage of C StdLib
        return strncmp(charPtr(), str, std::min(blobSize/8, (ArchitectureType)strlen(str))) == 0;
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
        while(mask < number)
            mask *= base;
        while(number > 0 && mask > 0.001) {
            if(mask == 1) // TODO
                put('.');
            mask /= base;
            if(mask == 0)
                break;
            NumberType digit = number/mask;
            digit = floor(digit); // TODO
            number -= digit*mask;
            put('0'+digit);
        }
    }

    Serialize(Task& _task, Symbol _symbol) :task(_task), symbol(_symbol), blobSize(0) {
        symbolObject = task.context->getSymbolObject(_symbol);
    }

    Serialize(Task& _task) :Serialize(_task, _task.context->create({{PreDef_BlobType, PreDef_Text}})) {

    }

    void serializeBlob(Symbol symbol) {
        auto srcSymbolObject = task.context->getSymbolObject(symbol);
        auto src = reinterpret_cast<const uint8_t*>(srcSymbolObject->blobData.get());
        if(srcSymbolObject->blobSize) {
            Symbol type = PreDef_Void;
            task.getUncertain(symbol, PreDef_BlobType, type);
            switch(type) {
                case PreDef_Text: {
                    ArchitectureType len = srcSymbolObject->blobSize/8;
                    bool spaces = false;
                    for(ArchitectureType i = 0; i < len; ++i)
                        if(src[i] == ' ' || src[i] == '\t' || src[i] == '\n') {
                            spaces = true;
                            break;
                        }
                    if(spaces)
                        put('"');
                    for(ArchitectureType i = 0; i < len; ++i)
                        put(src[i]);
                    if(spaces)
                        put('"');
                }   break;
                case PreDef_Natural:
                    serializeNumber(task.template accessBlobData<uint64_t>(srcSymbolObject));
                    break;
                case PreDef_Integer:
                    serializeNumber(task.template accessBlobData<int64_t>(srcSymbolObject));
                    break;
                case PreDef_Float:
                    serializeNumber(task.template accessBlobData<double>(srcSymbolObject));
                    break;
                default: {
                    for(ArchitectureType i = 0; i < strlen(HRLRawBegin); ++i)
                        put(HRLRawBegin[i]);
                    ArchitectureType len = (srcSymbolObject->blobSize+3)/4;
                    for(ArchitectureType i = 0; i < len; ++i) {
                        uint8_t nibble = (src[i/2]>>((i%2)*4))&0x0F;
                        if(nibble < 0xA)
                            put('0'+nibble);
                        else
                            put('A'+nibble-0xA);
                    }
                }   break;
            }
        } else {
            put('#');
            serializeNumber(symbol);
        }
    }

    void serializeEntity(Symbol entity, std::function<Symbol(Symbol)> followCallback = nullptr) {
        Symbol followAttribute;
        while(true) {
            auto topIter = task.context->topIndex.find(entity);
            assert(topIter != task.context->topIndex.end());

            if(followCallback)
                followAttribute = followCallback(entity);

            put('(');
            put('\n');
            put('\t');
            serializeBlob(entity);
            put(';');
            put('\n');

            std::set<Symbol>* lastAttribute = nullptr;
            for(auto& j : topIter->second->subIndices[EAV]) {
                if(followCallback && j.first == followAttribute) {
                    lastAttribute = &j.second;
                    continue;
                }
                put('\t');
                serializeBlob(j.first);
                for(auto& k : j.second) {
                    put(' ');
                    serializeBlob(k);
                }
                put(';');
                put('\n');
            }
            if(!followCallback || !lastAttribute) {
                put(')');
                return;
            }

            put('\t');
            serializeBlob(followAttribute);
            for(auto iter = lastAttribute->begin(); iter != --lastAttribute->end(); ++iter) {
                put(' ');
                serializeBlob(*iter);
            }
            entity = *lastAttribute->rbegin();
            put('\n');
            put(')');
            put(' ');
        }
    }
};
