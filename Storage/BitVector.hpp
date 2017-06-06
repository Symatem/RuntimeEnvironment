#include <Storage/BitVectorBucket.hpp>

struct BitVector {
    Symbol symbol;
    PageRefType pageRef;
    NativeNaturalType address, offsetInBucket, indexInBucket;
    Natural16 type;
    BpTreeBitVector bpTree;
    BitVectorBucket* bucket;
    enum State {
        Empty,
        InBucket,
        Fragmented
    } state;

    BitVector() {}

    BitVector(Symbol _symbol) {
        symbol = _symbol;
        BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
        if(!superPage->ontology.bitVectors.find<Key>(iter, symbol)) {
            state = Empty;
            return;
        }
        address = iter.getValue();
        pageRef = address/bitsPerPage;
        offsetInBucket = address-pageRef*bitsPerPage;
        if(offsetInBucket > 0) {
            state = InBucket;
            bucket = dereferencePage<BitVectorBucket>(pageRef);
            indexInBucket = bucket->getIndexOfOffset(offsetInBucket);
        } else {
            state = Fragmented;
            bpTree.rootPageRef = pageRef;
        }
    }

    BitVector& getBitVector() {
        return *this;
    }

    void allocateInBucket(NativeNaturalType size) {
        assert(size > 0);
        if(superPage->freeBitVectorBuckets[type].isEmpty()) {
            pageRef = acquirePage();
            bucket = dereferencePage<BitVectorBucket>(pageRef);
            bucket->init(type);
            assert(superPage->freeBitVectorBuckets[type].insert(pageRef));
        } else {
            pageRef = superPage->freeBitVectorBuckets[type].getOne<First, false>();
            bucket = dereferencePage<BitVectorBucket>(pageRef);
        }
        indexInBucket = bucket->allocateIndex(size, symbol, pageRef);
        offsetInBucket = bucket->getDataOffset(indexInBucket);
        address = pageRef*bitsPerPage+offsetInBucket;
    }

    void freeFromBucket() {
        assert(state == InBucket);
        bucket->freeIndex(indexInBucket, pageRef);
    }

    void updateAddress(NativeNaturalType address) {
        BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
        superPage->ontology.bitVectors.find<Key>(iter, symbol);
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
        return (state == Fragmented) ? iter[0]->pageRef*bitsPerPage+BpTreeBitVector::Page::valueOffset+iter[0]->index : address+offset;
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
    NativeIntegerType interoperation(BitVector src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        NativeNaturalType dstEndOffset = dstOffset+length, srcEndOffset = srcOffset+length;
        if(dstOffset >= dstEndOffset || dstEndOffset > getSize() ||
           srcOffset >= srcEndOffset || srcEndOffset > src.getSize())
            return 0;
        NativeNaturalType segment[2], intersection, result;
        BpTreeBitVector::Iterator<dir != 0> iter[2];
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

    NativeIntegerType compare(BitVector other) {
        if(symbol == other.symbol)
            return 0;
        NativeNaturalType size = getSize(), otherSize = other.getSize();
        if(size < otherSize)
            return -1;
        if(size > otherSize)
            return 1;
        return interoperation<0>(other, 0, 0, size);
    }

    bool slice(BitVector src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        if(symbol == src.symbol && dstOffset == srcOffset)
            return false;
        if(dstOffset <= srcOffset) {
            if(!interoperation<-1>(src, dstOffset, srcOffset, length))
                return false;
        } else {
            if(!interoperation<+1>(src, dstOffset, srcOffset, length))
                return false;
        }
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
        BitVector srcBitVector = *this;
        if(size == 0) {
            state = Empty;
            superPage->ontology.bitVectors.erase<Key>(symbol);
            --superPage->bitVectorCount;
        } else if(BitVectorBucket::isBucketAllocatable(size)) {
            type = BitVectorBucket::getType(size);
            srcBitVector.type = BitVectorBucket::getType(size+length);
            if(srcBitVector.state == Fragmented || type != srcBitVector.type) {
                state = InBucket;
                allocateInBucket(size);
                interoperation(srcBitVector, 0, 0, offset);
                interoperation(srcBitVector, offset, end, size-offset);
                updateAddress(address);
            } else {
                interoperation<-1>(*this, offset, end, size-offset);
                bucket->setSize(indexInBucket, size);
            }
        } else {
            BpTreeBitVector::Iterator<true> from, to;
            bpTree.find<Rank>(from, offset);
            bpTree.find<Rank>(to, offset+length-1);
            bpTree.erase(from, to);
            address = bpTree.rootPageRef*bitsPerPage;
            updateAddress(address);
        }
        if(srcBitVector.state == InBucket && !(state == InBucket && type == srcBitVector.type))
            srcBitVector.freeFromBucket();
        if(srcBitVector.state == Fragmented && state != Fragmented)
            bpTree.erase();
        assert(size == getSize());
        return true;
    }

    bool increaseSize(NativeNaturalType offset, NativeNaturalType length) {
        NativeNaturalType size = getSize();
        if(size >= size+length || offset > size)
            return false;
        BitVector srcBitVector = *this;
        size += length;
        if(BitVectorBucket::isBucketAllocatable(size)) {
            state = InBucket;
            type = BitVectorBucket::getType(size);
            srcBitVector.type = BitVectorBucket::getType(size-length);
            if(srcBitVector.state == Empty || type != srcBitVector.type)
                allocateInBucket(size);
            else {
                bucket->setSize(indexInBucket, size);
                interoperation<1>(*this, offset+length, offset, size-length-offset);
            }
        } else {
            BpTreeBitVector::Iterator<true> iter;
            state = Fragmented;
            if(srcBitVector.state == Fragmented) {
                bpTree.find<Rank>(iter, offset);
                bpTree.insert(iter, length, nullptr);
            } else {
                bpTree.rootPageRef = 0;
                bpTree.find<First>(iter);
                bpTree.insert(iter, size, nullptr);
            }
            address = bpTree.rootPageRef*bitsPerPage;
        }
        switch(srcBitVector.state) {
            case Empty:
                superPage->ontology.bitVectors.insert(symbol, address);
                ++superPage->bitVectorCount;
                break;
            case InBucket:
                if(state != InBucket || type != srcBitVector.type) {
                    interoperation(srcBitVector, 0, 0, offset);
                    interoperation(srcBitVector, offset+length, offset, size-length-offset);
                    srcBitVector.freeFromBucket();
                } else
                    break;
            case Fragmented:
                updateAddress(address);
                break;
        }
        assert(size == getSize());
        return true;
    }

    void deepCopy(BitVector src) {
        if(symbol == src.symbol)
            return;
        NativeNaturalType srcSize = src.getSize();
        setSize(srcSize);
        interoperation(src, 0, 0, srcSize);
    }

    template<bool overwrite>
    bool externalOperate(typename conditional<overwrite, const void*, void*>::type data, NativeNaturalType offset, NativeNaturalType length) {
        typedef typename conditional<overwrite, const NativeNaturalType*, NativeNaturalType*>::type CopyType0;
        typedef typename conditional<overwrite, NativeNaturalType*, const NativeNaturalType*>::type CopyType1;
        if(length == 0 || offset+length > getSize())
            return false;
        if(state == InBucket) {
            bitwiseCopySwap<overwrite>(reinterpret_cast<CopyType0>(data), reinterpret_cast<CopyType1>(superPage),
                                       0, address+offset, length);
        } else {
            BpTreeBitVector::Iterator<false> iter;
            bpTree.find<Rank>(iter, offset);
            offset = 0;
            while(true) {
                NativeNaturalType segment = min(length, static_cast<NativeNaturalType>(iter[0]->endIndex-iter[0]->index));
                bitwiseCopySwap<overwrite>(reinterpret_cast<CopyType0>(data), reinterpret_cast<CopyType1>(superPage),
                                           offset, addressOfInteroperation(iter, 0), segment);
                length -= segment;
                if(length == 0)
                    break;
                iter.template advance<1>(0, segment);
                offset += segment;
            }
        }
        return true;
    }

    void chaCha20(ChaCha20& context) {
        ChaCha20 buffer, mask;
        NativeNaturalType endOffset = getSize(), offset = 0;
        while(offset < endOffset) {
            NativeNaturalType sliceLength = min(endOffset-offset, sizeOfInBits<ChaCha20>::value);
            mask.generate(context);
            externalOperate<false>(&buffer, offset, sliceLength);
            for(Natural8 i = 0; i < 8; ++i)
                buffer.block64[i] ^= mask.block64[i];
            externalOperate<true>(&buffer, offset, sliceLength);
            offset += sliceLength;
        }
    }
};



void OntologyStruct::releaseSymbol(Symbol symbol) {
    BitVector(symbol).setSize(0);
    if(symbol == symbolsEnd-1)
        --symbolsEnd;
    else
        freeSymbols.insert(symbol);
}
