#include "../MM/BpTree.hpp"

typedef ArchitectureType Symbol;
struct Exception {};
enum IndexType {
    EAV = 0, AVE = 1, VEA = 2,
    EVA = 3, AEV = 4, VAE = 5
};
union Triple {
    Symbol pos[3];
    struct {
        Symbol entity, attribute, value;
    };
    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :entity(_entity), attribute(_attribute), value(_value) {}
    bool operator<(const Triple& other) const {
        for(ArchitectureType i = 0; i < 3; ++i)
            if(pos[i] < other.pos[i]) return true;
            else if(pos[i] > other.pos[i]) return false;
        return false;
    }
    Triple reordered(ArchitectureType to) {
        ArchitectureType alpha[] = { 0, 1, 2, 0, 1, 2 };
        ArchitectureType  beta[] = { 1, 2, 0, 2, 0, 1 };
        ArchitectureType gamma[] = { 2, 0, 1, 1, 2, 0 };
        return {pos[alpha[to]], pos[beta[to]], pos[gamma[to]]};
    }
    Triple normalized(ArchitectureType from) {
        ArchitectureType alpha[] = { 0, 2, 1, 0, 1, 2 };
        ArchitectureType  beta[] = { 1, 0, 2, 2, 0, 1 };
        ArchitectureType gamma[] = { 2, 1, 0, 1, 2, 0 };
        return {pos[alpha[from]], pos[beta[from]], pos[gamma[from]]};
    }
};

#define PreDefWrapper(token) PreDef_##token
enum PreDefSymbols {
#include "PreDefSymbols.hpp"
};
#undef PreDefWrapper

#define PreDefWrapper(token) #token
const char* PreDefSymbols[] = {
#include "PreDefSymbols.hpp"
};
#undef PreDefWrapper

#define getSymbolByName(Name) \
    Symbol Name##Symbol = task.getGuaranteed(task.block, PreDef_##Name);

#define getSymbolObjectByName(Name) \
    getSymbolByName(Name) \
    auto Name##SymbolObject = task.context->getSymbolObject(Name##Symbol);

#define checkBlobType(Name, expectedType) \
if(task.query(1, {Name##Symbol, PreDef_BlobType, expectedType}) == 0) \
    task.throwException("Invalid Blob Type");

#define getUncertainSymbolObjectByName(Name, DefaultValue) \
    Symbol Name##Symbol; ArchitectureType Name##Value; \
    if(task.getUncertain(task.block, PreDef_##Name, Name##Symbol)) { \
        checkBlobType(Name, PreDef_Natural) \
        Name##Value = task.accessBlobData<ArchitectureType>(task.context->getSymbolObject(Name##Symbol)); \
    } else \
        Name##Value = DefaultValue;


struct Context { // TODO: : public Storage {
    struct SymbolObject {
        ArchitectureType blobSize;
        std::unique_ptr<ArchitectureType> blobData;
        std::map<Symbol, std::set<Symbol>> subIndices[6];

        void allocate(ArchitectureType _size, ArchitectureType preserve = 0) {
            if(blobSize == _size)
                return;

            ArchitectureType* _data;
            if(_size) {
                _data = new ArchitectureType[(_size+ArchitectureSize-1)/ArchitectureSize];
                if(_size%ArchitectureSize > 0)
                    _data[blobSize/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(_size%ArchitectureSize);
            } else
                _data = NULL;

            ArchitectureType length = std::min(std::min(blobSize, _size), preserve);
            if(length > 0)
                bitwiseCopy<-1>(_data, blobData.get(), 0, 0, length);
            blobSize = _size;
            blobData.reset(_data);
        }

        void reallocate(ArchitectureType _size) {
            allocate(_size, _size);
        }

        bool replacePartial(const SymbolObject& other, ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
            auto end = dstOffset+length;
            if(end <= dstOffset || end > blobSize)
                return false;
            end = srcOffset+length;
            if(end <= srcOffset || end > other.blobSize)
                return false;
            bitwiseCopy(blobData.get(), other.blobData.get(), dstOffset, srcOffset, length);
            return true;
        }

        template<typename DataType>
        void overwrite(DataType data) {
            allocate(sizeof(data)*8);
            *reinterpret_cast<DataType*>(blobData.get()) = data;
        }

        void overwrite(const SymbolObject& original) {
            allocate(original.blobSize);
            replacePartial(original, original.blobSize, 0, 0);
        }

        int compareBlob(const SymbolObject& other) const {
            if(blobSize < other.blobSize)
                return -1;
            if(blobSize > other.blobSize)
                return 1;
            return memcmp(blobData.get(), other.blobData.get(), (blobSize+7)/8);
        }

        bool link(bool reverseIndex, ArchitectureType index, Symbol beta, Symbol gamma) {
            auto& forward = subIndices[index];
            auto outerIter = forward.find(beta);

            if(outerIter == forward.end())
                forward.insert(std::make_pair(beta, std::set<Symbol>{gamma}));
            else if(outerIter->second.find(gamma) == outerIter->second.end())
                outerIter->second.insert(gamma);
            else
                return false;

            if(reverseIndex) {
                auto& reverse = subIndices[index+3];
                outerIter = reverse.find(gamma);
                if(outerIter == reverse.end())
                    reverse.insert(std::make_pair(gamma, std::set<Symbol>{beta}));
                else if(outerIter->second.find(beta) == outerIter->second.end())
                    outerIter->second.insert(beta);
            }

            return true;
        }

        bool unlink(bool reverseIndex, ArchitectureType index, Symbol beta, Symbol gamma) {
            auto& forward = subIndices[index];
            auto outerIter = forward.find(beta);
            if(outerIter == forward.end())
                return false;
            auto innerIter = outerIter->second.find(gamma);
            if(innerIter == outerIter->second.end())
                return false;
            outerIter->second.erase(innerIter);
            if(outerIter->second.empty())
                forward.erase(outerIter);

            if(reverseIndex) {
                auto& reverse = subIndices[index+3];
                auto outerIter = reverse.find(gamma);
                auto innerIter = outerIter->second.find(beta);
                outerIter->second.erase(innerIter);
                if(outerIter->second.empty())
                    reverse.erase(outerIter);
            }

            return true;
        };
    };

    enum IndexMode {
        MonoIndex = 1,
        TriIndex = 3,
        HexaIndex = 6
    } indexMode;
    Symbol nextSymbol;
    typedef std::map<Symbol, std::unique_ptr<SymbolObject>>::iterator TopIter;
    std::map<Symbol, std::unique_ptr<SymbolObject>> topIndex;
    struct BlobIndexCompare {
        bool operator()(const SymbolObject* a, const SymbolObject* b) const {
            return a->compareBlob(*b) < 0;
        }
    };
    std::map<SymbolObject*, Symbol, BlobIndexCompare> textIndex;

    SymbolObject* getSymbolObject(Symbol symbol) {
        auto topIter = topIndex.find(symbol);
        assert(topIter != topIndex.end());
        return topIter->second.get();
    }

    TopIter SymbolFactory(Symbol symbol) {
        return topIndex.insert(std::make_pair(symbol, std::unique_ptr<SymbolObject>(new SymbolObject()))).first;
    }

    bool link(Triple triple) {
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        bool reverseIndex = (indexMode == HexaIndex);
        for(ArchitectureType i = 0; i < indexCount; ++i) {
            auto topIter = topIndex.find(triple.pos[i]);
            if(topIter == topIndex.end())
                topIter = SymbolFactory(triple.pos[i]);
            if(!topIter->second->link(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                return false;
        }
        /* TODO: Blob merge index
        if(triple.pos[1] == PreDef_BlobType && triple.pos[2] == PreDef_Text) {
            SymbolObject* symbolObject = getSymbolObject(triple.pos[0]);
            if(symbolObject->blobSize > 0)
                textIndex.insert(std::make_pair(symbolObject, triple.pos[0]));
        }*/
        return true;
    }

    template<bool skip = false>
    bool unlink(std::set<Triple> triples, std::set<Symbol> skipSymbols = {}) {
        std::set<Symbol> dirty;
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        bool reverseIndex = (indexMode == HexaIndex);
        for(auto& triple : triples)
            for(ArchitectureType i = 0; i < indexCount; ++i) {
                dirty.insert(triple.pos[i]);
                if(skip && skipSymbols.find(triple.pos[i]) != skipSymbols.end())
                    continue;
                auto topIter = topIndex.find(triple.pos[i]);
                if(topIter == topIndex.end() ||
                   !topIter->second->unlink(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                    return false;
                /* TODO: Blob merge index
                if(triple.pos[1] == PreDef_BlobType && triple.pos[2] == PreDef_Text) {
                    SymbolObject* symbolObject = getSymbolObject(triple.pos[0]);
                    if(symbolObject->blobSize > 0)
                        textIndex.erase(symbolObject);
                }*/
            }
        for(auto alpha : dirty) {
            auto topIter = topIndex.find(alpha);
            bool empty = true;
            for(ArchitectureType i = 0; i < indexCount; ++i)
                if(!topIter->second->subIndices[i].empty()) {
                    empty = false;
                    break;
                }
            if(empty)
                topIndex.erase(topIter);
        }
        return true;
    }

    Symbol create(std::set<std::pair<Symbol, Symbol>> links = {}) {
        Symbol symbol = nextSymbol++;
        SymbolFactory(symbol);
        for(auto l : links)
            link({symbol, l.first, l.second});
        return symbol;
    }

    // TODO: Remove useage of C++ StdLib
    Symbol createFromStream(std::istream& stream) {
        stream.seekg(0, std::ios::end);
        ArchitectureType len = stream.tellg();
        stream.seekg(0, std::ios::beg);
        Symbol symbol = create({{PreDef_BlobType, PreDef_Text}});
        SymbolObject* symbolObject = getSymbolObject(symbol);
        symbolObject->allocate(len*8);
        stream.read(reinterpret_cast<char*>(symbolObject->blobData.get()), len);
        return symbol;
    }

    Symbol createFromData(const char* src) {
        ArchitectureType len = 0;
        while(src[len])
            ++len;
        Symbol symbol = create({{PreDef_BlobType, PreDef_Text}});
        SymbolObject* symbolObject = getSymbolObject(symbol);
        symbolObject->allocate(len*8);
        auto dst = reinterpret_cast<uint8_t*>(symbolObject->blobData.get());
        for(ArchitectureType i = 0; i < len; ++i)
            dst[i] = src[i];
        return symbol;
    }

    template<typename DataType>
    Symbol createFromData(DataType src) {
        Symbol blobType;
        if(typeid(DataType) == typeid(uint64_t))
            blobType = PreDef_Natural;
        else if(typeid(DataType) == typeid(int64_t))
            blobType = PreDef_Integer;
        else if(typeid(DataType) == typeid(double))
            blobType = PreDef_Float;
        else
            abort();
        Symbol symbol = create({{PreDef_BlobType, blobType}});
        getSymbolObject(symbol)->overwrite(src);
        return symbol;
    }

    Symbol mergeBlob(Symbol type, Symbol symbol) {
        // TODO: Blob merge index

        return symbol;
    }

    Context() :nextSymbol(0), indexMode(HexaIndex) {
        while(nextSymbol < sizeof(PreDefSymbols)/sizeof(void*))
            link({PreDef_RunTimeEnvironment, PreDef_Holds, createFromData(PreDefSymbols[nextSymbol])});

        Symbol ArchitectureSizeSymbol = createFromData(ArchitectureSize);
        link({PreDef_RunTimeEnvironment, PreDef_Holds, ArchitectureSizeSymbol});
        link({PreDef_RunTimeEnvironment, PreDef_ArchitectureSize, ArchitectureSizeSymbol});
    }

    ArchitectureType searchGGG(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        if(topIter == topIndex.end()) abort();
        auto& subIndex = topIter->second->subIndices[0];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        auto gammaIter = betaIter->second.find(triple.pos[2]);
        if(gammaIter == betaIter->second.end())
            return 0;
        if(callback) callback();
        return 1;
    }

    ArchitectureType searchGGV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        if(callback)
            for(auto gamma : betaIter->second) {
                triple.pos[2] = gamma;
                callback();
            }
        return betaIter->second.size();
    }

    ArchitectureType searchGVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        ArchitectureType count = 0;
        for(auto& beta : subIndex) {
            count += beta.second.size();
            if(callback) {
                triple.pos[1] = beta.first;
                for(auto gamma : beta.second) {
                    triple.pos[2] = gamma;
                    callback();
                }
            }
        }
        return count;
    }

    ArchitectureType searchGIV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        std::set<Symbol> result;
        for(auto& beta : subIndex)
            result.insert(beta.second.begin(), beta.second.end());
        if(callback)
            for(auto gamma : result) {
                triple.pos[2] = gamma;
                callback();
            }
        return result.size();
    }

    ArchitectureType searchGVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        if(callback)
            for(auto& beta : subIndex) {
                triple.pos[1] = beta.first;
                callback();
            }
        return subIndex.size();
    }

    ArchitectureType searchVII(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        if(callback)
            for(auto& alpha : topIndex) {
                triple.pos[0] = alpha.first;
                callback();
            }
        return topIndex.size();
    }

    ArchitectureType searchVVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        ArchitectureType count = 0;
        for(auto& alpha : topIndex) {
            auto& subIndex = alpha.second->subIndices[index];
            count += subIndex.size();
            if(callback) {
                triple.pos[0] = alpha.first;
                for(auto& beta : subIndex) {
                    triple.pos[1] = beta.first;
                    callback();
                }
            }
        }
        return count;
    }

    ArchitectureType searchVVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        ArchitectureType count = 0;
        for(auto& alpha : topIndex) {
            triple.pos[0] = alpha.first;
            for(auto& beta : alpha.second->subIndices[0]) {
                count += beta.second.size();
                if(callback) {
                    triple.pos[1] = beta.first;
                    for(auto gamma : beta.second) {
                        triple.pos[2] = gamma;
                        callback();
                    }
                }
            }
        }
        return count;
    }
};
