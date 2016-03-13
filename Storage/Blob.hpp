#include "BpTree.hpp"

typedef NativeNaturalType Symbol;

namespace Storage {
    struct Blob {
        NativeNaturalType size;
        NativeNaturalType data[0];
    };
    BpTree<Symbol, Blob*> blobs;
    Symbol maxSymbol = 0;

    Symbol createSymbol() {
        // TODO: Symbol free pool
        return ++maxSymbol;
    }

    void modifiedBlob(Symbol symbol) {
        // TODO: Improve performance
        // blobIndex.eraseElement(symbol);
    }

    void setBlobSize(Symbol symbol, NativeNaturalType size, NativeNaturalType preserve = 0) {
        BpTree<Symbol, Blob*>::Iterator<false> iter;
        bool existing = blobs.at(iter, symbol);
        if(size == 0) {
            if(existing) {
                free(iter.getValue());
                blobs.erase(iter);
            }
            return;
        }
        Blob* newBlob = reinterpret_cast<Blob*>(malloc((size+2*ArchitectureSize-1)/ArchitectureSize*ArchitectureSize));
        if(existing) {
            Blob* oldBlob = iter.getValue();
            if(oldBlob->size > 0) {
                NativeNaturalType length = min(oldBlob->size, size, preserve);
                if(length > 0)
                    bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&newBlob->data),
                                    reinterpret_cast<NativeNaturalType*>(&oldBlob->data),
                                    0, 0, length);
                free(oldBlob);
            }
        } else {
            blobs.insert(iter, symbol, newBlob);
            assert(blobs.at(iter, symbol));
        }
        newBlob->size = size;
        if(size%ArchitectureSize > 0)
            newBlob->data[size/ArchitectureSize] &= BitMask<NativeNaturalType>::fillLSBs(size%ArchitectureSize);
        iter.setValue(newBlob);
    }

    void setBlobSizePreservingData(Symbol symbol, NativeNaturalType size) {
        setBlobSize(symbol, size, size);
    }

    NativeNaturalType getBlobSize(Symbol symbol) {
        BpTree<Symbol, Blob*>::Iterator<false> iter;
        return (blobs.at(iter, symbol)) ? iter.getValue()->size : 0;
    }

    NativeNaturalType* accessBlobData(Symbol symbol) {
        BpTree<Symbol, Blob*>::Iterator<false> iter;
        assert(blobs.at(iter, symbol));
        return reinterpret_cast<NativeNaturalType*>(&iter.getValue()->data);
    }

    int compareBlobs(Symbol a, Symbol b) {
        if(a == b)
            return 0;
        NativeNaturalType sizeA = getBlobSize(a),
                         sizeB = getBlobSize(b);
        if(sizeA < sizeB)
            return -1;
        if(sizeA > sizeB)
            return 1;
        if(sizeA == 0)
            return 0;
        return memcmp(accessBlobData(a), accessBlobData(b), (sizeA+7)/8);
    }

    bool overwriteBlobPartial(Symbol dst, Symbol src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        NativeNaturalType dstSize = getBlobSize(dst),
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

    void cloneBlob(Symbol dst, Symbol src) {
        if(dst == src)
            return;
        NativeNaturalType srcSize = getBlobSize(src);
        setBlobSize(dst, srcSize);
        bitwiseCopy(accessBlobData(dst), accessBlobData(src), 0, 0, srcSize);
        modifiedBlob(dst);
    }

    template<typename DataType>
    void overwriteBlob(Symbol dst, DataType src) {
        setBlobSize(dst, sizeof(src)*8);
        *reinterpret_cast<DataType*>(accessBlobData(dst)) = src;
        modifiedBlob(dst);
    }

    bool eraseFromBlob(Symbol symbol, NativeNaturalType begin, NativeNaturalType end) {
        NativeNaturalType size = getBlobSize(symbol);
        if(begin >= end || end > size)
            return false;
        NativeNaturalType* data = accessBlobData(symbol);
        auto rest = size-end;
        if(rest > 0)
            bitwiseCopy(data, data, begin, end, rest);
        setBlobSizePreservingData(symbol, rest+begin);
        modifiedBlob(symbol);
        return true;
    }

    bool insertIntoBlob(Symbol dst, const NativeNaturalType* src, NativeNaturalType begin, NativeNaturalType length) {
        assert(length > 0);
        NativeNaturalType dstSize = getBlobSize(dst);
        auto newBlobSize = dstSize+length, rest = dstSize-begin;
        if(dstSize >= newBlobSize || begin > dstSize)
            return false;
        setBlobSizePreservingData(dst, newBlobSize);
        NativeNaturalType* dstData = accessBlobData(dst);
        if(rest > 0)
            bitwiseCopy(dstData, dstData, begin+length, begin, rest);
        bitwiseCopy(dstData, src, begin, 0, length);
        modifiedBlob(dst);
        return true;
    }

    void releaseSymbol(Symbol symbol) {
        // TODO: Symbol free pool
        setBlobSize(symbol, 0);
    }
};
