#include <Storage/SuperPage.hpp>

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
        return sizeOfInBits<BlobBucketHeader>::value;
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
        return getDataOffset()-(getSizeOffset()+getSizeBits()*getMaxCount());
    }

    bool isEmpty() const {
        return header.count == 0;
    }

    bool isFull() const {
        return header.count == getMaxCount();
    }

    void generateStats(struct Stats& stats) {
        stats.uninhabitable += getPadding();
        stats.totalMetaData += bitsPerPage-getPadding()-getDataBits()*getMaxCount();
        stats.inhabitedMetaData += (getSizeBits()+architectureSize)*header.count;
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
        assert(getSize(index) > 0);
        if(isFull()) {
            assert(superPage->fullBlobBuckets.erase<Key>(pageRef));
            assert(superPage->freeBlobBuckets[header.type].insert(pageRef));
        }
        --header.count;
        if(isEmpty()) {
            assert(superPage->freeBlobBuckets[header.type].erase<Key>(pageRef));
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
            assert(superPage->fullBlobBuckets.insert(pageRef));
            assert(superPage->freeBlobBuckets[header.type].erase<Key>(pageRef));
        }
        return index;
    }

    void setSize(NativeNaturalType index, NativeNaturalType size) {
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                 reinterpret_cast<const NativeNaturalType*>(&size),
                                 getSizeOffset()+index*getSizeBits(), 0, getSizeBits());
    }

    NativeNaturalType getSize(NativeNaturalType index) const {
        NativeNaturalType size = 0;
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&size),
                                 reinterpret_cast<const NativeNaturalType*>(this),
                                 0, getSizeOffset()+index*getSizeBits(), getSizeBits());
        return size;
    }

    void setSymbol(NativeNaturalType index, NativeNaturalType symbol) {
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                 reinterpret_cast<const NativeNaturalType*>(&symbol),
                                 getSymbolOffset()+index*architectureSize, 0, architectureSize);
    }

    NativeNaturalType getSymbol(NativeNaturalType index) const {
        Symbol symbol;
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&symbol),
                                 reinterpret_cast<const NativeNaturalType*>(this),
                                 0, getSymbolOffset()+index*architectureSize, architectureSize);
        return symbol;
    }

    static Natural16 getType(NativeNaturalType size) {
        return binarySearch<NativeNaturalType>(0, blobBucketTypeCount, [&](NativeNaturalType index) {
            return blobBucketType[index] < size;
        });
    }

    static bool isBucketAllocatable(NativeNaturalType size) {
        return size <= blobBucketType[blobBucketTypeCount-1];
    }
};
