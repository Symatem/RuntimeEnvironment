#include "BpTree.hpp"

namespace Storage {
    BpTree<Symbol, NativeNaturalType> blobs;
    BpTree<Symbol> freeSymbols; // TODO: Fix scrutinizeExistence
    Symbol symbolCount = 0;

    void updateStats() {
        // usage.wilderness = 0;
        usage.uninhabitable = 0;
        usage.totalMetaData = bitsPerPage;
        usage.inhabitedMetaData = sizeof(SuperPage)*8;
        usage.totalBlobData = 0;
        usage.inhabitedBlobData = 0;
        blobs.updateStats();
    }

    Symbol createSymbol() {
        /*if(freeSymbols.elementCount)
            return freeSymbols.getAndEraseFromSet();
        else */
            return symbolCount++;
    }

    void modifiedBlob(Symbol symbol) {
        // TODO: Improve performance
        // blobIndex.eraseElement(symbol);
    }

    NativeNaturalType accessBlobData(Symbol symbol) {
        BpTree<Symbol, NativeNaturalType>::Iterator<false> iter;
        assert(blobs.find(iter, symbol));
        return iter.getValue();
    }

    NativeNaturalType getBlobSize(Symbol symbol) {
        BpTree<Symbol, NativeNaturalType>::Iterator<false> iter;
        if(!blobs.find(iter, symbol))
            return 0;
        return *dereferenceBits(iter.getValue()-ArchitectureSize);
    }

    void setBlobSize(Symbol symbol, NativeNaturalType size, NativeNaturalType preserve = 0) {
        BpTree<Symbol, NativeNaturalType>::Iterator<true> iter;
        NativeNaturalType oldBlob, oldBlobSize;
        if(blobs.find(iter, symbol)) {
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
            assert(blobs.find(iter, symbol));
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
        return *(dereferenceBits<DataType>(accessBlobData(src))+srcIndex);
    }

    template <typename DataType>
    DataType readBlob(Symbol src) {
        return readBlobAt<DataType>(src, 0);
    }

    template<typename DataType>
    void writeBlobAt(Symbol dst, NativeNaturalType dstIndex, DataType src) {
        *(dereferenceBits<DataType>(accessBlobData(dst))+dstIndex) = src;
    }

    template<typename DataType>
    void writeBlob(Symbol dst, DataType src) {
        setBlobSize(dst, sizeof(src)*8);
        writeBlobAt(dst, 0, src);
        modifiedBlob(dst);
    }

    NativeIntegerType compareBlobs(Symbol a, Symbol b) {
        if(a == b)
            return 0;
        NativeNaturalType sizeA = getBlobSize(a), sizeB = getBlobSize(b);
        if(sizeA < sizeB)
            return -1;
        if(sizeA > sizeB)
            return 1;
        if(sizeA == 0)
            return 0;
        return bitwiseCompare(reinterpret_cast<const NativeNaturalType*>(ptr),
                              reinterpret_cast<const NativeNaturalType*>(ptr),
                              accessBlobData(a), accessBlobData(b), sizeA);
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
                    accessBlobData(dst)+dstOffset, accessBlobData(src)+srcOffset, length);
        modifiedBlob(dst);
        return true;
    }

    void cloneBlob(Symbol dst, Symbol src) {
        if(dst == src)
            return;
        NativeNaturalType srcSize = getBlobSize(src);
        setBlobSize(dst, srcSize);
        sliceBlob(dst, src, 0, 0, srcSize);
        modifiedBlob(dst);
    }

    bool decreaseBlobSize(Symbol symbol, NativeNaturalType at, NativeNaturalType count) {
        NativeNaturalType size = getBlobSize(symbol), end = at+count;
        if(at >= end || end > size)
            return false;
        auto rest = size-end;
        if(rest > 0)
            sliceBlob(symbol, symbol, at, end, rest);
        setBlobSizePreservingData(symbol, at+rest);
        modifiedBlob(symbol);
        return true;
    }

    bool increaseBlobSize(Symbol symbol, NativeNaturalType at, NativeNaturalType count) {
        assert(count > 0);
        NativeNaturalType size = getBlobSize(symbol);
        auto newBlobSize = size+count, rest = size-at;
        if(size >= newBlobSize || at > size)
            return false;
        setBlobSizePreservingData(symbol, newBlobSize);
        if(rest > 0)
            sliceBlob(symbol, symbol, at+count, at, rest);
        return true;
    }

    void releaseSymbol(Symbol symbol) {
        setBlobSize(symbol, 0);
        /*if(symbol == symbolCount-1)
            --symbolCount;
        else
            freeSymbols.insert(symbol);*/
    }
};
