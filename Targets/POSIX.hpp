#include "../Interpreter/Primitives.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>

void resetStats(struct Storage::Stats& stats) {
    stats.elementCount = 0;
    stats.uninhabitable = 0;
    stats.totalMetaData = 0;
    stats.inhabitedMetaData = 0;
    stats.totalPayload = 0;
    stats.inhabitedPayload = 0;
}

void printStatsPartial(struct Storage::Stats& stats) {
    NativeNaturalType totalBits = stats.uninhabitable+stats.totalMetaData+stats.totalPayload;
    assert(totalBits%Storage::bitsPerPage == 0);
    printf("%10llu bits %2.2f %%\n", totalBits, 100.0*totalBits/(Storage::superPage->pagesEnd*Storage::bitsPerPage));
    printf("    Uninhabitable: %10llu bits %2.2f %%\n", stats.uninhabitable, 100.0*stats.uninhabitable/totalBits);
    printf("    Meta Data:     %10llu bits %2.2f %%\n", stats.totalMetaData, 100.0*stats.totalMetaData/totalBits);
    printf("      Inhabited:   %10llu bits %2.2f %%\n", stats.inhabitedMetaData, 100.0*stats.inhabitedMetaData/stats.totalMetaData);
    printf("      Vacant:      %10llu bits\n", stats.totalMetaData-stats.inhabitedMetaData);
    printf("    Payload:       %10llu bits %2.2f %%\n", stats.totalPayload, 100.0*stats.totalPayload/totalBits);
    printf("      Inhabited:   %10llu bits %2.2f %%\n", stats.inhabitedPayload, 100.0*stats.inhabitedPayload/stats.totalPayload);
    printf("      Vacant:      %10llu bits\n", stats.totalPayload-stats.inhabitedPayload);
}

void printStats() {
    struct Storage::Stats metaStructs;
    resetStats(metaStructs);
    metaStructs.totalMetaData += Storage::bitsPerPage;
    metaStructs.inhabitedMetaData += sizeOfInBits<Storage::SuperPage>::value;
    Storage::superPage->freeSymbols.generateStats(metaStructs);
    Storage::superPage->fullBlobBuckets.generateStats(metaStructs);
    for(NativeNaturalType i = 0; i < Storage::blobBucketTypeCount; ++i)
        Storage::superPage->freeBlobBuckets[i].generateStats(metaStructs);
    Storage::superPage->blobs.generateStats(metaStructs);

    NativeNaturalType totalBits = Storage::superPage->pagesEnd*Storage::bitsPerPage,
                      totalRecyclableBits = Storage::countFreePages()*Storage::bitsPerPage;

    printf("Stats:\n");
    printf("  Global:          %10llu bits\n", totalBits);
    printf("    Recyclable:    %10llu bits %2.2f %%\n", totalRecyclableBits, 100.0*totalRecyclableBits/totalBits);
    printf("  Meta Structures  ");
    printStatsPartial(metaStructs);

    printf("\n");
}



void assertFailed(const char* str) {
    puts(str);
    abort();
}

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

Integer32 file;

NativeNaturalType bytesForPages(NativeNaturalType pagesEnd) {
    const NativeNaturalType mmapChunkSize = 1<<24;
    return (pagesEnd*Storage::bitsPerPage+mmapChunkSize-1)/mmapChunkSize*mmapChunkSize/8;
}

void Storage::resizeMemory(NativeNaturalType _pagesEnd) {
    assert(_pagesEnd < maxPageCount);
    munmap(superPage, bytesForPages(Storage::maxPageCount));
    assert(ftruncate(file, bytesForPages(_pagesEnd)) == 0);
    assert(MMAP_FUNC(superPage, bytesForPages(Storage::maxPageCount),
                     PROT_READ|PROT_WRITE, MAP_FIXED|MAP_FILE|MAP_SHARED, file, 0) == superPage);
    superPage->pagesEnd = _pagesEnd;
}

void loadStorage(const Integer8* path) {
    file = open(path, O_RDWR|O_CREAT, 0666);
    assert(file >= 0);
    assert(ftruncate(file, 0) == 0);
    assert(ftruncate(file, bytesForPages(Storage::minPageCount)) == 0);
    Storage::superPage = reinterpret_cast<Storage::SuperPage*>(
                         MMAP_FUNC(nullptr, bytesForPages(Storage::maxPageCount),
                                   PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, file, 0));
    assert(Storage::superPage != MAP_FAILED);
    Storage::superPage->pagesEnd = Storage::minPageCount;
}

void unloadStorage() {
    NativeNaturalType size = Storage::superPage->pagesEnd*Storage::bitsPerPage/8;
    munmap(Storage::superPage, bytesForPages(Storage::maxPageCount));
    assert(ftruncate(file, size) == 0);
    assert(close(file) == 0);
}
