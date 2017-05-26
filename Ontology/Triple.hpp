#include <DataStructures/ContentIndex.hpp>

struct OntologyStruct : public DataStructure<MetaVector<VoidType, BitVectorContainer>> {
    typedef DataStructure<MetaVector<VoidType, BitVectorContainer>> Super;

    OntologyStruct(Symbol symbol) :Super(symbol) {}

    void init() {
        if(Super::isEmpty())
            Super::insertRange(0, 7);
    }

    auto getSubIndex(NativeNaturalType subIndex) {
        return Super::template getValueAt<PairSet<Symbol, Symbol, OntologyStruct>>(subIndex);
    }

    auto getBitMap() {
        return Super::template getValueAt<BitMap<OntologyStruct>>(6);
    }
};

#define Wrapper(token) token##Symbol
enum PreDefinedSymbols {
#include <Ontology/Symbols.hpp>
};
#undef Wrapper

#define Wrapper(token) #token
const char* PreDefinedSymbols[] = {
#include <Ontology/Symbols.hpp>
};
#undef Wrapper

const Symbol preDefinedSymbolsCount = sizeof(PreDefinedSymbols)/sizeof(void*);

#define forEachSubIndex() \
    NativeNaturalType indexCount = (indexMode == MonoIndex) ? 1 : 3; \
    for(NativeNaturalType subIndex = 0; subIndex < indexCount; ++subIndex)

struct Triple {
    Symbol pos[3];

    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :pos{_entity, _attribute, _value} {}

    template<bool forward, bool link>
    bool subIndexOperate(OntologyStruct& alpha, NativeNaturalType subIndex) {
        auto pair = forward
            ? Pair<Symbol, Symbol>{pos[(subIndex+1)%3], pos[(subIndex+2)%3]}
            : Pair<Symbol, Symbol>{pos[(subIndex+2)%3], pos[(subIndex+1)%3]};
        auto beta = alpha.getSubIndex(subIndex+(forward ? 0 : 3));
        return link
            ? beta.insertElement(pair)
            : beta.eraseElement(pair);
    }

    Triple reordered(NativeNaturalType subIndex) {
        static constexpr NativeNaturalType
            alpha[] = {0, 1, 2, 0, 1, 2},
             beta[] = {1, 2, 0, 2, 0, 1},
            gamma[] = {2, 0, 1, 1, 2, 0};
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }

    Triple normalized(NativeNaturalType subIndex) {
        static constexpr NativeNaturalType
            alpha[] = {0, 2, 1, 0, 1, 2},
             beta[] = {1, 0, 2, 2, 0, 1},
            gamma[] = {2, 1, 0, 1, 2, 0};
        return {pos[alpha[subIndex]], pos[beta[subIndex]], pos[gamma[subIndex]]};
    }

    bool operator==(Triple const& other) {
        return pos[0] == other.pos[0] && pos[1] == other.pos[1] && pos[2] == other.pos[2];
    }

    bool operator<(Triple const& other) {
        if(pos[2] < other.pos[2])
            return true;
        if(pos[2] > other.pos[2])
            return false;
        if(pos[1] < other.pos[1])
            return true;
        if(pos[1] > other.pos[1])
            return false;
        return pos[0] < other.pos[0];
    }

    bool operator>(Triple const& other) {
        if(pos[2] > other.pos[2])
            return true;
        if(pos[2] < other.pos[2])
            return false;
        if(pos[1] > other.pos[1])
            return true;
        if(pos[1] < other.pos[1])
            return false;
        return pos[0] > other.pos[0];
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

enum QueryMode {
    Match,
    Varying,
    Ignore
};

enum QueryMask {
    MMM, VMM, IMM, MVM, VVM, IVM, MIM, VIM, IIM,
    MMV, VMV, IMV, MVV, VVV, IVV, MIV, VIV, IIV,
    MMI, VMI, IMI, MVI, VVI, IVI, MII, VII, III
};

bool linkInSubIndex(Triple triple, NativeNaturalType subIndex) {
    OntologyStruct alpha(triple.pos[subIndex]);
    alpha.init();
    if(!triple.subIndexOperate<true, true>(alpha, subIndex))
        return false;
    if(indexMode == HexaIndex)
        assert((triple.subIndexOperate<false, true>(alpha, subIndex)));
    return true;
}

bool link(Triple triple) {
    forEachSubIndex()
        if(!linkInSubIndex(triple, subIndex))
            return false;
    if(triple.pos[1] == BitMapTypeSymbol)
        modifiedBitVector(triple.pos[0]);
    return true;
}

NativeNaturalType searchMMM(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    NativeNaturalType betaIndex, gammaIndex;
    if(alpha.isEmpty())
        return 0;
    auto beta = alpha.getSubIndex(subIndex);
    if(!beta.findElement({triple.pos[1], triple.pos[2]}, betaIndex, gammaIndex))
        return 0;
    if(callback)
        callback(triple);
    return 1;
}

NativeNaturalType searchMMI(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    NativeNaturalType betaIndex;
    if(alpha.isEmpty())
        return 0;
    auto beta = alpha.getSubIndex(subIndex);
    if(!beta.findFirstKey(triple.pos[1], betaIndex))
        return 0;
    if(callback)
        callback(triple);
    return 1;
}

NativeNaturalType searchMII(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    if(alpha.isEmpty())
        return 0;
    if(callback)
        callback(triple);
    return 1;
}

NativeNaturalType searchIII(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    return 0;
}

NativeNaturalType searchMMV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    NativeNaturalType betaIndex;
    if(alpha.isEmpty())
        return 0;
    auto beta = alpha.getSubIndex(subIndex);
    if(!beta.findFirstKey(triple.pos[1], betaIndex))
        return 0;
    if(callback)
        beta.iterateSecondKeys(betaIndex, [&](Symbol gammaResult) {
            triple.pos[2] = gammaResult;
            callback(triple.normalized(subIndex));
        });
    return beta.getSecondKeyCount(betaIndex);
}

NativeNaturalType searchMVV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    NativeNaturalType count = 0;
    if(alpha.isEmpty())
        return 0;
    auto beta = alpha.getSubIndex(subIndex);
    beta.iterateElements([&](Pair<Symbol, Symbol> betaResult) {
        if(callback) {
            triple.pos[1] = betaResult.first;
            triple.pos[2] = betaResult.second;
            callback(triple.normalized(subIndex));
        }
        ++count;
    });
    return count;
}

NativeNaturalType searchMIV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    if(alpha.isEmpty())
        return 0;
    BitVectorGuard<DataStructure<Set<Symbol>>> result;
    auto beta = alpha.getSubIndex(subIndex);
    beta.iterateElements([&](Pair<Symbol, Symbol> betaResult) {
        result.insertElement(betaResult.second);
    });
    if(callback)
        result.iterateElements([&](Symbol gamma) {
            triple.pos[2] = gamma;
            callback(triple.normalized(subIndex));
        });
    return result.getElementCount();
}

NativeNaturalType searchMVI(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    OntologyStruct alpha(triple.pos[0]);
    if(alpha.isEmpty())
        return 0;
    auto beta = alpha.getSubIndex(subIndex);
    if(callback)
        beta.iterateFirstKeys([&](Symbol betaResult) {
            triple.pos[1] = betaResult;
            callback(triple.normalized(subIndex));
        });
    return beta.getFirstKeyCount();
}

NativeNaturalType searchVII(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    if(callback)
        superPage->bitVectors.iterate([&](BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
            triple.pos[0] = iter.getKey();
            callback(triple.normalized(subIndex));
        });
    return superPage->bitVectorCount;
}

NativeNaturalType searchVVI(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType count = 0;
    superPage->bitVectors.iterate([&](BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
        triple.pos[0] = iter.getKey();
        OntologyStruct alpha(triple.pos[0]);
        auto beta = alpha.getSubIndex(subIndex);
        if(callback)
            beta.iterateFirstKeys([&](Symbol betaResult) {
                triple.pos[1] = betaResult;
                callback(triple.normalized(subIndex));
            });
        count += beta.getFirstKeyCount();
    });
    return count;
}

NativeNaturalType searchVVV(NativeNaturalType subIndex, Triple triple, Closure<void(Triple)> callback) {
    NativeNaturalType count = 0;
    superPage->bitVectors.iterate([&](BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
        triple.pos[0] = iter.getKey();
        OntologyStruct alpha(triple.pos[0]);
        auto beta = alpha.getSubIndex(subIndex);
        beta.iterateElements([&](Pair<Symbol, Symbol> betaResult) {
            if(callback) {
                triple.pos[1] = betaResult.first;
                triple.pos[2] = betaResult.second;
                callback(triple);
            }
            ++count;
        });
    });
    return count;
}

NativeNaturalType query(QueryMask mask, Triple triple = {VoidSymbol, VoidSymbol, VoidSymbol}, Closure<void(Triple)> callback = nullptr) {
    struct QueryMethod {
        NativeNaturalType subIndex;
        NativeNaturalType(*function)(NativeNaturalType, Triple, Closure<void(Triple)>);
    };
    const QueryMethod lookup[] = {
        {EAV, &searchMMM},
        {AVE, &searchMMV},
        {AVE, &searchMMI},
        {VEA, &searchMMV},
        {VEA, &searchMVV},
        {VAE, &searchMVI},
        {VEA, &searchMMI},
        {VEA, &searchMVI},
        {VEA, &searchMII},
        {EAV, &searchMMV},
        {AVE, &searchMVV},
        {AVE, &searchMVI},
        {EAV, &searchMVV},
        {EAV, &searchVVV},
        {AVE, &searchVVI},
        {EVA, &searchMVI},
        {VEA, &searchVVI},
        {VEA, &searchVII},
        {EAV, &searchMMI},
        {AEV, &searchMVI},
        {AVE, &searchMII},
        {EAV, &searchMVI},
        {EAV, &searchVVI},
        {AVE, &searchVII},
        {EAV, &searchMII},
        {EAV, &searchVII},
        {EAV, &searchIII},
    };
    assert(mask < sizeof(lookup)/sizeof(QueryMethod));
    QueryMethod method = lookup[mask];
    Triple match = triple;
    BitVectorGuard<DataStructure<Set<Triple>>> resultSet;
    auto monoIndexLambda = [&](Triple result) {
        static constexpr NativeNaturalType maskDivisor[] = {1, 3, 9};
        for(NativeNaturalType i = 0; i < 3; ++i) {
            NativeNaturalType mode = (mask/maskDivisor[i])%3;
            if(mode == Ignore)
                result.pos[i] = VoidSymbol;
            else if(mode == Match && match.pos[i] != result.pos[i])
                return;
        }
        if(resultSet.insertElement(result) && callback)
            callback(result);
    };
    switch(indexMode) {
        case MonoIndex:
            if(method.subIndex != EAV) {
                method.subIndex = EAV;
                method.function = &searchVVV;
                (*method.function)(method.subIndex, triple, monoIndexLambda);
                return resultSet.getElementCount();
            }
        case TriIndex:
            if(method.subIndex >= 3) {
                method.subIndex -= 3;
                method.function = &searchMIV;
            }
        case HexaIndex:
            return (*method.function)(method.subIndex, triple.reordered(method.subIndex), callback);
    }
}

bool valueCountIs(Symbol entity, Symbol attribute, NativeNaturalType size) {
    return query(MMV, {entity, attribute, VoidSymbol}) == size;
}

bool tripleExists(Triple triple) {
    return query(MMM, triple) == 1;
}

/* TODO void setIndexMode(IndexMode _indexMode) {
    assert(_indexMode != indexMode);
    if(_indexMode > indexMode) {
        NativeNaturalType indexCount = (_indexMode == MonoIndex) ? 1 : 3;
        searchVVV(EAV, {VoidSymbol, VoidSymbol, VoidSymbol}, [&](Triple result) {
            for(NativeNaturalType subIndex = indexMode; subIndex < indexCount; ++subIndex)
                linkInSubIndex(result, subIndex);
        });
    } else
        superPage->bitVectors.iterate([&](BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
            OntologyStruct alpha(iter.getKey());
            for(NativeNaturalType subIndex = _indexMode; subIndex < indexMode; ++subIndex)
                releaseSymbol(alpha.getSubIndex(subIndex));
        });
    indexMode = _indexMode;
}*/

bool unlinkWithoutReleasing(Triple triple, bool skipEnabled = false, Symbol skip = VoidSymbol) {
    forEachSubIndex() {
        if(skipEnabled && triple.pos[subIndex] == skip)
            continue;
        OntologyStruct alpha(triple.pos[subIndex]);
        if(alpha.isEmpty())
            return false;
        if(!triple.subIndexOperate<true, false>(alpha, subIndex))
            return false;
        if(indexMode == HexaIndex)
            assert((triple.subIndexOperate<false, false>(alpha, subIndex)));
    }
    if(triple.pos[1] == BitMapTypeSymbol)
        modifiedBitVector(triple.pos[0]);
    return true;
}

void tryToReleaseSymbol(Symbol symbol) {
    OntologyStruct alpha(symbol);
    if(alpha.isEmpty())
        return;
    forEachSubIndex() {
        auto beta = alpha.getSubIndex(subIndex);
        if(!beta.isEmpty())
            return;
    }
    releaseSymbol(symbol);
}

bool unlink(Triple triple) {
    if(!unlinkWithoutReleasing(triple))
        return false;
    for(NativeNaturalType i = 0; i < 3; ++i)
        tryToReleaseSymbol(triple.pos[i]);
    return true;
}

bool unlink(Symbol symbol) {
    OntologyStruct alpha(symbol);
    if(alpha.isEmpty())
        return false;
    BitVectorGuard<DataStructure<Set<Symbol>>> dirty;
    forEachSubIndex() {
        auto beta = alpha.getSubIndex(subIndex);
        beta.iterateFirstKeys([&](Symbol betaResult) {
            dirty.insertElement(betaResult);
        });
        beta.iterateElements([&](Pair<Symbol, Symbol> betaResult) {
            dirty.insertElement(betaResult.second);
            unlinkWithoutReleasing(Triple(symbol, betaResult.first, betaResult.second).normalized(subIndex), true, symbol);
        });
    }
    releaseSymbol(symbol);
    dirty.iterateElements([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
    return true;
}

void setSolitary(Triple triple, bool linkVoidSymbol = false) {
    BitVectorGuard<DataStructure<Set<Symbol>>> dirty;
    bool toLink = (linkVoidSymbol || triple.pos[2] != VoidSymbol);
    query(MMV, triple, [&](Triple result) {
        if(triple.pos[2] == result.pos[2])
            toLink = false;
        else
            dirty.insertElement(result.pos[2]);
    });
    if(toLink)
        link(triple);
    dirty.iterateElements([&](Symbol symbol) {
        unlinkWithoutReleasing({triple.pos[0], triple.pos[1], symbol});
    });
    if(!linkVoidSymbol)
        dirty.insertElement(triple.pos[0]);
    dirty.insertElement(triple.pos[1]);
    dirty.iterateElements([&](Symbol symbol) {
        tryToReleaseSymbol(symbol);
    });
}

bool getUncertain(Symbol entity, Symbol attribute, Symbol& value) {
    return (query(MMV, {entity, attribute, VoidSymbol}, [&](Triple result) {
        value = result.pos[2];
    }) == 1);
}

void scrutinizeExistence(Symbol symbol) {
    BitVectorGuard<DataStructure<Set<Symbol>>> symbols;
    symbols.insertElement(symbol);
    while(!symbols.isEmpty()) {
        symbol = symbols.eraseLastElement();
        if(query(VMM, {VoidSymbol, HoldsSymbol, symbol}) > 0)
            continue;
        query(MMV, {symbol, HoldsSymbol, VoidSymbol}, [&](Triple result) {
            symbols.insertElement(result.pos[2]);
        });
        unlink(symbol); // TODO
    }
}
