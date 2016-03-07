#include "Interpreter/PreDefProcedures.hpp"
#include <sys/stat.h>
#include <dirent.h>

struct Thread thread;

bool stringEndsWith(const char* str, const char* end) {
    return strcmp(str+strlen(str)-strlen(end), end) == 0;
}

void loadFromPath(Identifier parentPackage, bool execute, char* path) {
    if(path[strlen(path)-1] == '/')
        path[strlen(path)-1] = 0;

    struct stat s;
    char buffer[64];
    if(stat(path, &s) != 0) return;
    if(s.st_mode & S_IFDIR) {
        DIR* dp = opendir(path);
        if(dp == NULL) {
            printf("Could not open directory %s\n", path);
            exit(1);
        }

        ArchitectureType slashIndex = 0;
        for(ArchitectureType i = strlen(path)-1; i > 0; --i)
            if(path[i] == '/') {
                slashIndex = i+1;
                break;
            }

        strcpy(buffer, path+slashIndex);
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
        if(file == PreDef_Void) {
            printf("Could not open file %s\n", path);
            exit(2);
        }

        thread.deserializationTask(file, parentPackage);
        if(thread.uncaughtException()) {
            printf("Exception occurred while deserializing file %s\n", path);
            exit(3);
        }

        if(!execute) return;
        if(!thread.executeDeserialized()) {
            printf("Nothing to execute in file %s\n", path);
            exit(4);
        } else if(thread.uncaughtException()) {
            printf("Exception occurred while executing file %s\n", path);
            exit(5);
        }
    }
}

int main(int argc, char** argv) {
    Storage::load();
    Ontology::fillPreDef();

    bool execute = false;
    for(ArchitectureType i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "-h") == 0) {
            printf("This is not the help page you are looking for.\n");
            printf("No, seriously, RTFM.\n");
            exit(6);
        } else if(strcmp(argv[i], "-e") == 0) {
            execute = true;
            continue;
        }
        loadFromPath(PreDef_Void, execute, argv[i]);
    }
    thread.clear();

    return 0;
}
