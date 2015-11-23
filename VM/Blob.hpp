#include "Definition.hpp"

template<typename type>
struct BitMask {
    const static type empty = 0, one = 1, full = ~empty;
    constexpr static type fillLSBs(uint64_t len) {
        return (len == sizeof(type)*8) ? full : (one<<len)-one;
    }
    constexpr static type fillMSBs(uint64_t len) {
        return (len == sizeof(type)*8) ? full : ~((one<<(sizeof(type)*8-len))-one);
    }
};

ArchitectureType aquireSegmentFrom(const ArchitectureType* src, uint64_t& srcOffset, uint64_t totalLen) {
    auto index = srcOffset/ArchitectureSize,
         begin = srcOffset%ArchitectureSize,
         firstPart = ArchitectureSize-begin;
    srcOffset += totalLen;

    if(firstPart < totalLen)
        return (src[index]>>begin)|((src[index+1]&BitMask<ArchitectureType>::fillLSBs(totalLen-firstPart))<<firstPart);
    else
        return (src[index]>>begin)&BitMask<ArchitectureType>::fillLSBs(totalLen);
}

void bitWiseCopyForward(ArchitectureType* dst, const ArchitectureType* src,
                        ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
    auto index = dstOffset/ArchitectureSize,
         endIndex = (dstOffset+length)/ArchitectureSize,
         beginLen = ArchitectureSize-(dstOffset%ArchitectureSize),
         endLen = (dstOffset+length)%ArchitectureSize;

    if(index == endIndex) {
        ArchitectureType mask = BitMask<ArchitectureType>::fillLSBs(endLen)<<dstOffset;
        dst[index] = (dst[index]&~mask)|(mask&(aquireSegmentFrom(src, srcOffset, endLen)<<dstOffset));
        return;
    }

    ArchitectureType mask = BitMask<ArchitectureType>::fillMSBs(beginLen);
    dst[index] = (dst[index]&~mask)|(mask&(aquireSegmentFrom(src, srcOffset, beginLen)<<(ArchitectureSize-beginLen)));

    while(++index < endIndex)
        dst[index] = aquireSegmentFrom(src, srcOffset, ArchitectureSize);

    if(endLen == 0) return;
    mask = BitMask<ArchitectureType>::fillLSBs(endLen);
    dst[index] = (dst[index]&~mask)|(mask&aquireSegmentFrom(src, srcOffset, endLen));
}

void bitWiseCopyReverse(ArchitectureType* dst, const ArchitectureType* src,
                        ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
    auto index = (dstOffset+length)/ArchitectureSize,
         endIndex = dstOffset/ArchitectureSize,
         beginLen = (dstOffset+length)%ArchitectureSize,
         endLen = ArchitectureSize-(dstOffset%ArchitectureSize);

    if(index == endIndex) {
        ArchitectureType mask = BitMask<ArchitectureType>::fillLSBs(endLen)<<dstOffset;
        dst[index] = (dst[index]&~mask)|(mask&(aquireSegmentFrom(src, srcOffset, endLen)<<dstOffset));
        return;
    }

    ArchitectureType mask = BitMask<ArchitectureType>::fillLSBs(beginLen);
    dst[index] = (dst[index]&~mask)|(mask&aquireSegmentFrom(src, srcOffset, beginLen));

    while(--index > endIndex)
        dst[index] = aquireSegmentFrom(src, srcOffset, ArchitectureSize);

    if(endLen == 0) return;
    mask = BitMask<ArchitectureType>::fillMSBs(endLen);
    dst[index] = (dst[index]&~mask)|(mask&aquireSegmentFrom(src, srcOffset, endLen));
}

void bitWiseCopy(ArchitectureType* dst, const ArchitectureType* src,
                 ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
    bool reverse;
    if(dst == src) {
        if(dstOffset == srcOffset) return;
        reverse = (dstOffset > srcOffset);
    } else
        reverse = (dst > src);
    if(reverse)
        bitWiseCopyForward(dst, src, length, dstOffset, srcOffset);
    else
        bitWiseCopyReverse(dst, src, length, dstOffset, srcOffset);
}

class Blob {
    static std::unique_ptr<ArchitectureType> getMemory(ArchitectureType size) {
        if(size == 0) return nullptr;
        auto data = new ArchitectureType[(size+ArchitectureSize-1)/ArchitectureSize];
        if(size%ArchitectureSize > 0)
            data[size/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        return std::unique_ptr<ArchitectureType>(data);
    }

    public:
    ArchitectureType size;
    std::unique_ptr<ArchitectureType> data;

    Blob() :size(0) { }

    Blob(Blob&& other) :size(other.size), data(std::move(other.data)) {
        other.size = 0;
    }

    Blob& operator=(Blob&& other) {
        size = other.size;
        other.size = 0;
        data = std::move(other.data);
        return *this;
    }

    int compare(const Blob& other) const {
        if(size < other.size) return -1;
        if(size > other.size) return 1;
        return memcmp(data.get(), other.data.get(), (size+7)/8);
    }

    void clear() {
        if(size == 0) return;
        size = 0;
        data.reset();
    }

    void allocate(ArchitectureType _size, ArchitectureType preserve = 0) {
        if(size == _size) return;
        auto _data = getMemory(_size);
        ArchitectureType length = std::min(std::min(size, _size), preserve);
        if(length > 0)
            bitWiseCopyForward(_data.get(), data.get(), length, 0, 0);
        size = _size;
        data = std::move(_data);
    }

    void reallocate(ArchitectureType _size) {
        allocate(_size, _size);
    }

    void overwrite(ArchitectureType _size, const ArchitectureType* ptr) {
        allocate(_size);
        if(size > 0)
            bitWiseCopyForward(data.get(), ptr, size, 0, 0);
    }

    void overwrite(uint64_t value) {
        overwrite(ArchitectureSize, reinterpret_cast<const ArchitectureType*>(&value));
    }

    void overwrite(int64_t value) {
        overwrite(ArchitectureSize, reinterpret_cast<const ArchitectureType*>(&value));
    }

    void overwrite(double value) {
        overwrite(ArchitectureSize, reinterpret_cast<const ArchitectureType*>(&value));
    }

    void overwrite(const char* str) {
        overwrite(strlen(str)*8, reinterpret_cast<const ArchitectureType*>(str));
    }

    void overwrite(const Blob& other) {
        overwrite(other.size, other.data.get());
    }

    void overwrite(std::istream& stream) {
        stream.seekg(0, std::ios::end);
        std::streamsize len = stream.tellg();
        stream.seekg(0, std::ios::beg);
        allocate(len*8);
        stream.read(reinterpret_cast<char*>(data.get()), len);
    }

    bool replacePartial(const Blob& other, ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
        auto end = dstOffset+length;
        if(end <= dstOffset || end > size) return false;
        end = srcOffset+length;
        if(end <= srcOffset || end > other.size) return false;
        bitWiseCopy(data.get(), other.data.get(), length, dstOffset, srcOffset);
        return true;
    }
};
