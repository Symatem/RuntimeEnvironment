#include <Targets/POSIX.hpp>
#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <errno.h>

#define checkNodeExistence() \
    if(fi->fh == VoidSymbol) \
        return -ENOENT;

#define resolvePath(node, path) \
    int error = resolvePathPartial(node, node, node, path); \
    if(error != -EEXIST) \
        return error;

#define setTimestamp(node, META_INDEX) { \
    time_t now; \
    time(&now); \
    Symbol metaSymbol; \
    assert(getUncertain(node, MetaSymbol, metaSymbol)); \
    Blob(metaSymbol).writeAt<Natural64>(META_INDEX, now); \
}

#define getNodeOfPath(path) \
    Symbol node; \
    resolvePath(node, path);

#define META_CTIME 0
#define META_MTIME 1
#define META_ATIME 2
#define META_UID 6
#define META_GID 7
#define META_RDEV 8
#define META_MODE 18

extern "C" {

enum FSymbols {
    RootSymbol = 83,
    EntrySymbol,
    NameSymbol,
    LinkSymbol,
    MetaSymbol
};

int resolvePathPartial(Symbol& parent, Symbol& entry, Symbol& node, const char*& pos) {
    if(*pos != '/')
        return -EINVAL;
    entry = parent = VoidSymbol;
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
            Symbol metaSymbol;
            assert(getUncertain(node, MetaSymbol, metaSymbol));
            mode_t mode = Blob(metaSymbol).readAt<Natural16>(META_MODE);
            if(!S_ISDIR(mode)) {
                pos = begin;
                return -ENOTDIR;
            }
            NativeNaturalType at;
            Symbol name = createSymbol();
            stringToBlob(begin, pos-begin, name);
            bool found = blobIndex.find(name, at);
            unlink(name);
            if(!found) {
                pos = begin;
                return -ENOENT;
            }
            name = blobIndex.readElementAt(at);
            found = false;
            query(MMV, {node, EntrySymbol, VoidSymbol}, [&](Triple result) {
                if(tripleExists({result.pos[2], NameSymbol, name})) {
                    found = true;
                    entry = result.pos[0];
                    parent = node;
                    getUncertain(result.pos[2], LinkSymbol, node);
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
    time_t now;
    time(&now);
    Symbol metaSymbol = createSymbol();
    link({node, MetaSymbol, metaSymbol});
    Blob metaBlob(metaSymbol);
    metaBlob.setSize(304);
    metaBlob.writeAt<Natural64>(META_CTIME, now);
    metaBlob.writeAt<Natural64>(META_MTIME, now);
    metaBlob.writeAt<Natural64>(META_ATIME, now);
    metaBlob.writeAt<Natural32>(META_UID, geteuid());
    metaBlob.writeAt<Natural32>(META_GID, getegid());
    metaBlob.writeAt<Natural32>(META_RDEV, rdev);
    metaBlob.writeAt<Natural16>(META_MODE, mode);
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
            node = VoidSymbol;
            return -EINVAL;
        }

    if(node == VoidSymbol) {
        node = createSymbol();
        fillNode(node, mode, rdev);
    }

    Symbol entry = createSymbol(),
           name = createFromString(path);
    blobIndex.insertElement(name);
    link({entry, NameSymbol, name});
    link({entry, LinkSymbol, node});

    setTimestamp(parent, META_MTIME);
    link({parent, EntrySymbol, entry});
    return 0;
}

int symatem_statfs(const char* path, struct statvfs* stbuf) {
    stbuf->f_bsize = bitsPerPage/8;
    stbuf->f_blocks = superPage->pagesEnd;
    stbuf->f_bfree = stbuf->f_bavail = -1; // TODO
    stbuf->f_files = superPage->symbolsEnd; // TODO
    stbuf->f_ffree = -1-superPage->symbolsEnd;
    stbuf->f_namemax = 255;
    return 0;
}

int symatem_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    checkNodeExistence();
    stbuf->st_ino = fi->fh;
    stbuf->st_nlink = query(VMM, {VoidSymbol, LinkSymbol, fi->fh});
    stbuf->st_size = Blob(fi->fh).getSize()/8;
    stbuf->st_blocks = (stbuf->st_size+511)/512;

    Symbol metaSymbol;
    assert(getUncertain(fi->fh, MetaSymbol, metaSymbol));
    Blob metaBlob(metaSymbol);
    stbuf->st_ctime = metaBlob.readAt<Natural64>(META_CTIME);
    stbuf->st_mtime = metaBlob.readAt<Natural64>(META_MTIME);
    stbuf->st_atime = metaBlob.readAt<Natural64>(META_ATIME);
    stbuf->st_uid = metaBlob.readAt<Natural32>(META_UID);
    stbuf->st_gid = metaBlob.readAt<Natural32>(META_GID);
    stbuf->st_rdev = metaBlob.readAt<Natural32>(META_RDEV);
    stbuf->st_mode = metaBlob.readAt<Natural16>(META_MODE);
    return 0;
}

int symatem_getattr(const char* path, struct stat* stbuf) {
    struct fuse_file_info fi;
    resolvePath(fi.fh, path);
    return symatem_fgetattr(path, stbuf, &fi);
}

int symatem_faccess(const char* path, int mask, struct fuse_file_info* fi) {
    checkNodeExistence();

    Symbol metaSymbol;
    assert(getUncertain(fi->fh, MetaSymbol, metaSymbol));
    Blob metaBlob(metaSymbol);
    mode_t mode = metaBlob.readAt<Natural16>(META_MODE);
    bool ownerUser = metaBlob.readAt<Natural32>(META_UID) == geteuid();
    bool ownerGroup = metaBlob.readAt<Natural32>(META_GID) == getegid();

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
    setTimestamp(fi->fh, META_ATIME);
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

    query(VMM, {VoidSymbol, LinkSymbol, fi->fh}, [&](Triple entryResult) {
        query(VMM, {VoidSymbol, EntrySymbol, entryResult.pos[0]}, [&](Triple parentResult) {
            entryFi.fh = parentResult.pos[0];
            symatem_fgetattr(NULL, &stbuf, &entryFi);
            filler(buf, "..", &stbuf, 0);
        });
    });

    query(MMV, {fi->fh, EntrySymbol, VoidSymbol}, [&](Triple result) {
        Symbol name;
        if(getUncertain(result.pos[2], NameSymbol, name)
           && getUncertain(result.pos[2], LinkSymbol, entryFi.fh)) {
            Blob srcBlob(name);
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
    Symbol node = VoidSymbol;
    return makeNode(node, path, mode, rdev);
}

int symatem_unlink(const char* path) {
    Symbol parent, entry, node, name;
    int error = resolvePathPartial(parent, entry, node, path);
    if(error != -EEXIST)
        return error;

    Symbol metaSymbol;
    assert(getUncertain(node, MetaSymbol, metaSymbol));
    mode_t mode = Blob(metaSymbol).readAt<Natural16>(META_MODE);
    if(S_ISDIR(mode) && query(MMV, {node, EntrySymbol, VoidSymbol}) > 0)
        return -ENOTEMPTY;

    if(parent != VoidSymbol) {
        setTimestamp(parent, META_MTIME);
        getUncertain(entry, NameSymbol, name);
        unlink(entry);
        if(query(VMM, {VoidSymbol, NameSymbol, name}) == 0) {
            blobIndex.eraseElement(name);
            unlink(name);
        }
    }

    if(query(VMM, {VoidSymbol, LinkSymbol, node}) == 0) {
        GuardedBitstreamDataStructure<BitstreamSet<Symbol>> dirty;
        query(MIV, {node, VoidSymbol, VoidSymbol}, [&](Triple result) {
            dirty.insertElement(result.pos[2]);
        });
        iterateElements(dirty, [&](Symbol symbol) {
            unlink(symbol);
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
    Blob srcBlob(node);
    NativeNaturalType length = min(static_cast<NativeNaturalType>(size), srcBlob.getSize()/8);
    srcBlob.externalOperate<false>(const_cast<Integer8*>(buf), 0, length*8);
    buf[length] = 0;
    return 0;
}

int symatem_symlink(const char* from, const char* to) {
    getNodeOfPath(from);
    int result = makeNode(node, to, S_IFLNK, 0);
    if(result == 0) {
        NativeNaturalType length = strlen(to);
        Blob dstBlob(node);
        dstBlob.increaseSize(0, length*8);
        dstBlob.externalOperate<true>(const_cast<Integer8*>(to), 0, length*8);
        modifiedBlob(node);
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
    Symbol metaSymbol;
    assert(getUncertain(node, MetaSymbol, metaSymbol));
    Blob(metaSymbol).writeAt<Natural16>(META_MODE, mode);
    return 0;
}

int symatem_chown(const char* path, uid_t uid, gid_t gid) {
    getNodeOfPath(path);
    Symbol metaSymbol;
    assert(getUncertain(node, MetaSymbol, metaSymbol));
    Blob metaBlob(metaSymbol);
    metaBlob.writeAt<Natural32>(META_UID, uid);
    metaBlob.writeAt<Natural32>(META_GID, gid);
    return 0;
}

int symatem_utimens(const char* path, const struct timespec tv[2]) {
    getNodeOfPath(path);
    Symbol metaSymbol;
    assert(getUncertain(node, MetaSymbol, metaSymbol));
    Blob metaBlob(metaSymbol);
    metaBlob.writeAt<Natural64>(META_MTIME, tv[1].tv_sec);
    metaBlob.writeAt<Natural64>(META_ATIME, tv[0].tv_sec);
    return 0;
}

int symatem_ftruncate(const char* path, off_t size, struct fuse_file_info* fi) {
    checkNodeExistence();
    Blob dataBlob(fi->fh);
    dataBlob.setSize(size*8);
    for(NativeNaturalType i = 0; i < static_cast<NativeNaturalType>(size); ++i)
        dataBlob.writeAt<Natural8>(i, 0);
    modifiedBlob(fi->fh);
    return 0;
}

int symatem_truncate(const char* path, off_t size) {
    struct fuse_file_info fi;
    resolvePath(fi.fh, path);
    return symatem_ftruncate(path, size, &fi);
}

int symatem_fallocate(const char* path, int mode, off_t offset, off_t length, struct fuse_file_info* fi) {
    checkNodeExistence();
    if(static_cast<NativeNaturalType>(offset+length)*8 > Blob(fi->fh).getSize())
        return symatem_ftruncate(path, offset+length, fi);
    return 0;
}

int symatem_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    fi->fh = VoidSymbol;
    return makeNode(fi->fh, path, mode, 0);
}

int symatem_open(const char* path, struct fuse_file_info* fi) {
    resolvePath(fi->fh, path);
    checkNodeExistence();
    setTimestamp(fi->fh, META_ATIME);
    return symatem_faccess(path, R_OK, fi);
}

int symatem_release(const char* path, struct fuse_file_info* fi) {
    checkNodeExistence();
    return 0;
}

int symatem_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    Blob srcBlob(fi->fh);
    if((offset+size)*8 > srcBlob.getSize())
        return 0;
    srcBlob.externalOperate<false>(const_cast<Integer8*>(buf), offset*8, size*8);
    return size;
}

int symatem_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    checkNodeExistence();
    setTimestamp(fi->fh, META_MTIME);
    Blob dstBlob(fi->fh);
    NativeIntegerType diff = (offset+size)*8-dstBlob.getSize();
    if(diff > 0)
        dstBlob.increaseSize(dstBlob.getSize(), diff);
    dstBlob.externalOperate<true>(const_cast<Integer8*>(buf), offset*8, size*8);
    modifiedBlob(fi->fh);
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
    .utimens = symatem_utimens,
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
       fuse_opt_add_arg(&fargs, "-f") == -1 ||
       fuse_opt_add_arg(&fargs, "-s") == -1 ||
       fuse_opt_add_arg(&fargs, "-o") == -1 ||
       fuse_opt_add_arg(&fargs, "volname=SymatemFS") == -1 ||
       fuse_opt_add_arg(&fargs, argv[argc-1]) == -1) {
        printf("Failed to set FUSE options\n");
        fuse_opt_free_args(&fargs);
        return -2;
    }

    loadStorage(argv[argc-2]);
    if(tryToFillPreDefined(20))
        fillNode(RootSymbol, S_IFDIR|S_IRWXU|S_IRWXG|S_IRWXO, 0);

    Integer32 result = fuse_main(fargs.argc, fargs.argv, &symatem_oper, nullptr);
    fuse_opt_free_args(&fargs);

    unloadStorage();
    return result;
}

}
