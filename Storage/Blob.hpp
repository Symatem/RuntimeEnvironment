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
    Symbol symbol;
    PageRefType pageRef;
    NativeNaturalType address, offset, index;
    Natural16 type;
    BpTreeBlob bpTree;
    BlobBucket* bucket;
    enum State {
        Empty,
        InBucket,
        Fragmented
    } state;

    Blob() {}

    Blob(Symbol _symbol) {
        symbol = _symbol;
        BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
        if(!blobs.find<Key>(iter, symbol)) {
            state = Empty;
            return;
        }
        address = iter.getValue();
        pageRef = address/bitsPerPage;
        offset = address-pageRef*bitsPerPage;
        if(offset) {
            state = InBucket;
            bucket = dereferencePage<BlobBucket>(pageRef);
            index = bucket->indexOfOffset(offset);
        } else {
            state = Fragmented;
            bpTree.rootPageRef = pageRef;
        }
    }

    NativeNaturalType getSize() {
        switch(state) {
            case Empty:
                return 0;
            case InBucket:
                return bpTree.getElementCount();
            case Fragmented:
                return bucket->getSize(index);
        }
    }

    template<NativeIntegerType dir>
    static NativeIntegerType segmentInteroperation(NativeNaturalType dst, NativeNaturalType src, NativeNaturalType length) {
        if(dir == 0)
            return bitwiseCompare(reinterpret_cast<NativeNaturalType*>(heapBegin),
                                  reinterpret_cast<const NativeNaturalType*>(heapBegin),
                                  dst, src, length);
        else {
            bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(heapBegin),
                             reinterpret_cast<const NativeNaturalType*>(heapBegin),
                             dst, src, length);
            return 0;
        }
    }

    template<NativeIntegerType dir, typename IteratorType>
    NativeNaturalType preInteroperation(IteratorType& iter, NativeNaturalType offset) {
        if(dir == 1) {
            if(state == Fragmented) {
                if(iter[0]->index > 0)
                    return iter[0]->index;
                iter.template advance<-1>(1);
                iter[0]->index = iter[0]->endIndex;
                return iter[0]->index;
            } else
                return offset;
        } else
            return (state == Fragmented) ? iter[0]->endIndex-iter[0]->index : bucket->getSize(index)-offset;
    }

    template<typename IteratorType>
    NativeNaturalType offsetOfInteroperation(IteratorType& iter, NativeNaturalType offset) {
        return (state == Fragmented) ? iter[0]->pageRef*bitsPerPage+BpTreeBlob::Page::valueOffset+iter[0]->index : address+offset;
    }

    template<NativeIntegerType dir, typename IteratorType>
    void postInteroperation(IteratorType& iter, NativeNaturalType& offset, NativeNaturalType intersection) {
        if(dir == 1) {
            if(state == Fragmented) {
                assert(iter.template advance<1>(0, intersection) == 0);
            } else
                offset -= intersection;
        } else {
            if(state == Fragmented) {
                assert(iter.template advance<-1>(0, intersection) == 0);
            } else
                offset += intersection;
        }
    }

    template<NativeIntegerType dir>
    NativeIntegerType interoperation(Blob& src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        // TODO: Check out of bounds
        NativeNaturalType dstSegment, srcSegment, intersection;
        BpTreeBlob::Iterator<dir != 0> dstIter, srcIter;
        if(state == Fragmented)
            bpTree.find<Rank>(dstIter, dstOffset);
        if(src.state == Fragmented)
            src.bpTree.find<Rank>(srcIter, srcOffset);
        if(dir == 1) {
            dstOffset += length;
            srcOffset += length;
        }
        while(length > 0) {
            dstSegment = preInteroperation<dir>(dstIter, dstOffset);
            srcSegment = src.preInteroperation<dir>(srcIter, srcOffset);
            intersection = min(dstSegment, srcSegment, length);
            NativeIntegerType result = segmentInteroperation<dir>(offsetOfInteroperation(dstIter, dstOffset),
                                                                  src.offsetOfInteroperation(srcIter, srcOffset),
                                                                  intersection);
            if(result != 0)
                return result;
            postInteroperation<dir>(dstIter, dstOffset, intersection);
            src.postInteroperation<dir>(srcIter, srcOffset, intersection);
            length -= intersection;
        }
        return 0;
    }

    NativeIntegerType compare(Blob& other) {
        if(symbol == other.symbol)
            return 0;
        NativeNaturalType size = getSize(), otherSize = other.getSize();
        if(size < otherSize)
            return -1;
        if(size > otherSize)
            return 1;
        if(size == 0)
            return 0;
        return interoperation<0>(other, 0, 0, size);
    }

    bool sliceBlob(Blob& src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        if(symbol == src.symbol && dstOffset == srcOffset)
            return false;
        if(dstOffset <= srcOffset) {
            if(!interoperation<-1>(src, dstOffset, srcOffset, length))
                return false;
        } else {
            if(!interoperation<1>(src, dstOffset, srcOffset, length))
                return false;
        }
        modifiedBlob(symbol);
        return true;
    }

    void allocateInBucket(NativeNaturalType size) {
        assert(size > 0);
        state = InBucket;
        if(freeBlobBuckets[type].empty()) {
            pageRef = aquirePage();
            bucket = dereferencePage<BlobBucket>(pageRef);
            bucket->init(type);
            assert(freeBlobBuckets[type].insert(pageRef));
        } else {
            pageRef = freeBlobBuckets[type].pullOneOut<First>();
            bucket = dereferencePage<BlobBucket>(pageRef);
        }
        index = bucket->allocateIndex(size, symbol, pageRef);
        address = pageRef*bitsPerPage+bucket->offsetOfIndex(index);
    }

    void freeFromBucket() {
        assert(state == InBucket);
        bucket->freeIndex(index, pageRef);
    }

    void updateAddress(NativeNaturalType address) {
        BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
        blobs.find<Key>(iter, symbol);
        iter.setValue(address);
    }

    bool decreaseBlobSize(NativeNaturalType at, NativeNaturalType count) {
        NativeNaturalType size = getSize(), end = at+count;
        if(at >= end || end > size)
            return false;
        size -= count;
        if(size == 0) {
            if(state == Fragmented)
                bpTree.erase();
            else
                freeFromBucket();
            blobs.erase<Key>(symbol);
        } else if(BlobBucket::isBucketAllocatable(size)) {
            Blob dstBlob;
            dstBlob.type = BlobBucket::getType(size);
            if(state == Fragmented || type != dstBlob.type) {
                dstBlob.allocateInBucket(size);
                dstBlob.interoperation<1>(*this, 0, 0, at);
                dstBlob.interoperation<1>(*this, at, end, size-at);
                if(state == InBucket)
                    freeFromBucket();
                updateAddress(dstBlob.address);
            } else
                interoperation<-1>(*this, at, end, size-at);
        } else {
            BpTreeBlob::Iterator<true> from, to;
            bpTree.find<Rank>(from, at);
            bpTree.find<Rank>(to, at+count-1);
            bpTree.erase(from, to);
            updateAddress(bpTree.rootPageRef);
        }
        modifiedBlob(symbol);
        return true;
    }

    bool increaseBlobSize(NativeNaturalType at, NativeNaturalType count) {
        NativeNaturalType size = getSize();
        if(size >= size+count || at > size)
            return false;
        size += count;
        Blob dstBlob;
        if(BlobBucket::isBucketAllocatable(size)) {
            dstBlob.type = BlobBucket::getType(size);
            if(state == Empty || type != dstBlob.type)
                dstBlob.allocateInBucket(size);
            else
                interoperation<-1>(*this, at, at+count, size-count-at);
        } else {
            BpTreeBlob::Iterator<true> iter;
            dstBlob.state = Fragmented;
            dstBlob.bpTree = bpTree;
            dstBlob.bpTree.find<Rank>(iter, at);
            dstBlob.bpTree.insert(iter, count, nullptr);
            dstBlob.address = dstBlob.bpTree.rootPageRef;
        }
        if(state == InBucket) {
            dstBlob.interoperation<1>(*this, 0, 0, at);
            dstBlob.interoperation<1>(*this, at, at+count, size-count-at);
            freeFromBucket();
        }
        if(state == Empty)
            blobs.insert(symbol, dstBlob.address);
        else
            updateAddress(dstBlob.address);
        modifiedBlob(symbol);
        return true;
    }

    void setSize(NativeNaturalType newSize) {
        NativeNaturalType oldSize = getSize();
        if(oldSize < newSize)
            increaseBlobSize(oldSize, newSize-oldSize);
        else if(oldSize > newSize)
            decreaseBlobSize(newSize, oldSize-newSize);
    }

    void deepCopy(Blob& src) {
        if(symbol == src.symbol)
            return;
        NativeNaturalType srcSize = src.getSize();
        setSize(srcSize);
        interoperation<1>(src, 0, 0, srcSize);
        modifiedBlob(symbol);
    }

    template <typename DataType>
    DataType readBlobAt(Symbol src, NativeNaturalType srcIndex) {
        // TODO
    }

    template <typename DataType>
    DataType readBlob(Symbol src) {
        return readBlobAt<DataType>(src, 0);
    }

    template<typename DataType>
    void writeBlobAt(Symbol dst, NativeNaturalType dstIndex, DataType src) {
        // TODO
    }

    template<typename DataType>
    void writeBlob(Symbol dst, DataType src) {
        setSize(dst, sizeOfInBits<DataType>::value);
        writeBlobAt(dst, 0, src);
        modifiedBlob(dst);
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
