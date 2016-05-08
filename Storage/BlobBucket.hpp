#include "BpContainers.hpp"

namespace Storage {

// TODO: Redistribution if there are many almost empty buckets of the same type
const NativeNaturalType blobBucketTypeCount = 15, blobBucketType[blobBucketTypeCount] =
    {8, 16, 32, 64, 192, 320, 640, 1152, 2112, 3328, 6016, 7552, 10112, 15232, 30641};
// blobBucketTypeCount = 12, {8, 16, 32, 64, 192, 320, 640, 1152, 3328, 7552, 15232, 30641};
BpTreeSet<PageRefType> fullBlobBuckets, freeBlobBuckets[blobBucketTypeCount];

struct BlobBucketHeader : public BasePage {
    Natural16 type, count, freeIndex;
};

struct BlobBucket {
    BlobBucketHeader header;

    NativeNaturalType getSizeBits() const {
        return BitMask<NativeNaturalType>::ceilLog2(getDataBits());
    }

    NativeNaturalType getDataBits() const {
        return blobBucketType[header.type];
    }

    NativeNaturalType getSizeOffset() const {
        return architecturePadding(sizeOfInBits<BlobBucketHeader>::value);
    }

    NativeNaturalType getDataOffset() const {
        return getSymbolOffset()-getDataBits()*getMaxCount();
    }

    NativeNaturalType getSymbolOffset() const {
        return bitsPerPage-architectureSize*getMaxCount();
    }

    NativeNaturalType indexOfOffset(NativeNaturalType bitOffset) const {
        return (bitOffset-getDataOffset())/getDataBits();
    }

    NativeNaturalType offsetOfIndex(NativeNaturalType index) const {
        return getDataOffset()+index*getDataBits();
    }

    NativeNaturalType getMaxCount() const {
        return (bitsPerPage-getSizeOffset())/(getSizeBits()+architectureSize+getDataBits());
    }

    NativeNaturalType getPadding() const {
        return bitsPerPage-getSizeOffset()-(getSizeBits()+architectureSize+getDataBits())*getMaxCount();
    }

    bool isEmpty() const {
        return header.count == 0;
    }

    bool isFull() const {
        return header.count == getMaxCount();
    }

    void generateStats(struct Stats& stats) {
        stats.elementCount += header.count;
        stats.uninhabitable += getPadding();
        stats.totalMetaData += getDataOffset();
        stats.inhabitedMetaData += getDataOffset();
        stats.totalPayload += getDataBits()*getMaxCount();
        stats.inhabitedPayload += getDataBits()*header.count;
    }

    void init(NativeNaturalType type) {
        header.type = type;
        header.count = 0;
        header.freeIndex = 0;
        for(NativeNaturalType index = 0; index < getMaxCount(); ++index) {
            setSize(index, 0);
            setSymbol(index, index+1);
        }
    }

    void freeIndex(NativeNaturalType index, PageRefType pageRef) {
        if(isFull()) {
            assert(fullBlobBuckets.erase<Key>(pageRef));
            assert(freeBlobBuckets[header.type].insert(pageRef));
        }
        --header.count;
        if(isEmpty()) {
            assert(freeBlobBuckets[header.type].erase<Key>(pageRef));
            releasePage(pageRef);
        } else {
            setSize(index, 0);
            setSymbol(index, header.freeIndex);
            header.freeIndex = index;
        }
    }

    NativeNaturalType allocateIndex(NativeNaturalType size, Symbol symbol, PageRefType pageRef) {
        assert(size > 0 && header.count < getMaxCount());
        ++header.count;
        NativeNaturalType index = header.freeIndex;
        header.freeIndex = getSymbol(header.freeIndex);
        setSize(index, size);
        setSymbol(index, symbol);
        if(isFull()) {
            assert(fullBlobBuckets.insert(pageRef));
            assert(freeBlobBuckets[header.type].erase<Key>(pageRef));
        }
        return index;
    }

    void setSize(NativeNaturalType index, NativeNaturalType size) {
        Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                 reinterpret_cast<const NativeNaturalType*>(&size),
                                 getSizeOffset()+index*getSizeBits(), 0, getSizeBits());
    }

    NativeNaturalType getSize(NativeNaturalType index) const {
        NativeNaturalType size = 0;
        Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&size),
                                 reinterpret_cast<const NativeNaturalType*>(this),
                                 0, getSizeOffset()+index*getSizeBits(), getSizeBits());
        return size;
    }

    void setSymbol(NativeNaturalType index, NativeNaturalType symbol) {
        Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                 reinterpret_cast<const NativeNaturalType*>(&symbol),
                                 getSymbolOffset()+index*architectureSize, 0, architectureSize);
    }

    NativeNaturalType getSymbol(NativeNaturalType index) const {
        Symbol symbol;
        Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&symbol),
                                 reinterpret_cast<const NativeNaturalType*>(this),
                                 0, getSymbolOffset()+index*architectureSize, architectureSize);
        return symbol;
    }

    static Natural16 getType(NativeNaturalType size) {
        return binarySearch<NativeNaturalType>(blobBucketTypeCount, [&](NativeNaturalType index) {
            return blobBucketType[index] < size;
        });
    }

    static bool isBucketAllocatable(NativeNaturalType size) {
        return size <= blobBucketType[blobBucketTypeCount-1];
    }
};

};
