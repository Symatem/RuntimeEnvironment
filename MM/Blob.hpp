#include "BpTree.hpp"

const char* HRLRawBegin = "raw:";

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
            bitwiseCopy<-1>(_data.get(), data.get(), 0, 0, length);
        size = _size;
        data = std::move(_data);
    }

    void reallocate(ArchitectureType _size) {
        allocate(_size, _size);
    }

    void overwrite(ArchitectureType _size, const ArchitectureType* ptr) {
        allocate(_size);
        if(size > 0)
            bitwiseCopy<-1>(data.get(), ptr, 0, 0, size);
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
        bitwiseCopy(data.get(), other.data.get(), dstOffset, srcOffset, length);
        return true;
    }
};
