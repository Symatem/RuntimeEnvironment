#include <Targets/POSIX.hpp>

extern "C" {

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

bool readBoolean() {
    tryRead(1);
    return buffer[0] == 0xC3;
}

NativeNaturalType readNatural() {
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

bool sendNatural(NativeNaturalType value) {
    if(value < 0x80) {
        buffer[0] = value;
        return trySend(1);
    } else if(value <= 0xFF) {
        buffer[0] = 0xCC;
        buffer[1] = value;
        return trySend(2);
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
    buffer[0] = 0x90; // 0xC0 TODO: Workaround
    return trySend(1);
}

bool sendBinaryHeader(NativeNaturalType size) {
    if(size <= 0xFF) {
        buffer[0] = 0xC4;
        buffer[1] = size;
        return trySend(2);
    } else if(size <= 0xFFFF) {
        buffer[0] = 0xC5;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural16&>(buffer) = swapedEndian(static_cast<Natural16>(size));
        return trySend(2);
    } else {
        buffer[0] = 0xC6;
        if(!trySend(1))
            return false;
        reinterpret_cast<Natural32&>(buffer) = swapedEndian(static_cast<Natural32>(size));
        return trySend(4);
    }
}

bool sendArrayHeader(NativeNaturalType size) {
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
    const Integer8 *port = "1337", *path = "/dev/zero";

    for(Integer32 i = 1; i < argc-1; ++i) {
        if(Storage::substrEqual(argv[i], "--port"))
            port = argv[i+1];
        else if(Storage::substrEqual(argv[i], "--path"))
            path = argv[i+1];
    }

    loadStorage(path);
    Ontology::tryToFillPreDefined();

    memset(&conf, 0, sizeof(conf));
    conf.ai_flags = AI_V4MAPPED|AI_PASSIVE;
    conf.ai_family = AF_INET6;
    conf.ai_socktype = SOCK_STREAM;
    if(getaddrinfo("::", port, &conf, &addressInfo) < 0) {
        perror("getaddrinfo");
        return 2;
    }
    sockfd = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if(sockfd < 0) {
        perror("socket");
        return 3;
    }
    if(bind(sockfd, addressInfo->ai_addr, addressInfo->ai_addrlen) < 0) {
        perror("bind");
        return 4;
    }
    if(listen(sockfd, 1) < 0) {
        perror("listen");
        return 5;
    }
    freeaddrinfo(addressInfo);
    printf("Listening ...\n");
    struct sockaddr_storage remoteAddr;
    unsigned int addrSize = sizeof(remoteAddr);
    sockfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&remoteAddr), &addrSize);
    assert(sockfd >= 0);
    Natural32 flag = 1;
    assert(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<Integer8*>(&flag), sizeof(Natural32)) >= 0);
    assert(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<Integer8*>(&flag), sizeof(Natural32)) >= 0);
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
        } else ifIsCommand("getBlobSize") {
            assert(parameterCount == 1);
            sendNatural(Storage::Blob(readNatural()).getSize());
        } else ifIsCommand("setBlobSize") {
            assert(parameterCount == 2);
            Storage::Blob(readNatural()).setSize(readNatural());
            sendNil();
        } else ifIsCommand("decreaseBlobSize") {
            assert(parameterCount == 3);
            Storage::Blob(readNatural()).decreaseSize(readNatural(), readNatural());
            sendNil();
        } else ifIsCommand("increaseBlobSize") {
            assert(parameterCount == 3);
            Storage::Blob(readNatural()).increaseSize(readNatural(), readNatural());
            sendNil();
        } else ifIsCommand("readBlob") {
            assert(parameterCount >= 1 && parameterCount <= 3);
            Storage::Blob blob(readNatural());
            Natural64 offset = (parameterCount >= 2) ? readNatural() : 0,
                      length = (parameterCount >= 3) ? readNatural() : blob.getSize();
            sendBinaryHeader((length+7)/8);
            while(length > 0) {
                Natural64 segmentLength = min(length, sizeof(buffer)*8ULL);
                blob.externalOperate<false>(buffer, offset, segmentLength);
                offset += segmentLength;
                length -= segmentLength;
                trySend((segmentLength+7)/8);
            }
        } else ifIsCommand("writeBlob") {
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
        } else ifIsCommand("deserializeBlob") {
            assert(parameterCount == 1 || parameterCount == 2);
            Deserializer deserializer;
            deserializer.input = readNatural();
            deserializer.package = (parameterCount >= 2) ? readNatural() : Ontology::VoidSymbol;
            Symbol exception = deserializer.deserialize();
            if(exception == Ontology::VoidSymbol) {
                sendArrayHeader(deserializer.queue.size());
                deserializer.queue.iterate([](Symbol symbol) {
                    sendNatural(symbol);
                });
            } else
                sendNatural(exception);
        } /*else ifIsCommand("compare") {
            // TODO
        } else ifIsCommand("slice") {
            // TODO
        } else ifIsCommand("deepCopy") {
            // TODO
        }*/ else ifIsCommand("query") {
            assert(parameterCount == 5);
            bool countOnly = readBoolean();
            auto mask = readNatural();
            Ontology::Triple triple = {readNatural(), readNatural(), readNatural()};
            Ontology::QueryMode mode[3] = {
                static_cast<Ontology::QueryMode>(mask%3),
                static_cast<Ontology::QueryMode>((mask/3)%3),
                static_cast<Ontology::QueryMode>((mask/9)%3)
            };
            Ontology::BlobVector<true, Symbol> result;
            auto count = Ontology::query(static_cast<Ontology::QueryMask>(mask), triple, [&](Ontology::Triple triple) {
                for(NativeNaturalType i = 0; i < 3; ++i)
                    if(mode[i] == Ontology::Varying)
                        result.push_back(triple.pos[i]);
            });
            if(countOnly)
                sendNatural(count);
            else {
                sendArrayHeader(result.size());
                result.iterate([](Symbol symbol) {
                    sendNatural(symbol);
                });
            }
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
