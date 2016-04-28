#include "../Interpreter/Primitives.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
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
    printf("    Uninhabitable: %10llu bits\n", stats.uninhabitable);
    printf("    Meta Data:     %10llu bits\n", stats.totalMetaData);
    printf("      Inhabited:   %10llu bits\n", stats.inhabitedMetaData);
    printf("      Vacant:      %10llu bits\n", stats.totalMetaData-stats.inhabitedMetaData);
    printf("    Payload:       %10llu bits\n", stats.totalPayload);
    printf("      Inhabited:   %10llu bits\n", stats.inhabitedPayload);
    printf("      Vacant:      %10llu bits\n", stats.totalPayload-stats.inhabitedPayload);
}

void printStats() {
    struct Storage::Stats stats;
    printf("Stats:\n");
    printf("  Global:          %10llu bits\n", Storage::pageCount*Storage::bitsPerPage);
    printf("    Wilderness:    %10llu bits\n", Storage::countFreePages()*Storage::bitsPerPage);
    printf("    Super Page:    %10llu bits\n", Storage::bitsPerPage);
    printf("      Inhabited:   %10llu bits\n", sizeOfInBits<Storage::SuperPage>::value);
    printf("      Vacant:      %10llu bits\n", Storage::bitsPerPage-sizeOfInBits<Storage::SuperPage>::value);

    resetStats(stats);
    Storage::fullBlobBuckets.generateStats(stats, [&](Storage::BpTreeSet<PageRefType>::Iterator<false>& iter) {
        Storage::dereferencePage<Storage::BlobBucket>(iter.getKey())->generateStats(stats);
    });
    for(NativeNaturalType i = 0; i < Storage::blobBucketTypeCount; ++i)
        Storage::freeBlobBuckets[i].generateStats(stats, [&](Storage::BpTreeSet<PageRefType>::Iterator<false>& iter) {
            Storage::dereferencePage<Storage::BlobBucket>(iter.getKey())->generateStats(stats);
        });
    printf("  Blob Buckets:    %10llu\n", stats.elementCount); // TODO elementCount is both, B+Tree and Blobs
    printStatsPartial(stats);

    resetStats(stats);
    Storage::blobs.generateStats(stats);
    printf("  Symbols:         %10llu\n", Storage::symbolCount);
    printf("    Recyclable:    %10llu\n", Storage::symbolCount-stats.elementCount);
    printf("    Used:          %10llu\n", stats.elementCount);
    printf("      Meta:        %10llu\n", stats.elementCount-Ontology::symbols.size());
    printf("      User:        %10llu\n", Ontology::symbols.size());
    printStatsPartial(stats);

    printf("  Triples:         %10llu\n", Ontology::query(13, {}));
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

NativeNaturalType bytesForPages(NativeNaturalType pageCount) {
    const NativeNaturalType mmapChunkSize = 1<<24;
    return (pageCount*Storage::bitsPerPage+mmapChunkSize-1)/mmapChunkSize*mmapChunkSize/8;
}

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    if(pageCount)
        munmap(heapBegin, bytesForPages(pageCount));
    assert(_pageCount < maxPageCount);
    pageCount = _pageCount;
    assert(ftruncate(file, bytesForPages(pageCount)) == 0);
    assert(MMAP_FUNC(heapBegin, bytesForPages(pageCount),
                     PROT_READ|PROT_WRITE, MAP_FIXED|MAP_FILE|MAP_SHARED,
                     file, 0) == heapBegin);
}

void loadStorage(const Integer8* path) {
    file = open(path, O_RDWR|O_CREAT, 0666);
    assert(file >= 0);
    Storage::pageCount = lseek(file, 0, SEEK_END)/(Storage::bitsPerPage/8);
    if(Storage::pageCount < Storage::minPageCount)
        Storage::pageCount = Storage::minPageCount;
    Storage::heapBegin = MMAP_FUNC(nullptr, bytesForPages(Storage::maxPageCount), PROT_NONE, MAP_FILE|MAP_SHARED, file, 0);
    assert(Storage::heapBegin != MAP_FAILED);
    Storage::resizeMemory(Storage::pageCount);
}

void unloadStorage() {
    if(Storage::pageCount)
        munmap(Storage::heapBegin, bytesForPages(Storage::pageCount));
    assert(ftruncate(file, bytesForPages(Storage::pageCount)) == 0);
    assert(close(file) == 0);
}
