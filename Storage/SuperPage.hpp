#include "BpContainers.hpp"

namespace Storage {

struct FreePage : public BasePage {
    PageRefType next;
};

// TODO: Redistribution if there are many almost empty buckets of the same type
const NativeNaturalType blobBucketTypeCount = 15,
        blobBucketType[blobBucketTypeCount] = {8, 16, 32, 64, 192, 320, 640, 1152, 2112, 3328, 6016, 7552, 10112, 15232, 30641};
// blobBucketTypeCount = 11, {8, 16, 32, 64, 192, 320, 640, 1152, 3328, 7552, 15232};

struct SuperPage : public BasePage {
    PageRefType freePage;
    Symbol symbolCount;
    BpTreeSet<Symbol> freeSymbols;
    BpTreeSet<PageRefType> fullBlobBuckets, freeBlobBuckets[blobBucketTypeCount];
    BpTreeMap<Symbol, NativeNaturalType> blobs;
} *superPage;
PageRefType pageCount;

template<typename PageType>
PageType* dereferencePage(PageRefType pageRef) {
    assert(pageRef < pageCount);
    return reinterpret_cast<PageType*>(reinterpret_cast<char*>(superPage)+bitsPerPage/8*pageRef);
}

PageRefType referenceOfPage(void* page) {
    PageRefType pageRef = (reinterpret_cast<Natural64>(page)-reinterpret_cast<Natural64>(superPage))*8/bitsPerPage;
    assert(pageRef < pageCount);
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
        resizeMemory(pageCount+1);
        return pageCount-1;
    }
}

void releasePage(PageRefType pageRef) {
    assert(superPage);
    if(pageRef == pageCount-1)
        resizeMemory(pageCount-1);
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
