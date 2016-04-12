#include "../Interpreter/Procedures.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

int file;

NativeNaturalType bytesForPages(NativeNaturalType pageCount) {
    return (pageCount*Storage::bitsPerPage+Storage::mmapBucketSize-1)/Storage::mmapBucketSize*Storage::mmapBucketSize/8;
}

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    if(pageCount)
        munmap(ptr, bytesForPages(pageCount));
    assert(_pageCount < maxPageCount);
    pageCount = _pageCount;
    assert(ftruncate(file, bytesForPages(pageCount)) == 0);
    assert(MMAP_FUNC(ptr, bytesForPages(pageCount),
                     PROT_READ|PROT_WRITE, MAP_FIXED|MAP_FILE|MAP_SHARED,
                     file, 0) == ptr);
}

void printStats() {
    struct Storage::UsageStats usage;
    usage.uninhabitable = 0;
    usage.totalMetaData = Storage::bitsPerPage;
    usage.inhabitedMetaData = sizeof(Storage::SuperPage)*8;
    usage.totalPayload = 0;
    usage.inhabitedPayload = 0;
    Storage::fullBlobBuckets.updateStats(usage, [&](Storage::BpTreeSet<PageRefType>::Iterator<false>& iter) {
        Storage::dereferencePage<Storage::BlobBucket>(iter.getKey())->updateStats(usage);
    });
    for(NativeNaturalType i = 0; i < Storage::blobBucketTypeCount; ++i)
        Storage::freeBlobBuckets[i].updateStats(usage, [&](Storage::BpTreeSet<PageRefType>::Iterator<false>& iter) {
            Storage::dereferencePage<Storage::BlobBucket>(iter.getKey())->updateStats(usage);
        });
    Storage::blobs.updateStats(usage);
    NativeNaturalType wilderness =
        Storage::pageCount*Storage::bitsPerPage
        -usage.uninhabitable
        -usage.totalMetaData
        -usage.totalPayload;
    assert(wilderness == Storage::countFreePages()*Storage::bitsPerPage);
    // assert(Storage::symbolCount == Storage::blobs.elementCount+Storage::freeSymbols.elementCount);
    printf("Stats:\n");
    printf("  Total:           %10llu bits\n", Storage::pageCount*Storage::bitsPerPage);
    printf("    Wilderness:    %10llu bits\n", wilderness);
    printf("    Uninhabitable: %10llu bits\n", usage.uninhabitable);
    printf("    Meta Data:     %10llu bits\n", usage.totalMetaData);
    printf("      Inhabited:   %10llu bits\n", usage.inhabitedMetaData);
    printf("      Vacant:      %10llu bits\n", usage.totalMetaData-usage.inhabitedMetaData);
    printf("    Payload:       %10llu bits\n", usage.totalPayload);
    printf("      Inhabited:   %10llu bits\n", usage.inhabitedPayload);
    printf("      Vacant:      %10llu bits\n", usage.totalPayload-usage.inhabitedPayload);
    printf("  Total:           %10llu symbols\n", Storage::symbolCount);
    // printf("    Recyclable:    %10llu symbols\n", Storage::symbolCount-Storage::blobs.elementCount);
    // printf("    Used:          %10llu symbols\n", Storage::blobs.elementCount);
    // printf("      Meta:        %10llu symbols\n", Storage::blobs.elementCount-Ontology::symbols.size());
    printf("      User:        %10llu symbols\n", Ontology::symbols.size());
    printf("  Total:           %10llu triples\n", Ontology::query(13, {}));
    printf("\n");
}

Symbol createFromFile(const char* path) {
    int fd = open(path, O_RDONLY);
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

void loadFromPath(Thread& thread, Symbol parentPackage, bool execute, char* path) {
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
                loadFromPath(thread, package, execute, buffer);
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



Thread thread;

int main(int argc, char** argv) {
    file = open("./data", O_RDWR|O_CREAT, 0666);
    assert(file >= 0);
    Storage::pageCount = lseek(file, 0, SEEK_END)/(Storage::bitsPerPage/8);
    if(Storage::pageCount < Storage::minPageCount)
        Storage::pageCount = Storage::minPageCount;
    Storage::ptr = reinterpret_cast<char*>(MMAP_FUNC(nullptr, bytesForPages(Storage::maxPageCount), PROT_NONE, MAP_FILE|MAP_SHARED, file, 0));
    assert(Storage::ptr != MAP_FAILED);

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
        loadFromPath(thread, Ontology::VoidSymbol, execute, argv[i]);
    }
    thread.clear();
    printStats();

    if(Storage::pageCount)
        munmap(Storage::ptr, bytesForPages(Storage::pageCount));
    assert(ftruncate(file, bytesForPages(Storage::pageCount)) == 0);
    assert(close(file) == 0);
    return 0;
}
