#include "../Interpreter/Primitives.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>

extern "C" {

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
    printf("  Triples:          %10llu\n", Ontology::query(Ontology::VVV));
    printf("\n");

    assert(recyclableBits+metaStructs.total+fullBuckets.total+freeBuckets.total+fragmented.total == totalBits);
}



void assertFailed(const char* str) {
    puts(str);
    exit(1);
}

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

Integer32 file = -1;
struct stat fileStat;

NativeNaturalType bytesForPages(NativeNaturalType pagesEnd) {
    const NativeNaturalType mmapChunkSize = 1<<28;
    return (pagesEnd*Storage::bitsPerPage+mmapChunkSize-1)/mmapChunkSize*mmapChunkSize/8;
}

void loadStorage(const char* path) {
    assert(file < 0);
    Integer32 mmapFlags = MAP_FIXED;
    if(Storage::substrEqual(path, "/dev/zero")) {
        mmapFlags |= MAP_PRIVATE|MAP_ANON;
        file = -1;
    } else {
        mmapFlags |= MAP_SHARED|MAP_FILE;
        file = open(path, O_RDWR|O_CREAT, 0666);
        if(file < 0) {
            printf("Could not open data path.\n");
            exit(1);
        }
        assert(file >= 0);
        assert(fstat(file, &fileStat) == 0);
        if(S_ISREG(fileStat.st_mode)) {
            if(fileStat.st_size == 0)
                assert(ftruncate(file, bytesForPages(Storage::minPageCount)) == 0);
        } else if(S_ISBLK(fileStat.st_mode) || S_ISCHR(fileStat.st_mode)) {

        } else {
            printf("Data path must be \"/dev/zero\", a file or a device.\n");
            exit(2);
        }
    }

    Storage::superPage = reinterpret_cast<Storage::SuperPage*>(0x200000000);
    assert(MMAP_FUNC(Storage::superPage, bytesForPages(Storage::maxPageCount), PROT_READ|PROT_WRITE, mmapFlags, file, 0) != MAP_FAILED);
    if(file < 0 || fileStat.st_size == 0)
        Storage::superPage->pagesEnd = Storage::minPageCount;
    else if(S_ISREG(fileStat.st_mode))
        assert(Storage::superPage->pagesEnd*Storage::bitsPerPage/8 == static_cast<NativeNaturalType>(fileStat.st_size));
}

void unloadStorage() {
    printStats();
    NativeNaturalType size = Storage::superPage->pagesEnd*Storage::bitsPerPage/8;
    munmap(Storage::superPage, bytesForPages(Storage::maxPageCount));
    if(file < 0)
        return;
    if(S_ISREG(fileStat.st_mode))
        assert(ftruncate(file, size) == 0);
    assert(close(file) == 0);
    file = -1;
}

}

void Storage::resizeMemory(NativeNaturalType _pagesEnd) {
    assert(_pagesEnd < maxPageCount);
    if(file >= 0 && S_ISREG(fileStat.st_mode) && bytesForPages(_pagesEnd) > static_cast<NativeNaturalType>(fileStat.st_size)) {
        assert(ftruncate(file, bytesForPages(_pagesEnd)) == 0);
        assert(fstat(file, &fileStat) == 0);
    }
    superPage->pagesEnd = _pagesEnd;
}
