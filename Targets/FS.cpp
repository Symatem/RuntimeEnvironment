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



void* symatem_init(struct fuse_conn_info* conn) {
/*#ifdef __APPLE__
	FUSE_ENABLE_SETVOLNAME(conn);
	FUSE_ENABLE_XTIMES(conn);
#endif*/
	return NULL;
}

void symatem_destroy(void* userdata) {

}

int symatem_statfs(const char* path, struct statvfs* stbuf) {
    stbuf->f_bsize = Storage::bitsPerPage/8;
    stbuf->f_blocks = Storage::pageCount;
    stbuf->f_bfree = 0; // TODO
    stbuf->f_bavail = 0; // TODO
    stbuf->f_files = 0; // TODO
    stbuf->f_ffree = 0; // TODO
    stbuf->f_namemax = 0; // TODO
	return 0;
}

int symatem_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    checkNodeExistence();
    Symbol symbol;
    stbuf->st_ino = fi->fh;
    stbuf->st_nlink = Ontology::query(1, {Ontology::ValueSymbol, fi->fh, Ontology::VoidSymbol});
    stbuf->st_size = Storage::getBlobSize(fi->fh)/8;
    stbuf->st_blocks = (stbuf->st_size+511)/512;
    stbuf->st_mode = Ontology::getUncertain(fi->fh, ModeSymbol, symbol) ? Storage::readBlobAt<mode_t>(symbol) : 0;
    stbuf->st_uid = Ontology::getUncertain(fi->fh, UIDSymbol, symbol) ? Storage::readBlobAt<uid_t>(symbol) : geteuid();
    stbuf->st_gid = Ontology::getUncertain(fi->fh, GIDSymbol, symbol) ? Storage::readBlobAt<gid_t>(symbol) : getegid();
    stbuf->st_rdev = Ontology::getUncertain(fi->fh, RdevSymbol, symbol) ? Storage::readBlobAt<dev_t>(symbol) : 0;
    stbuf->st_ctime = Ontology::getUncertain(fi->fh, CTimeSymbol, symbol) ? Storage::readBlobAt<time_t>(symbol) : 0;
    stbuf->st_mtime = Ontology::getUncertain(fi->fh, MTimeSymbol, symbol) ? Storage::readBlobAt<time_t>(symbol) : stbuf->st_ctime;
    stbuf->st_atime = Ontology::getUncertain(fi->fh, ATimeSymbol, symbol) ? Storage::readBlobAt<time_t>(symbol) : stbuf->st_mtime;
	return 0;
}

int symatem_getattr(const char* path, struct stat* stbuf) {
    struct fuse_file_info fi;
    resolvePath(fi.fh, path);
	return symatem_fgetattr(path, stbuf, &fi);
}

int symatem_faccess(const char* path, int mask, struct fuse_file_info* fi) {
    checkNodeExistence();

    Symbol symbol;
    mode_t mode = Ontology::getUncertain(fi->fh, ModeSymbol, symbol) ? Storage::readBlobAt<mode_t>(symbol) : 0;
    bool ownerUser = Ontology::getUncertain(fi->fh, UIDSymbol, symbol) ? Storage::readBlobAt<uid_t>(symbol) == geteuid() : true;
    bool ownerGroup = Ontology::getUncertain(fi->fh, GIDSymbol, symbol) ? Storage::readBlobAt<gid_t>(symbol) == getegid() : true;

    if((mode&R_OK) && !(ownerUser && mode&S_IRUSR) && !(ownerGroup && mode&S_IRGRP) && !(mode&S_IROTH))
        return -EACCES;

    if((mode&W_OK) && !(ownerUser && mode&S_IWUSR) && !(ownerGroup && mode&S_IWGRP) && !(mode&S_IWOTH))
        return -EACCES;

    if((mode&X_OK) && !(ownerUser && mode&S_IXUSR) && !(ownerGroup && mode&S_IXGRP) && !(mode&S_IXOTH))
        return -EACCES;

	return 0;
}

int symatem_access(const char* path, int mask) {
    struct fuse_file_info fi;
    resolvePath(fi.fh, path);
	return symatem_faccess(path, mask, &fi);
}

int symatem_opendir(const char* path, struct fuse_file_info* fi) {
    resolvePath(fi->fh, path);
    checkNodeExistence();
	return symatem_faccess(path, R_OK, fi);
}

int symatem_releasedir(const char* path, struct fuse_file_info* fi) {
    checkNodeExistence();
	return 0;
}

int symatem_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    Ontology::query(9, {fi->fh, Ontology::EntitySymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol name;
        struct fuse_file_info fi;
        struct stat stbuf;
        if(Ontology::getUncertain(result.pos[0], Ontology::AttributeSymbol, name)
           && Ontology::getUncertain(result.pos[0], Ontology::ValueSymbol, fi.fh)) {
            NativeNaturalType bytes = Storage::getBlobSize(name)/8;
            Integer8* nameStr = static_cast<Integer8*>(malloc(bytes+1));
            for(NativeNaturalType i = 0; i < bytes; ++i)
                nameStr[i] = Storage::readBlobAt<Integer8>(name, i);
            nameStr[bytes] = 0;
            symatem_fgetattr(path, &stbuf, &fi);
            filler(buf, nameStr, &stbuf, 0);
            free(nameStr);
        }
    });
	return 0;
}

int symatem_mknod(const char* path, mode_t mode, dev_t rdev) {
    if(mode == S_IFSOCK || mode == S_IFLNK || mode == S_IFBLK || mode == S_IFCHR || mode == S_IFIFO)
        return -ENOSYS;
    Symbol node = Ontology::VoidSymbol;
    return makeNode(node, path, mode, rdev);
}

int symatem_unlink(const char* path) {
    return -ENOSYS; // TODO
    getNodeOfPath(path);
    // TODO: Only unlink in one parent
    Ontology::query(1, {Ontology::ValueSymbol, node, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Ontology::unlink(result.pos[0]);
    });
    Ontology::unlink(node);
	return 0;
}

int symatem_mkdir(const char* path, mode_t mode) {
	return symatem_mknod(path, mode, 0);
}

int symatem_rmdir(const char* path) {
	return symatem_unlink(path);
}

int symatem_readlink(const char* path, char* buf, size_t size) {
    getNodeOfPath(path);
    NativeNaturalType len = min(static_cast<NativeNaturalType>(size), Storage::getBlobSize(node)/8);
    for(NativeNaturalType i = 0; i < len; ++i)
        buf[i] = Storage::readBlobAt<Integer8>(node, i);
    buf[len] = 0;
	return 0;
}

int symatem_symlink(const char* from, const char* to) {
    getNodeOfPath(from);
	int result = makeNode(node, to, S_IFLNK, 0);
    if(result == 0) {
        NativeNaturalType len = strlen(to);
        Storage::setBlobSize(node, len*8);
        for(NativeNaturalType i = 0; i < len; ++i)
            Storage::writeBlobAt<Integer8>(node, i, to[i]);
    }
    return result;
}

int symatem_link(const char* from, const char* to) {
    getNodeOfPath(from);
    return makeNode(node, to, 0, 0);
}

int symatem_rename(const char* from, const char* to) {
    getNodeOfPath(from);
    int result = symatem_link(from, to);
    if(result)
        return result;
	return unlink(from);
}

int symatem_chmod(const char* path, mode_t mode) {
    getNodeOfPath(path);
    Symbol symbol;
    if(!Ontology::getUncertain(node, ModeSymbol, symbol)) {
        symbol = Storage::createSymbol();
        Ontology::link({node, ModeSymbol, symbol});
    }
    Storage::writeBlob<mode_t>(symbol, mode);
	return 0;
}

int symatem_chown(const char* path, uid_t uid, gid_t gid) {
    getNodeOfPath(path);
    Symbol symbol;
    if(!Ontology::getUncertain(node, UIDSymbol, symbol)) {
        symbol = Storage::createSymbol();
        Ontology::link({node, UIDSymbol, symbol});
    }
    Storage::writeBlob<uid_t>(symbol, gid);
    if(!Ontology::getUncertain(node, GIDSymbol, symbol)) {
        symbol = Storage::createSymbol();
        Ontology::link({node, GIDSymbol, symbol});
    }
    Storage::writeBlob<gid_t>(symbol, gid);
    return 0;
}

int symatem_ftruncate(const char* path, off_t size, struct fuse_file_info* fi) {
    checkNodeExistence();
    Storage::setBlobSize(fi->fh, size*8);
    for(NativeNaturalType i = 0; i < size; ++i)
        Storage::writeBlobAt<Natural8>(fi->fh, i, 0);
	return 0;
}

int symatem_fallocate(const char* path, int mode, off_t offset, off_t length, struct fuse_file_info* fi) {
    checkNodeExistence();
    if(offset+length > Storage::getBlobSize(fi->fh)/8)
        return symatem_ftruncate(path, offset+length, fi);
	return 0;
}

int symatem_truncate(const char* path, off_t size) {
    struct fuse_file_info fi;
    resolvePath(fi.fh, path);
    return symatem_ftruncate(path, size, &fi);
}

int symatem_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    fi->fh = Ontology::VoidSymbol;
    return makeNode(fi->fh, path, mode, 0);
}

int symatem_open(const char* path, struct fuse_file_info* fi) {
    resolvePath(fi->fh, path);
    checkNodeExistence();
	return symatem_faccess(path, R_OK, fi);
}

int symatem_release(const char* path, struct fuse_file_info* fi) {
    checkNodeExistence();
	return 0;
}

int symatem_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    if(offset+size > Storage::getBlobSize(fi->fh)/8)
        return -EINVAL;
    for(NativeNaturalType i = offset; i < offset+size; ++i)
        buf[i] = Storage::readBlobAt<Integer8>(fi->fh, i);
	return size;
}

int symatem_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    if(offset+size > Storage::getBlobSize(fi->fh)/8) {
        int result = symatem_ftruncate(path, offset+size, fi);
        if(result)
            return result;
    }
    for(NativeNaturalType i = offset; i < offset+size; ++i)
        Storage::writeBlobAt<Integer8>(fi->fh, i, buf[i]);
	return size;
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
