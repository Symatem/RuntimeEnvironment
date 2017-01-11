#include <Foundation/Lambda.hpp>

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

template<typename DataType>
constexpr static DataType swapedEndian(DataType value);

template<>
constexpr Natural16 swapedEndian(Natural16 value) {
    return __builtin_bswap16(value);
}

template<>
constexpr Natural32 swapedEndian(Natural32 value) {
    return __builtin_bswap32(value);
}

template<>
constexpr Natural64 swapedEndian(Natural64 value) {
    return __builtin_bswap64(value);
}

template<NativeIntegerType dir>
NativeNaturalType readSegmentFrom(const NativeNaturalType* src, NativeNaturalType& srcOffset, NativeNaturalType length) {
    if(dir == +1)
        srcOffset -= length;
    NativeNaturalType lower = srcOffset%architectureSize,
                      index = srcOffset/architectureSize,
                      result = src[index]>>lower;
    lower = architectureSize-lower;
    if(lower < length)
        result |= src[index+1]<<lower;
    result &= BitMask<NativeNaturalType>::fillLSBs(length);
    if(dir == -1)
        srcOffset += length;
    return result;
}

void writeSegmentTo(NativeNaturalType* dst, NativeNaturalType keepMask, NativeNaturalType src) {
    *dst &= keepMask;
    *dst |= (~keepMask)&src;
}

NativeIntegerType bitwiseCompare(const NativeNaturalType* a, const NativeNaturalType* b,
                                 NativeNaturalType aOffset, NativeNaturalType bOffset,
                                 NativeNaturalType length) {
    aOffset += length;
    bOffset += length;
    while(length > 0) {
        NativeNaturalType segment = min(length, static_cast<NativeNaturalType>(architectureSize));
        NativeIntegerType diff = readSegmentFrom<+1>(a, aOffset, segment)-readSegmentFrom<+1>(b, bOffset, segment);
        if(diff != 0)
            return diff;
        length -= segment;
    }
    return 0;
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
                       readSegmentFrom<0>(src, srcOffset, length)<<lowSkip);
        return;
    }
    if(dir == -1) {
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillLSBs(lowSkip),
                       readSegmentFrom<dir>(src, srcOffset, architectureSize-lowSkip)<<lowSkip);
        while(++index < lastIndex)
            dst[index] = readSegmentFrom<dir>(src, srcOffset, architectureSize);
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillMSBs(highSkip),
                       readSegmentFrom<dir>(src, srcOffset, architectureSize-highSkip));
    } else {
        srcOffset += length;
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillMSBs(highSkip),
                       readSegmentFrom<dir>(src, srcOffset, architectureSize-highSkip));
        while(--index > lastIndex)
            dst[index] = readSegmentFrom<dir>(src, srcOffset, architectureSize);
        writeSegmentTo(dst+index,
                       BitMask<NativeNaturalType>::fillLSBs(lowSkip),
                       readSegmentFrom<dir>(src, srcOffset, architectureSize-lowSkip)<<lowSkip);
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
