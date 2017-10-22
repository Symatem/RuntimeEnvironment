#include <Foundation/Bitwise.hpp>

const NativeNaturalType bitsPerPage = 1<<15, minPageCount = 1;

struct BasePage {
    NativeNaturalType transaction;
};

struct Stats {
    NativeNaturalType total,
                      uninhabitable,
                      totalMetaData,
                      totalPayload,
                      inhabitedMetaData,
                      inhabitedPayload;
};

void resizeMemory(NativeNaturalType pagesEnd);

template<typename PageType>
PageType* dereferencePage(PageRefType pageRef);
PageRefType referenceOfPage(void* page);
PageRefType acquirePage();
void releasePage(PageRefType pageRef);
