#include <Storage/SuperPage.hpp>

struct BitVectorBucketHeader : public BasePage {
    Natural16 type, count, freeIndex;
};

struct BitVectorBucket {
    BitVectorBucketHeader header;

    NativeNaturalType getMinDataBits() const {
        return (header.type == 0) ? 0 : bitVectorBucketType[header.type-1];
    }

    NativeNaturalType getMaxDataBits() const {
        return bitVectorBucketType[header.type];
    }

    NativeNaturalType getSizeBits() const {
        return BitMask<NativeNaturalType>::ceilLog2(getMaxDataBits()-getMinDataBits());
    }

    NativeNaturalType getHeaderEnd() const {
        return sizeOfInBits<BitVectorBucketHeader>::value;
    }

    NativeNaturalType getElementLength() const {
        return getMaxDataBits()+getSizeBits()+architectureSize*2;
    }

    NativeNaturalType getIndexOfOffset(NativeNaturalType offset) const {
        return (offset-getHeaderEnd())/getElementLength();
    }

    NativeNaturalType getDataOffset(NativeNaturalType index) const {
        return getHeaderEnd()+index*getElementLength();
    }

    NativeNaturalType getSizeOffset(NativeNaturalType index) const {
        return getDataOffset(index)+getMaxDataBits();
    }

    NativeNaturalType getLocationOffset(NativeNaturalType index) const {
        return getSizeOffset(index)+getSizeBits();
    }

    NativeNaturalType getMaxElementCount() const {
        return (bitsPerPage-getHeaderEnd())/getElementLength();
    }

    NativeNaturalType getPadding() const {
        return bitsPerPage-getHeaderEnd()-getMaxElementCount()*getElementLength();
    }

    void setSize(NativeNaturalType index, NativeNaturalType size) {
        assert(size > getMinDataBits() && size <= getMaxDataBits());
        size -= getMinDataBits()+1;
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                        reinterpret_cast<const NativeNaturalType*>(&size),
                        getSizeOffset(index), 0, getSizeBits());
    }

    NativeNaturalType getSize(NativeNaturalType index) const {
        NativeNaturalType size = 0;
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&size),
                        reinterpret_cast<const NativeNaturalType*>(this),
                        0, getSizeOffset(index), getSizeBits());
        return size+getMinDataBits()+1;
    }

    void setLocation(NativeNaturalType index, Pair<NativeNaturalType, NativeNaturalType> location) {
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                        reinterpret_cast<const NativeNaturalType*>(&location),
                        getLocationOffset(index), 0, architectureSize*2);
    }

    NativeNaturalType getSymbol(NativeNaturalType index) const {
        Symbol symbol;
        bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&symbol),
                        reinterpret_cast<const NativeNaturalType*>(this),
                        0, getLocationOffset(index)+architectureSize, architectureSize);
        return symbol;
    }

    void generateStats(struct Stats& stats) {
        stats.uninhabitable += getPadding();
        stats.totalMetaData += bitsPerPage-getPadding()-getMaxDataBits()*getMaxElementCount();
        stats.inhabitedMetaData += (getSizeBits()+architectureSize)*header.count;
        stats.totalPayload += getMaxDataBits()*getMaxElementCount();
        stats.inhabitedPayload += getMaxDataBits()*header.count;
    }

    void init(NativeNaturalType type) {
        header.type = type;
        header.count = 0;
        header.freeIndex = 0;
        for(NativeNaturalType index = 0; index < getMaxElementCount(); ++index)
            setLocation(index, {0, index+1});
    }

    void freeIndex(NativeNaturalType index, PageRefType pageRef) {
        assert(getSize(index) > 0);
        if(isFull()) {
            assert(superPage->fullBitVectorBuckets.erase<Key>(pageRef));
            assert(superPage->freeBitVectorBuckets[header.type].insert(pageRef));
        }
        --header.count;
        if(isEmpty()) {
            assert(superPage->freeBitVectorBuckets[header.type].erase<Key>(pageRef));
            releasePage(pageRef);
        } else {
            setLocation(index, {0, header.freeIndex});
            header.freeIndex = index;
        }
    }

    NativeNaturalType allocateIndex(NativeNaturalType size, Symbol spaceSymbol, Symbol symbol, PageRefType pageRef) {
        assert(size > 0 && header.count < getMaxElementCount());
        ++header.count;
        NativeNaturalType index = header.freeIndex;
        header.freeIndex = getSymbol(header.freeIndex);
        setSize(index, size);
        setLocation(index, {spaceSymbol, symbol});
        if(isFull()) {
            assert(superPage->fullBitVectorBuckets.insert(pageRef));
            assert(superPage->freeBitVectorBuckets[header.type].erase<Key>(pageRef));
        }
        return index;
    }

    bool isEmpty() const {
        return header.count == 0;
    }

    bool isFull() const {
        return header.count == getMaxElementCount();
    }

    static Natural16 getType(NativeNaturalType size) {
        return binarySearch<NativeNaturalType>(0, bitVectorBucketTypeCount, [&](NativeNaturalType index) {
            return bitVectorBucketType[index] < size;
        });
    }

    static bool isBucketAllocatable(NativeNaturalType size) {
        return size <= bitVectorBucketType[bitVectorBucketTypeCount-1];
    }
};
