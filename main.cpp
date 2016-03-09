#include "Platform/POSIX.hpp"

Thread thread;

int main(int argc, char** argv) {
    Storage::load();
    Ontology::tryToFillPreDef();

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
        loadFromPath(thread, PreDef_Void, execute, argv[i]);
    }
    thread.clear();

    return 0;
}
