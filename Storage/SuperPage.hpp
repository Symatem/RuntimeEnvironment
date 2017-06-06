#include <Storage/BpContainers.hpp>

struct FreePage : public BasePage {
    PageRefType next;
};

// TODO: Redistribution if there are many almost empty buckets of the same type
const NativeNaturalType bitVectorBucketType[] = {8, 16, 32, 64, 128, 320, 576, 1344, 2432, 4544, 8064, 16192},
                        bitVectorBucketTypeCount = sizeof(bitVectorBucketType)/sizeof(NativeNaturalType);
const char* gitRef = "git:" macroToString(GIT_REF);

struct OntologyStruct {
    Symbol symbolsEnd;
    BpTreeSet<Symbol> freeSymbols;
    BpTreeMap<Symbol, NativeNaturalType> bitVectors;

    void iterateSymbols(Closure<void(Symbol)> callback) {
        bitVectors.iterateKeys(callback);
    }

    Symbol createSymbol() {
        return freeSymbols.isEmpty()
            ? symbolsEnd++
            : freeSymbols.getOne<First, true>();
    }

    void releaseSymbol(Symbol symbol);
};

struct SuperPage : public BasePage {
    Natural64 version;
    Natural8 gitRef[44], architectureSizeLog2;
    NativeNaturalType bitVectorCount;
    PageRefType pagesEnd, freePage;
    BpTreeSet<PageRefType> fullBitVectorBuckets, freeBitVectorBuckets[bitVectorBucketTypeCount];
    // BpTreeMap<Symbol, OntologyStruct> ontologies;
    OntologyStruct ontology;

    void init() {
        version = 0;
        memcpy(gitRef, ::gitRef, sizeof(gitRef));
        architectureSizeLog2 = BitMask<NativeNaturalType>::ceilLog2(architectureSize);
        bitVectorCount = 0;
        ontology.symbolsEnd = 0;
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
    if(superPage->freePage) {
        PageRefType pageRef = superPage->freePage;
        auto freePage = dereferencePage<FreePage>(pageRef);
        superPage->freePage = freePage->next;
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
        auto freePage = dereferencePage<FreePage>(pageRef);
        freePage->next = superPage->freePage;
        superPage->freePage = pageRef;
    }
}

NativeNaturalType countFreePages() {
    auto superPage = dereferencePage<SuperPage>(0);
    if(!superPage->freePage)
        return 0;
    NativeNaturalType count = 1;
    auto freePage = dereferencePage<FreePage>(superPage->freePage);
    while(freePage->next) {
        freePage = dereferencePage<FreePage>(freePage->next);
        ++count;
    }
    return count;
}
