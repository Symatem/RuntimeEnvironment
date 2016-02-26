#include "Modi.hpp"
#include <dirent.h>

void loadFromPath(Symbol parentPackage, bool execute, std::string path) {
    if(path[path.size()-1] == '/') path.resize(path.size()-1);

    struct stat s;
    if(stat(path.c_str(), &s) != 0) return;
    if(s.st_mode & S_IFDIR) {
        DIR* dp = opendir(path.c_str());
        if(dp == NULL) {
            interfaceBuffer = "Could not open directory ";
            interfaceBuffer += path;
            return;
        }

        auto slashIndex = path.rfind('/');
        std::string name = (slashIndex != std::string::npos) ? path.substr(slashIndex+1) : path;
        Symbol package = task.context.createFromData(name.c_str());
        if(parentPackage == PreDef_Void) parentPackage = package;
        task.context.link({package, PreDef_Holds, parentPackage});

        struct dirent* entry;
        while(interfaceBuffer.empty() && (entry = readdir(dp)))
            if(entry->d_name[0] != '.')
                loadFromPath(package, execute, path+'/'+entry->d_name);
        closedir(dp);
    } else if(s.st_mode & S_IFREG) {
        if(!stringEndsWith(path, ".sym")) return;

        Symbol file = task.context.createSymbolFromFile(path.c_str());
        if(file == PreDef_Void) {
            interfaceBuffer = "Could not open file ";
            interfaceBuffer += path;
            return;
        }

        task.deserializationTask(file, parentPackage);
        if(task.uncaughtException()) {
            interfaceBuffer = "Exception occurred while deserializing file ";
            interfaceBuffer += path;
            return;
        }

        if(!execute) return;
        if(!task.executeDeserialized()) {
            interfaceBuffer = "Nothing to execute in file ";
            interfaceBuffer += path;
            return;
        } else if(task.uncaughtException()) {
            interfaceBuffer = "Exception occurred while executing file ";
            interfaceBuffer += path;
            return;
        }
    }
}

int main(int argc, const char** argv) {
    context.init();
    init();

    /*const std::string errorMessage = CSI+"1;31m[Error]"+CSI+"m",
                      warningMessage = CSI+"1;33m[Warning]"+CSI+"m",
                      successMessage = CSI+"1;32m[Success]"+CSI+"m",
                      infoMessage = CSI+"1;36m[Info]"+CSI+"m";*/

    bool execute = false, terminateAfterwards  = false;
    for(ArchitectureType i = 1; interfaceBuffer.empty() && i < argc; ++i) {
        if(strcmp(argv[i], "-h") == 0) {
            std::cout << "This is not the help page you are looking for." << std::endl;
            std::cout << "No, seriously, RTFM." << std::endl;
            terminate(2);
        } else if(strcmp(argv[i], "-e") == 0) {
            execute = true;
            continue;
        } else if(strcmp(argv[i], "-t") == 0) {
            terminateAfterwards = true;
            continue;
        }
        loadFromPath(PreDef_Void, execute, argv[i]);
    }
    if(terminateAfterwards) {
        if(interfaceBuffer.empty())
            terminate(0);
        else {
            std::cout << interfaceBuffer << std::endl;
            terminate(1);
        }
    }
    if(interfaceBuffer.empty())
        task.clear();

    while(true) {
        render();
        switch(mode) {
            case Mode_Browse:
                pollKeyboard(ModeBrowse);
                break;
            case Mode_Input:
                pollKeyboard(ModeInput);
                break;
        }
    }

    return 0;
}
