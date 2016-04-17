#include "BlobBucket.hpp"

namespace Storage {

BpTreeMap<Symbol, NativeNaturalType> blobs;
// BpTreeSet<Symbol> freeSymbols; // TODO: Fix scrutinizeExistence
Symbol symbolCount = 0;

Symbol createSymbol() {
    /*if(freeSymbols.elementCount)
        return freeSymbols.pullOneOut<First>();
    else */
        return symbolCount++;
}

void modifiedBlob(Symbol symbol) {
    // TODO: Improve performance
    // blobIndex.eraseElement(symbol);
}

struct Blob {
    BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
    bool fragmented;
    PageRefType pageRef;
    NativeNaturalType offset;
    BpTreeBlob bpTree;
    BlobBucket* bucket;
    NativeNaturalType index;

    bool categorize(Symbol symbol) {
        if(!blobs.find<Key>(iter, symbol))
            return false;
        NativeNaturalType address = iter.getValue();
        PageRefType pageRef = address/bitsPerPage;
        NativeNaturalType offset = address-pageRef*bitsPerPage;
        if(offset) {
            fragmented = false;
            bucket = dereferencePage<BlobBucket>(pageRef);
            index = bucket->indexOfOffset(offset);
        } else {
            fragmented = true;
            bpTree.rootPageRef = pageRef;
        }
        return true;
    }

    NativeNaturalType allocate(NativeNaturalType size) {
        NativeNaturalType type = BlobBucket::getBucketType(size);
        if(type < blobBucketTypeCount) {
            fragmented = false;
            if(freeBlobBuckets[type].empty()) {
                pageRef = aquirePage();
                bucket = dereferencePage<BlobBucket>(pageRef);
                bucket->init(type);
                assert(freeBlobBuckets[type].insert(pageRef));
            } else {
                pageRef = freeBlobBuckets[type].pullOneOut<First>();
                bucket = dereferencePage<BlobBucket>(pageRef);
            }
            index = bucket->allocateIndex(size, iter.getKey(), pageRef);
            return pageRef*bitsPerPage+bucket->offsetOfIndex(index);
        } else {
            fragmented = true;
            // TODO: Allocate tree
            return bpTree.rootPageRef;
        }
    }

    NativeNaturalType getSize() {
        return (fragmented) ? bpTree.getElementCount() : bucket->getSize(index);
    }
};

NativeNaturalType accessBlobData(Symbol symbol) {
    BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter;
    assert(blobs.find<Key>(iter, symbol));
    return iter.getValue();
}

NativeNaturalType getBlobSize(Symbol symbol) {
    BpTreeMap<Symbol, NativeNaturalType>::Iterator<false> iter;
    if(!blobs.find<Key>(iter, symbol))
        return 0;
    return *dereferenceBits(iter.getValue()-architectureSize);
}

void setBlobSize(Symbol symbol, NativeNaturalType size, NativeNaturalType preserve = 0) {
    BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
    NativeNaturalType oldBlob, oldBlobSize;
    if(blobs.find<Key>(iter, symbol)) {
        oldBlob = iter.getValue();
        oldBlobSize = getBlobSize(symbol);
    } else
        oldBlob = oldBlobSize = 0;
    if(oldBlob && oldBlobSize == size)
        return;
    NativeNaturalType newBlob;
    if(size > 0) {
        newBlob = pointerToNatural(malloc((size+2*architectureSize-1)/architectureSize*architectureSize))+sizeof(NativeNaturalType);
        *reinterpret_cast<NativeNaturalType*>(newBlob+size/architectureSize*sizeof(NativeNaturalType)) = 0;
        newBlob = (newBlob-pointerToNatural(heapBegin))*8;
    }
    if(!oldBlob) {
        if(size == 0)
            return;
        blobs.insert(iter, symbol, newBlob);
        assert(blobs.find<Key>(iter, symbol));
    } else if(oldBlobSize > 0) {
        NativeNaturalType length = min(oldBlobSize, size, preserve);
        if(length > 0)
            bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(heapBegin),
                            reinterpret_cast<const NativeNaturalType*>(heapBegin),
                            newBlob, oldBlob, length);
        free(dereferenceBits(oldBlob-architectureSize));
        if(size == 0) {
            blobs.erase(iter);
            return;
        }
    }
    *dereferenceBits(newBlob-architectureSize) = size;
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
    setBlobSize(dst, sizeOfInBits<DataType>::value);
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
    return bitwiseCompare(reinterpret_cast<const NativeNaturalType*>(heapBegin),
                          reinterpret_cast<const NativeNaturalType*>(heapBegin),
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
    bitwiseCopy(reinterpret_cast<NativeNaturalType*>(heapBegin),
                reinterpret_cast<const NativeNaturalType*>(heapBegin),
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
