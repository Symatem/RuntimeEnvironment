#include <Storage/BitVectorBucket.hpp>

struct BitVector {
    struct Location {
        SymbolSpace* symbolSpace;
        Symbol symbol;

        Location(SymbolSpace* _symbolSpace, Symbol _symbol) :symbolSpace(_symbolSpace), symbol(_symbol) {}

        bool operator==(const Location& other) const {
            return symbolSpace == other.symbolSpace && symbol == other.symbol;
        }

        bool getAddress(NativeNaturalType& address) {
            BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
            if(!symbolSpace->bitVectors.find<Key>(iter, symbol))
                return false;
            address = iter.getValue();
            return true;
        }

        void setAddress(NativeNaturalType address) {
            BpTreeMap<Symbol, NativeNaturalType>::Iterator<true> iter;
            symbolSpace->bitVectors.find<Key>(iter, symbol);
            iter.setValue(address);
        }

        void insertAddress(NativeNaturalType address) {
            symbolSpace->bitVectors.insert(symbol, address);
            ++symbolSpace->bitVectorCount;
        }

        void eraseAddress() {
            symbolSpace->bitVectors.erase<Key>(symbol);
            --symbolSpace->bitVectorCount;
        }
    } location;

    PageRefType pageRef;
    NativeNaturalType address, offsetInPage, indexInBucket;
    Natural16 bucketType;
    BpTreeBitVector bpTree;
    BitVectorBucket* bucket;
    enum State {
        Empty,
        InBucket,
        Fragmented
    } state;

    BitVector(SymbolSpace* symbolSpace, Symbol symbol) :location(symbolSpace, symbol) {
        if(!location.getAddress(address)) {
            state = Empty;
            return;
        }
        pageRef = address/bitsPerPage;
        offsetInPage = address-pageRef*bitsPerPage;
        if(offsetInPage > 0) {
            state = InBucket;
            bucket = dereferencePage<BitVectorBucket>(pageRef);
            indexInBucket = bucket->getIndexOfOffset(offsetInPage);
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
        if(superPage->freeBitVectorBuckets[bucketType].isEmpty()) {
            pageRef = acquirePage();
            bucket = dereferencePage<BitVectorBucket>(pageRef);
            bucket->init(bucketType);
            assert(superPage->freeBitVectorBuckets[bucketType].insert(pageRef));
        } else {
            pageRef = superPage->freeBitVectorBuckets[bucketType].template getOne<First, false>();
            bucket = dereferencePage<BitVectorBucket>(pageRef);
        }
        indexInBucket = bucket->allocateIndex(size, location.symbol, pageRef);
        offsetInPage = bucket->getDataOffset(indexInBucket);
        address = pageRef*bitsPerPage+offsetInPage;
    }

    void freeFromBucket() {
        assert(state == InBucket);
        bucket->freeIndex(indexInBucket, pageRef);
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
        if(location == other.location)
            return 0;
        NativeNaturalType size = getSize(), otherSize = other.getSize();
        if(size < otherSize)
            return -1;
        if(size > otherSize)
            return 1;
        return interoperation<0>(other, 0, 0, size);
    }

    bool slice(BitVector src, NativeNaturalType dstOffset, NativeNaturalType srcOffset, NativeNaturalType length) {
        if(location == src.location && dstOffset == srcOffset)
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
            location.eraseAddress();
        } else if(BitVectorBucket::isBucketAllocatable(size)) {
            bucketType = BitVectorBucket::getType(size);
            srcBitVector.bucketType = BitVectorBucket::getType(size+length);
            if(srcBitVector.state == Fragmented || bucketType != srcBitVector.bucketType) {
                state = InBucket;
                allocateInBucket(size);
                interoperation(srcBitVector, 0, 0, offset);
                interoperation(srcBitVector, offset, end, size-offset);
                location.setAddress(address);
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
            location.setAddress(address);
        }
        if(srcBitVector.state == InBucket && !(state == InBucket && bucketType == srcBitVector.bucketType))
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
            bucketType = BitVectorBucket::getType(size);
            srcBitVector.bucketType = BitVectorBucket::getType(size-length);
            if(srcBitVector.state == Empty || bucketType != srcBitVector.bucketType)
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
                location.insertAddress(address);
                break;
            case InBucket:
                if(state != InBucket || bucketType != srcBitVector.bucketType) {
                    interoperation(srcBitVector, 0, 0, offset);
                    interoperation(srcBitVector, offset+length, offset, size-length-offset);
                    srcBitVector.freeFromBucket();
                } else
                    break;
            case Fragmented:
                location.setAddress(address);
                break;
        }
        assert(size == getSize());
        return true;
    }

    void deepCopy(BitVector src) {
        if(location == src.location)
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



void SymbolSpace::releaseSymbol(Symbol symbol) {
    BitVector(this, symbol).setSize(0);
    if(symbol == symbolsEnd-1)
        --symbolsEnd;
    else
        recyclableSymbols.insert(symbol);
}
