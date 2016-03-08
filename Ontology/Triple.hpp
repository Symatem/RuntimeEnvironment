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

#define forEachSubIndex \
    ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3; \
    for(ArchitectureType subIndex = 0; subIndex < indexCount; ++subIndex)

union Triple {
    Identifier pos[3];
    struct {
        Identifier entity, attribute, value;
    };
    Triple() {};
    Triple(Identifier _entity, Identifier _attribute, Identifier _value)
        :entity(_entity), attribute(_attribute), value(_value) {}
    Triple forwardIndex(ArchitectureType* subIndices, ArchitectureType subIndex) {
        return {subIndices[subIndex], pos[(subIndex+1)%3], pos[(subIndex+2)%3]};
    }
    Triple reverseIndex(ArchitectureType* subIndices, ArchitectureType subIndex) {
        return {subIndices[subIndex+3], pos[(subIndex+2)%3], pos[(subIndex+1)%3]};
    }
    Triple reordered(ArchitectureType subIndex) {
        ArchitectureType alpha[] = { 0, 1, 2, 0, 1, 2 },
                          beta[] = { 1, 2, 0, 2, 0, 1 },
                         gamma[] = { 2, 0, 1, 1, 2, 0 };
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }
    Triple normalized(ArchitectureType subIndex) {
        ArchitectureType alpha[] = { 0, 2, 1, 0, 1, 2 },
                          beta[] = { 1, 0, 2, 2, 0, 1 },
                         gamma[] = { 2, 1, 0, 1, 2, 0 };
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }
};

namespace Ontology {
    Set<true, Identifier, Identifier[6]> symbols;
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

    bool linkInSubIndex(Triple triple) {
        Set<false, Identifier, Identifier> beta;
        beta.symbol = triple.pos[0];
        ArchitectureType betaIndex;
        Set<false, Identifier> gamma;
        if(beta.find(triple.pos[1], betaIndex))
            gamma.symbol = beta[betaIndex].value;
        else {
            gamma.symbol = Storage::createIdentifier();
            beta.insert(betaIndex, {triple.pos[1], gamma.symbol});
        }
        bool result = gamma.insertElement(triple.pos[2]);
        return result;
    }

    bool link(Triple triple) {
        ArchitectureType alphaIndex;
        forEachSubIndex {
            if(!symbols.find(triple.pos[subIndex], alphaIndex)) {
                symbols.insert(alphaIndex, triple.pos[subIndex]);
                for(ArchitectureType i = 0; i < indexMode; ++i)
                    symbols[alphaIndex].value[i] = Storage::createIdentifier();
            }
            if(!linkInSubIndex(triple.forwardIndex(symbols[alphaIndex].value, subIndex)))
                return false;
            if(indexMode == HexaIndex)
                assert(linkInSubIndex(triple.reverseIndex(symbols[alphaIndex].value, subIndex)));
        }
        if(triple.pos[1] == PreDef_BlobType)
            Storage::modifiedBlob(triple.pos[0]);
        return true;
    }

    ArchitectureType searchGGG(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType alphaIndex, betaIndex, gammaIndex;
        if(!symbols.find(triple.pos[0], alphaIndex))
            return 0;

        Set<false, Identifier, Identifier> beta;
        beta.symbol = symbols[alphaIndex].value[index];
        if(!beta.find(triple.pos[1], betaIndex))
            return 0;

        Set<false, Identifier> gamma;
        gamma.symbol = beta[betaIndex].value;
        if(!gamma.find(triple.pos[2], gammaIndex))
            return 0;
        if(callback)
            callback();
        return 1;
    }

    ArchitectureType searchGGV(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType alphaIndex, betaIndex;
        if(!symbols.find(triple.pos[0], alphaIndex))
            return 0;

        Set<false, Identifier, Identifier> beta;
        beta.symbol = symbols[alphaIndex].value[index];
        if(!beta.find(triple.pos[1], betaIndex))
            return 0;

        Set<false, Identifier> gamma;
        gamma.symbol = beta[betaIndex].value;
        if(callback)
            gamma.iterate([&](Pair<Identifier, ArchitectureType[0]>& gammaResult) {
                triple.pos[2] = gammaResult;
                callback();
            });
        return gamma.size();
    }

    ArchitectureType searchGVV(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType alphaIndex, count = 0;
        if(!symbols.find(triple.pos[0], alphaIndex))
            return 0;

        Set<false, Identifier, Identifier> beta;
        beta.symbol = symbols[alphaIndex].value[index];
        beta.iterate([&](Pair<Identifier, Identifier>& betaResult) {
            Set<false, Identifier> gamma;
            gamma.symbol = betaResult.value;
            if(callback) {
                triple.pos[1] = betaResult.key;
                gamma.iterate([&](Pair<Identifier, ArchitectureType[0]>& gammaResult) {
                    triple.pos[2] = gammaResult;
                    callback();
                });
            }
            count += gamma.size();
        });

        return count;
    }

    ArchitectureType searchGIV(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType alphaIndex;
        if(!symbols.find(triple.pos[0], alphaIndex))
            return 0;

        Set<true, Identifier> result;
        Set<false, Identifier, Identifier> beta;
        beta.symbol = symbols[alphaIndex].value[index];
        beta.iterate([&](Pair<Identifier, Identifier>& betaResult) {
            Set<false, Identifier> gamma;
            gamma.symbol = betaResult.value;
            gamma.iterate([&](Pair<Identifier, ArchitectureType[0]>& gammaResult) {
                result.insertElement(gammaResult);
            });
        });

        if(callback)
            result.iterate([&](Identifier gamma) {
                triple.pos[2] = gamma;
                callback();
            });
        return result.size();
    }

    ArchitectureType searchGVI(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType alphaIndex;
        if(!symbols.find(triple.pos[0], alphaIndex))
            return 0;

        Set<false, Identifier, Identifier> beta;
        beta.symbol = symbols[alphaIndex].value[index];
        if(callback)
            beta.iterate([&](Pair<Identifier, Identifier>& betaResult) {
                triple.pos[1] = betaResult.key;
                callback();
            });
        return beta.size();
    }

    ArchitectureType searchVII(ArchitectureType index, Triple& triple, Closure<void> callback) {
        if(callback)
            symbols.iterate([&](Pair<Identifier, Identifier[6]>& alphaResult) {
                triple.pos[0] = alphaResult.key;
                callback();
            });
        return symbols.size();
    }

    ArchitectureType searchVVI(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType count = 0;
        symbols.iterate([&](Pair<Identifier, Identifier[6]>& alphaResult) {
            Set<false, Identifier, Identifier> beta;
            beta.symbol = alphaResult.value[index];
            if(callback) {
                triple.pos[0] = alphaResult.key;
                beta.iterate([&](Pair<Identifier, Identifier>& betaResult) {
                    triple.pos[1] = betaResult.key;
                    callback();
                });
            }
            count += beta.size();
        });
        return count;
    }

    ArchitectureType searchVVV(ArchitectureType index, Triple& triple, Closure<void> callback) {
        ArchitectureType count = 0;
        symbols.iterate([&](Pair<Identifier, Identifier[6]>& alphaResult) {
            triple.pos[0] = alphaResult.key;
            Set<false, Identifier, Identifier> beta;
            beta.symbol = alphaResult.value[index];
            beta.iterate([&](Pair<Identifier, Identifier>& betaResult) {
                Set<false, Identifier> gamma;
                gamma.symbol = betaResult.value;
                if(callback) {
                    triple.pos[1] = betaResult.key;
                    gamma.iterate([&](Pair<Identifier, ArchitectureType[0]>& gammaResult) {
                        triple.pos[2] = gammaResult;
                        callback();
                    });
                }
                count += gamma.size();
            });
        });
        return count;
    }

    ArchitectureType query(ArchitectureType mode, Triple triple, Closure<void, Triple> callback = nullptr) {
        struct QueryMethod {
            uint8_t index, pos, size;
            ArchitectureType(*function)(ArchitectureType, Triple&, Closure<void>);
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

        Closure<void> handleNext = [&]() {
            Triple result;
            for(ArchitectureType i = 0; i < method.size; ++i)
                result.pos[i] = triple.pos[method.pos+i];
            callback(result);
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
                    callback(triple);
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

    bool unlinkInSubIndex(Triple triple) {
        Set<false, Identifier, Identifier> alpha;
        alpha.symbol = triple.pos[0];
        ArchitectureType alphaIndex, betaIndex;
        if(!alpha.find(triple.pos[1], alphaIndex))
            return false;
        Set<false, Identifier> beta;
        beta.symbol = alpha[alphaIndex].value;
        if(!beta.find(triple.pos[2], betaIndex))
            return false;
        beta.erase(betaIndex);
        if(beta.empty()) {
            alpha.erase(alphaIndex);
            Storage::releaseIdentifier(beta.symbol);
        }
        return true;
    }

    bool unlinkWithoutReleasing(Triple triple, bool skipEnabled = false, Identifier skip = PreDef_Void) {
        ArchitectureType alphaIndex;
        forEachSubIndex {
            if(skipEnabled && triple.pos[subIndex] == skip)
                continue;
            if(!symbols.find(triple.pos[subIndex], alphaIndex))
                return false;
            if(!unlinkInSubIndex(triple.forwardIndex(symbols[alphaIndex].value, subIndex)))
                return false;
            if(indexMode == HexaIndex)
                assert(unlinkInSubIndex(triple.reverseIndex(symbols[alphaIndex].value, subIndex)));
        }
        if(triple.pos[1] == PreDef_BlobType)
            Storage::modifiedBlob(triple.pos[0]);
        return true;
    }

    void tryToReleaseSymbol(Identifier symbol) {
        ArchitectureType alphaIndex;
        assert(symbols.find(symbol, alphaIndex));
        forEachSubIndex {
            Set<false, Identifier, Identifier> beta;
            beta.symbol = symbols[alphaIndex].value[subIndex];
            if(!beta.empty())
                return;
        }
        for(ArchitectureType subIndex = 0; subIndex < indexCount; ++subIndex)
            Storage::releaseIdentifier(symbols[alphaIndex].value[subIndex]);
        Storage::releaseIdentifier(symbol);
    }

    bool unlink(Triple triple) {
        if(!unlinkWithoutReleasing(triple))
            return false;
        for(ArchitectureType i = 0; i < 3; ++i)
            tryToReleaseSymbol(triple.pos[i]);
        return true;
    }

    bool unlink(Identifier symbol) {
        ArchitectureType alphaIndex;
        if(!symbols.find(symbol, alphaIndex)) {
            Storage::releaseIdentifier(symbol);
            return false;
        }
        Set<true, Identifier> dirty;
        forEachSubIndex {
            Set<false, Identifier, Identifier> beta;
            beta.symbol = symbols[alphaIndex].value[subIndex];
            beta.iterate([&](Pair<Identifier, Identifier>& betaResult) {
                dirty.insertElement(betaResult.key);
                Set<false, Identifier> gamma;
                gamma.symbol = betaResult.value;
                gamma.iterate([&](Pair<Identifier, ArchitectureType[0]>& gammaResult) {
                    dirty.insertElement(gammaResult.key);
                    unlinkWithoutReleasing(Triple(symbol, betaResult.key, gammaResult.key).normalized(subIndex), true, symbol);
                });
            });
        }
        dirty.iterate([&](Identifier symbol) {
            tryToReleaseSymbol(symbol);
        });
        for(ArchitectureType subIndex = 0; subIndex < indexCount; ++subIndex)
            Storage::releaseIdentifier(symbols[alphaIndex].value[subIndex]);
        Storage::releaseIdentifier(symbol);
        return true;
    }

    void setSolitary(Triple triple, bool linkVoid = false) {
        Set<true, Identifier> dirty;
        bool toLink = (linkVoid || triple.value != PreDef_Void);
        query(9, triple, [&](Triple result) {
            if((triple.pos[2] == result.pos[0]) && (linkVoid || result.pos[0] != PreDef_Void))
                toLink = false;
            else
                dirty.insertElement(result.pos[0]);
        });
        if(toLink)
            link(triple);
        dirty.iterate([&](Identifier symbol) {
            unlinkWithoutReleasing({triple.pos[0], triple.pos[1], symbol});
        });
        if(!linkVoid)
            dirty.insertElement(triple.pos[0]);
        dirty.insertElement(triple.pos[1]);
        dirty.iterate([&](Identifier symbol) {
            tryToReleaseSymbol(symbol);
        });
    }

    bool getUncertain(Identifier alpha, Identifier beta, Identifier& gamma) {
        return (query(9, {alpha, beta, PreDef_Void}, [&](Triple result) {
            gamma = result.pos[0];
        }) == 1);
    }

    void scrutinizeExistence(Identifier symbol) {
        Set<true, Identifier> symbols;
        symbols.insertElement(symbol);
        while(!symbols.empty()) {
            symbol = symbols.pop_back();
            if(query(1, {PreDef_Void, PreDef_Holds, symbol}) > 0)
                continue;
            query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result) {
                symbols.insertElement(result.pos[0]);
            });
            // TODO: Prevent double free
            unlink(symbol);
        }
    }

    void overwriteBlobWithString(Identifier symbol, const char* src, ArchitectureType len) {
        link({symbol, PreDef_BlobType, PreDef_Text});
        Storage::setBlobSize(symbol, len*8);
        auto dst = reinterpret_cast<uint8_t*>(Storage::accessBlobData(symbol));
        for(ArchitectureType i = 0; i < len; ++i)
            dst[i] = src[i];
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
            crash("createFromData<InvalidType>");
        Identifier symbol = Storage::createIdentifier();
        link({symbol, PreDef_BlobType, blobType});
        Storage::overwriteBlob(symbol, src);
        return symbol;
    }

    Identifier createFromFile(const char* path) {
        // TODO: Move to POSIX API
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
        overwriteBlobWithString(symbol, src, len);
        return symbol;
    }

    Identifier createFromData(const char* src) {
        return createFromData(src, strlen(src));
    }

    void fillPreDef() {
        const Identifier preDefSymbolsEnd = sizeof(PreDefSymbols)/sizeof(void*);
        Storage::maxIdentifier = preDefSymbolsEnd;
        for(Identifier symbol = 0; symbol < preDefSymbolsEnd; ++symbol) {
            const char* str = PreDefSymbols[symbol];
            overwriteBlobWithString(symbol, str, strlen(str));
            link({PreDef_RunTimeEnvironment, PreDef_Holds, symbol});
            blobIndex.insertElement(symbol);
        }
        Identifier ArchitectureSizeSymbol = createFromData(ArchitectureSize);
        link({PreDef_RunTimeEnvironment, PreDef_Holds, ArchitectureSizeSymbol});
        link({PreDef_RunTimeEnvironment, PreDef_ArchitectureSize, ArchitectureSizeSymbol});
    }
};
