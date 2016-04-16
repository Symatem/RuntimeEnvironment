#include "../Interpreter/Procedures.hpp"
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



Thread thread;

Symbol createFromFile(const char* path) {
    Integer32 fd = open(path, O_RDONLY);
    if(fd < 0)
        return Ontology::VoidSymbol;
    Symbol dst = Storage::createSymbol();
    Ontology::link({dst, Ontology::BlobTypeSymbol, Ontology::TextSymbol});
    NativeNaturalType length = lseek(fd, 0, SEEK_END);
    Storage::setBlobSize(dst, length*8);
    lseek(fd, 0, SEEK_SET);
    char buffer[64];
    NativeNaturalType dstIndex = 0;
    while(length > 0) {
        NativeNaturalType count = min((NativeNaturalType)sizeof(buffer), length);
        read(fd, buffer, count);
        for(NativeNaturalType j = 0; j < count; ++j)
            Storage::writeBlobAt<char>(dst, dstIndex++, buffer[j]);
        length -= count;
    }
    close(fd);
    return dst;
}

void loadFromPath(Symbol parentPackage, bool execute, char* path) {
    NativeNaturalType pathLen = strlen(path);
    if(path[pathLen-1] == '/')
        path[pathLen-1] = 0;
    struct stat s;
    char buffer[64];
    if(stat(path, &s) != 0)
        return;
    if(s.st_mode & S_IFDIR) {
        DIR* dp = opendir(path);
        assert(dp);
        NativeNaturalType slashIndex = 0;
        for(NativeNaturalType i = pathLen-1; i > 0; --i)
            if(path[i] == '/') {
                slashIndex = i+1;
                break;
            }
        Storage::bitwiseCopy(reinterpret_cast<NativeNaturalType*>(buffer),
                             reinterpret_cast<NativeNaturalType*>(path),
                             0, slashIndex*8, (pathLen-slashIndex)*8);
        buffer[pathLen-slashIndex] = 0;
        Symbol package = Ontology::createFromString(const_cast<const char*>(buffer));
        Ontology::blobIndex.insertElement(package);
        if(parentPackage == Ontology::VoidSymbol)
            parentPackage = package;
        thread.link({package, Ontology::HoldsSymbol, parentPackage});
        struct dirent* entry;
        while((entry = readdir(dp)))
            if(entry->d_name[0] != '.') {
                sprintf(buffer, "%s/%s", path, entry->d_name);
                loadFromPath(package, execute, buffer);
            }
        closedir(dp);
    } else if(s.st_mode & S_IFREG) {
        if(!Storage::substrEqual<true>(path, ".sym"))
            return;
        Symbol file = createFromFile(path);
        assert(file != Ontology::VoidSymbol);
        thread.deserializationTask(file, parentPackage);
        if(thread.uncaughtException()) {
            printf("Exception occurred while deserializing file %s.\n", path);
            exit(2);
        }
        if(!execute)
            return;
        if(!thread.executeDeserialized()) {
            printf("Nothing to execute in file %s.\n", path);
            exit(3);
        } else if(thread.uncaughtException()) {
            printf("Exception occurred while executing file %s.\n", path);
            exit(4);
        }
    }
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
    return (pageCount*Storage::bitsPerPage+Storage::mmapBucketSize-1)/Storage::mmapBucketSize*Storage::mmapBucketSize/8;
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

Integer32 main(Integer32 argc, Integer8** argv) {
    file = open("./data", O_RDWR|O_CREAT, 0666);
    assert(file >= 0);
    Storage::pageCount = lseek(file, 0, SEEK_END)/(Storage::bitsPerPage/8);
    if(Storage::pageCount < Storage::minPageCount)
        Storage::pageCount = Storage::minPageCount;
    Storage::heapBegin = MMAP_FUNC(nullptr, bytesForPages(Storage::maxPageCount), PROT_NONE, MAP_FILE|MAP_SHARED, file, 0);
    assert(Storage::heapBegin != MAP_FAILED);
    Storage::resizeMemory(Storage::pageCount);
    Ontology::tryToFillPreDefined();

    bool execute = false;
    for(NativeNaturalType i = 1; i < argc; ++i) {
        if(Storage::substrEqual(argv[i], "-h")) {
            printf("This is not the help page you are looking for.\n");
            printf("No, seriously, RTFM.\n");
            exit(4);
        } else if(Storage::substrEqual(argv[i], "-e")) {
            execute = true;
            continue;
        }
        loadFromPath(Ontology::VoidSymbol, execute, argv[i]);
    }
    thread.clear();
    printStats();

    if(Storage::pageCount)
        munmap(Storage::heapBegin, bytesForPages(Storage::pageCount));
    assert(ftruncate(file, bytesForPages(Storage::pageCount)) == 0);
    assert(close(file) == 0);
    return 0;
}
