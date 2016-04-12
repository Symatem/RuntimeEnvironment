#include "Containers.hpp"

namespace Ontology {

#define Wrapper(token) token##Symbol
enum PreDefinedSymbols {
#include "Symbols.hpp"
};
#undef Wrapper

#define Wrapper(token) #token
const char* PreDefinedSymbols[] = {
#include "Symbols.hpp"
};
#undef Wrapper

#define forEachSubIndex \
    NativeNaturalType indexCount = (indexMode == MonoIndex) ? 1 : 3; \
    for(NativeNaturalType subIndex = 0; subIndex < indexCount; ++subIndex)

#define eraseSymbol \
    for(NativeNaturalType subIndex = 0; subIndex < indexMode; ++subIndex) \
        Storage::releaseSymbol(element.value[subIndex]); \
    Storage::releaseSymbol(symbol); \
    symbols.erase(alphaIndex);

struct Triple {
    Symbol pos[3];

    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :pos{_entity, _attribute, _value} {}

    Triple forwardIndex(NativeNaturalType* subIndices, NativeNaturalType subIndex) {
        return {subIndices[subIndex], pos[(subIndex+1)%3], pos[(subIndex+2)%3]};
    }

    Triple reverseIndex(NativeNaturalType* subIndices, NativeNaturalType subIndex) {
        return {subIndices[subIndex+3], pos[(subIndex+2)%3], pos[(subIndex+1)%3]};
    }

    Triple reordered(NativeNaturalType subIndex) {
        NativeNaturalType alpha[] = {0, 1, 2, 0, 1, 2},
                           beta[] = {1, 2, 0, 2, 0, 1},
                          gamma[] = {2, 0, 1, 1, 2, 0};
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }

    Triple normalized(NativeNaturalType subIndex) {
        NativeNaturalType alpha[] = {0, 2, 1, 0, 1, 2},
                           beta[] = {1, 0, 2, 2, 0, 1},
                          gamma[] = {2, 1, 0, 1, 2, 0};
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }
};

enum IndexType {
    EAV = 0, AVE = 1, VEA = 2,
    EVA = 3, AEV = 4, VAE = 5
};

enum IndexMode {
    MonoIndex = 1,
    TriIndex = 3,
    HexaIndex = 6
} indexMode = HexaIndex;

BlobSet<false, Symbol, Symbol[6]> symbols;
BlobIndex<false> blobIndex;

bool linkInSubIndex(Triple triple) {
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = triple.pos[0];
    NativeNaturalType betaIndex;
    BlobSet<false, Symbol> gamma;
    if(beta.find(triple.pos[1], betaIndex))
        gamma.symbol = beta.readElementAt(betaIndex).value;
    else {
        gamma.symbol = Storage::createSymbol();
        beta.insert(betaIndex, {triple.pos[1], gamma.symbol});
    }
    bool result = gamma.insertElement(triple.pos[2]);
    return result;
}

bool linkTriplePartial(Triple triple, NativeNaturalType subIndex) {
    NativeNaturalType alphaIndex;
    Pair<Symbol, Symbol[6]> element;
    if(!symbols.find(triple.pos[subIndex], alphaIndex)) {
        element.key = triple.pos[subIndex];
        for(NativeNaturalType i = 0; i < indexMode; ++i)
            element.value[i] = Storage::createSymbol();
        symbols.insert(alphaIndex, element);
    } else
        element = symbols.readElementAt(alphaIndex);
    if(!linkInSubIndex(triple.forwardIndex(element.value, subIndex)))
        return false;
    if(indexMode == HexaIndex)
        assert(linkInSubIndex(triple.reverseIndex(element.value, subIndex)));
    return true;
}

bool link(Triple triple) {
    forEachSubIndex
        linkTriplePartial(triple, subIndex);
    if(triple.pos[1] == BlobTypeSymbol)
        Storage::modifiedBlob(triple.pos[0]);
    return true;
}

NativeNaturalType searchGGG(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType alphaIndex, betaIndex, gammaIndex;
    if(!symbols.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = symbols.readElementAt(alphaIndex).value[subIndex];
    if(!beta.find(triple.pos[1], betaIndex))
        return 0;
    BlobSet<false, Symbol> gamma;
    gamma.symbol = beta.readElementAt(betaIndex).value;
    if(!gamma.find(triple.pos[2], gammaIndex))
        return 0;
    if(callback)
        callback();
    return 1;
}

NativeNaturalType searchGGV(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType alphaIndex, betaIndex;
    if(!symbols.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = symbols.readElementAt(alphaIndex).value[subIndex];
    if(!beta.find(triple.pos[1], betaIndex))
        return 0;
    BlobSet<false, Symbol> gamma;
    gamma.symbol = beta.readElementAt(betaIndex).value;
    if(callback)
        gamma.iterate([&](Pair<Symbol, NativeNaturalType[0]> gammaResult) {
            triple.pos[2] = gammaResult;
            callback();
        });
    return gamma.size();
}

NativeNaturalType searchGVV(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType alphaIndex, count = 0;
    if(!symbols.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = symbols.readElementAt(alphaIndex).value[subIndex];
    beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
        BlobSet<false, Symbol> gamma;
        gamma.symbol = betaResult.value;
        if(callback) {
            triple.pos[1] = betaResult.key;
            gamma.iterate([&](Pair<Symbol, NativeNaturalType[0]> gammaResult) {
                triple.pos[2] = gammaResult;
                callback();
            });
        }
        count += gamma.size();
    });
    return count;
}

NativeNaturalType searchGIV(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType alphaIndex;
    if(!symbols.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<true, Symbol> result;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = symbols.readElementAt(alphaIndex).value[subIndex];
    beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
        BlobSet<false, Symbol> gamma;
        gamma.symbol = betaResult.value;
        gamma.iterate([&](Pair<Symbol, NativeNaturalType[0]> gammaResult) {
            result.insertElement(gammaResult);
        });
    });
    if(callback)
        result.iterate([&](Symbol gamma) {
            triple.pos[2] = gamma;
            callback();
        });
    return result.size();
}

NativeNaturalType searchGVI(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType alphaIndex;
    if(!symbols.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = symbols.readElementAt(alphaIndex).value[subIndex];
    if(callback)
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            triple.pos[1] = betaResult.key;
            callback();
        });
    return beta.size();
}

NativeNaturalType searchVII(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    if(callback)
        symbols.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            triple.pos[0] = alphaResult.key;
            callback();
        });
    return symbols.size();
}

NativeNaturalType searchVVI(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType count = 0;
    symbols.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = alphaResult.value[subIndex];
        if(callback) {
            triple.pos[0] = alphaResult.key;
            beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
                triple.pos[1] = betaResult.key;
                callback();
            });
        }
        count += beta.size();
    });
    return count;
}

NativeNaturalType searchVVV(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType count = 0;
    symbols.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
        triple.pos[0] = alphaResult.key;
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = alphaResult.value[subIndex];
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            BlobSet<false, Symbol> gamma;
            gamma.symbol = betaResult.value;
            if(callback) {
                triple.pos[1] = betaResult.key;
                gamma.iterate([&](Pair<Symbol, NativeNaturalType[0]> gammaResult) {
                    triple.pos[2] = gammaResult;
                    callback();
                });
            }
            count += gamma.size();
        });
    });
    return count;
}

NativeNaturalType query(NativeNaturalType mode, Triple triple, Closure<void(Triple)> callback = nullptr) {
    struct QueryMethod {
        NativeNaturalType subIndex, pos, size;
        NativeNaturalType(*function)(NativeNaturalType, Triple&, Closure<void()>);
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
    Closure<void()> handleNext = [&]() {
        Triple result;
        for(NativeNaturalType i = 0; i < method.size; ++i)
            result.pos[i] = triple.pos[method.pos+i];
        callback(result);
    };
    if(indexMode == MonoIndex) {
        if(method.subIndex != EAV) {
            method.subIndex = EAV;
            method.function = &searchVVV;
            handleNext = [&]() {
                NativeNaturalType subIndex = 0;
                if(mode%3 == 1) ++subIndex;
                if(mode%9 >= 3 && mode%9 < 6) {
                    triple.pos[subIndex] = triple.pos[1];
                    ++subIndex;
                }
                if(mode >= 9 && mode < 18)
                    triple.pos[subIndex] = triple.pos[2];
                callback(triple);
            };
        }
    } else if(indexMode == TriIndex && method.subIndex >= 3) {
        method.subIndex -= 3;
        method.pos = 2;
        method.function = &searchGIV;
    }
    triple = triple.reordered(method.subIndex);
    if(!callback)
        handleNext = nullptr;
    return (*method.function)(method.subIndex, triple, handleNext);
}

// TODO: Test
void setIndexMode(IndexMode _indexMode) {
    assert(indexMode != _indexMode);
    if(indexMode < _indexMode) {
        Triple triple;
        searchVVV(EAV, triple, [&]() {
            NativeNaturalType indexCount = (_indexMode == MonoIndex) ? 1 : 3;
            for(NativeNaturalType subIndex = indexMode; subIndex < indexCount; ++subIndex)
                linkTriplePartial(triple, subIndex);
        });
    } else
        symbols.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            for(NativeNaturalType subIndex = _indexMode; subIndex < indexMode; ++subIndex) {
                BlobSet<false, Symbol, Symbol> beta;
                beta.symbol = alphaResult.value[subIndex];
                beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
                    Storage::releaseSymbol(betaResult.value);
                });
                beta.clear();
            }
        });
    indexMode = _indexMode;
}

bool unlinkInSubIndex(Triple triple) {
    BlobSet<false, Symbol, Symbol> alpha;
    alpha.symbol = triple.pos[0];
    NativeNaturalType alphaIndex, betaIndex;
    if(!alpha.find(triple.pos[1], alphaIndex))
        return false;
    BlobSet<false, Symbol> beta;
    beta.symbol = alpha.readElementAt(alphaIndex).value;
    if(!beta.find(triple.pos[2], betaIndex))
        return false;
    beta.erase(betaIndex);
    if(beta.empty()) {
        alpha.erase(alphaIndex);
        Storage::releaseSymbol(beta.symbol);
    }
    return true;
}

bool unlinkWithoutReleasing(Triple triple, bool skipEnabled = false, Symbol skip = VoidSymbol) {
    NativeNaturalType alphaIndex;
    forEachSubIndex {
        if(skipEnabled && triple.pos[subIndex] == skip)
            continue;
        if(!symbols.find(triple.pos[subIndex], alphaIndex))
            return false;
        Pair<Symbol, Symbol[6]> element = symbols.readElementAt(alphaIndex);
        if(!unlinkInSubIndex(triple.forwardIndex(element.value, subIndex)))
            return false;
        if(indexMode == HexaIndex)
            assert(unlinkInSubIndex(triple.reverseIndex(element.value, subIndex)));
    }
    if(triple.pos[1] == BlobTypeSymbol)
        Storage::modifiedBlob(triple.pos[0]);
    return true;
}

void tryToReleaseSymbol(Symbol symbol) {
    NativeNaturalType alphaIndex;
    assert(symbols.find(symbol, alphaIndex));
    Pair<Symbol, Symbol[6]> element = symbols.readElementAt(alphaIndex);
    forEachSubIndex {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        if(!beta.empty())
            return;
    }
    eraseSymbol
}

bool unlink(Triple triple) {
    if(!unlinkWithoutReleasing(triple))
        return false;
    for(NativeNaturalType i = 0; i < 3; ++i)
        tryToReleaseSymbol(triple.pos[i]);
    return true;
}

bool unlink(Symbol symbol) {
    NativeNaturalType alphaIndex;
    if(!symbols.find(symbol, alphaIndex)) {
        Storage::releaseSymbol(symbol);
        return false;
    }
    Pair<Symbol, Symbol[6]> element = symbols.readElementAt(alphaIndex);
    BlobSet<true, Symbol> dirty;
    forEachSubIndex {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            dirty.insertElement(betaResult.key);
            BlobSet<false, Symbol> gamma;
            gamma.symbol = betaResult.value;
            gamma.iterate([&](Pair<Symbol, NativeNaturalType[0]> gammaResult) {
                dirty.insertElement(gammaResult.key);
                unlinkWithoutReleasing(Triple(symbol, betaResult.key, gammaResult.key).normalized(subIndex), true, symbol);
            });
        });
    }
    dirty.iterate([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
    eraseSymbol
    return true;
}

void setSolitary(Triple triple, bool linkVoidSymbol = false) {
    BlobSet<true, Symbol> dirty;
    bool toLink = (linkVoidSymbol || triple.pos[2] != VoidSymbol);
    query(9, triple, [&](Triple result) {
        if((triple.pos[2] == result.pos[0]) && (linkVoidSymbol || result.pos[0] != VoidSymbol))
            toLink = false;
        else
            dirty.insertElement(result.pos[0]);
    });
    if(toLink)
        link(triple);
    dirty.iterate([&](Symbol symbol) {
        unlinkWithoutReleasing({triple.pos[0], triple.pos[1], symbol});
    });
    if(!linkVoidSymbol)
        dirty.insertElement(triple.pos[0]);
    dirty.insertElement(triple.pos[1]);
    dirty.iterate([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
}

bool getUncertain(Symbol entity, Symbol attribute, Symbol& value) {
    return (query(9, {entity, attribute, VoidSymbol}, [&](Triple result) {
        value = result.pos[0];
    }) == 1);
}

void scrutinizeExistence(Symbol symbol) {
    BlobSet<true, Symbol> symbols;
    symbols.insertElement(symbol);
    while(!symbols.empty()) {
        symbol = symbols.pop_back();
        if(query(1, {VoidSymbol, HoldsSymbol, symbol}) > 0)
            continue;
        query(9, {symbol, HoldsSymbol, VoidSymbol}, [&](Triple result) {
            symbols.insertElement(result.pos[0]);
        });
        unlink(symbol); // TODO
    }
}

template<typename DataType>
Symbol createFromData(DataType src) {
    Symbol blobType;
    if(isSame<DataType, NativeNaturalType>::value)
        blobType = NaturalSymbol;
    else if(isSame<DataType, NativeIntegerType>::value)
        blobType = IntegerSymbol;
    else if(isSame<DataType, NativeFloatType>::value)
        blobType = FloatSymbol;
    else
        assert(false);
    Symbol symbol = Storage::createSymbol();
    link({symbol, BlobTypeSymbol, blobType});
    Storage::writeBlob(symbol, src);
    return symbol;
}

Symbol createFromSlice(Symbol src, NativeNaturalType srcOffset, NativeNaturalType length) {
    Symbol dst = Storage::createSymbol();
    Storage::setBlobSize(dst, length);
    Storage::sliceBlob(dst, src, 0, srcOffset, length);
    return dst;
}

void stringToBlob(const char* src, NativeNaturalType length, Symbol dst) {
    link({dst, BlobTypeSymbol, TextSymbol});
    Storage::setBlobSize(dst, length*8);
    for(NativeNaturalType i = 0; i < length; ++i)
        Storage::writeBlobAt<char>(dst, i, src[i]);
    Storage::modifiedBlob(dst);
}

Symbol createFromString(const char* src) {
    Symbol dst = Storage::createSymbol();
    stringToBlob(src, strlen(src), dst);
    return dst;
}

void tryToFillPreDefined() {
    const Symbol preDefinedSymbolsEnd = sizeof(PreDefinedSymbols)/sizeof(void*);
    Storage::symbolCount = preDefinedSymbolsEnd;
    // TODO: Persist these symbols
    blobIndex.symbol = Storage::createSymbol();
    symbols.symbol = Storage::createSymbol();
    if(!symbols.empty())
        return;
    for(Symbol symbol = 0; symbol < preDefinedSymbolsEnd; ++symbol) {
        const char* str = PreDefinedSymbols[symbol];
        stringToBlob(str, strlen(str), symbol);
        link({RunTimeEnvironmentSymbol, HoldsSymbol, symbol});
        blobIndex.insertElement(symbol);
    }
    Symbol ArchitectureSize = createFromData(architectureSize);
    link({RunTimeEnvironmentSymbol, HoldsSymbol, ArchitectureSize});
    link({RunTimeEnvironmentSymbol, ArchitectureSizeSymbol, ArchitectureSize});
}

};
