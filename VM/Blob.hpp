#include "Definition.hpp"

template<typename type>
struct BitMask {
    const static type empty = 0, one = 1, full = ~empty;
    constexpr static type fillLSBs(ArchitectureType len) {
        return (len == sizeof(type)*8) ? full : (one<<len)-one;
    }
    constexpr static type fillMSBs(ArchitectureType len) {
        return (len == 0) ? empty : ~((one<<(sizeof(type)*8-len))-one);
    }
};

ArchitectureType aquireSegmentFrom(const ArchitectureType* src, uint64_t& srcOffset, uint64_t length) {
    ArchitectureType lower = srcOffset%ArchitectureSize,
                     index = srcOffset/ArchitectureSize,
                     firstPart = ArchitectureSize-lower,
                     result = src[index]>>lower;
    srcOffset += length;

    if(firstPart < length)
        result |= src[index+1]<<firstPart;

    return result&BitMask<ArchitectureType>::fillLSBs(length);
}

void writeSegmentTo(ArchitectureType* dst, ArchitectureType keepMask, ArchitectureType input) {
    *dst &= keepMask;
    *dst |= (~keepMask)&input;
}

void bitWiseCopyForward(ArchitectureType* dst, const ArchitectureType* src,
                        ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
    assert(length > 0);
    ArchitectureType index = dstOffset/ArchitectureSize,
                     endIndex = (dstOffset+length-1)/ArchitectureSize,
                     lowSkip = dstOffset%ArchitectureSize,
                     highSkip = (endIndex+1)*ArchitectureSize-dstOffset-length;

    if(index == endIndex) {
        writeSegmentTo(dst+index,
                       BitMask<ArchitectureType>::fillLSBs(lowSkip)|BitMask<ArchitectureType>::fillMSBs(highSkip),
                       aquireSegmentFrom(src, srcOffset, length)<<lowSkip);
        return;
    }

    writeSegmentTo(dst+index,
                   BitMask<ArchitectureType>::fillLSBs(lowSkip),
                   aquireSegmentFrom(src, srcOffset, ArchitectureSize-lowSkip)<<lowSkip);

    while(++index < endIndex)
        dst[index] = aquireSegmentFrom(src, srcOffset, ArchitectureSize);

    writeSegmentTo(dst+index,
                   BitMask<ArchitectureType>::fillMSBs(highSkip),
                   aquireSegmentFrom(src, srcOffset, ArchitectureSize-highSkip));
}

void bitWiseCopyReverse(ArchitectureType* dst, const ArchitectureType* src,
                        ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
    assert(length > 0);
    ArchitectureType index = (dstOffset+length-1)/ArchitectureSize,
                     beginIndex = dstOffset/ArchitectureSize,
                     lowSkip = dstOffset%ArchitectureSize,
                     highSkip = (index+1)*ArchitectureSize-dstOffset-length;

    if(index == beginIndex) {
        writeSegmentTo(dst+index,
                       BitMask<ArchitectureType>::fillLSBs(lowSkip)|BitMask<ArchitectureType>::fillMSBs(highSkip),
                       aquireSegmentFrom(src, srcOffset, length)<<lowSkip);
        return;
    }

    writeSegmentTo(dst+index,
                   BitMask<ArchitectureType>::fillMSBs(highSkip),
                   aquireSegmentFrom(src, srcOffset, ArchitectureSize-highSkip));

    while(--index > beginIndex)
        dst[index] = aquireSegmentFrom(src, srcOffset, ArchitectureSize);

    writeSegmentTo(dst+index,
                   BitMask<ArchitectureType>::fillLSBs(lowSkip),
                   aquireSegmentFrom(src, srcOffset, ArchitectureSize-lowSkip)<<lowSkip);
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
        bitWiseCopyReverse(dst, src, length, dstOffset, srcOffset);
    else
        bitWiseCopyForward(dst, src, length, dstOffset, srcOffset);
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

    void serializeRaw(std::ostream& stream) const {
        stream << HRLRawBegin << std::hex << std::uppercase;
        auto ptr = reinterpret_cast<const uint8_t*>(data.get());
        for(size_t i = 0; i < (size+3)/4; ++i)
            stream << ((ptr[i/2]>>((i%2)*4))&0x0F);
        stream << std::dec;
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
