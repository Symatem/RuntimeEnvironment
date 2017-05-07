#include <External/HrlDeserialize.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>

extern "C" {

#ifdef __LP64__
#define PrintFormatNatural "llu"
#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif
const NativeNaturalType maxPageCount = 1<<22;
#else
#define PrintFormatNatural "u"
#define MMAP_FUNC mmap
const NativeNaturalType maxPageCount = 1<<12;
#endif

#define printStatsLine(name, amount, total) \
    printf(name "%10" PrintFormatNatural " bits %2.2f %%\n", amount, 100.0*(amount)/(total))

void resetStats(struct Stats& stats) {
    stats.uninhabitable = 0;
    stats.totalMetaData = 0;
    stats.inhabitedMetaData = 0;
    stats.totalPayload = 0;
    stats.inhabitedPayload = 0;
}

void printStatsPartial(struct Stats& stats) {
    stats.total = stats.uninhabitable+stats.totalMetaData+stats.totalPayload;
    assert(stats.total%bitsPerPage == 0);
    printStatsLine("", stats.total, superPage->pagesEnd*bitsPerPage);
    printStatsLine("      Uninhabitable ", stats.uninhabitable, stats.total);
    printStatsLine("      Meta Data     ", stats.totalMetaData, stats.total);
    printStatsLine("        Inhabited   ", stats.inhabitedMetaData, stats.totalMetaData);
    printStatsLine("      Payload       ", stats.totalPayload, stats.total);
    printStatsLine("        Inhabited   ", stats.inhabitedPayload, stats.totalPayload);
}

void printStats() {
    printf("Stats:\n");

    struct Stats metaStructs, blobIndex, fullBuckets, freeBuckets, fragmented;
    resetStats(metaStructs);
    resetStats(blobIndex);
    resetStats(fullBuckets);
    resetStats(freeBuckets);
    resetStats(fragmented);
    metaStructs.totalMetaData += bitsPerPage;
    metaStructs.inhabitedMetaData += sizeOfInBits<SuperPage>::value;
    NativeNaturalType totalBits = superPage->pagesEnd*bitsPerPage,
                      recyclableBits = countFreePages()*bitsPerPage;
    NativeNaturalType recyclableSymbolCount = 0, blobCount = 0, blobInBucketTypes[blobBucketTypeCount+1];
    for(NativeNaturalType i = 0; i < blobBucketTypeCount+1; ++i)
        blobInBucketTypes[i] = 0;

    superPage->freeSymbols.generateStats(metaStructs, [&](BpTreeSet<Symbol>::Iterator<false>& iter) {
        ++recyclableSymbolCount;
    });
    superPage->fullBlobBuckets.generateStats(metaStructs, [&](BpTreeSet<PageRefType>::Iterator<false>& iter) {
        dereferencePage<BlobBucket>(iter.getKey())->generateStats(fullBuckets);
    });
    for(NativeNaturalType i = 0; i < blobBucketTypeCount; ++i)
        superPage->freeBlobBuckets[i].generateStats(metaStructs, [&](BpTreeSet<PageRefType>::Iterator<false>& iter) {
            dereferencePage<BlobBucket>(iter.getKey())->generateStats(freeBuckets);
        });
    superPage->blobs.generateStats(blobIndex, [&](BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
        Blob blob(iter.getKey());
        ++blobInBucketTypes[BlobBucket::getType(blob.getSize())];
        if(blob.state == Blob::Fragmented)
            blob.bpTree.generateStats(fragmented);
        ++blobCount;
    });

    printf("  Global            %10" PrintFormatNatural " bits %" PrintFormatNatural " pages\n", totalBits, superPage->pagesEnd);
    printStatsLine("    Recyclable      ", recyclableBits, totalBits);
    printf("    Meta Structures ");
    printStatsPartial(metaStructs);
    printf("    Blob Index      ");
    printStatsPartial(blobIndex);
    printf("    Full Buckets    ");
    printStatsPartial(fullBuckets);
    printf("    Free Buckets    ");
    printStatsPartial(freeBuckets);
    printf("    Fragmented      ");
    printStatsPartial(fragmented);
    printf("  Symbols           %10" PrintFormatNatural "\n", superPage->symbolsEnd);
    printf("    Recyclable      %10" PrintFormatNatural "\n", recyclableSymbolCount);
    printf("    Empty           %10" PrintFormatNatural "\n", superPage->symbolsEnd-recyclableSymbolCount-blobCount);
    printf("    Blobs           %10" PrintFormatNatural "\n", blobCount);
    for(NativeNaturalType i = 0; i < blobBucketTypeCount; ++i)
        printf("      %10" PrintFormatNatural "    %10" PrintFormatNatural "\n", blobBucketType[i], blobInBucketTypes[i]);
    printf("      Fragmented    %10" PrintFormatNatural "\n", blobInBucketTypes[blobBucketTypeCount]);
    printf("  Triples:          %10" PrintFormatNatural "\n", query(VVV));
    printf("\n");

    assert(recyclableBits+metaStructs.total+blobIndex.total+fullBuckets.total+freeBuckets.total+fragmented.total == totalBits);
}



Integer32 file = -1, sockfd = -1;
struct stat fileStat;

void exportOntology(const char* path) {
    BinaryOntologyEncoder encoder;
    encoder.encode();
    int fd = open(path, O_WRONLY|O_CREAT, 0666);
    Natural8 buffer[512];
    for(NativeNaturalType offset = 0, size = encoder.blob.getSize(); offset < size; ) {
        NativeNaturalType sliceLength = min(size-offset, static_cast<NativeNaturalType>(sizeof(buffer)*8));
        encoder.blob.externalOperate<false>(buffer, offset, sliceLength);
        assert(write(fd, buffer, sliceLength/8) > 0);
        offset += sliceLength;
    }
    close(fd);
}

void importOntology(const char* path) {
    BinaryOntologyDecoder decoder;
    int fd = open(path, O_RDONLY, 0666);
    Natural8 buffer[512];
    struct stat fdStat;
    assert(fstat(fd, &fdStat) == 0);
    NativeNaturalType size = fdStat.st_size*8;
    decoder.blob.setSize(size);
    for(NativeNaturalType offset = 0; offset < size; ) {
        NativeNaturalType sliceLength = min(size-offset, static_cast<NativeNaturalType>(sizeof(buffer)*8));
        assert(read(fd, buffer, sliceLength/8) > 0);
        decoder.blob.externalOperate<true>(buffer, offset, sliceLength);
        offset += sliceLength;
    }
    close(fd);
    decoder.decode();
}

NativeNaturalType bytesForPages(NativeNaturalType pagesEnd) {
    const NativeNaturalType mmapChunkSize = 1<<28;
    return (pagesEnd*bitsPerPage+mmapChunkSize-1)/mmapChunkSize*mmapChunkSize/8;
}

void unloadStorage() {
    if(sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    printStats();
    NativeNaturalType size = superPage->pagesEnd*bitsPerPage/8;
    munmap(superPage, bytesForPages(maxPageCount));
    if(file < 0)
        return;
    if(S_ISREG(fileStat.st_mode))
        assert(ftruncate(file, size) == 0);
    assert(close(file) == 0);
    file = -1;
}

void onExit(int signo) {
    unloadStorage();
    exit(0);
}

void loadStorage(const char* path) {
    assert(signal(SIGINT, onExit) != SIG_ERR);

    assert(file < 0);
    Integer32 mmapFlags = 0;
    if(substrEqual(path, "/dev/zero")) {
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
                assert(ftruncate(file, bytesForPages(minPageCount)) == 0);
        } else if(S_ISBLK(fileStat.st_mode) || S_ISCHR(fileStat.st_mode)) {

        } else {
            printf("Data path must be \"/dev/zero\", a file or a device.\n");
            exit(2);
        }
    }

    superPage = reinterpret_cast<SuperPage*>(MMAP_FUNC(0, bytesForPages(maxPageCount), PROT_READ|PROT_WRITE, mmapFlags, file, 0));
    printf("superPage: %p\n", superPage);
    assert(superPage != MAP_FAILED);
    if(file < 0 || fileStat.st_size == 0)
        superPage->pagesEnd = minPageCount;
    else if(S_ISREG(fileStat.st_mode))
        assert(superPage->pagesEnd*bitsPerPage/8 == static_cast<NativeNaturalType>(fileStat.st_size));
}

}

void resizeMemory(NativeNaturalType _pagesEnd) {
    assert(_pagesEnd < maxPageCount);
    if(file >= 0 && S_ISREG(fileStat.st_mode) && bytesForPages(_pagesEnd) > static_cast<NativeNaturalType>(fileStat.st_size)) {
        assert(ftruncate(file, bytesForPages(_pagesEnd)) == 0);
        assert(fstat(file, &fileStat) == 0);
    }
    superPage->pagesEnd = _pagesEnd;
}
