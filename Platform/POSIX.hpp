#include "../Interpreter/PreDefProcedures.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

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

void Storage::load() {
    file = open("./data", O_RDWR|O_CREAT); // TODO: Debugging
    assert(file >= 0);
    assert(ftruncate(file, bytesForPages(pageCount)) == 0); // TODO: Debugging
    pageCount = lseek(file, 0, SEEK_END)/(bitsPerPage/8);
    if(pageCount < minPageCount)
        pageCount = minPageCount;
    ptr = reinterpret_cast<char*>(MMAP_FUNC(nullptr, bytesForPages(maxPageCount), PROT_NONE, MAP_FILE|MAP_SHARED, file, 0));
    assert(ptr != MAP_FAILED);
    resizeMemory(pageCount);
}

void Storage::unload() {
    if(pageCount)
        munmap(ptr, bytesForPages(pageCount));
    assert(ftruncate(file, bytesForPages(pageCount)) == 0);
    assert(close(file) == 0);
}

Symbol createFromFile(const char* path) {
    int fd = open(path, O_RDONLY);
    if(fd < 0)
        return PreDef_Void;
    Symbol symbol = Storage::createSymbol();
    Ontology::link({symbol, PreDef_BlobType, PreDef_Text});
    NativeNaturalType len = lseek(fd, 0, SEEK_END);
    Storage::setBlobSize(symbol, len*8);
    lseek(fd, 0, SEEK_SET);
    read(fd, reinterpret_cast<char*>(Storage::accessBlobData(symbol)), len);
    close(fd);
    return symbol;
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
        if(dp == nullptr)
            crash("Could not open directory");
        NativeNaturalType slashIndex = 0;
        for(NativeNaturalType i = pathLen-1; i > 0; --i)
            if(path[i] == '/') {
                slashIndex = i+1;
                break;
            }
        Storage::bitwiseCopy(reinterpret_cast<NativeNaturalType*>(buffer),
                             reinterpret_cast<NativeNaturalType*>(path),
                             0, slashIndex*8, pathLen-slashIndex);
        buffer[pathLen-slashIndex] = 0;
        Symbol package = Ontology::createFromData(const_cast<const char*>(buffer));
        Ontology::blobIndex.insertElement(package);
        if(parentPackage == PreDef_Void)
            parentPackage = package;
        thread.link({package, PreDef_Holds, parentPackage});
        struct dirent* entry;
        while((entry = readdir(dp)))
            if(entry->d_name[0] != '.') {
                sprintf(buffer, "%s/%s", path, entry->d_name);
                loadFromPath(thread, package, execute, buffer);
            }
        closedir(dp);
    } else if(s.st_mode & S_IFREG) {
        if(!stringEndsWith(path, ".sym"))
            return;
        Symbol file = createFromFile(path);
        if(file == PreDef_Void)
            crash("Could not open file");
        thread.deserializationTask(file, parentPackage);
        if(thread.uncaughtException())
            crash("Exception occurred while deserializing file");
        if(!execute)
            return;
        if(!thread.executeDeserialized())
            crash("Nothing to execute");
        else if(thread.uncaughtException())
            crash("Exception occurred while executing");
    }
}
