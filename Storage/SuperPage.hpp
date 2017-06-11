#include <Storage/BpContainers.hpp>

struct RecyclablePage : public BasePage {
    PageRefType next;
};

// TODO: Redistribution if there are many almost empty buckets of the same type
const NativeNaturalType bitVectorBucketType[] = {8, 16, 32, 64, 128, 320, 576, 1344, 2432, 4544, 8064, 16192},
                        bitVectorBucketTypeCount = sizeof(bitVectorBucketType)/sizeof(NativeNaturalType);
const char* gitRef = "git:" macroToString(GIT_REF);

struct SymbolSpaceState {
    Symbol symbolsEnd;
    NativeNaturalType bitVectorCount;
    BpTreeSet<Symbol> recyclableSymbols;
    BpTreeMap<Symbol, NativeNaturalType> bitVectors;
};

struct SymbolSpace {
    Symbol spaceSymbol;
    SymbolSpaceState state;

    SymbolSpace() {}
    SymbolSpace(Symbol _spaceSymbol);

    void updateState();

    void iterateSymbols(Closure<void(Symbol)> callback) {
        state.bitVectors.iterateKeys(callback);
    }

    Symbol createSymbol() {
        Symbol symbol = state.recyclableSymbols.isEmpty()
            ? state.symbolsEnd++
            : state.recyclableSymbols.getOne<First, true>();
        updateState();
        return symbol;
    }

    void releaseSymbol(Symbol symbol);
} heapSymbolSpace;

struct SuperPage : public BasePage {
    Natural64 version;
    Natural8 gitRef[44], architectureSizeLog2;
    PageRefType pagesEnd, recyclablePage;
    BpTreeSet<PageRefType> fullBitVectorBuckets, freeBitVectorBuckets[bitVectorBucketTypeCount];
    BpTreeMap<Symbol, SymbolSpaceState> symbolSpaces;

    void init(bool resetPagesEnd) {
        version = 0;
        memcpy(gitRef, ::gitRef, sizeof(gitRef));
        architectureSizeLog2 = BitMask<NativeNaturalType>::ceilLog2(architectureSize);
        if(resetPagesEnd)
            pagesEnd = minPageCount;
        heapSymbolSpace = SymbolSpace(0);
    }
} *superPage;

template<typename PageType>
PageType* dereferencePage(PageRefType pageRef) {
    assert(pageRef < superPage->pagesEnd);
    return reinterpret_cast<PageType*>(reinterpret_cast<Natural8*>(superPage)+bitsPerPage/8*pageRef);
}

PageRefType referenceOfPage(void* page) {
    PageRefType pageRef = (reinterpret_cast<Natural64>(page)-reinterpret_cast<Natural64>(superPage))*8/bitsPerPage;
    assert(pageRef < superPage->pagesEnd);
    return pageRef;
}

PageRefType acquirePage() {
    assert(superPage);
    if(superPage->recyclablePage) {
        PageRefType pageRef = superPage->recyclablePage;
        auto recyclablePage = dereferencePage<RecyclablePage>(pageRef);
        superPage->recyclablePage = recyclablePage->next;
        return pageRef;
    } else {
        resizeMemory(superPage->pagesEnd+1);
        return superPage->pagesEnd-1;
    }
}

void releasePage(PageRefType pageRef) {
    assert(superPage);
    if(pageRef == superPage->pagesEnd-1)
        resizeMemory(superPage->pagesEnd-1);
    else {
        auto superPage = dereferencePage<SuperPage>(0);
        auto recyclablePage = dereferencePage<RecyclablePage>(pageRef);
        recyclablePage->next = superPage->recyclablePage;
        superPage->recyclablePage = pageRef;
    }
}

NativeNaturalType countRecyclablePages() {
    auto superPage = dereferencePage<SuperPage>(0);
    if(!superPage->recyclablePage)
        return 0;
    NativeNaturalType count = 1;
    auto recyclablePage = dereferencePage<RecyclablePage>(superPage->recyclablePage);
    while(recyclablePage->next) {
        recyclablePage = dereferencePage<RecyclablePage>(recyclablePage->next);
        ++count;
    }
    return count;
}
