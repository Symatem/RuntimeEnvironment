#include <Storage/BlobBucket.hpp>

Symbol createSymbol() {
    /* TODO: Fix scrutinizeExistence
    if(freeSymbols.elementCount)
        return superPage->freeSymbols.getOne<First, true>();
    else */
        return superPage->symbolsEnd++;
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

    void allocateInBucket(NativeNaturalType size) {
        assert(size > 0);
        if(superPage->freeBlobBuckets[type].empty()) {
            pageRef = acquirePage();
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
            if(state == Fragmented) {
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

    NativeNaturalType getSize() {
        switch(state) {
            case Empty:
                return 0;
            case InBucket:
                return bucket->getSize(indexInBucket);
            case Fragmented:
                return bpTree.getElementCount();
        }
        assert(false);
    }

    void setSize(NativeNaturalType newSize) {
        NativeNaturalType oldSize = getSize();
        if(oldSize < newSize)
            increaseSize(oldSize, newSize-oldSize);
        else if(oldSize > newSize)
            decreaseSize(newSize, oldSize-newSize);
    }

    bool decreaseSize(NativeNaturalType offset, NativeNaturalType length) {
        NativeNaturalType size = getSize(), end = offset+length;
        if(offset >= end || end > size)
            return false;
        size -= length;
        Blob srcBlob = *this;
        if(size == 0) {
            state = Empty;
            superPage->blobs.erase<Key>(symbol);
        } else if(BlobBucket::isBucketAllocatable(size)) {
            type = BlobBucket::getType(size);
            srcBlob.type = BlobBucket::getType(size+length);
            if(srcBlob.state == Fragmented || type != srcBlob.type) {
                state = InBucket;
                allocateInBucket(size);
                interoperation(srcBlob, 0, 0, offset);
                interoperation(srcBlob, offset, end, size-offset);
                updateAddress(address);
            } else {
                interoperation<-1>(*this, offset, end, size-offset);
                bucket->setSize(indexInBucket, size);
            }
        } else {
            BpTreeBlob::Iterator<true> from, to;
            bpTree.find<Rank>(from, offset);
            bpTree.find<Rank>(to, offset+length-1);
            bpTree.erase(from, to);
            address = bpTree.rootPageRef*bitsPerPage;
            updateAddress(address);
        }
        if(srcBlob.state == InBucket && !(state == InBucket && type == srcBlob.type))
            srcBlob.freeFromBucket();
        if(srcBlob.state == Fragmented && state != Fragmented)
            bpTree.erase();
        modifiedBlob(symbol);
        assert(size == getSize());
        return true;
    }

    bool increaseSize(NativeNaturalType offset, NativeNaturalType length) {
        NativeNaturalType size = getSize();
        if(size >= size+length || offset > size)
            return false;
        Blob srcBlob = *this;
        size += length;
        if(BlobBucket::isBucketAllocatable(size)) {
            state = InBucket;
            type = BlobBucket::getType(size);
            srcBlob.type = BlobBucket::getType(size-length);
            if(srcBlob.state == Empty || type != srcBlob.type)
                allocateInBucket(size);
            else {
                bucket->setSize(indexInBucket, size);
                interoperation<1>(*this, offset+length, offset, size-length-offset);
            }
        } else {
            BpTreeBlob::Iterator<true> iter;
            state = Fragmented;
            if(srcBlob.state == Fragmented) {
                bpTree.find<Rank>(iter, offset);
                bpTree.insert(iter, length, nullptr);
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
                    interoperation(srcBlob, 0, 0, offset);
                    interoperation(srcBlob, offset+length, offset, size-length-offset);
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

    void deepCopy(Blob src) {
        if(symbol == src.symbol)
            return;
        NativeNaturalType srcSize = src.getSize();
        setSize(srcSize);
        interoperation(src, 0, 0, srcSize);
        modifiedBlob(symbol);
    }

    template<bool swap>
    void externalOperate(void* data, NativeNaturalType offset, NativeNaturalType length) {
        assert(length > 0 && offset+length <= getSize());
        if(state == InBucket) {
            bitwiseCopySwap<swap>(reinterpret_cast<NativeNaturalType*>(data),
                                  reinterpret_cast<NativeNaturalType*>(superPage),
                                  0, address+offset,
                                  length);
        } else {
            BpTreeBlob::Iterator<false> iter;
            bpTree.find<Rank>(iter, offset);
            offset = 0;
            while(true) {
                NativeNaturalType segment = min(length, static_cast<NativeNaturalType>(iter[0]->endIndex-iter[0]->index));
                bitwiseCopySwap<swap>(reinterpret_cast<NativeNaturalType*>(data),
                                      reinterpret_cast<NativeNaturalType*>(superPage),
                                      offset, addressOfInteroperation(iter, 0), segment);
                length -= segment;
                if(length == 0)
                    break;
                iter.template advance<1>(0, segment);
                offset += segment;
            }
        }
    }

    template<typename DataType>
    DataType readAt(NativeNaturalType srcIndex = 0) {
        DataType dst;
        externalOperate<false>(&dst, srcIndex*sizeOfInBits<DataType>::value, sizeOfInBits<DataType>::value);
        return dst;
    }

    template<typename DataType>
    void writeAt(NativeNaturalType dstIndex, DataType src) {
        externalOperate<true>(&src, dstIndex*sizeOfInBits<DataType>::value, sizeOfInBits<DataType>::value);
    }

    template<typename DataType>
    void write(DataType src) {
        setSize(sizeOfInBits<DataType>::value);
        externalOperate<true>(&src, 0, sizeOfInBits<DataType>::value);
        modifiedBlob(symbol);
    }

    void encodeBvlNatural(NativeNaturalType& dstOffset, NativeNaturalType src) {
        assert(dstOffset <= getSize());
        NativeNaturalType srcLength = BitMask<NativeNaturalType>::ceilLog2((src < 2) ? src : src-1),
                          dstLength = BitMask<NativeNaturalType>::ceilLog2(srcLength),
                          sliceLength = 1;
        Natural8 flagBit = 1;
        dstLength += 1<<dstLength;
        increaseSize(dstOffset, dstLength);
        --src;
        NativeNaturalType endOffset = dstOffset+dstLength-1;
        while(dstOffset < endOffset) {
            externalOperate<true>(&flagBit, dstOffset++, 1);
            externalOperate<true>(&src, dstOffset, sliceLength);
            src >>= sliceLength;
            dstOffset += sliceLength;
            sliceLength <<= 1;
        }
        flagBit = 0;
        externalOperate<true>(&flagBit, dstOffset++, 1);
    }

    NativeNaturalType decodeBvlNatural(NativeNaturalType& srcOffset) {
        assert(srcOffset < getSize());
        NativeNaturalType dstOffset = 0, sliceLength = 1, dst = 0;
        while(true) {
            Natural8 flagBit = 0;
            externalOperate<false>(&flagBit, srcOffset++, 1);
            if(!flagBit)
                return (dstOffset == 0) ? dst : dst+1;
            NativeNaturalType buffer = 0;
            externalOperate<false>(&buffer, srcOffset, sliceLength);
            dst |= buffer<<dstOffset;
            srcOffset += sliceLength;
            dstOffset += sliceLength;
            sliceLength <<= 1;
            assert(dstOffset < architectureSize);
        }
    }
};

void releaseSymbol(Symbol symbol) {
    Blob(symbol).setSize(0);
    /* TODO: Fix scrutinizeExistence
    if(symbol == superPage->symbolsEnd-1)
        --superPage->symbolsEnd;
    else
        superPage->freeSymbols.insert(symbol); */
}
