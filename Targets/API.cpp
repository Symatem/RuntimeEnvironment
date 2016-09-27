#include "POSIX.hpp"

extern "C" {

Symbol createFromFile(const char* path) {
    Integer32 fd = open(path, O_RDONLY);
    if(fd < 0)
        return Ontology::VoidSymbol;
    Symbol dstSymbol = Storage::createSymbol();
    Ontology::link({dstSymbol, Ontology::BlobTypeSymbol, Ontology::TextSymbol});
    NativeNaturalType length = lseek(fd, 0, SEEK_END);
    Storage::Blob dstBlob(dstSymbol);
    dstBlob.increaseSize(0, length*8);
    lseek(fd, 0, SEEK_SET);
    Natural8 src[512];
    NativeNaturalType dstIndex = 0;
    while(length > 0) {
        NativeNaturalType count = min(static_cast<NativeNaturalType>(sizeof(src)), length);
        read(fd, src, count);
        dstBlob.externalOperate<true>(src, dstIndex*8, count*8);
        dstIndex += count;
        length -= count;
    }
    close(fd);
    Storage::modifiedBlob(dstSymbol);
    return dstSymbol;
}

void loadFromPath(Symbol parentPackage, char* path) {
    NativeNaturalType pathLen = strlen(path);
    if(path[pathLen-1] == '/')
        path[pathLen-1] = 0;
    struct stat s;
    char buffer[64];
    if(stat(path, &s) != 0)
        return;
    if(s.st_mode & S_IFDIR) {
        DIR* dp = opendir(path);
        assert(dp);
        NativeNaturalType slashIndex = 0;
        for(NativeNaturalType i = pathLen-1; i > 0; --i)
            if(path[i] == '/') {
                slashIndex = i+1;
                break;
            }
        Storage::bitwiseCopy(reinterpret_cast<NativeNaturalType*>(buffer),
                             reinterpret_cast<NativeNaturalType*>(path),
                             0, slashIndex*8, (pathLen-slashIndex)*8);
        buffer[pathLen-slashIndex] = 0;
        Symbol package = Ontology::createFromString(const_cast<const char*>(buffer));
        Ontology::blobIndex.insertElement(package);
        if(parentPackage == Ontology::VoidSymbol)
            parentPackage = package;
        Ontology::link({package, Ontology::HoldsSymbol, parentPackage});
        struct dirent* entry;
        while((entry = readdir(dp)))
            if(entry->d_name[0] != '.') {
                sprintf(buffer, "%s/%s", path, entry->d_name);
                loadFromPath(package, buffer);
            }
        closedir(dp);
    } else if(s.st_mode & S_IFREG) {
        if(!Storage::substrEqual<true>(path, ".sym"))
            return;
        Deserializer deserializer;
        deserializer.input = createFromFile(path);
        deserializer.package = parentPackage;
        assert(deserializer.input != Ontology::VoidSymbol);
        if(!deserializer.deserialize()) {
            printf("Exception occurred while deserializing file %s.\n", path);
            exit(2);
        }
    }
}

int sockfd;
struct addrinfo conf, *addressInfo;
Natural8 buffer[128];

#define ifIsCommand(str) \
    if(Storage::substrEqual(reinterpret_cast<Integer8*>(buffer), str))

bool tryRead(Natural8 count) {
    if(recv(sockfd, buffer, count, 0) > 0)
        return true;
    sockfd = -1;
    return false;
}

bool trySend(Natural8 count) {
    if(send(sockfd, buffer, count, 0) > 0)
        return true;
    sockfd = -1;
    return false;
}

Natural64 readNatural() {
    tryRead(1);
    switch(buffer[0]) {
        case 0xCC:
            tryRead(1);
            return buffer[0];
        case 0xCD:
            tryRead(2);
            return swapedEndian(reinterpret_cast<Natural16&>(buffer));
        case 0xCE:
            tryRead(4);
            return swapedEndian(reinterpret_cast<Natural32&>(buffer));
        case 0xCF:
            tryRead(8);
            return swapedEndian(reinterpret_cast<Natural64&>(buffer));
        default:
            if(buffer[0]&0x80) {
                sockfd = -1;
                return 0;
            } else
                return buffer[0];
    }
}

bool sendNatural(Natural64 value) {
    if(value < 0x80) {
        buffer[0] = 0x80|value;
        return trySend(1);
    } else if(value <= 0xFF) {
        buffer[0] = 0xCC;
        if(!trySend(1))
            return false;
        buffer[0] = value;
        return trySend(1);
    } else if(value <= 0xFFFF) {
        buffer[0] = 0xCD;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural16&>(buffer) = swapedEndian(static_cast<Natural16>(value));
        return trySend(2);
    } else if(value <= 0xFFFFFFFF) {
        buffer[0] = 0xCE;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural32&>(buffer) = swapedEndian(static_cast<Natural32>(value));
        return trySend(4);
    } else {
        buffer[0] = 0xCF;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural64&>(buffer) = swapedEndian(static_cast<Natural64>(value));
        return trySend(8);
    }
}

bool sendNil() {
    buffer[0] = 0xC0;
    return trySend(1);
}

bool sendArrayHeader(Natural64 size) {
    if(size <= 0xF) {
        buffer[0] = 0x90|size;
        return trySend(1);
    } else if(size <= 0xFFFF) {
        buffer[0] = 0xDC;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural16&>(buffer) = swapedEndian(static_cast<Natural16>(size));
        return trySend(2);
    } else {
        buffer[0] = 0xDD;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural32&>(buffer) = swapedEndian(static_cast<Natural32>(size));
        return trySend(4);
    }
}

Integer32 main(Integer32 argc, Integer8** argv) {
    if(argc < 2) {
        printf("Expected path argument.\n");
        exit(4);
    }
    loadStorage(argv[1]);
    Ontology::tryToFillPreDefined();

    for(NativeNaturalType i = 2; i < static_cast<NativeNaturalType>(argc); ++i)
        loadFromPath(Ontology::VoidSymbol, argv[i]);

    memset(&conf, 0, sizeof(conf));
    conf.ai_flags = AI_V4MAPPED|AI_PASSIVE;
    conf.ai_family = AF_INET6;
    conf.ai_socktype = SOCK_STREAM;
    if(getaddrinfo("::", "1337", &conf, &addressInfo) < 0) {
        perror("getaddrinfo");
        return 1;
    }
    sockfd = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if(sockfd < 0) {
        perror("socket");
        return 2;
    }
    if(bind(sockfd, addressInfo->ai_addr, addressInfo->ai_addrlen) < 0) {
        perror("bind");
        return 3;
    }
    if(listen(sockfd, 1) < 0) {
        perror("listen");
        return 4;
    }
    freeaddrinfo(addressInfo);
    printf("Listening ...\n");
    struct sockaddr_storage remoteAddr;
    unsigned int addrSize = sizeof(remoteAddr);
    sockfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&remoteAddr), &addrSize);
    assert(sockfd >= 0);
    printf("Connected\n");

    while(sockfd) {
        if(recv(sockfd, buffer, 1, 0) == 0)
            break;
        assert((buffer[0]&0xF0) == 0x90);
        Natural8 parameterCount = buffer[0]&0x0F;
        assert(parameterCount > 0);
        --parameterCount;
        tryRead(1);
        assert((buffer[0]&0xE0) == 0xA0);
        Natural8 commandStrLen = buffer[0]&0x1F;
        tryRead(commandStrLen);
        buffer[commandStrLen] = 0;
        ifIsCommand("createSymbol") {
            assert(parameterCount == 0);
            sendNatural(Storage::createSymbol());
        } else ifIsCommand("releaseSymbol") {
            assert(parameterCount == 1);
            Ontology::unlink(readNatural());
            sendNil();
        } else ifIsCommand("getSize") {
            assert(parameterCount == 1);
            sendNatural(Storage::Blob(readNatural()).getSize());
        } else ifIsCommand("setSize") {
            assert(parameterCount == 2);
            Storage::Blob(readNatural()).setSize(readNatural());
            sendNil();
        } else ifIsCommand("decreaseSize") {
            assert(parameterCount == 3);
            Storage::Blob(readNatural()).decreaseSize(readNatural(), readNatural());
            sendNil();
        } else ifIsCommand("increaseSize") {
            assert(parameterCount == 3);
            Storage::Blob(readNatural()).increaseSize(readNatural(), readNatural());
            sendNil();
        } else ifIsCommand("read") {
            assert(parameterCount == 3);
            Storage::Blob blob(readNatural());
            Natural64 offset = readNatural(), length = readNatural();
            buffer[0] = 0xC6;
            trySend(1);
            reinterpret_cast<Natural32&>(buffer) = swapedEndian((length+7)/8);
            trySend(4);
            while(length > 0) {
                Natural64 segmentLength = min(length, sizeof(buffer)*8ULL);
                blob.externalOperate<false>(buffer, offset, segmentLength);
                offset += segmentLength;
                length -= segmentLength;
                trySend((segmentLength+7)/8);
            }
        } else ifIsCommand("write") {
            assert(parameterCount == 4);
            Storage::Blob blob(readNatural());
            Natural64 offset = readNatural(), length = readNatural(), payloadLength = 0;
            tryRead(1);
            switch(buffer[0]) {
                case 0xC4:
                    tryRead(1);
                    payloadLength = buffer[0];
                    break;
                case 0xC5:
                    tryRead(2);
                    payloadLength = swapedEndian(reinterpret_cast<Natural16&>(buffer));
                    break;
                case 0xC6:
                    tryRead(4);
                    payloadLength = swapedEndian(reinterpret_cast<Natural32&>(buffer));
                    break;
            }
            assert(payloadLength == (length+7)/8);
            while(length > 0) {
                Natural64 segmentLength = min(length, sizeof(buffer)*8ULL);
                tryRead((segmentLength+7)/8);
                blob.externalOperate<true>(buffer, offset, segmentLength);
                offset += segmentLength;
                length -= segmentLength;
            }
            sendNil();
        } /*else ifIsCommand("compare") {
            // TODO
        } else ifIsCommand("slice") {
            // TODO
        } else ifIsCommand("deepCopy") {
            // TODO
        }*/ else ifIsCommand("query") {
            // TODO: Return count only flag
            assert(parameterCount == 4);
            auto mask = readNatural();
            Ontology::Triple triple = {readNatural(), readNatural(), readNatural()};
            Ontology::QueryMode mode[3] = {
                static_cast<Ontology::QueryMode>(mask%3),
                static_cast<Ontology::QueryMode>((mask/3)%3),
                static_cast<Ontology::QueryMode>((mask/9)%3)
            };
            Ontology::BlobVector<true, Symbol> result;
            Ontology::query(static_cast<Ontology::QueryMask>(mask), triple, [&](Ontology::Triple triple) {
                for(NativeNaturalType i = 0; i < 3; ++i)
                    if(mode[i] == Ontology::Varying)
                        result.push_back(triple.pos[i]);
            });
            sendArrayHeader(result.size());
            result.iterate([](Symbol symbol) {
                sendNatural(symbol);
            });
        } else ifIsCommand("link") {
            assert(parameterCount == 3);
            Ontology::link({readNatural(), readNatural(), readNatural()});
            sendNil();
        } else ifIsCommand("unlink") {
            assert(parameterCount == 3);
            Ontology::unlink({readNatural(), readNatural(), readNatural()});
            sendNil();
        } else
            assert(false);
    }

    unloadStorage();
    return 0;
}

}
