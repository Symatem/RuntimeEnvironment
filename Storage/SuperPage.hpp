#include <Storage/BpContainers.hpp>

struct RecyclablePage : public BasePage {
    PageRefType next;
};

// TODO: Redistribution if there are many almost empty buckets of the same type
const NativeNaturalType bitVectorBucketType[] = {8, 16, 32, 64, 128, 320, 576, 1344, 2432, 4544, 8064, 16192},
                        bitVectorBucketTypeCount = sizeof(bitVectorBucketType)/sizeof(NativeNaturalType);
const char* gitRef = "git:" macroToString(GIT_REF);

struct SymbolSpace {
    Symbol symbolsEnd;
    NativeNaturalType bitVectorCount;
    BpTreeSet<Symbol> recyclableSymbols;
    BpTreeMap<Symbol, NativeNaturalType> bitVectors;

    void init() {
        symbolsEnd = 0;
        bitVectorCount = 0;
    }

    void iterateSymbols(Closure<void(Symbol)> callback) {
        bitVectors.iterateKeys(callback);
    }

    Symbol createSymbol() {
        return recyclableSymbols.isEmpty()
            ? symbolsEnd++
            : recyclableSymbols.getOne<First, true>();
    }

    void releaseSymbol(Symbol symbol);
};

struct SuperPage : public BasePage {
    Natural64 version;
    Natural8 gitRef[44], architectureSizeLog2;
    PageRefType pagesEnd, recyclablePage;
    BpTreeSet<PageRefType> fullBitVectorBuckets, freeBitVectorBuckets[bitVectorBucketTypeCount];
    SymbolSpace heap;

    void init() {
        version = 0;
        memcpy(gitRef, ::gitRef, sizeof(gitRef));
        architectureSizeLog2 = BitMask<NativeNaturalType>::ceilLog2(architectureSize);
        heap.init();
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
