#include "POSIX.hpp"
#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <errno.h>

#define checkNodeExistence() \
    if(fi->fh == Ontology::VoidSymbol) \
        return -ENOENT;

#define resolvePath(node, path) \
    int error = resolvePathPartial(node, node, node, path); \
    if(error != -EEXIST) \
        return error;

#define getNodeOfPath(path) \
    Symbol node; \
    resolvePath(node, path);

#define setAttribute(node, AttributeSymbol, value) { \
    Symbol symbol; \
    if(!Ontology::getUncertain(node, AttributeSymbol, symbol)) { \
        symbol = Storage::createSymbol(); \
        Ontology::link({node, AttributeSymbol, symbol}); \
    } \
    Storage::writeBlob(symbol, value); \
}

#define setTimestamp(node, AttributeSymbol) { \
    time_t now; \
    time(&now); \
    setAttribute(node, AttributeSymbol, now); \
}

enum FSymbols {
    RootSymbol = 83,
    ModeSymbol,
    EntrySymbol,
    NameSymbol,
    LinkSymbol,
    UIDSymbol,
    GIDSymbol,
    RdevSymbol,
    ATimeSymbol,
    MTimeSymbol,
    CTimeSymbol
};

int resolvePathPartial(Symbol& parent, Symbol& entry, Symbol& node, const char*& pos) {
    if(*pos != '/')
        return -EINVAL;
    entry = parent = Ontology::VoidSymbol;
    node = RootSymbol;
    const char* begin = pos+1;
    if(*begin)
        while(true) {
            ++pos;
            if(*pos != '/' && *pos != 0)
                continue;
            if(pos == begin) {
                begin = pos+1;
                continue;
            }
            Symbol symbol;
            mode_t mode = Ontology::getUncertain(node, ModeSymbol, symbol) ? Storage::readBlobAt<mode_t>(symbol) : 0;
            if(!S_ISDIR(mode)) {
                pos = begin;
                return -ENOTDIR;
            }
            NativeNaturalType at;
            Symbol name = Storage::createSymbol();
            Ontology::stringToBlob(begin, pos-begin, name);
            bool found = Ontology::blobIndex.find(name, at);
            Storage::releaseSymbol(name);
            if(!found) {
                pos = begin;
                return -ENOENT;
            }
            name = Ontology::blobIndex.readElementAt(at);
            found = false;
            Ontology::query(9, {node, EntrySymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                if(Ontology::tripleExists({result.pos[0], NameSymbol, name})) {
                    found = true;
                    entry = result.pos[0];
                    parent = node;
                    Ontology::getUncertain(result.pos[0], Ontology::LinkSymbol, node);
                }
            });
            if(!found) {
                pos = begin;
                return -ENOENT;
            }
            if(*pos == 0)
                break;
            begin = pos+1;
        }
    pos = nullptr;
    return -EEXIST;
}

void fillNode(Symbol node, mode_t mode, dev_t rdev) {
    setAttribute(node, ModeSymbol, mode);
    setAttribute(node, UIDSymbol, geteuid());
    setAttribute(node, GIDSymbol, getegid());
    if(rdev > 0)
        setAttribute(node, RdevSymbol, rdev);
    setTimestamp(node, CTimeSymbol);
}

int makeNode(Symbol& node, const char* path, mode_t mode, dev_t rdev) {
    if(mode == S_IFSOCK || mode == S_IFLNK || mode == S_IFBLK || mode == S_IFCHR || mode == S_IFIFO)
        return -ENOSYS; // TODO

    Symbol parent;
    int error = resolvePathPartial(parent, parent, parent, path);
    if(error != -ENOENT) {
        node = parent;
        return error;
    }

    for(const char* pos = path; *pos; ++pos)
        if(*pos == '/') {
            node = Ontology::VoidSymbol;
            return -EINVAL;
        }

    if(node == Ontology::VoidSymbol) {
        node = Storage::createSymbol();
        fillNode(node, mode, rdev);
    }

    Symbol entry = Storage::createSymbol(),
           name = Ontology::createFromString(path);
    Ontology::blobIndex.insertElement(name);
    Ontology::link({entry, NameSymbol, name});
    Ontology::link({entry, Ontology::LinkSymbol, node});

    setTimestamp(parent, MTimeSymbol);
    Ontology::link({parent, EntrySymbol, entry});
    return 0;
}

int symatem_statfs(const char* path, struct statvfs* stbuf) {
    stbuf->f_bsize = Storage::bitsPerPage/8;
    stbuf->f_blocks = Storage::superPage->pagesEnd;
    stbuf->f_bfree = stbuf->f_bavail = -1; // TODO
    stbuf->f_files = Storage::superPage->symbolsEnd; // TODO
    stbuf->f_ffree = -1-Storage::superPage->symbolsEnd;
    stbuf->f_namemax = 255;
    return 0;
}

int symatem_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    checkNodeExistence();
    Symbol symbol;
    stbuf->st_ino = fi->fh;
    stbuf->st_nlink = Ontology::query(1, {Ontology::VoidSymbol, Ontology::LinkSymbol, fi->fh});
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

    // TODO: Sticky Bit, Setgid Bit, Setuid Bit

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
    setTimestamp(fi->fh, ATimeSymbol);
    return symatem_faccess(path, R_OK, fi);
}

int symatem_releasedir(const char* path, struct fuse_file_info* fi) {
    checkNodeExistence();
    return 0;
}

int symatem_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    struct fuse_file_info entryFi;
    struct stat stbuf;

    symatem_fgetattr(path, &stbuf, fi);
    filler(buf, ".", &stbuf, 0);

    Ontology::query(1, {Ontology::VoidSymbol, Ontology::LinkSymbol, fi->fh}, [&](Ontology::Triple entryResult) {
        Ontology::query(1, {Ontology::VoidSymbol, EntrySymbol, entryResult.pos[0]}, [&](Ontology::Triple parentResult) {
            entryFi.fh = parentResult.pos[0];
            symatem_fgetattr(NULL, &stbuf, &entryFi);
            filler(buf, "..", &stbuf, 0);
        });
    });

    Ontology::query(9, {fi->fh, EntrySymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol name;
        if(Ontology::getUncertain(result.pos[0], NameSymbol, name)
           && Ontology::getUncertain(result.pos[0], Ontology::LinkSymbol, entryFi.fh)) {
            Storage::Blob srcBlob(name);
            NativeNaturalType length = srcBlob.getSize()/8;
            Integer8* nameStr = static_cast<Integer8*>(malloc(length+1));
            srcBlob.externalOperate<false>(const_cast<Integer8*>(nameStr), 0, length*8);
            nameStr[length] = 0;
            symatem_fgetattr(NULL, &stbuf, &entryFi);
            filler(buf, nameStr, &stbuf, 0);
            free(nameStr);
        }
    });
    return 0;
}

int symatem_mknod(const char* path, mode_t mode, dev_t rdev) {
    Symbol node = Ontology::VoidSymbol;
    return makeNode(node, path, mode, rdev);
}

int symatem_unlink(const char* path) {
    Symbol parent, entry, node, name;
    int error = resolvePathPartial(parent, entry, node, path);
    if(error != -EEXIST)
        return error;

    Symbol symbol;
    mode_t mode = Ontology::getUncertain(node, ModeSymbol, symbol) ? Storage::readBlobAt<mode_t>(symbol) : 0;
    if(S_ISDIR(mode) && Ontology::query(9, {node, EntrySymbol, Ontology::VoidSymbol}) > 0)
        return -ENOTEMPTY;

    if(parent != Ontology::VoidSymbol) {
        setTimestamp(parent, MTimeSymbol);
        Ontology::getUncertain(entry, NameSymbol, name);
        Ontology::unlink(entry);
        if(Ontology::query(1, {Ontology::VoidSymbol, NameSymbol, name}) == 0)
            Ontology::unlink(name);
    }

    if(Ontology::query(1, {Ontology::VoidSymbol, Ontology::LinkSymbol, node}) == 0) {
        Ontology::BlobSet<true, Symbol> dirty;
        Ontology::query(15, {node, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
            dirty.insertElement(result.pos[0]);
        });
        dirty.iterate([&](Symbol symbol) {
            Ontology::unlink(symbol);
        });
    }
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
    Storage::Blob srcBlob(node);
    NativeNaturalType length = min(static_cast<NativeNaturalType>(size), srcBlob.getSize()/8);
    srcBlob.externalOperate<false>(const_cast<Integer8*>(buf), 0, length*8);
    buf[length] = 0;
    return 0;
}

int symatem_symlink(const char* from, const char* to) {
    printf("symatem_symlink %s\n", from);
    getNodeOfPath(from);
    printf("symatem_symlink %llu\n", node);
    int result = makeNode(node, to, S_IFLNK, 0);
    if(result == 0) {
        NativeNaturalType length = strlen(to);
        Storage::Blob dstBlob(node);
        dstBlob.increaseSize(0, length*8);
        dstBlob.externalOperate<true>(const_cast<Integer8*>(to), 0, length*8);
        Storage::modifiedBlob(node);
    }
    return result;
}

int symatem_link(const char* from, const char* to) {
    getNodeOfPath(from);
    return makeNode(node, to, 0, 0);
}

int symatem_rename(const char* from, const char* to) {
    int result = symatem_link(from, to);
    return result ? result : symatem_unlink(from);
}

int symatem_chmod(const char* path, mode_t mode) {
    getNodeOfPath(path);
    setAttribute(node, ModeSymbol, mode);
    return 0;
}

int symatem_chown(const char* path, uid_t uid, gid_t gid) {
    getNodeOfPath(path);
    setAttribute(node, UIDSymbol, uid);
    setAttribute(node, GIDSymbol, gid);
    return 0;
}

int symatem_ftruncate(const char* path, off_t size, struct fuse_file_info* fi) {
    checkNodeExistence();
    Storage::setBlobSize(fi->fh, size*8);
    for(NativeNaturalType i = 0; i < size; ++i)
        Storage::writeBlobAt<Natural8>(fi->fh, i, 0);
    return 0;
}

int symatem_truncate(const char* path, off_t size) {
    struct fuse_file_info fi;
    resolvePath(fi.fh, path);
    return symatem_ftruncate(path, size, &fi);
}

int symatem_fallocate(const char* path, int mode, off_t offset, off_t length, struct fuse_file_info* fi) {
    checkNodeExistence();
    if((offset+length)*8 > Storage::getBlobSize(fi->fh))
        return symatem_ftruncate(path, offset+length, fi);
    return 0;
}

int symatem_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    fi->fh = Ontology::VoidSymbol;
    return makeNode(fi->fh, path, mode, 0);
}

int symatem_open(const char* path, struct fuse_file_info* fi) {
    resolvePath(fi->fh, path);
    checkNodeExistence();
    setTimestamp(fi->fh, ATimeSymbol);
    return symatem_faccess(path, R_OK, fi);
}

int symatem_release(const char* path, struct fuse_file_info* fi) {
    checkNodeExistence();
    return 0;
}

int symatem_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    Storage::Blob srcBlob(fi->fh);
    if((offset+size)*8 > srcBlob.getSize())
        return 0;
    srcBlob.externalOperate<false>(const_cast<Integer8*>(buf), offset*8, size*8);
    return size;
}

int symatem_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    setTimestamp(fi->fh, MTimeSymbol);
    Storage::Blob dstBlob(fi->fh);
    NativeIntegerType diff = (offset+size)*8-dstBlob.getSize();
    if(diff > 0)
        dstBlob.increaseSize(dstBlob.getSize(), diff);
    dstBlob.externalOperate<true>(const_cast<Integer8*>(buf), offset*8, size*8);
    Storage::modifiedBlob(fi->fh);
    return size;
}

int symatem_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    checkNodeExistence();
    return -ENOSYS; // TODO
}



struct fuse_operations symatem_oper = {
    .statfs = symatem_statfs,
    .fgetattr = symatem_fgetattr,
    .getattr = symatem_getattr,
    .access = symatem_access,
    .opendir = symatem_opendir,
    .releasedir = symatem_releasedir,
    .readdir = symatem_readdir,
    .mknod = symatem_mknod,
    .unlink = symatem_unlink,
    .mkdir = symatem_mkdir,
    .rmdir = symatem_rmdir,
    .readlink = symatem_readlink,
    .symlink = symatem_symlink,
    .link = symatem_link,
    .rename = symatem_rename,
    .chmod = symatem_chmod,
    .chown = symatem_chown,
    .ftruncate = symatem_ftruncate,
    .truncate = symatem_truncate,
#if defined(HAVE_POSIX_FALLOCATE) || defined(__APPLE__)
    .fallocate = symatem_fallocate,
#endif
    .create = symatem_create,
    .open = symatem_open,
    .release = symatem_release,
    .read = symatem_read,
    .write = symatem_write,
    .fsync = symatem_fsync,
    .flag_nullpath_ok = 1
};

Integer32 main(Integer32 argc, Integer8** argv) {
    if(argc < 3) {
        printf("Usage: SymatemFS [DataFile] [MountPoint]\n");
        return -1;
    }

    struct fuse_args fargs = FUSE_ARGS_INIT(0, NULL);
    if(fuse_opt_add_arg(&fargs, argv[0]) == -1 ||
       fuse_opt_add_arg(&fargs, "-d") == -1 ||
       fuse_opt_add_arg(&fargs, "-s") == -1 ||
       fuse_opt_add_arg(&fargs, "-o") == -1 ||
       fuse_opt_add_arg(&fargs, "volname=SymatemFS") == -1 ||
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
