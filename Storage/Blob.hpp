#include "BpTree.hpp"

typedef ArchitectureType Identifier;

namespace Storage {
    Identifier maxIdentifier = 0;
    BpTree<Identifier, ArchitectureType*> blobs;

    Identifier createIdentifier() {
        // TODO: Identifier free pool
        return ++maxIdentifier;
    }

    void modifiedBlob(Identifier identifier) {
        // TODO
        // blobIndex.eraseElement(symbol);
    }

    void setBlobSize(Identifier identifier, ArchitectureType size, ArchitectureType preserve = 0) {
        BpTree<Identifier, ArchitectureType*>::Iterator<false> iter(blobs);
        bool existing = blobs.at(iter, identifier);
        if(size == 0) {
            if(existing) {
                free(iter.getValue());
                blobs.erase(iter);
            }
            return;
        }
        ArchitectureType* newBlob = reinterpret_cast<ArchitectureType*>(malloc((size+2*ArchitectureSize-1)/ArchitectureSize*ArchitectureSize));
        if(existing) {
            ArchitectureType* oldBlob = iter.getValue();
            if(*oldBlob > 0) {
                ArchitectureType length = min(*oldBlob, size, preserve);
                if(length > 0)
                    bitwiseCopy<-1>(newBlob+1, oldBlob+1, 0, 0, length);
                free(oldBlob);
            }
        } else {
            assert(size > 0);
            blobs.insert(iter, identifier, newBlob);
            iter.end = blobs.layerCount; // TODO
            assert(blobs.at(iter, identifier));
        }
        if(size%ArchitectureSize > 0)
            newBlob[size/ArchitectureSize+1] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        *newBlob = size;
        iter.setValue(newBlob);
    }

    void setBlobSizePreservingData(Identifier identifier, ArchitectureType size) {
        setBlobSize(identifier, size, size);
    }

    ArchitectureType getBlobSize(Identifier identifier) {
        BpTree<Identifier, ArchitectureType*>::Iterator<false> iter(blobs);
        return (blobs.at(iter, identifier)) ? *iter.getValue() : 0;
    }

    ArchitectureType* accessBlobData(Identifier identifier) {
        BpTree<Identifier, ArchitectureType*>::Iterator<false> iter(blobs);
        assert(blobs.at(iter, identifier));
        return iter.getValue()+1;
    }

    int compareBlobs(Identifier a, Identifier b) {
        if(a == b)
            return 0;
        ArchitectureType sizeA = getBlobSize(a),
                         sizeB = getBlobSize(b);
        if(sizeA < sizeB)
            return -1;
        if(sizeA > sizeB)
            return 1;
        if(sizeA == 0)
            return 0;
        return memcmp(accessBlobData(a), accessBlobData(b), (sizeA+7)/8);
    }

    bool overwriteBlobPartial(Identifier dst, Identifier src, ArchitectureType dstOffset, ArchitectureType srcOffset, ArchitectureType length) {
        ArchitectureType dstSize = getBlobSize(dst),
                         srcSize = getBlobSize(src);
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
        if(dst == src)
            return;
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

    void releaseIdentifier(Identifier identifier) {
        // TODO: Identifier free pool
        setBlobSize(identifier, 0);
    }
};
