#include "../Interpreter/PreDefProcedures.hpp"
#include <string> // TODO: Remove dependencies
#include <dirent.h>
#include <sys/stat.h>

struct Thread thread;

bool stringEndsWith(const char* str, const char* end) {
    return strcmp(str+strlen(str)-strlen(end), end) == 0;
}

void loadFromPath(Identifier parentPackage, bool execute, std::string path) {
    if(path[path.size()-1] == '/') path.resize(path.size()-1);

    struct stat s;
    if(stat(path.c_str(), &s) != 0) return;
    if(s.st_mode & S_IFDIR) {
        DIR* dp = opendir(path.c_str());
        if(dp == NULL) {
            printf("Could not open directory %s\n", path.c_str());
            exit(1);
        }

        auto slashIndex = path.rfind('/');
        std::string name = (slashIndex != std::string::npos) ? path.substr(slashIndex+1) : path;
        Identifier package = Ontology::createFromData(name.c_str());
        if(parentPackage == PreDef_Void) parentPackage = package;
        thread.link({package, PreDef_Holds, parentPackage});

        struct dirent* entry;
        while((entry = readdir(dp)))
            if(entry->d_name[0] != '.')
                loadFromPath(package, execute, path+'/'+entry->d_name);
        closedir(dp);
    } else if(s.st_mode & S_IFREG) {
        if(!stringEndsWith(path.c_str(), ".sym")) return;

        Identifier file = Ontology::createFromFile(path.c_str());
        if(file == PreDef_Void) {
            printf("Could not open file %s\n", path.c_str());
            exit(2);
        }

        thread.deserializationTask(file, parentPackage);
        if(thread.uncaughtException()) {
            printf("Exception occurred while deserializing file %s\n", path.c_str());
            exit(3);
        }

        if(!execute) return;
        if(!thread.executeDeserialized()) {
            printf("Nothing to execute in file %s\n", path.c_str());
            exit(4);
        } else if(thread.uncaughtException()) {
            printf("Exception occurred while executing file %s\n", path.c_str());
            exit(5);
        }
    }
}

int main(int argc, const char** argv) {
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
