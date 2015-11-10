#include "Extend.hpp"

class Context {
    struct SearchIndexEntry {
        Extend extend;
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

    struct ExtendIndexCompare {
        bool operator()(const Extend* a, const Extend* b) const {
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
    std::map<Extend*, Symbol, ExtendIndexCompare> rawIndex, textIndex, naturalIndex, integerIndex, floatIndex;
    bool debug;

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

    Extend* getExtend(Symbol symbol) {
        return &topIndex.find(symbol)->second->extend;
    }

    std::map<Extend*, Symbol, ExtendIndexCompare>* extendIndexOfType(Symbol type) {
        switch(type) {
            case PreDef_Void:
                return &rawIndex;
            case PreDef_Text:
                return &textIndex;
            case PreDef_Natural:
                return &naturalIndex;
            case PreDef_Integer:
                return &integerIndex;
            case PreDef_Float:
                return &floatIndex;
            default:
                assert(false);
                return nullptr;
        }
    }

    template<Symbol type, bool index>
    Symbol symbolFor(Extend&& extend) {
        assert(extend.size > 0);

        auto extendIndex = extendIndexOfType(type);
        if(index) {
            auto iter = extendIndex->find(&extend);
            if(iter != extendIndex->end())
                return iter->second;
        }

        Symbol symbol = nextSymbol++;
        auto pair = std::make_pair(&SymbolFactory(symbol)->second->extend, symbol);
        *pair.first = std::move(extend);
        if(index)
            extendIndex->insert(pair);
        if(type != PreDef_Void)
            link({pair.second, PreDef_Extend, type});

        return symbol;
    }

    template<Symbol type, bool search, typename T>
    Symbol symbolFor(T value) {
        Extend extend;
        extend.overwrite(value);
        return symbolFor<type, search>(std::move(extend));
    }

    Context() :nextSymbol(0), indexMode(HexaIndex), debug(true) {
        while(nextSymbol < sizeof(PreDefStrings)/sizeof(void*))
            link({PreDef_Foundation, PreDef_Holds, symbolFor<PreDef_Text, true>(PreDefStrings[nextSymbol])});
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
