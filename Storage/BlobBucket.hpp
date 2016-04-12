#include "BpContainers.hpp"

namespace Storage {

// TODO: Test BlobBuckets
const NativeNaturalType blobBucketTypeCount = 15, blobBucketType[blobBucketTypeCount] =
    {8, 16, 32, 64, 192, 320, 640, 1152, 2112, 3328, 6016, 7552, 10112, 15232, 30641};
// blobBucketTypeCount = 12, {8, 16, 32, 64, 192, 320, 640, 1152, 3328, 7552, 15232, 30641};
BpTreeSet<PageRefType> fullBlobBuckets, freeBlobBuckets[blobBucketTypeCount];

struct BlobBucketHeader : public BasePage {
    NativeNaturalType type, count, freeIndex;
};

struct BlobBucket {
    BlobBucketHeader header;

    NativeNaturalType getDataBits() const {
        return blobBucketType[header.type];
    }

    NativeNaturalType getSizeBits() const {
        return BitMask<NativeNaturalType>::ceilLog2(getDataBits());
    }

    NativeNaturalType getDataOffset() const {
        return sizeof(BlobBucketHeader)*8;
    }

    NativeNaturalType getSizeOffset() const {
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
        return (bitsPerPage-getDataOffset())/(getSizeBits()+architectureSize+getDataBits());
    }

    NativeNaturalType getPadding() const {
        return bitsPerPage-getDataOffset()-(getSizeBits()+architectureSize+getDataBits())*getMaxCount();
    }

    bool isEmpty() const {
        return header.count == 0;
    }

    bool isFull() const {
        return header.count == getMaxCount()-1;
    }

    void updateStats(struct UsageStats& usage) {
        usage.totalMetaData += getDataOffset();
        usage.inhabitedMetaData += getDataOffset();
        usage.totalPayload += getDataBits()*getMaxCount();
        usage.inhabitedPayload += getDataBits()*header.count;
        usage.uninhabitable += getPadding();
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

    void freeIndex(NativeNaturalType index) {
        --header.count;
        setSize(index, 0);
        setSymbol(index, header.freeIndex);
        header.freeIndex = index;
    }

    NativeNaturalType allocateIndex() {
        ++header.count;
        NativeNaturalType index = header.freeIndex;
        header.freeIndex = getSymbol(header.freeIndex);
        return index;
    }

    void setSize(NativeNaturalType index, NativeNaturalType size) {
        Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                 reinterpret_cast<const NativeNaturalType*>(&size),
                                 getSizeOffset()+index*getSizeBits(), 0, getSizeBits());
    }

    NativeNaturalType getSize(NativeNaturalType index) const {
        NativeNaturalType size;
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

    static NativeNaturalType allocateBlob(NativeNaturalType size, Symbol symbol) {
        NativeNaturalType type = binarySearch<NativeNaturalType>(blobBucketTypeCount, [&](NativeNaturalType index) {
            return blobBucketType[index] < size;
        });
        if(type >= blobBucketTypeCount)
            return 0;
        PageRefType pageRef;
        BlobBucket* bucket;
        if(freeBlobBuckets[type].empty()) {
            pageRef = aquirePage();
            bucket = dereferencePage<BlobBucket>(pageRef);
            bucket->init(type);
            assert(freeBlobBuckets[type].insert(pageRef));
        } else {
            pageRef = freeBlobBuckets[type].pullOneOut<First>();
            bucket = dereferencePage<BlobBucket>(pageRef);
        }
        NativeNaturalType index = bucket->allocateIndex();
        bucket->setSize(index, size);
        bucket->setSymbol(index, symbol);
        if(bucket->isFull()) {
            assert(fullBlobBuckets.insert(pageRef));
            assert(freeBlobBuckets[type].erase<Key>(pageRef));
        }
        return pageRef*bitsPerPage+bucket->offsetOfIndex(index);
    }

    const static NativeNaturalType addressOffsetMask = bitsPerPage-1;

    static void setBlobSize(NativeNaturalType address, NativeNaturalType size) {
        PageRefType pageRef = address/bitsPerPage;
        BlobBucket* bucket = dereferencePage<BlobBucket>(pageRef);
        NativeNaturalType index = bucket->indexOfOffset(address&addressOffsetMask);
        if(size == 0) {
            if(bucket->isFull()) {
                assert(fullBlobBuckets.erase<Key>(pageRef));
                assert(freeBlobBuckets[bucket->header.type].insert(pageRef));
            }
            bucket->freeIndex(index);
            if(bucket->isEmpty())
                releasePage(pageRef);
        } else
            bucket->setSize(index, size);
    }

    static NativeNaturalType getBlobSize(NativeNaturalType address) {
        PageRefType pageRef = address/bitsPerPage;
        BlobBucket* bucket = dereferencePage<BlobBucket>(pageRef);
        NativeNaturalType index = bucket->indexOfOffset(address&addressOffsetMask);
        return bucket->getSize(index);
    }

    static NativeNaturalType getBlobSymbol(NativeNaturalType address) {
        PageRefType pageRef = address/bitsPerPage;
        BlobBucket* bucket = dereferencePage<BlobBucket>(pageRef);
        NativeNaturalType index = bucket->indexOfOffset(address&addressOffsetMask);
        return bucket->getSymbol(index);
    }
};

};
