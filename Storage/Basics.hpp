#include "StdLib.hpp"

namespace Storage {

template<typename DataType>
struct BitMask {
    const static NativeNaturalType bits = sizeOfInBits<DataType>::value;
    const static DataType empty = 0, one = 1, full = ~empty;
    constexpr static DataType fillLSBs(NativeNaturalType len) {
        return (len == bits) ? full : (one<<len)-one;
    }
    constexpr static DataType fillMSBs(NativeNaturalType len) {
        return (len == 0) ? empty : ~((one<<(bits-len))-one);
    }
    constexpr static NativeNaturalType clz(DataType value);
    constexpr static NativeNaturalType ctz(DataType value);
    constexpr static NativeNaturalType ceilLog2(DataType value) {
        return bits-clz(value);
    }
};

template<>
constexpr NativeNaturalType BitMask<Natural32>::clz(Natural32 value) {
    return __builtin_clzl(value);
}

template<>
constexpr NativeNaturalType BitMask<Natural32>::ctz(Natural32 value) {
    return __builtin_ctzl(value);
}

template<>
constexpr NativeNaturalType BitMask<Natural64>::clz(Natural64 value) {
    return __builtin_clzll(value);
}

template<>
constexpr NativeNaturalType BitMask<Natural64>::ctz(Natural64 value) {
    return __builtin_ctzll(value);
}

constexpr NativeNaturalType architecturePadding(NativeNaturalType bits) {
    return (bits+architectureSize-1)/architectureSize*architectureSize;
}

NativeNaturalType pointerToNatural(void* ptr) {
    return reinterpret_cast<long unsigned>(ptr);
}

template<NativeIntegerType dir>
NativeNaturalType aquireSegmentFrom(const NativeNaturalType* src, NativeNaturalType& srcOffset, NativeNaturalType length) {
    if(dir == +1)
        srcOffset -= length;
    NativeNaturalType lower = srcOffset%architectureSize,
                      index = srcOffset/architectureSize,
                      firstPart = architectureSize-lower,
                      result = src[index]>>lower;
    if(dir == -1)
        srcOffset += length;
    if(firstPart < length)
        result |= src[index+1]<<firstPart;
    return result&BitMask<NativeNaturalType>::fillLSBs(length);
}

void writeSegmentTo(NativeNaturalType* dst, NativeNaturalType keepMask, NativeNaturalType input) {
    *dst &= keepMask;
    *dst |= (~keepMask)&input;
}

NativeIntegerType bitwiseCompare(const NativeNaturalType* a, const NativeNaturalType* b,
                                 NativeNaturalType aOffset, NativeNaturalType bOffset,
                                 NativeNaturalType length) {
    // TODO: Endian order
    while(length >= architectureSize) {
        NativeIntegerType diff = aquireSegmentFrom<-1>(a, aOffset, architectureSize)-aquireSegmentFrom<-1>(b, bOffset, architectureSize);
        if(diff != 0)
            return diff;
        length -= architectureSize;
    }
    if(length == 0)
        return 0;
    return aquireSegmentFrom<0>(a, aOffset, length)-aquireSegmentFrom<0>(b, bOffset, length);
}

template<bool atEnd = false>
bool substrEqual(const char* a, const char* b) {
    NativeNaturalType aOffset, aLen = strlen(a), bLen = strlen(b);
    if(atEnd) {
        if(aLen < bLen)
            return false;
        aOffset = (aLen-bLen)*8;
    } else if(aLen != bLen)
        return false;
    else
        aOffset = 0;
    return bitwiseCompare(reinterpret_cast<const NativeNaturalType*>(a),
                          reinterpret_cast<const NativeNaturalType*>(b),
                          aOffset, 0, bLen*8) == 0;
}

template<NativeIntegerType dir>
void bitwiseCopy(NativeNaturalType* dst, const NativeNaturalType* src,
                 NativeNaturalType dstOffset, NativeNaturalType srcOffset,
                 NativeNaturalType length) {
    assert(length > 0);
    NativeNaturalType index, lastIndex, lowSkip, highSkip;
    if(dir == -1) {
        index = dstOffset/architectureSize;
        lastIndex = (dstOffset+length-1)/architectureSize;
        highSkip = lastIndex;
    } else {
        index = (dstOffset+length-1)/architectureSize;
        lastIndex = dstOffset/architectureSize;
        highSkip = index;
    }
    lowSkip = dstOffset%architectureSize;
    highSkip = (highSkip+1)*architectureSize-dstOffset-length;
    if(index == lastIndex) {
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillLSBs(lowSkip)|BitMask<NativeNaturalType>::fillMSBs(highSkip),
                       aquireSegmentFrom<0>(src, srcOffset, length)<<lowSkip);
        return;
    }
    if(dir == -1) {
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillLSBs(lowSkip),
                       aquireSegmentFrom<dir>(src, srcOffset, architectureSize-lowSkip)<<lowSkip);
        while(++index < lastIndex)
            dst[index] = aquireSegmentFrom<dir>(src, srcOffset, architectureSize);
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillMSBs(highSkip),
                       aquireSegmentFrom<dir>(src, srcOffset, architectureSize-highSkip));
    } else {
        srcOffset += length;
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillMSBs(highSkip),
                       aquireSegmentFrom<dir>(src, srcOffset, architectureSize-highSkip));
        while(--index > lastIndex)
            dst[index] = aquireSegmentFrom<dir>(src, srcOffset, architectureSize);
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillLSBs(lowSkip),
                       aquireSegmentFrom<dir>(src, srcOffset, architectureSize-lowSkip)<<lowSkip);
    }
}

void bitwiseCopy(NativeNaturalType* dst, const NativeNaturalType* src,
                 NativeNaturalType dstOffset, NativeNaturalType srcOffset,
                 NativeNaturalType length) {
    bool downward;
    if(dst == src) {
        if(dstOffset == srcOffset)
            return;
        downward = (dstOffset < srcOffset);
    } else
        downward = (dst < src);
    if(downward)
        bitwiseCopy<-1>(dst, src, dstOffset, srcOffset, length);
    else
        bitwiseCopy<+1>(dst, src, dstOffset, srcOffset, length);
}

template<bool swap>
void bitwiseCopySwap(typename conditional<swap, const NativeNaturalType*, NativeNaturalType*>::type aPtr,
                     typename conditional<swap, NativeNaturalType*, const NativeNaturalType*>::type bPtr,
                     NativeNaturalType aOffset, NativeNaturalType bOffset, NativeNaturalType length);

template<>
void bitwiseCopySwap<false>(NativeNaturalType* aPtr, const NativeNaturalType* bPtr,
                            NativeNaturalType aOffset, NativeNaturalType bOffset, NativeNaturalType length) {
    bitwiseCopy<-1>(aPtr, bPtr, aOffset, bOffset, length);
}

template<>
void bitwiseCopySwap<true>(const NativeNaturalType* aPtr, NativeNaturalType* bPtr,
                           NativeNaturalType aOffset, NativeNaturalType bOffset, NativeNaturalType length) {
    bitwiseCopy<-1>(bPtr, aPtr, bOffset, aOffset, length);
}

const NativeNaturalType
      bitsPerPage = 1<<15,
      minPageCount = 1,
      maxPageCount = 2000*512;

struct Stats {
    NativeNaturalType elementCount,
                      uninhabitable,
                      totalMetaData,
                      totalPayload,
                      inhabitedMetaData,
                      inhabitedPayload;
};

struct BasePage {
    NativeNaturalType transaction;
};

void resizeMemory(NativeNaturalType pageCount);

// TODO: Remove
template<typename DataType = NativeNaturalType>
DataType* dereferenceBits(Symbol address);

template<typename PageType>
PageType* dereferencePage(PageRefType pageRef);
PageRefType referenceOfPage(void* page);
PageRefType aquirePage();
void releasePage(PageRefType pageRef);

};
