#include "../Platform/CppDummy.hpp"

namespace Storage {

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

struct FreePage : public BasePage {
    PageRefType next;
};

struct SuperPage : public BasePage {
    PageRefType freePage;
};

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

const NativeNaturalType
      bitsPerPage = 1<<15,
      mmapBucketSize = 1<<24,
      minPageCount = 1,
      maxPageCount = 2000*512;

void* heapBegin;
PageRefType pageCount;

void resizeMemory(NativeNaturalType pageCount);

template<typename DataType = NativeNaturalType>
DataType* dereferenceBits(Symbol address) {
    return reinterpret_cast<DataType*>(reinterpret_cast<char*>(heapBegin)+address/8);
}

template<typename PageType>
PageType* dereferencePage(PageRefType pageRef) {
    assert(pageRef < pageCount);
    return reinterpret_cast<PageType*>(reinterpret_cast<char*>(heapBegin)+bitsPerPage/8*pageRef);
}

PageRefType aquirePage() {
    assert(heapBegin);
    auto superPage = dereferencePage<SuperPage>(0);
    if(superPage->freePage) {
        PageRefType pageRef = superPage->freePage;
        auto freePage = dereferencePage<FreePage>(pageRef);
        superPage->freePage = freePage->next;
        return pageRef;
    } else {
        resizeMemory(pageCount+1);
        return pageCount-1;
    }
}

void releasePage(PageRefType pageRef) {
    assert(heapBegin);
    if(pageRef == pageCount-1)
        resizeMemory(pageCount-1);
    else {
        auto superPage = dereferencePage<SuperPage>(0);
        auto freePage = dereferencePage<FreePage>(pageRef);
        freePage->next = superPage->freePage;
        superPage->freePage = pageRef;
    }
}

NativeNaturalType countFreePages() {
    auto superPage = dereferencePage<SuperPage>(0);
    if(!superPage->freePage)
        return 0;
    NativeNaturalType count = 1;
    auto freePage = dereferencePage<FreePage>(superPage->freePage);
    while(freePage->next) {
        freePage = dereferencePage<FreePage>(freePage->next);
        ++count;
    }
    return count;
}

};
