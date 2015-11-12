#include "Modi.hpp"

int main(int argc, const char** argv) {
    init();

    /*const std::string errorMessage = CSI+"1;31m[Error]"+CSI+"m",
                      warningMessage = CSI+"1;33m[Warning]"+CSI+"m",
                      successMessage = CSI+"1;32m[Success]"+CSI+"m",
                      infoMessage = CSI+"1;36m[Info]"+CSI+"m";*/

    bool execute = false;
    for(ArchitectureType i = 1; interfaceBuffer.empty() && i < argc; ++i) {
        if(strcmp(argv[i], "-d") == 0) {
            execute = false;
            continue;
        } else if(strcmp(argv[i], "-e") == 0) {
            execute = true;
            continue;
        }

        DIR* dp = opendir(argv[i]);
        if(dp == NULL) {
            interfaceBuffer = "Could not open directory ";
            interfaceBuffer += argv[i];
            break;
        }
        std::string directoryPath = argv[i];
        if(directoryPath[directoryPath.size()-1] == '/') directoryPath.resize(directoryPath.size()-1);
        std::string directoryName = directoryPath;
        auto slashIndex = directoryName.rfind('/');
        if(slashIndex != std::string::npos)
            directoryName = directoryName.substr(slashIndex+1);
        directoryPath += '/';
        Symbol package = task.symbolFor<false>(directoryName);
        task.link({package, PreDef_Holds, package});
        struct dirent* entry;
        while((entry = readdir(dp))) {
            auto len = strlen(entry->d_name);
            if(len < 4 || strncmp(entry->d_name+len-3, ".os", 3) != 0) continue;
            std::string filePath = directoryPath+entry->d_name;
            std::ifstream file(filePath);
            if(!file.good()) {
                interfaceBuffer = "Could not open file ";
                interfaceBuffer += filePath;
                break;
            }
            task.evaluateExtend(task.symbolFor<false>(file), execute, package);
            if(execute) {
                // task.executeInfinite(); // TODO
                if(task.uncaughtException()) {
                    interfaceBuffer = "Could not execute file ";
                    interfaceBuffer += filePath;
                }
                break;
            } else if(task.uncaughtException()) {
                interfaceBuffer = "Could not parse file ";
                interfaceBuffer += filePath;
                break;
            }
        }
        closedir(dp);
    }

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
