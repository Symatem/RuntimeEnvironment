#include "BpContainers.hpp"

namespace Storage {

struct FreePage : public BasePage {
    PageRefType next;
};

// TODO: Redistribution if there are many almost empty buckets of the same type
const NativeNaturalType blobBucketTypeCount = 12,
        blobBucketType[blobBucketTypeCount] = {8, 16, 32, 64, 128, 320, 832, 1344, 2432, 4544, 8064, 16192};
// Alternative: Use 576 instead of 832 ?

struct SuperPage : public BasePage {
    Symbol pagesEnd, symbolsEnd;
    PageRefType freePage;
    BpTreeSet<Symbol> freeSymbols;
    BpTreeSet<PageRefType> fullBlobBuckets, freeBlobBuckets[blobBucketTypeCount];
    BpTreeMap<Symbol, NativeNaturalType> blobs;
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

PageRefType aquirePage() {
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

};
