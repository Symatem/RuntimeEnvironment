#include "../Platform/CppDummy.hpp"

class BasePage {
    public:
    NativeNaturalType transaction;
};

namespace Storage {
    template<int dir>
    NativeNaturalType aquireSegmentFrom(const NativeNaturalType* src, NativeNaturalType& srcOffset, NativeNaturalType length) {
        if(dir == +1)
            srcOffset -= length;
        NativeNaturalType lower = srcOffset%ArchitectureSize,
                         index = srcOffset/ArchitectureSize,
                         firstPart = ArchitectureSize-lower,
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
        while(length >= ArchitectureSize) {
            NativeIntegerType diff = aquireSegmentFrom<-1>(a, aOffset, ArchitectureSize)-aquireSegmentFrom<-1>(b, bOffset, ArchitectureSize);
            if(diff != 0)
                return diff;
            length -= ArchitectureSize;
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

    template<int dir>
    void bitwiseCopy(NativeNaturalType* dst, const NativeNaturalType* src,
                     NativeNaturalType dstOffset, NativeNaturalType srcOffset,
                     NativeNaturalType length) {
        assert(length > 0);
        NativeNaturalType index, lastIndex, lowSkip, highSkip;
        if(dir == -1) {
            index = dstOffset/ArchitectureSize;
            lastIndex = (dstOffset+length-1)/ArchitectureSize;
            highSkip = lastIndex;
        } else {
            index = (dstOffset+length-1)/ArchitectureSize;
            lastIndex = dstOffset/ArchitectureSize;
            highSkip = index;
        }
        lowSkip = dstOffset%ArchitectureSize;
        highSkip = (highSkip+1)*ArchitectureSize-dstOffset-length;
        if(index == lastIndex) {
            writeSegmentTo(dst+index,
                           BitMask<NativeNaturalType>::fillLSBs(lowSkip)|BitMask<NativeNaturalType>::fillMSBs(highSkip),
                           aquireSegmentFrom<0>(src, srcOffset, length)<<lowSkip);
            return;
        }
        if(dir == -1) {
            writeSegmentTo(dst+index,
                           BitMask<NativeNaturalType>::fillLSBs(lowSkip),
                           aquireSegmentFrom<dir>(src, srcOffset, ArchitectureSize-lowSkip)<<lowSkip);
            while(++index < lastIndex)
                dst[index] = aquireSegmentFrom<dir>(src, srcOffset, ArchitectureSize);
            writeSegmentTo(dst+index,
                           BitMask<NativeNaturalType>::fillMSBs(highSkip),
                           aquireSegmentFrom<dir>(src, srcOffset, ArchitectureSize-highSkip));
        } else {
            srcOffset += length;
            writeSegmentTo(dst+index,
                           BitMask<NativeNaturalType>::fillMSBs(highSkip),
                           aquireSegmentFrom<dir>(src, srcOffset, ArchitectureSize-highSkip));
            while(--index > lastIndex)
                dst[index] = aquireSegmentFrom<dir>(src, srcOffset, ArchitectureSize);
            writeSegmentTo(dst+index,
                           BitMask<NativeNaturalType>::fillLSBs(lowSkip),
                           aquireSegmentFrom<dir>(src, srcOffset, ArchitectureSize-lowSkip)<<lowSkip);
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

    NativeNaturalType bytesForPages(NativeNaturalType _pageCount) {
        return (_pageCount*bitsPerPage+mmapBucketSize-1)/mmapBucketSize*mmapBucketSize/8;
    }

    int file;
    char* ptr;
    PageRefType pageCount;

    void resizeMemory(NativeNaturalType pageCount);
    void load();
    void unload();

    template<typename DataType = NativeNaturalType>
    DataType* dereferenceBits(Symbol address) {
        return reinterpret_cast<DataType*>(address/8+ptr);
    }

    template<typename PageType>
    PageType* dereferencePage(PageRefType pageRef) {
        assert(pageRef < pageCount);
        return reinterpret_cast<PageType*>(ptr+bitsPerPage/8*pageRef);
    }

    PageRefType aquirePage();
    void releasePage(PageRefType pageRef);
};

// TODO: Use inline linked list instead
class PagePool {
    typedef uint16_t IndexType;
    PageRefType rootPageRef;

    public:
    class Page : public BasePage {
        public:
        static const NativeNaturalType
            HeaderBits = sizeof(BasePage)*8+sizeof(IndexType)*8+sizeof(PageRefType)*8,
            BodyBits = Storage::bitsPerPage-HeaderBits,
            PageRefBits = sizeof(PageRefType)*8,
            Capacity = (BodyBits-PageRefBits)/PageRefBits,
            PageRefsBitOffset = architecturePadding(HeaderBits);

        IndexType count;
        PageRefType chain, pageRefs[Capacity];

        void init(PageRefType _chain = 0) {
            count = 0;
            chain = _chain;
        }

        bool contains(PageRefType item) {
            for(NativeNaturalType i = 0; i < count; ++i)
                if(pageRefs[i] == item)
                    return true;
            return false;
        }

        void debugPrint() {
            printf("%hu\n", count);
            for(NativeNaturalType i = 0; i < count; ++i) {
                if(i > 0)
                    printf(", ");
                printf("%llu", pageRefs[i]);
            }
            printf("\n");
        }

        void push(PageRefType pageRef) {
            assert(count < Capacity);
            pageRefs[count++] = pageRef;
        }

        PageRefType pop() {
            assert(count > 0);
            return pageRefs[--count];
        }
    };

    void init() {
        rootPageRef = 0;
    }

    bool empty() const {
        return (rootPageRef == 0);
    }

    bool contains(PageRefType item) {
        PageRefType pageRef = rootPageRef;
        while(pageRef != 0) {
            auto page = Storage::dereferencePage<Page>(pageRef);
            if(pageRef == item || page->contains(item))
                return true;
            pageRef = page->chain;
        }
        return false;
    }

    void debugPrint() {
        PageRefType pageRef = rootPageRef;
        while(pageRef != 0) {
            auto page = Storage::dereferencePage<Page>(pageRef);
            printf("Page %llu ", pageRef);
            page->debugPrint();
            pageRef = page->chain;
        }
    }

    void push(PageRefType pageRef) {
        if(rootPageRef == 0) {
            rootPageRef = pageRef;
            auto page = Storage::dereferencePage<Page>(rootPageRef);
            page->init();
            return;
        }
        auto page = Storage::dereferencePage<Page>(rootPageRef);
        if(page->count == Page::Capacity) {
            PageRefType chain = rootPageRef;
            rootPageRef = pageRef;
            page = Storage::dereferencePage<Page>(rootPageRef);
            page->init(chain);
        } else
            page->push(pageRef);
    }

    PageRefType pop() {
        assert(!empty());
        auto page = Storage::dereferencePage<Page>(rootPageRef);
        if(page->count == 0) {
            PageRefType pageRef = rootPageRef;
            rootPageRef = page->chain;
            return pageRef;
        } else
            return page->pop();
    }
};

class SuperPage : public BasePage {
    public:
    PagePool freePool;
};

namespace Storage {
    PageRefType aquirePage() {
        assert(ptr);
        PageRefType pageRef;
        auto superPage = dereferencePage<SuperPage>(0);
        if(superPage->freePool.empty()) {
            resizeMemory(pageCount+1);
            pageRef = pageCount-1;
        } else
            pageRef = superPage->freePool.pop();
        return pageRef;
    }

    void releasePage(PageRefType pageRef) {
        assert(ptr);
        if(pageRef == pageCount-1)
            resizeMemory(pageCount-1);
        else
            dereferencePage<SuperPage>(0)->freePool.push(pageRef);
    }
};
