#include "BlobBucket.hpp"

namespace Storage {

Symbol createSymbol() {
    /* TODO: Fix scrutinizeExistence
    if(freeSymbols.elementCount)
        return superPage->freeSymbols.getOne<First, true>();
    else */
        return superPage->symbolCount++;
}

void modifiedBlob(Symbol symbol) {
    // TODO: Improve performance
    // blobIndex.eraseElement(symbol);
}

struct Blob {
    Symbol symbol;
    PageRefType pageRef;
    NativeNaturalType address, offsetInBucket, indexInBucket;
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
        if(!superPage->blobs.find<Key>(iter, symbol)) {
            state = Empty;
            return;
        }
        address = iter.getValue();
        pageRef = address/bitsPerPage;
        offsetInBucket = address-pageRef*bitsPerPage;
        if(offsetInBucket > 0) {
            state = InBucket;
            bucket = dereferencePage<BlobBucket>(pageRef);
            indexInBucket = bucket->indexOfOffset(offsetInBucket);
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
                return bucket->getSize(indexInBucket);
            case Fragmented:
                return bpTree.getElementCount();
        }
    }

    template<NativeIntegerType dir>
    static NativeIntegerType segmentInteroperation(NativeNaturalType dst, NativeNaturalType src, NativeNaturalType length) {
        if(dir == 0)
            return bitwiseCompare(reinterpret_cast<NativeNaturalType*>(superPage),
                                  reinterpret_cast<const NativeNaturalType*>(superPage),
                                  dst, src, length);
        else {
            bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(superPage),
                             reinterpret_cast<const NativeNaturalType*>(superPage),
                             dst, src, length);
            return 0;
        }
    }

    template<NativeIntegerType dir, typename IteratorType>
    NativeNaturalType getSegmentSize(IteratorType& iter, NativeNaturalType offset) {
        if(dir == 1) {
            if(state == Fragmented) { // TODO: Rethink
                if(iter[0]->index > 0)
                    return iter[0]->index;
                iter.template advance<-1>(1);
                iter[0]->index = iter[0]->endIndex;
                return iter[0]->endIndex;
            } else
                return offset;
        } else
            return (state == Fragmented) ? iter[0]->endIndex-iter[0]->index : bucket->getSize(indexInBucket)-offset;
    }

    template<typename IteratorType>
    NativeNaturalType addressOfInteroperation(IteratorType& iter, NativeNaturalType offset) {
        return (state == Fragmented) ? iter[0]->pageRef*bitsPerPage+BpTreeBlob::Page::valueOffset+iter[0]->index : address+offset;
    }

    template<NativeIntegerType dir, typename IteratorType>
    void advanceBySegmentSize(IteratorType& iter, NativeNaturalType& offset, NativeNaturalType intersection) {
        if(dir == 1) {
            if(state == Fragmented)
                iter.template advance<-1>(0, intersection);
            else
                offset -= intersection;
        } else {
            if(state == Fragmented)
                iter.template advance<+1>(0, intersection);
            else
                offset += intersection;
        }
    }

    template<NativeIntegerType dir = -1>
    NativeIntegerType interoperation(Blob src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        NativeNaturalType dstEndOffset = dstOffset+length, srcEndOffset = srcOffset+length;
        if(dstOffset >= dstEndOffset || dstEndOffset > getSize() ||
           srcOffset >= srcEndOffset || srcEndOffset > src.getSize())
            return 0;
        NativeNaturalType segment[2], intersection, result;
        BpTreeBlob::Iterator<dir != 0> iter[2];
        if(dir == 1) {
            dstOffset = dstEndOffset;
            srcOffset = srcEndOffset;
        }
        if(state == Fragmented)
            bpTree.find<Rank>(iter[0], dstOffset);
        if(src.state == Fragmented)
            src.bpTree.find<Rank>(iter[1], srcOffset);
        while(true) {
            segment[0] = getSegmentSize<dir>(iter[0], dstOffset);
            segment[1] = src.getSegmentSize<dir>(iter[1], srcOffset);
            intersection = min(segment[0], segment[1], length);
            if(dir == 1) {
                advanceBySegmentSize<1>(iter[0], dstOffset, intersection);
                src.advanceBySegmentSize<1>(iter[1], srcOffset, intersection);
            }
            result = segmentInteroperation<dir>(addressOfInteroperation(iter[0], dstOffset),
                                                src.addressOfInteroperation(iter[1], srcOffset),
                                                intersection);
            length -= intersection;
            if(length == 0 || result != 0)
                break;
            if(dir != 1) {
                advanceBySegmentSize<-1>(iter[0], dstOffset, intersection);
                src.advanceBySegmentSize<-1>(iter[1], srcOffset, intersection);
            }
        }
        return (dir == 0) ? result : 1;
    }

    NativeIntegerType compare(Blob other) {
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

    bool slice(Blob src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        if(symbol == src.symbol && dstOffset == srcOffset)
            return false;
        if(dstOffset <= srcOffset) {
            if(!interoperation<-1>(src, dstOffset, srcOffset, length))
                return false;
        } else {
            if(!interoperation<+1>(src, dstOffset, srcOffset, length))
                return false;
        }
        modifiedBlob(symbol);
        return true;
    }

    void allocateInBucket(NativeNaturalType size) {
        assert(size > 0);
        if(superPage->freeBlobBuckets[type].empty()) {
            pageRef = aquirePage();
            bucket = dereferencePage<BlobBucket>(pageRef);
            bucket->init(type);
            assert(superPage->freeBlobBuckets[type].insert(pageRef));
        } else {
            pageRef = superPage->freeBlobBuckets[type].getOne<First, false>();
            bucket = dereferencePage<BlobBucket>(pageRef);
        }
        indexInBucket = bucket->allocateIndex(size, symbol, pageRef);
        offsetInBucket = bucket->offsetOfIndex(indexInBucket);
        address = pageRef*bitsPerPage+offsetInBucket;
    }

    void freeFromBucket() {
        assert(state == InBucket);
        bucket->freeIndex(indexInBucket, pageRef);
    }

    void updateAddress(NativeNaturalType address) {
        BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
        superPage->blobs.find<Key>(iter, symbol);
        iter.setValue(address);
    }

    bool decreaseSize(NativeNaturalType at, NativeNaturalType count) {
        NativeNaturalType size = getSize(), end = at+count;
        if(at >= end || end > size)
            return false;
        size -= count;
        Blob srcBlob = *this;
        if(size == 0) {
            state = Empty;
            superPage->blobs.erase<Key>(symbol);
        } else if(BlobBucket::isBucketAllocatable(size)) {
            type = BlobBucket::getType(size);
            srcBlob.type = BlobBucket::getType(size+count);
            if(srcBlob.state == Fragmented || type != srcBlob.type) {
                state = InBucket;
                allocateInBucket(size);
                interoperation(srcBlob, 0, 0, at);
                interoperation(srcBlob, at, end, size-at);
                updateAddress(address);
            } else {
                interoperation<-1>(*this, at, end, size-at);
                bucket->setSize(indexInBucket, size);
            }
        } else {
            BpTreeBlob::Iterator<true> from, to;
            bpTree.find<Rank>(from, at);
            bpTree.find<Rank>(to, at+count-1);
            bpTree.erase(from, to);
            address = bpTree.rootPageRef*bitsPerPage;
            updateAddress(address);
        }
        if(srcBlob.state == InBucket && !(state == InBucket && type == srcBlob.type))
            srcBlob.freeFromBucket();
        if(srcBlob.state == Fragmented && srcBlob.state != Fragmented)
            bpTree.erase();
        modifiedBlob(symbol);
        assert(size == getSize());
        return true;
    }

    bool increaseSize(NativeNaturalType at, NativeNaturalType count) {
        NativeNaturalType size = getSize();
        if(size >= size+count || at > size)
            return false;
        Blob srcBlob = *this;
        size += count;
        if(BlobBucket::isBucketAllocatable(size)) {
            state = InBucket;
            type = BlobBucket::getType(size);
            srcBlob.type = BlobBucket::getType(size-count);
            if(srcBlob.state == Empty || type != srcBlob.type)
                allocateInBucket(size);
            else {
                bucket->setSize(indexInBucket, size);
                interoperation<1>(*this, at+count, at, size-count-at);
            }
        } else {
            BpTreeBlob::Iterator<true> iter;
            state = Fragmented;
            if(srcBlob.state == Fragmented) {
                bpTree.find<Rank>(iter, at);
                bpTree.insert(iter, count, nullptr);
            } else {
                bpTree.rootPageRef = 0;
                bpTree.find<First>(iter);
                bpTree.insert(iter, size, nullptr);
            }
            address = bpTree.rootPageRef*bitsPerPage;
        }
        switch(srcBlob.state) {
            case Empty:
                superPage->blobs.insert(symbol, address);
                break;
            case InBucket:
                if(state != InBucket || type != srcBlob.type) {
                    interoperation(srcBlob, 0, 0, at);
                    interoperation(srcBlob, at+count, at, size-count-at);
                    srcBlob.freeFromBucket();
                } else
                    break;
            case Fragmented:
                updateAddress(address);
                break;
        }
        modifiedBlob(symbol);
        assert(size == getSize());
        return true;
    }

    void setSize(NativeNaturalType newSize) {
        NativeNaturalType oldSize = getSize();
        if(oldSize < newSize)
            increaseSize(oldSize, newSize-oldSize);
        else if(oldSize > newSize)
            decreaseSize(newSize, oldSize-newSize);
    }

    void deepCopy(Blob src) {
        if(symbol == src.symbol)
            return;
        NativeNaturalType srcSize = src.getSize();
        setSize(srcSize);
        interoperation(src, 0, 0, srcSize);
        modifiedBlob(symbol);
    }

    template<bool swap>
    void externalOperate(void* data, NativeNaturalType at, NativeNaturalType count) {
        assert(count > 0 && at+count <= getSize());
        if(state == InBucket) {
            bitwiseCopySwap<swap>(reinterpret_cast<NativeNaturalType*>(data),
                                  reinterpret_cast<NativeNaturalType*>(superPage),
                                  0, address+at,
                                  count);
        } else {
            BpTreeBlob::Iterator<false> iter;
            bpTree.find<Rank>(iter, at);
            at = 0;
            while(true) {
                NativeNaturalType segment = min(count, static_cast<NativeNaturalType>(iter[0]->endIndex-iter[0]->index));
                bitwiseCopySwap<swap>(reinterpret_cast<NativeNaturalType*>(data),
                                      reinterpret_cast<NativeNaturalType*>(superPage),
                                      at, addressOfInteroperation(iter, 0), segment);
                count -= segment;
                if(count == 0)
                   break;
                iter.template advance<1>(0, segment);
                at += segment;
            }
        }
    }
};

// TODO: Cleanup code: Inline these methods?

NativeNaturalType getBlobSize(Symbol symbol) {
    return Blob(symbol).getSize();
}

void setBlobSize(Symbol symbol, NativeNaturalType size) {
    Blob(symbol).setSize(size);
}

template<typename DataType>
DataType readBlobAt(Symbol srcSymbol, NativeNaturalType srcIndex = 0) {
    DataType dst;
    Blob(srcSymbol).externalOperate<false>(&dst, srcIndex*sizeOfInBits<DataType>::value, sizeOfInBits<DataType>::value);
    return dst;
}

template<typename DataType>
void writeBlobAt(Symbol dstSymbol, NativeNaturalType dstIndex, DataType src) {
    Blob(dstSymbol).externalOperate<true>(&src, dstIndex*sizeOfInBits<DataType>::value, sizeOfInBits<DataType>::value);
}

template<typename DataType>
void writeBlob(Symbol dstSymbol, DataType src) {
    Blob dstBlob(dstSymbol);
    dstBlob.setSize(sizeOfInBits<DataType>::value);
    dstBlob.externalOperate<true>(&src, 0, sizeOfInBits<DataType>::value);
    modifiedBlob(dstSymbol);
}

NativeIntegerType compareBlobs(Symbol aSymbol, Symbol bSymbol) {
    return Blob(aSymbol).compare(Blob(bSymbol));
}

bool sliceBlob(Symbol dstSymbol, Symbol srcSymbol, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
    return Blob(dstSymbol).slice(Blob(srcSymbol), dstOffset, srcOffset, length);
}

void cloneBlob(Symbol dstSymbol, Symbol srcSymbol) {
    Blob(dstSymbol).deepCopy(Blob(srcSymbol));
}

bool decreaseBlobSize(Symbol symbol, NativeNaturalType at, NativeNaturalType count) {
    return Blob(symbol).decreaseSize(at, count);
}

bool increaseBlobSize(Symbol symbol, NativeNaturalType at, NativeNaturalType count) {
    return Blob(symbol).increaseSize(at, count);
}

void releaseSymbol(Symbol symbol) {
    Blob(symbol).setSize(0);
    /* TODO: Fix scrutinizeExistence
    if(symbol == superPage->symbolCount-1)
        --superPage->symbolCount;
    else
        superPage->freeSymbols.insert(symbol); */
}

};
