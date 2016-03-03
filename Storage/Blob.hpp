#include "BpTree.hpp"

typedef ArchitectureType Identifier;

namespace Storage {
    Identifier nextIdentifier = 0;
    std::map<Identifier, ArchitectureType*> blobs; // TODO: Use B+Tree instead
    std::map<Identifier, Identifier[6]> identifiers; // TODO: Use B+Tree instead

    Identifier createIdentifier() {
        // TODO
        return nextIdentifier++;
    }

    void modifiedBlob(Identifier identifier) {
        // TODO
        // blobIndex.eraseElement(symbol);
    }

    void setBlobSize(Identifier identifier, ArchitectureType size, ArchitectureType preserve = 0) {
        auto iter = blobs.find(identifier);
        if(size == 0) {
            assert(iter != blobs.end());
            free(iter->second);
            blobs.erase(iter);
            return;
        }
        ArchitectureType* newBlob = reinterpret_cast<ArchitectureType*>(malloc((size+2*ArchitectureSize-1)/ArchitectureSize*ArchitectureSize));
        if(size%ArchitectureSize > 0)
            newBlob[size/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        if(iter == blobs.end()) {
            assert(size > 0);
            iter = blobs.insert({identifier, newBlob}).first;
        } else if(*iter->second > 0) {
            ArchitectureType length = min(*iter->second, size, preserve);
            if(length > 0)
                bitwiseCopy<-1>(newBlob+1, iter->second+1, 0, 0, length);
            free(iter->second);
        }
        *newBlob = size;
        iter->second = newBlob;
    }

    void setBlobSizePreservingData(Identifier identifier, ArchitectureType size) {
        setBlobSize(identifier, size, size);
    }

    ArchitectureType getBlobSize(Identifier identifier) {
        auto iter = blobs.find(identifier);
        return (iter == blobs.end()) ? 0 : *iter->second;
    }

    ArchitectureType* accessBlobData(Identifier identifier) {
        auto iter = blobs.find(identifier);
        return (iter == blobs.end()) ? NULL : iter->second+1;
    }

    int compareBlobs(Identifier a, Identifier b) {
        ArchitectureType sizeA = getBlobSize(a),
                         sizeB = getBlobSize(b);
        if(sizeA < sizeB)
            return -1;
        if(sizeA > sizeB)
            return 1;
        return memcmp(accessBlobData(a), accessBlobData(b), (sizeA+7)/8);
    }

    bool overwriteBlobPartial(Identifier dst, Identifier src, ArchitectureType dstOffset, ArchitectureType srcOffset, ArchitectureType length) {
        ArchitectureType dstSize = getBlobSize(dst);
        ArchitectureType srcSize = getBlobSize(src);
        auto end = dstOffset+length;
        if(end <= dstOffset || end > dstSize)
            return false;
        end = srcOffset+length;
        if(end <= srcOffset || end > srcSize)
            return false;
        bitwiseCopy(accessBlobData(dst), accessBlobData(src), dstOffset, srcOffset, length);
        modifiedBlob(dst);
        return true;
    }

    void cloneBlob(Identifier dst, Identifier src) {
        ArchitectureType srcSize = getBlobSize(src);
        setBlobSize(dst, srcSize);
        bitwiseCopy(accessBlobData(dst), accessBlobData(src), 0, 0, srcSize);
        modifiedBlob(dst);
    }

    template<typename DataType>
    void overwriteBlob(Identifier dst, DataType src) {
        setBlobSize(dst, sizeof(src)*8);
        *reinterpret_cast<DataType*>(accessBlobData(dst)) = src;
        modifiedBlob(dst);
    }

    bool eraseFromBlob(Identifier identifier, ArchitectureType begin, ArchitectureType end) {
        ArchitectureType size = getBlobSize(identifier);
        if(begin >= end || end > size)
            return false;
        ArchitectureType* data = accessBlobData(identifier);
        auto rest = size-end;
        if(rest > 0)
            bitwiseCopy(data, data, begin, end, rest);
        setBlobSizePreservingData(identifier, rest+begin);
        modifiedBlob(identifier);
        return true;
    }

    bool insertIntoBlob(Identifier dst, ArchitectureType* src, ArchitectureType begin, ArchitectureType length) {
        assert(length > 0);
        ArchitectureType dstSize = getBlobSize(dst);
        auto newBlobSize = dstSize+length, rest = dstSize-begin;
        if(dstSize >= newBlobSize || begin > dstSize)
            return false;
        setBlobSizePreservingData(dst, newBlobSize);
        ArchitectureType* dstData = accessBlobData(dst);
        if(rest > 0)
            bitwiseCopy(dstData, dstData, begin+length, begin, rest);
        bitwiseCopy(dstData, src, begin, 0, length);
        modifiedBlob(dst);
        return true;
    }

    void destroyIdentifier(Identifier identifier) {
        // TODO
        setBlobSize(identifier, 0);
    }
};
