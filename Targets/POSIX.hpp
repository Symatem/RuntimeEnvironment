#include "../Interpreter/Primitives.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>

#define printStatsLine(name, amount, total) \
    printf(name "%10llu bits %2.2f %%\n", amount, 100.0*(amount)/(total))

void resetStats(struct Storage::Stats& stats) {
    stats.uninhabitable = 0;
    stats.totalMetaData = 0;
    stats.inhabitedMetaData = 0;
    stats.totalPayload = 0;
    stats.inhabitedPayload = 0;
}

void printStatsPartial(struct Storage::Stats& stats) {
    stats.total = stats.uninhabitable+stats.totalMetaData+stats.totalPayload;
    assert(stats.total%Storage::bitsPerPage == 0);
    printStatsLine("", stats.total, Storage::superPage->pagesEnd*Storage::bitsPerPage);
    printStatsLine("      Uninhabitable ", stats.uninhabitable, stats.total);
    printStatsLine("      Meta Data     ", stats.totalMetaData, stats.total);
    printStatsLine("        Inhabited   ", stats.inhabitedMetaData, stats.totalMetaData);
    printStatsLine("      Payload       ", stats.totalPayload, stats.total);
    printStatsLine("        Inhabited   ", stats.inhabitedPayload, stats.totalPayload);
}

void printStats() {
    printf("Pages:\n");
    struct Storage::Stats metaStructs, fullBuckets, freeBuckets, fragmented;
    resetStats(metaStructs);
    resetStats(fullBuckets);
    resetStats(freeBuckets);
    resetStats(fragmented);
    metaStructs.totalMetaData += Storage::bitsPerPage;
    metaStructs.inhabitedMetaData += sizeOfInBits<Storage::SuperPage>::value;
    NativeNaturalType totalBits = Storage::superPage->pagesEnd*Storage::bitsPerPage,
                      recyclableBits = Storage::countFreePages()*Storage::bitsPerPage;
    NativeNaturalType recyclableSymbolCount = 0, blobCount = 0, blobInBucketTypes[Storage::blobBucketTypeCount+1];
    for(NativeNaturalType i = 0; i < Storage::blobBucketTypeCount+1; ++i)
        blobInBucketTypes[i] = 0;

    Storage::superPage->freeSymbols.generateStats(metaStructs, [&](Storage::BpTreeSet<Symbol>::Iterator<false>& iter) {
        ++recyclableSymbolCount;
    });
    Storage::superPage->fullBlobBuckets.generateStats(metaStructs, [&](Storage::BpTreeSet<PageRefType>::Iterator<false>& iter) {
        Storage::dereferencePage<Storage::BlobBucket>(iter.getKey())->generateStats(fullBuckets);
    });
    for(NativeNaturalType i = 0; i < Storage::blobBucketTypeCount; ++i)
        Storage::superPage->freeBlobBuckets[i].generateStats(metaStructs, [&](Storage::BpTreeSet<PageRefType>::Iterator<false>& iter) {
            Storage::dereferencePage<Storage::BlobBucket>(iter.getKey())->generateStats(freeBuckets);
        });
    Storage::superPage->blobs.generateStats(metaStructs, [&](Storage::BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
        Storage::Blob blob(iter.getKey());
        ++blobInBucketTypes[Storage::BlobBucket::getType(blob.getSize())];
        if(blob.state == Storage::Blob::Fragmented)
            blob.bpTree.generateStats(fragmented);
        ++blobCount;
    });

    printf("Stats:\n");
    printf("  Global            %10llu bits %llu pages\n", totalBits, Storage::superPage->pagesEnd);
    printStatsLine("    Recyclable      ", recyclableBits, totalBits);
    printf("    Meta Structures ");
    printStatsPartial(metaStructs);
    printf("    Full Buckets    ");
    printStatsPartial(fullBuckets);
    printf("    Free Buckets    ");
    printStatsPartial(freeBuckets);
    printf("    Fragmented      ");
    printStatsPartial(fragmented);
    printf("  Symbols           %10llu\n", Storage::superPage->symbolsEnd);
    printf("    Recyclable      %10llu\n", recyclableSymbolCount);
    printf("    Empty           %10llu\n", Storage::superPage->symbolsEnd-recyclableSymbolCount-blobCount);
    printf("    Blobs           %10llu\n", blobCount);
    for(NativeNaturalType i = 0; i < Storage::blobBucketTypeCount; ++i)
        printf("      %10llu    %10llu\n", Storage::blobBucketType[i], blobInBucketTypes[i]);
    printf("      Fragmented    %10llu\n", blobInBucketTypes[Storage::blobBucketTypeCount]);
    printf("  Triples:          %10llu\n", Ontology::query(13, {}));
    printf("\n");

    assert(recyclableBits+metaStructs.total+fullBuckets.total+freeBuckets.total+fragmented.total == totalBits);
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
