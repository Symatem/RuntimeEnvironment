#include "BpTree.hpp"

namespace Storage {
    BpTree<Symbol, NativeNaturalType> blobs;
    Symbol maxSymbol = 0;

    Symbol createSymbol() {
        // TODO: Symbol free pool
        return ++maxSymbol;
    }

    void modifiedBlob(Symbol symbol) {
        // TODO: Improve performance
        // blobIndex.eraseElement(symbol);
    }

    NativeNaturalType accessBlobBits(Symbol symbol) {
        BpTree<Symbol, NativeNaturalType>::Iterator<false> iter;
        assert(blobs.at(iter, symbol));
        return iter.getValue();
    }

    NativeNaturalType getBlobSize(Symbol symbol) {
        BpTree<Symbol, NativeNaturalType>::Iterator<false> iter;
        if(!blobs.at(iter, symbol))
            return 0;
        return *dereferenceBits(iter.getValue()-ArchitectureSize);
    }

    void setBlobSize(Symbol symbol, NativeNaturalType size, NativeNaturalType preserve = 0) {
        BpTree<Symbol, NativeNaturalType>::Iterator<false> iter;
        NativeNaturalType oldBlob, oldBlobSize;
        if(blobs.at(iter, symbol)) {
            oldBlob = iter.getValue();
            oldBlobSize = getBlobSize(symbol);
        } else
            oldBlob = oldBlobSize = 0;
        if(oldBlob && oldBlobSize == size)
            return;

        NativeNaturalType newBlob;
        if(size > 0) {
            newBlob = reinterpret_cast<NativeNaturalType>(malloc((size+2*ArchitectureSize-1)/ArchitectureSize*ArchitectureSize))+sizeof(NativeNaturalType);
            *reinterpret_cast<NativeNaturalType*>(newBlob+size/ArchitectureSize*sizeof(NativeNaturalType)) = 0;
            newBlob = (newBlob-reinterpret_cast<NativeNaturalType>(ptr))*8;
        }

        if(!oldBlob) {
            if(size == 0)
                return;
            blobs.insert(iter, symbol, newBlob);
            assert(blobs.at(iter, symbol));
        } else if(oldBlobSize > 0) {
            NativeNaturalType length = min(oldBlobSize, size, preserve);
            if(length > 0)
                bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(ptr),
                                reinterpret_cast<const NativeNaturalType*>(ptr),
                                newBlob, oldBlob, length);
            free(dereferenceBits(oldBlob-ArchitectureSize));
            if(size == 0) {
                blobs.erase(iter);
                return;
            }
        }

        *dereferenceBits(newBlob-ArchitectureSize) = size;
        iter.setValue(newBlob);
    }

    void setBlobSizePreservingData(Symbol symbol, NativeNaturalType size) {
        setBlobSize(symbol, size, size);
    }

    template <typename DataType>
    DataType readBlobAt(Symbol src, NativeNaturalType srcIndex) {
        return *(dereferenceBits<DataType>(accessBlobBits(src))+srcIndex);
    }

    template <typename DataType>
    DataType readBlob(Symbol src) {
        return readBlobAt<DataType>(src, 0);
    }

    template<typename DataType>
    void writeBlobAt(Symbol dst, NativeNaturalType dstIndex, DataType src) {
        *(dereferenceBits<DataType>(accessBlobBits(dst))+dstIndex) = src;
    }

    template<typename DataType>
    void writeBlob(Symbol dst, DataType src) {
        setBlobSize(dst, sizeof(src)*8);
        writeBlobAt(dst, 0, src);
        modifiedBlob(dst);
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
        return memcmp(dereferenceBits(accessBlobBits(a)), dereferenceBits(accessBlobBits(b)), (sizeA+7)/8);
    }

    bool sliceBlob(Symbol dst, Symbol src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        NativeNaturalType dstSize = getBlobSize(dst),
                          srcSize = getBlobSize(src);
        auto end = dstOffset+length;
        if(end <= dstOffset || end > dstSize)
            return false;
        end = srcOffset+length;
        if(end <= srcOffset || end > srcSize)
            return false;
        bitwiseCopy(reinterpret_cast<NativeNaturalType*>(ptr),
                    reinterpret_cast<const NativeNaturalType*>(ptr),
                    accessBlobBits(dst)+dstOffset, accessBlobBits(src)+srcOffset, length);
        modifiedBlob(dst);
        return true;
    }

    void cloneBlob(Symbol dst, Symbol src) {
        if(dst == src)
            return;
        NativeNaturalType srcSize = getBlobSize(src);
        setBlobSize(dst, srcSize);
        bitwiseCopy(reinterpret_cast<NativeNaturalType*>(ptr),
                    reinterpret_cast<const NativeNaturalType*>(ptr),
                    accessBlobBits(dst), accessBlobBits(src), srcSize);
        modifiedBlob(dst);
    }

    bool eraseFromBlob(Symbol symbol, NativeNaturalType begin, NativeNaturalType end) {
        NativeNaturalType size = getBlobSize(symbol);
        if(begin >= end || end > size)
            return false;
        NativeNaturalType data = accessBlobBits(symbol);
        auto rest = size-end;
        if(rest > 0)
            bitwiseCopy(reinterpret_cast<NativeNaturalType*>(ptr),
                        reinterpret_cast<const NativeNaturalType*>(ptr),
                        data+begin, data+end, rest);
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
        NativeNaturalType data = accessBlobBits(dst);
        if(rest > 0)
            bitwiseCopy(reinterpret_cast<NativeNaturalType*>(ptr),
                        reinterpret_cast<const NativeNaturalType*>(ptr),
                        data+begin+length, data+begin, rest);
        bitwiseCopy(reinterpret_cast<NativeNaturalType*>(ptr),
                    src,
                    data+begin, 0, length);
        modifiedBlob(dst);
        return true;
    }

    void releaseSymbol(Symbol symbol) {
        // TODO: Symbol free pool
        setBlobSize(symbol, 0);
    }
};
