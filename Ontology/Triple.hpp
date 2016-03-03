#include "Containers.hpp"

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

union Triple {
    Identifier pos[3];
    struct {
        Identifier entity, attribute, value;
    };
    Triple() {};
    Triple(Identifier _entity, Identifier _attribute, Identifier _value)
        :entity(_entity), attribute(_attribute), value(_value) {}
    Triple reordered(ArchitectureType to) {
        ArchitectureType alpha[] = { 0, 1, 2, 0, 1, 2 },
                          beta[] = { 1, 2, 0, 2, 0, 1 },
                         gamma[] = { 2, 0, 1, 1, 2, 0 };
        return {pos[alpha[to]], pos[beta[to]], pos[gamma[to]]};
    }
    Triple normalized(ArchitectureType from) {
        ArchitectureType alpha[] = { 0, 2, 1, 0, 1, 2 },
                          beta[] = { 1, 0, 2, 2, 0, 1 },
                         gamma[] = { 2, 1, 0, 1, 2, 0 };
        return {pos[alpha[from]], pos[beta[from]], pos[gamma[from]]};
    }
};

namespace Ontology {
    BlobIndex blobIndex;

    enum IndexType {
        EAV = 0, AVE = 1, VEA = 2,
        EVA = 3, AEV = 4, VAE = 5
    };

    enum IndexMode {
        MonoIndex = 1,
        TriIndex = 3,
        HexaIndex = 6
    } indexMode = HexaIndex;

    bool subLink(Identifier symbol, ArchitectureType index, Identifier beta, Identifier gamma) {
        // TODO: Reimplement using Blobs
        /*auto& forward = subIndices[index];
        auto outerIter = forward.find(beta);

        if(outerIter == forward.end())
            forward.insert({beta, {gamma}});
        else if(outerIter->second.find(gamma) == outerIter->second.end())
            outerIter->second.insert(gamma);
        else
            return false;

        if(indexMode == HexaIndex) {
            auto& reverse = subIndices[index+3];
            outerIter = reverse.find(gamma);
            if(outerIter == reverse.end())
                reverse.insert({gamma, {beta}});
            else if(outerIter->second.find(beta) == outerIter->second.end())
                outerIter->second.insert(beta);
        }

        return true;*/
        return false;
    }

    bool subUnlink(Identifier symbol, ArchitectureType index, Identifier beta, Identifier gamma) {
        // TODO: Reimplement using Blobs
        /*auto& forward = subIndices[index];
        auto outerIter = forward.find(beta);
        if(outerIter == forward.end())
            return false;
        auto innerIter = outerIter->second.find(gamma);
        if(innerIter == outerIter->second.end())
            return false;
        outerIter->second.erase(innerIter);
        if(outerIter->second.empty())
            forward.erase(outerIter);

        if(indexMode == HexaIndex) {
            auto& reverse = subIndices[index+3];
            auto outerIter = reverse.find(gamma);
            auto innerIter = outerIter->second.find(beta);
            outerIter->second.erase(innerIter);
            if(outerIter->second.empty())
                reverse.erase(outerIter);
        }

        return true;*/
        return false;
    }

    bool link(Triple triple) {
        // TODO: Reimplement using Blobs
        /*ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        for(ArchitectureType i = 0; i < indexCount; ++i) {
            auto topIter = topIndex.find(triple.pos[i]);
            if(topIter == topIndex.end())
                topIter = SymbolFactory(triple.pos[i]);
            if(!subLink(topIter->second.get(), i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                return false;
        }
        if(triple.pos[1] == PreDef_BlobType)
            Storage::modifiedBlob(triple.pos[0]);
        return true;*/
        return false;
    }

    bool unlinkInternal(Triple triple, bool skipEnabled = false, Identifier skip = PreDef_Void) {
        // TODO: Reimplement using Blobs
        /*ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        for(ArchitectureType i = 0; i < indexCount; ++i) {
            if(skipEnabled && triple.pos[i] == skip)
                continue;
            auto topIter = topIndex.find(triple.pos[i]);
            if(topIter == topIndex.end() ||
               !subUnlink(topIter->second.get(), i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                return false;
        }
        if(triple.pos[1] == PreDef_BlobType)
            Storage::modifiedBlob(triple.pos[0]);
        return true;*/
        return false;
    }

    void checkSymbolLinkCount(Identifier symbol) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(symbol);
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        for(ArchitectureType i = 0; i < indexCount; ++i)
            if(!topIter->second->subIndices[i].empty())
                return;
        topIndex.erase(topIter);*/
    }

    void destroy(Identifier alpha) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(alpha);
        assert(topIter != topIndex.end());
        Set<true, Identifier> symbols;
        for(ArchitectureType i = EAV; i <= VEA; ++i)
            for(auto& beta : topIter->second->subIndices[i])
                for(auto gamma : beta.second) {
                    unlinkInternal(Triple(alpha, beta.first, gamma).normalized(i), true, alpha);
                    symbols.insertElement(beta.first);
                    symbols.insertElement(gamma);
                }
        topIndex.erase(topIter);
        symbols.iterate([&](Identifier symbol) {
            checkSymbolLinkCount(symbol);
        });*/
    }

    bool unlink(Triple triple) {
        if(!unlinkInternal(triple))
            return false;
        for(ArchitectureType i = 0; i < 3; ++i)
            checkSymbolLinkCount(triple.pos[i]);
        return true;
    }

    ArchitectureType searchGGG(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[0];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        auto gammaIter = betaIter->second.find(triple.pos[2]);
        if(gammaIter == betaIter->second.end())
            return 0;
        if(callback)
            callback();
        return 1;*/
        return 0;
    }

    ArchitectureType searchGGV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(triple.pos[0]);
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
        return betaIter->second.size();*/
        return 0;
    }

    ArchitectureType searchGVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(triple.pos[0]);
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
        return count;*/
        return 0;
    }

    ArchitectureType searchGIV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        Set<true, Identifier> result;
        for(auto& beta : subIndex)
            for(auto& gamma : beta.second)
                result.insertElement(gamma);
        if(callback)
            result.iterate([&](Identifier gamma) {
                triple.pos[2] = gamma;
                callback();
            });
        return result.size();*/
        return 0;
    }

    ArchitectureType searchGVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*auto topIter = topIndex.find(triple.pos[0]);
        assert(topIter != topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        if(callback)
            for(auto& beta : subIndex) {
                triple.pos[1] = beta.first;
                callback();
            }
        return subIndex.size();*/
        return 0;
    }

    ArchitectureType searchVII(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*if(callback)
            for(auto& alpha : topIndex) {
                triple.pos[0] = alpha.first;
                callback();
            }
        return topIndex.size();*/
        return 0;
    }

    ArchitectureType searchVVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*ArchitectureType count = 0;
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
        return count;*/
        return 0;
    }

    ArchitectureType searchVVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        // TODO: Reimplement using Blobs
        /*ArchitectureType count = 0;
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
        return count;*/
        return 0;
    }

    ArchitectureType query(ArchitectureType mode, Triple triple, std::function<void(Triple, ArchitectureType)> callback = nullptr) {
        struct QueryMethod {
            uint8_t index, pos, size;
            ArchitectureType(*function)(ArchitectureType, Triple&, std::function<void()>);
        };

        const QueryMethod lookup[] = {
            {EAV, 0, 0, &searchGGG},
            {AVE, 2, 1, &searchGGV},
            {AVE, 0, 0, nullptr},
            {VEA, 2, 1, &searchGGV},
            {VEA, 1, 2, &searchGVV},
            {VAE, 1, 1, &searchGVI},
            {VEA, 0, 0, nullptr},
            {VEA, 1, 1, &searchGVI},
            {VEA, 0, 0, nullptr},
            {EAV, 2, 1, &searchGGV},
            {AVE, 1, 2, &searchGVV},
            {AVE, 1, 1, &searchGVI},
            {EAV, 1, 2, &searchGVV},
            {EAV, 0, 3, &searchVVV},
            {AVE, 0, 2, &searchVVI},
            {EVA, 1, 1, &searchGVI},
            {VEA, 0, 2, &searchVVI},
            {VEA, 0, 1, &searchVII},
            {EAV, 0, 0, nullptr},
            {AEV, 1, 1, &searchGVI},
            {AVE, 0, 0, nullptr},
            {EAV, 1, 1, &searchGVI},
            {EAV, 0, 2, &searchVVI},
            {AVE, 0, 1, &searchVII},
            {EAV, 0, 0, nullptr},
            {EAV, 0, 1, &searchVII},
            {EAV, 0, 0, nullptr}
        };

        assert(mode < sizeof(lookup)/sizeof(QueryMethod));
        QueryMethod method = lookup[mode];
        if(method.function == nullptr)
            return 0;

        std::function<void()> handleNext = [&]() {
            Triple result;
            for(ArchitectureType i = 0; i < method.size; ++i)
                result.pos[i] = triple.pos[method.pos+i];
            callback(result, method.size);
        };

        if(indexMode == MonoIndex) {
            if(method.index != EAV) {
                method.index = EAV;
                method.function = &searchVVV;
                handleNext = [&]() {
                    ArchitectureType index = 0;
                    if(mode%3 == 1) ++index;
                    if(mode%9 >= 3 && mode%9 < 6) {
                        triple.pos[index] = triple.pos[1];
                        ++index;
                    }
                    if(mode >= 9 && mode < 18)
                        triple.pos[index] = triple.pos[2];
                    callback(triple, method.size);
                };
            }
        } else if(indexMode == TriIndex && method.index >= 3) {
            method.index -= 3;
            method.pos = 2;
            method.function = &searchGIV;
        }

        triple = triple.reordered(method.index);
        if(!callback) handleNext = nullptr;
        return (*method.function)(method.index, triple, handleNext);
    }

    template<typename DataType>
    Identifier createFromData(DataType src) {
        Identifier blobType;
        if(typeid(DataType) == typeid(uint64_t))
            blobType = PreDef_Natural;
        else if(typeid(DataType) == typeid(int64_t))
            blobType = PreDef_Integer;
        else if(typeid(DataType) == typeid(double))
            blobType = PreDef_Float;
        else
            abort();
        Identifier symbol = Storage::createIdentifier();
        link({symbol, PreDef_BlobType, blobType});
        Storage::overwriteBlob(symbol, src);
        return symbol;
    }

    Identifier createFromFile(const char* path) {
        int fd = open(path, O_RDONLY);
        if(fd < 0)
            return PreDef_Void;
        Identifier symbol = Storage::createIdentifier();
        link({symbol, PreDef_BlobType, PreDef_Text});
        ArchitectureType len = lseek(fd, 0, SEEK_END);
        Storage::setBlobSize(symbol, len*8);
        lseek(fd, 0, SEEK_SET);
        read(fd, reinterpret_cast<char*>(Storage::accessBlobData(symbol)), len);
        close(fd);
        return symbol;
    }

    Identifier createFromData(const char* src, ArchitectureType len) {
        Identifier symbol = Storage::createIdentifier();
        link({symbol, PreDef_BlobType, PreDef_Text});
        Storage::setBlobSize(symbol, len*8);
        auto dst = reinterpret_cast<uint8_t*>(Storage::accessBlobData(symbol));
        for(ArchitectureType i = 0; i < len; ++i)
            dst[i] = src[i];
        return symbol;
    }

    Identifier createFromData(const char* src) {
        ArchitectureType len = 0;
        while(src[len])
            ++len;
        return createFromData(src, len);
    }

    void fillPreDef() {
        const Identifier preDefSymbolsEnd = sizeof(PreDefSymbols)/sizeof(void*);
        while(Storage::nextIdentifier < preDefSymbolsEnd) {
            Identifier symbol = createFromData(PreDefSymbols[Storage::nextIdentifier]);
            link({PreDef_RunTimeEnvironment, PreDef_Holds, symbol});
        }
        for(Identifier symbol = 0; symbol < preDefSymbolsEnd; ++symbol)
            blobIndex.insertElement(symbol);
        Identifier ArchitectureSizeSymbol = createFromData(ArchitectureSize);
        link({PreDef_RunTimeEnvironment, PreDef_Holds, ArchitectureSizeSymbol});
        link({PreDef_RunTimeEnvironment, PreDef_ArchitectureSize, ArchitectureSizeSymbol});
    }
};
