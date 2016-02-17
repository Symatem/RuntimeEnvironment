#include "../Storage/BpTree.hpp"

#define PreDefWrapper(token) PreDef_##token
enum PreDefSymbols {
#include "PreDefSymbols.hpp"
};
#undef PreDefWrapper

#define PreDefWrapper(token) #token
const char* PreDefSymbols[] = {
#include "PreDefSymbols.hpp"
};
#undef PreDefWrapper

typedef ArchitectureType Symbol;
struct Exception {};
enum IndexType {
    EAV = 0, AVE = 1, VEA = 2,
    EVA = 3, AEV = 4, VAE = 5
};

union Triple {
    Symbol pos[3];
    struct {
        Symbol entity, attribute, value;
    };
    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :entity(_entity), attribute(_attribute), value(_value) {}
    bool operator<(const Triple& other) const {
        for(ArchitectureType i = 0; i < 3; ++i)
            if(pos[i] < other.pos[i]) return true;
            else if(pos[i] > other.pos[i]) return false;
        return false;
    }
    Triple reordered(ArchitectureType to) {
        ArchitectureType alpha[] = { 0, 1, 2, 0, 1, 2 },
                          beta[] = { 1, 2, 0, 2, 0, 1 },
                         gamma[] = { 2, 0, 1, 1, 2, 0 };
        return {pos[alpha[to]], pos[beta[to]], pos[gamma[to]]};
    }
    Triple normalized(ArchitectureType from) {
        ArchitectureType alpha[] = { 0, 2, 1, 0, 1, 2 },
                          beta[] = { 1, 0, 2, 2, 0, 1 },
                         gamma[] = { 2, 1, 0, 1, 2, 0 };
        return {pos[alpha[from]], pos[beta[from]], pos[gamma[from]]};
    }
};

struct SymbolObject {
    ArchitectureType blobSize;
    std::unique_ptr<ArchitectureType> blobData;
    std::map<Symbol, std::set<Symbol>> subIndices[6];

    void allocateBlob(ArchitectureType size, ArchitectureType preserve = 0) {
        if(blobSize == size)
            return;
        ArchitectureType* data;
        if(size) {
            data = new ArchitectureType[(size+ArchitectureSize-1)/ArchitectureSize];
            if(size%ArchitectureSize > 0)
                data[size/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        } else
            data = NULL;
        ArchitectureType length = std::min(std::min(blobSize, size), preserve);
        if(length > 0)
            bitwiseCopy<-1>(data, blobData.get(), 0, 0, length);
        blobSize = size;
        blobData.reset(data);
    }

    void reallocateBlob(ArchitectureType _size) {
        allocateBlob(_size, _size);
    }

    bool overwriteBlobPartial(const SymbolObject& src, ArchitectureType length, ArchitectureType dstOffset, ArchitectureType srcOffset) {
        auto end = dstOffset+length;
        if(end <= dstOffset || end > blobSize)
            return false;
        end = srcOffset+length;
        if(end <= srcOffset || end > src.blobSize)
            return false;
        bitwiseCopy(blobData.get(), src.blobData.get(), dstOffset, srcOffset, length);
        return true;
    }

    void overwriteBlob(const SymbolObject& src) {
        allocateBlob(src.blobSize);
        overwriteBlobPartial(src, src.blobSize, 0, 0);
    }

    template<typename DataType>
    void overwriteBlob(DataType src) {
        allocateBlob(sizeof(src)*8);
        *reinterpret_cast<DataType*>(blobData.get()) = src;
    }

    int compareBlob(const SymbolObject& other) const {
        if(blobSize < other.blobSize)
            return -1;
        if(blobSize > other.blobSize)
            return 1;
        return memcmp(blobData.get(), other.blobData.get(), (blobSize+7)/8);
    }

    bool link(bool reverseIndex, ArchitectureType index, Symbol beta, Symbol gamma) {
        auto& forward = subIndices[index];
        auto outerIter = forward.find(beta);

        if(outerIter == forward.end())
            forward.insert(std::make_pair(beta, std::set<Symbol>{gamma}));
        else if(outerIter->second.find(gamma) == outerIter->second.end())
            outerIter->second.insert(gamma);
        else
            return false;

        if(reverseIndex) {
            auto& reverse = subIndices[index+3];
            outerIter = reverse.find(gamma);
            if(outerIter == reverse.end())
                reverse.insert(std::make_pair(gamma, std::set<Symbol>{beta}));
            else if(outerIter->second.find(beta) == outerIter->second.end())
                outerIter->second.insert(beta);
        }

        return true;
    }

    bool unlink(bool reverseIndex, ArchitectureType index, Symbol beta, Symbol gamma) {
        auto& forward = subIndices[index];
        auto outerIter = forward.find(beta);
        if(outerIter == forward.end())
            return false;
        auto innerIter = outerIter->second.find(gamma);
        if(innerIter == outerIter->second.end())
            return false;
        outerIter->second.erase(innerIter);
        if(outerIter->second.empty())
            forward.erase(outerIter);

        if(reverseIndex) {
            auto& reverse = subIndices[index+3];
            auto outerIter = reverse.find(gamma);
            auto innerIter = outerIter->second.find(beta);
            outerIter->second.erase(innerIter);
            if(outerIter->second.empty())
                reverse.erase(outerIter);
        }

        return true;
    };
};
