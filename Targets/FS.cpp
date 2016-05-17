#include "POSIX.hpp"
#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <errno.h>

#define checkNodeExistence() \
    if(fi->fh == Ontology::VoidSymbol) \
        return -ENOENT;

#define resolvePath(node, path) \
    if(resolvePathPartial(path, node)) \
        node = Ontology::VoidSymbol;

#define getNodeOfPath(path) \
    Symbol node; \
    if(!resolvePathPartial(path, node)) \
        return -ENOENT;

enum FSymbols {
    RootSymbol = 83,
    ModeSymbol,
    UIDSymbol,
    GIDSymbol,
    RdevSymbol,
    ATimeSymbol,
    MTimeSymbol,
    CTimeSymbol
};

const char* resolvePathPartial(const char* pos, Symbol& node) {
    if(*pos != '/')
        return pos;

    node = RootSymbol;
    const char* begin = pos+1;
    while(true) {
        ++pos;
        if(*pos != '/' && *pos != 0)
            continue;

        if(pos == begin)
            return (pos[-1] == '/') ? nullptr : pos+1;

        NativeNaturalType at;
        Symbol name = Storage::createSymbol();
        Ontology::stringToBlob(begin, pos-begin, name);
        bool found = Ontology::blobIndex.find(name, at);
        Storage::releaseSymbol(name);
        if(!found)
            return begin;
        name = Ontology::blobIndex.readElementAt(at);

        found = false;
        Ontology::query(9, {node, Ontology::EntitySymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
            if(Ontology::tripleExists({result.pos[0], Ontology::AttributeSymbol, name})) {
                Ontology::getUncertain(result.pos[0], Ontology::ValueSymbol, node);
                found = true;
            }
        });
        if(!found)
            return begin;
        if(*pos == 0)
            break;
        begin = pos+1;
    }

    return nullptr;
}

void fillNode(Symbol node, mode_t mode, dev_t rdev) {
    time_t now;
    time(&now);

    Symbol symbol = Storage::createSymbol();
    Storage::writeBlob<mode_t>(symbol, mode);
    Ontology::link({node, ModeSymbol, symbol});

    symbol = Storage::createSymbol();
    Storage::writeBlob<uid_t>(symbol, geteuid());
    Ontology::link({node, UIDSymbol, symbol});

    symbol = Storage::createSymbol();
    Storage::writeBlob<gid_t>(symbol, getegid());
    Ontology::link({node, GIDSymbol, symbol});

    if(rdev > 0) {
        symbol = Storage::createSymbol();
        Storage::writeBlob<dev_t>(symbol, rdev);
        Ontology::link({node, RdevSymbol, symbol});
    }

    symbol = Storage::createSymbol();
    Storage::writeBlob<time_t>(symbol, now);
    Ontology::link({node, CTimeSymbol, symbol});
}

int makeNode(Symbol& node, const char* path, mode_t mode, dev_t rdev) {
    Symbol parent;
    const char* nameStr = resolvePathPartial(path, parent);
    if(!nameStr) {
        node = parent;
        return -EEXIST;
    }

    for(const char* pos = nameStr; *pos; ++pos)
        if(*pos == '/') {
            node = Ontology::VoidSymbol;
            return -EINVAL;
        }

    if(node == Ontology::VoidSymbol) {
        node = Storage::createSymbol();
        fillNode(node, mode, rdev);
    }

    Symbol entry = Storage::createSymbol(),
           name = Ontology::createFromString(nameStr);
    Ontology::blobIndex.insertElement(name);
    Ontology::link({entry, Ontology::AttributeSymbol, name});
    Ontology::link({entry, Ontology::ValueSymbol, node});
    Ontology::link({parent, Ontology::EntitySymbol, entry});
	return 0;
}



Integer32 main(Integer32 argc, Integer8** argv) {
	if(argc < 3) {
		printf("Usage: SymatemFS [DataFile] [MountPoint]\n");
		return -1;
	}

	struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);
	if(fuse_opt_add_arg(&fargs, argv[0]) == -1 ||
	   fuse_opt_add_arg(&fargs, "-d") == -1 ||
	   fuse_opt_add_arg(&fargs, "-s") == -1 ||
	   fuse_opt_add_arg(&fargs, argv[argc-1]) == -1) {
		printf("Failed to set FUSE options\n");
		fuse_opt_free_args(&fargs);
		return -2;
	}

	loadStorage(argv[argc-2]);
    if(Ontology::tryToFillPreDefined(20))
        fillNode(RootSymbol, S_IFDIR|S_IRWXU|S_IRWXG|S_IRWXO, 0);

	Integer32 result = fuse_main(fargs.argc, fargs.argv, &symatem_oper, nullptr);
	fuse_opt_free_args(&fargs);

	unloadStorage();
	return result;
}
