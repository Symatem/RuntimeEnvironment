#include "Interpreter/PreDefProcedures.hpp"

struct Thread thread;

void loadFromPath(Identifier parentPackage, bool execute, char* path) {
    ArchitectureType pathLen = strlen(path);
    if(path[pathLen-1] == '/')
        path[pathLen-1] = 0;

    struct stat s;
    char buffer[64];
    if(stat(path, &s) != 0) return;
    if(s.st_mode & S_IFDIR) {
        DIR* dp = opendir(path);
        if(dp == nullptr)
            crash("Could not open directory");

        ArchitectureType slashIndex = 0;
        for(ArchitectureType i = pathLen-1; i > 0; --i)
            if(path[i] == '/') {
                slashIndex = i+1;
                break;
            }

        Storage::bitwiseCopy(reinterpret_cast<ArchitectureType*>(buffer),
                             reinterpret_cast<ArchitectureType*>(path),
                             0, slashIndex*8, pathLen-slashIndex);
        buffer[pathLen-slashIndex] = 0;

        Identifier package = Ontology::createFromData(const_cast<const char*>(buffer));
        Ontology::blobIndex.insertElement(package);
        if(parentPackage == PreDef_Void)
            parentPackage = package;
        thread.link({package, PreDef_Holds, parentPackage});

        struct dirent* entry;
        while((entry = readdir(dp)))
            if(entry->d_name[0] != '.') {
                sprintf(buffer, "%s/%s", path, entry->d_name);
                loadFromPath(package, execute, buffer);
            }
        closedir(dp);
    } else if(s.st_mode & S_IFREG) {
        if(!stringEndsWith(path, ".sym")) return;

        Identifier file = Ontology::createFromFile(path);
        if(file == PreDef_Void)
            crash("Could not open file");

        thread.deserializationTask(file, parentPackage);
        if(thread.uncaughtException())
            crash("Exception occurred while deserializing file");

        if(!execute) return;
        if(!thread.executeDeserialized())
            crash("Nothing to execute");
        else if(thread.uncaughtException())
            crash("Exception occurred while executing");
    }
}

int main(int argc, char** argv) {
    Storage::load();
    Ontology::fillPreDef();

    bool execute = false;
    for(ArchitectureType i = 1; i < argc; ++i) {
        if(memcmp(argv[i], "-h", 2) == 0) {
            printf("This is not the help page you are looking for.\n");
            printf("No, seriously, RTFM.\n");
            _exit(2);
        } else if(memcmp(argv[i], "-e", 2) == 0) {
            execute = true;
            continue;
        }
        loadFromPath(PreDef_Void, execute, argv[i]);
    }
    thread.clear();

    return 0;
}
