#include "Ontology.hpp"

struct Context { // TODO: : public Storage {
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
    std::map<SymbolObject*, Symbol, BlobIndexCompare> blobIndex;

    SymbolObject* getSymbolObject(Symbol symbol) {
        auto topIter = topIndex.find(symbol);
        assert(topIter != topIndex.end());
        return topIter->second.get();
    }

    // TODO: Remove useage of C++ StdLib
    void debugPrintSymbol(Symbol symbol, std::ostream& stream = std::cout) {
        SymbolObject* symbolObject = getSymbolObject(symbol);
        stream.write(reinterpret_cast<const char*>(symbolObject->blobData.get()), symbolObject->blobSize/8);
    }

    // TODO: Needs to be called at every blob mutation
    bool unindexBlob(Symbol symbol) {
        auto iter = blobIndex.find(getSymbolObject(symbol));
        if(iter == blobIndex.end() || iter->second != symbol)
            return false;
        blobIndex.erase(iter);
        return true;
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
        return true;
    }

    template<bool skip = false>
    bool unlink(std::set<Triple> triples, std::set<Symbol> symbols = {}) {
        assert(!triples.empty());
        std::set<Symbol> dirty;
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        bool reverseIndex = (indexMode == HexaIndex);
        for(auto& triple : triples) {
            for(ArchitectureType i = 0; i < indexCount; ++i) {
                dirty.insert(triple.pos[i]);
                if(skip && symbols.find(triple.pos[i]) != symbols.end())
                    continue;
                auto topIter = topIndex.find(triple.pos[i]);
                if(topIter == topIndex.end() ||
                   !topIter->second->unlink(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                    return false;
            }
            if(triple.pos[1] == PreDef_BlobType)
                unindexBlob(triple.pos[0]);
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
        for(auto alpha : symbols)
            topIndex.erase(alpha);
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
        symbolObject->allocateBlob(len*8);
        stream.read(reinterpret_cast<char*>(symbolObject->blobData.get()), len);
        return symbol;
    }

    Symbol createFromData(const char* src, ArchitectureType len) {
        Symbol symbol = create({{PreDef_BlobType, PreDef_Text}});
        SymbolObject* symbolObject = getSymbolObject(symbol);
        symbolObject->allocateBlob(len*8);
        auto dst = reinterpret_cast<uint8_t*>(symbolObject->blobData.get());
        for(ArchitectureType i = 0; i < len; ++i)
            dst[i] = src[i];
        return symbol;
    }

    Symbol createFromData(const char* src) {
        ArchitectureType len = 0;
        while(src[len])
            ++len;
        return createFromData(src, len);
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
        getSymbolObject(symbol)->overwriteBlob(src);
        return symbol;
    }

    Context() :nextSymbol(0), indexMode(HexaIndex) {
        while(nextSymbol < sizeof(PreDefSymbols)/sizeof(void*)) {
            Symbol symbol = createFromData(PreDefSymbols[nextSymbol]);
            link({PreDef_RunTimeEnvironment, PreDef_Holds, symbol});
            blobIndex.insert(std::make_pair(getSymbolObject(symbol), symbol));
        }
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
