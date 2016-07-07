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

struct Triple {
    Symbol pos[3];

    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :pos{_entity, _attribute, _value} {}

    Triple forwardIndex(NativeNaturalType* subIndices, NativeNaturalType subIndex) {
        return {subIndices[subIndex], pos[(subIndex+1)%3], pos[(subIndex+2)%3]};
    }

    Triple invertedIndex(NativeNaturalType* subIndices, NativeNaturalType subIndex) {
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

BlobSet<false, Symbol, Symbol[6]> tripleIndex;
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
    return gamma.insertElement(triple.pos[2]);
}

bool linkTriplePartial(Triple triple, NativeNaturalType subIndex) {
    NativeNaturalType alphaIndex;
    Pair<Symbol, Symbol[6]> element;
    if(!tripleIndex.find(triple.pos[subIndex], alphaIndex)) {
        element.key = triple.pos[subIndex];
        for(NativeNaturalType i = 0; i < indexMode; ++i)
            element.value[i] = Storage::createSymbol();
        tripleIndex.insert(alphaIndex, element);
    } else
        element = tripleIndex.readElementAt(alphaIndex);
    if(!linkInSubIndex(triple.forwardIndex(element.value, subIndex)))
        return false;
    if(indexMode == HexaIndex)
        assert(linkInSubIndex(triple.invertedIndex(element.value, subIndex)));
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
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
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
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
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
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
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
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<true, Symbol> result;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
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
    if(!tripleIndex.find(triple.pos[0], alphaIndex))
        return 0;
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = tripleIndex.readElementAt(alphaIndex).value[subIndex];
    if(callback)
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            triple.pos[1] = betaResult.key;
            callback();
        });
    return beta.size();
}

NativeNaturalType searchVII(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    if(callback)
        tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            triple.pos[0] = alphaResult.key;
            callback();
        });
    return tripleIndex.size();
}

NativeNaturalType searchVVI(NativeNaturalType subIndex, Triple& triple, Closure<void()> callback) {
    NativeNaturalType count = 0;
    tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
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
    tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
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

bool valueCountIs(Symbol entity, Symbol attribute, NativeNaturalType size) {
    return query(9, {entity, attribute, VoidSymbol}) == size;
}

bool tripleExists(Triple triple) {
    return query(0, triple) == 1;
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
        tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
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
    BlobSet<false, Symbol, Symbol> beta;
    beta.symbol = triple.pos[0];
    NativeNaturalType betaIndex, gammaIndex;
    if(!beta.find(triple.pos[1], betaIndex))
        return false;
    BlobSet<false, Symbol> gamma;
    gamma.symbol = beta.readElementAt(betaIndex).value;
    if(!gamma.find(triple.pos[2], gammaIndex))
        return false;
    gamma.erase(gammaIndex);
    if(gamma.empty()) {
        beta.erase(betaIndex);
        Storage::releaseSymbol(gamma.symbol);
    }
    return true;
}

bool unlinkWithoutReleasing(Triple triple, bool skipEnabled = false, Symbol skip = VoidSymbol) {
    NativeNaturalType alphaIndex;
    forEachSubIndex {
        if(skipEnabled && triple.pos[subIndex] == skip)
            continue;
        if(!tripleIndex.find(triple.pos[subIndex], alphaIndex))
            return false;
        Pair<Symbol, Symbol[6]> element = tripleIndex.readElementAt(alphaIndex);
        if(!unlinkInSubIndex(triple.forwardIndex(element.value, subIndex)))
            return false;
        if(indexMode == HexaIndex)
            assert(unlinkInSubIndex(triple.invertedIndex(element.value, subIndex)));
    }
    if(triple.pos[1] == BlobTypeSymbol)
        Storage::modifiedBlob(triple.pos[0]);
    return true;
}

void eraseSymbol(Pair<Symbol, Symbol[6]>& element, NativeNaturalType alphaIndex, Symbol symbol) {
    for(NativeNaturalType subIndex = 0; subIndex < indexMode; ++subIndex) {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            Storage::releaseSymbol(betaResult.value);
        });
        Storage::releaseSymbol(beta.symbol);
    }
    tripleIndex.erase(alphaIndex);
    Storage::releaseSymbol(symbol);
}

void tryToReleaseSymbol(Symbol symbol) {
    NativeNaturalType alphaIndex;
    assert(tripleIndex.find(symbol, alphaIndex));
    Pair<Symbol, Symbol[6]> element = tripleIndex.readElementAt(alphaIndex);
    forEachSubIndex {
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = element.value[subIndex];
        if(!beta.empty())
            return;
    }
    eraseSymbol(element, alphaIndex, symbol);
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
    if(!tripleIndex.find(symbol, alphaIndex)) {
        Storage::releaseSymbol(symbol);
        return false;
    }
    Pair<Symbol, Symbol[6]> element = tripleIndex.readElementAt(alphaIndex);
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
    eraseSymbol(element, alphaIndex, symbol);
    dirty.iterate([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
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
    static_assert(sizeOfInBits<DataType>::value == architectureSize);
    Symbol blobType = VoidSymbol;
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
    Storage::Blob(symbol).write(src);
    return symbol;
}

Symbol createFromSlice(Symbol src, NativeNaturalType srcOffset, NativeNaturalType length) {
    Symbol dst = Storage::createSymbol();
    Storage::Blob dstBlob(dst);
    dstBlob.increaseSize(0, length);
    dstBlob.slice(Storage::Blob(src), 0, srcOffset, length);
    return dst;
}

void stringToBlob(const char* src, NativeNaturalType length, Symbol dstSymbol) {
    link({dstSymbol, BlobTypeSymbol, TextSymbol});
    Storage::Blob dstBlob(dstSymbol);
    dstBlob.increaseSize(0, length*8);
    dstBlob.externalOperate<true>(const_cast<Integer8*>(src), 0, length*8);
    Storage::modifiedBlob(dstSymbol);
}

Symbol createFromString(const char* src) {
    Symbol dst = Storage::createSymbol();
    stringToBlob(src, strlen(src), dst);
    return dst;
}

bool tryToFillPreDefined(NativeNaturalType additionalSymbols = 0) {
    const Symbol preDefinedSymbolsEnd = sizeof(PreDefinedSymbols)/sizeof(void*);
    tripleIndex.symbol = preDefinedSymbolsEnd;
    blobIndex.symbol = preDefinedSymbolsEnd+1;
    if(!tripleIndex.empty())
        return false;
    Storage::superPage->symbolsEnd = preDefinedSymbolsEnd+2+additionalSymbols;
    for(Symbol symbol = 0; symbol < preDefinedSymbolsEnd; ++symbol) {
        const char* str = PreDefinedSymbols[symbol];
        stringToBlob(str, strlen(str), symbol);
        link({RunTimeEnvironmentSymbol, HoldsSymbol, symbol});
        blobIndex.insertElement(symbol);
    }
    Symbol ArchitectureSize = createFromData<NativeNaturalType>(architectureSize);
    link({RunTimeEnvironmentSymbol, HoldsSymbol, ArchitectureSize});
    link({RunTimeEnvironmentSymbol, ArchitectureSizeSymbol, ArchitectureSize});
    return true;
}

};
