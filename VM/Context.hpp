#include "Blob.hpp"

class Context {
    struct SearchIndexEntry {
        Blob blob;
        std::map<Symbol, std::set<Symbol>> subIndices[6];

        bool link(bool reverseIndex, ArchitectureType index, Symbol beta, Symbol gamma) {
            auto& forward = subIndices[index];
            auto outerIter = forward.find(beta);

            if(outerIter == forward.end())
                forward.insert(std::make_pair(beta, std::set<Symbol>{gamma}));
            else if(outerIter->second.find(gamma) == outerIter->second.end())
                outerIter->second.insert(gamma);
            else return false;

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
            if(outerIter == forward.end()) return false;
            auto innerIter = outerIter->second.find(gamma);
            if(innerIter == outerIter->second.end()) return false;
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

    struct BlobIndexCompare {
        bool operator()(const Blob* a, const Blob* b) const {
            return a->compare(*b) < 0;
        }
    };

    public:
    enum IndexMode {
        MonoIndex = 1,
        TriIndex = 3,
        HexaIndex = 6
    } indexMode;
    Symbol nextSymbol;
    typedef std::map<Symbol, std::unique_ptr<SearchIndexEntry>>::iterator TopIter;
    std::map<Symbol, std::unique_ptr<SearchIndexEntry>> topIndex;
    std::map<Blob*, Symbol, BlobIndexCompare> textIndex;

    TopIter SymbolFactory(Symbol symbol) {
        return topIndex.insert(std::make_pair(symbol, std::unique_ptr<SearchIndexEntry>(new SearchIndexEntry()))).first;
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
        if(triple.pos[1] == PreDef_BlobType && triple.pos[2] == PreDef_Text) {
            Blob* blob = getBlob(triple.pos[0]);
            if(blob->size > 0)
                textIndex.insert(std::make_pair(blob, triple.pos[0]));
        }
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
                if(skip && skipSymbols.find(triple.pos[i]) != skipSymbols.end()) continue;
                auto topIter = topIndex.find(triple.pos[i]);
                if(topIter == topIndex.end() ||
                   !topIter->second->unlink(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                    return false;
                if(triple.pos[1] == PreDef_BlobType && triple.pos[2] == PreDef_Text) {
                    Blob* blob = getBlob(triple.pos[0]);
                    if(blob->size > 0)
                        textIndex.erase(blob);
                }
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

    Blob* getBlob(Symbol symbol) {
        return &topIndex.find(symbol)->second->blob;
    }

    template<Symbol type>
    Symbol symbolFor(Blob&& blob) {
        if(type == PreDef_Text) {
            auto iter = textIndex.find(&blob);
            if(iter != textIndex.end())
                return iter->second;
        }

        Symbol symbol = nextSymbol++;
        auto pair = std::make_pair(&SymbolFactory(symbol)->second->blob, symbol);
        *pair.first = std::move(blob);
        link({pair.second, PreDef_BlobType, type});

        return symbol;
    }

    template<Symbol type, typename T>
    Symbol symbolFor(T value) {
        Blob blob;
        blob.overwrite(value);
        return symbolFor<type>(std::move(blob));
    }

    Context() :nextSymbol(0), indexMode(HexaIndex) {
        while(nextSymbol < sizeof(PreDefSymbols)/sizeof(void*))
            link({PreDef_RunTimeEnvironment, PreDef_Holds, symbolFor<PreDef_Text>(PreDefSymbols[nextSymbol])});
        link({PreDef_RunTimeEnvironment, PreDef_ArchitectureSize, symbolFor<PreDef_Natural>(ArchitectureSize)});
    }

    ArchitectureType searchGGG(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        if(topIter == topIndex.end()) abort();
        auto& subIndex = topIter->second->subIndices[0];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end()) return 0;
        auto gammaIter = betaIter->second.find(triple.pos[2]);
        if(gammaIter == betaIter->second.end()) return 0;
        if(callback) callback();
        return 1;
    }

    ArchitectureType searchGGV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end()) return 0;
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
