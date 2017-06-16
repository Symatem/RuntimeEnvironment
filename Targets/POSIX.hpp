#include <External/BinaryOntologyCodec.hpp>
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
NativeNaturalType maxPageCount = static_cast<NativeNaturalType>(1)<<48;
#else
#define PrintFormatNatural "u"
#define MMAP_FUNC mmap
NativeNaturalType maxPageCount = static_cast<NativeNaturalType>(1)<<16;
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
    printStatsLine("    Uninhabitable ", stats.uninhabitable, stats.total);
    printStatsLine("    Meta Data     ", stats.totalMetaData, stats.total);
    printStatsLine("      Inhabited   ", stats.inhabitedMetaData, stats.totalMetaData);
    printStatsLine("    Payload       ", stats.totalPayload, stats.total);
    printStatsLine("      Inhabited   ", stats.inhabitedPayload, stats.totalPayload);
}

void printStats() {
    NativeNaturalType bitVectorInBucketTypes[bitVectorBucketTypeCount+1];
    for(NativeNaturalType i = 0; i < bitVectorBucketTypeCount+1; ++i)
        bitVectorInBucketTypes[i] = 0;
    struct Stats metaStructs, bitVectorIndex, fullBuckets, freeBuckets, fragmented;
    resetStats(metaStructs);
    resetStats(bitVectorIndex);
    resetStats(fullBuckets);
    resetStats(freeBuckets);
    resetStats(fragmented);
    printf("Stats:\n");
    superPage->symbolSpaces.generateStats(metaStructs, [&](BpTreeMap<Symbol, SymbolSpaceState>::Iterator<false> iter) {
        SymbolSpace symbolSpace(iter.getKey());
        NativeNaturalType recyclableSymbolCount = 0;
        symbolSpace.state.recyclableSymbols.generateStats(metaStructs, [&](BpTreeSet<Symbol>::Iterator<false>& iter) {
            ++recyclableSymbolCount;
        });
        symbolSpace.state.bitVectors.generateStats(bitVectorIndex, [&](BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter) {
            BitVector bitVector(BitVectorLocation(&symbolSpace, iter.getKey()));
            ++bitVectorInBucketTypes[BitVectorBucket::getType(bitVector.getSize())];
            if(bitVector.state == BitVector::Fragmented)
                bitVector.bpTree.generateStats(fragmented);
        });
        printf("SymbolSpace       %10" PrintFormatNatural "\n", symbolSpace.spaceSymbol);
        if(symbolSpace.spaceSymbol > 0)
            printf("  Triples:        %10" PrintFormatNatural "\n", reinterpret_cast<Ontology&>(symbolSpace).query(VVV));
        printf("  Symbols         %10" PrintFormatNatural "\n", symbolSpace.state.symbolsEnd);
        printf("  Recyclable      %10" PrintFormatNatural "\n", recyclableSymbolCount);
        printf("  Empty           %10" PrintFormatNatural "\n", symbolSpace.state.symbolsEnd-symbolSpace.state.bitVectorCount-recyclableSymbolCount);
        printf("  BitVectors      %10" PrintFormatNatural "\n", symbolSpace.state.bitVectorCount);
        for(NativeNaturalType i = 0; i < bitVectorBucketTypeCount; ++i)
            printf("    %10" PrintFormatNatural "    %10" PrintFormatNatural "\n", bitVectorBucketType[i], bitVectorInBucketTypes[i]);
        printf("    Fragmented    %10" PrintFormatNatural "\n", bitVectorInBucketTypes[bitVectorBucketTypeCount]);
        assert(symbolSpace.state.symbolsEnd-symbolSpace.state.bitVectorCount == recyclableSymbolCount);
    });
    NativeNaturalType totalBits = superPage->pagesEnd*bitsPerPage,
                      recyclableBits = countRecyclablePages()*bitsPerPage;
    metaStructs.totalMetaData += bitsPerPage;
    metaStructs.inhabitedMetaData += sizeOfInBits<SuperPage>::value;
    superPage->fullBitVectorBuckets.generateStats(metaStructs, [&](BpTreeSet<PageRefType>::Iterator<false>& iter) {
        dereferencePage<BitVectorBucket>(iter.getKey())->generateStats(fullBuckets);
    });
    for(NativeNaturalType i = 0; i < bitVectorBucketTypeCount; ++i)
        superPage->freeBitVectorBuckets[i].generateStats(metaStructs, [&](BpTreeSet<PageRefType>::Iterator<false>& iter) {
            dereferencePage<BitVectorBucket>(iter.getKey())->generateStats(freeBuckets);
        });
    printf("Global            %10" PrintFormatNatural " bits %" PrintFormatNatural " pages\n", totalBits, superPage->pagesEnd);
    printStatsLine("  Recyclable      ", recyclableBits, totalBits);
    printf("  Meta Structures ");
    printStatsPartial(metaStructs);
    printf("  BitVector Index ");
    printStatsPartial(bitVectorIndex);
    printf("  Full Buckets    ");
    printStatsPartial(fullBuckets);
    printf("  Free Buckets    ");
    printStatsPartial(freeBuckets);
    printf("  Fragmented      ");
    printStatsPartial(fragmented);
    assert(recyclableBits+metaStructs.total+bitVectorIndex.total+fullBuckets.total+freeBuckets.total+fragmented.total == totalBits);
}



Integer32 file = -1, sockfd = -1;
struct stat fileStat;

void exportOntology(const char* path, Ontology* srcOntology) {
    BitVectorGuard<BitVector> bitVector;
    BinaryOntologyEncoder encoder(bitVector, srcOntology);
    encoder.encode();
    int fd = open(path, O_WRONLY|O_CREAT, 0660);
    Natural8 buffer[512];
    for(NativeNaturalType offset = 0, size = encoder.bitVector.getSize(); offset < size; ) {
        NativeNaturalType sliceLength = min(size-offset, static_cast<NativeNaturalType>(sizeof(buffer)*8));
        bitVector.externalOperate<false>(buffer, offset, sliceLength);
        assert(write(fd, buffer, sliceLength/8) > 0);
        offset += sliceLength;
    }
    close(fd);
}

void importOntology(const char* path, Ontology* dstOntology) {
    BitVectorGuard<BitVector> bitVector;
    BinaryOntologyDecoder decoder(dstOntology, bitVector);
    int fd = open(path, O_RDONLY, 0660);
    Natural8 buffer[512];
    struct stat fdStat;
    assert(fstat(fd, &fdStat) == 0);
    NativeNaturalType size = fdStat.st_size*8;
    decoder.bitVector.setSize(size);
    for(NativeNaturalType offset = 0; offset < size; ) {
        NativeNaturalType sliceLength = min(size-offset, static_cast<NativeNaturalType>(sizeof(buffer)*8));
        assert(read(fd, buffer, sliceLength/8) > 0);
        bitVector.externalOperate<true>(buffer, offset, sliceLength);
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

    while(maxPageCount > 0x10) {
        superPage = reinterpret_cast<SuperPage*>(MMAP_FUNC(0, bytesForPages(maxPageCount), PROT_READ|PROT_WRITE, mmapFlags, file, 0));
        if(superPage != MAP_FAILED)
            break;
        maxPageCount >>= 1;
    }
    assert(superPage != MAP_FAILED);

    if(file < 0 || fileStat.st_size == 0)
        superPage->init(true);
    else if(S_ISREG(fileStat.st_mode)) {
        superPage->init(false);
        assert(superPage->pagesEnd*bitsPerPage/8 == static_cast<NativeNaturalType>(fileStat.st_size));
    }
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
