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

    bool link(bool reverseIndex, ArchitectureType index, Symbol beta, Symbol gamma) {
        auto& forward = subIndices[index];
        auto outerIter = forward.find(beta);

        if(outerIter == forward.end())
            forward.insert({beta, {gamma}});
        else if(outerIter->second.find(gamma) == outerIter->second.end())
            outerIter->second.insert(gamma);
        else
            return false;

        if(reverseIndex) {
            auto& reverse = subIndices[index+3];
            outerIter = reverse.find(gamma);
            if(outerIter == reverse.end())
                reverse.insert({gamma, {beta}});
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

namespace Ontology {
    enum IndexMode {
        MonoIndex = 1,
        TriIndex = 3,
        HexaIndex = 6
    } indexMode = HexaIndex;
    Symbol nextSymbol = 0;
    std::map<Symbol, std::unique_ptr<SymbolObject>> topIndex;

    SymbolObject* getSymbolObject(Symbol symbol) {
        auto topIter = topIndex.find(symbol);
        assert(topIter != topIndex.end());
        return topIter->second.get();
    }

    void* accessBlobData(Symbol symbol) {
        return getSymbolObject(symbol)->blobData.get();
    }

    ArchitectureType& accessBlobSize(Symbol symbol) {
        return getSymbolObject(symbol)->blobSize;
    }

    void allocateBlob(Symbol symbol, ArchitectureType size, ArchitectureType preserve = 0) {
        SymbolObject* symbolObject = getSymbolObject(symbol);
        if(symbolObject->blobSize == size)
            return;
        ArchitectureType* data;
        if(size) {
            data = new ArchitectureType[(size+ArchitectureSize-1)/ArchitectureSize];
            if(size%ArchitectureSize > 0)
                data[size/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        } else
            data = NULL;
        ArchitectureType length = std::min(std::min(symbolObject->blobSize, size), preserve);
        if(length > 0)
            bitwiseCopy<-1>(data, symbolObject->blobData.get(), 0, 0, length);
        symbolObject->blobSize = size;
        symbolObject->blobData.reset(data);
    }

    void reallocateBlob(Symbol symbol, ArchitectureType size) {
        allocateBlob(symbol, size, size);
    }

    bool overwriteBlobPartial(Symbol dst, Symbol src, ArchitectureType dstOffset, ArchitectureType srcOffset, ArchitectureType length) {
        SymbolObject* dstSymbolObject = getSymbolObject(dst);
        SymbolObject* srcSymbolObject = getSymbolObject(src);
        auto end = dstOffset+length;
        if(end <= dstOffset || end > dstSymbolObject->blobSize)
            return false;
        end = srcOffset+length;
        if(end <= srcOffset || end > srcSymbolObject->blobSize)
            return false;
        bitwiseCopy(dstSymbolObject->blobData.get(), srcSymbolObject->blobData.get(), dstOffset, srcOffset, length);
        return true;
    }

    void cloneBlob(Symbol dst, Symbol src) {
        SymbolObject* dstSymbolObject = getSymbolObject(dst);
        SymbolObject* srcSymbolObject = getSymbolObject(src);
        allocateBlob(dst,  srcSymbolObject->blobSize);
        bitwiseCopy(dstSymbolObject->blobData.get(), srcSymbolObject->blobData.get(), 0, 0, srcSymbolObject->blobSize);
    }

    template<typename DataType>
    void overwriteBlob(Symbol dst, DataType src) {
        SymbolObject* dstSymbolObject = getSymbolObject(dst);
        allocateBlob(dst, sizeof(src)*8);
        *reinterpret_cast<DataType*>(dstSymbolObject->blobData.get()) = src;
    }

    bool eraseFromBlob(Symbol symbol, ArchitectureType begin, ArchitectureType end) {
        SymbolObject* symbolObject = getSymbolObject(symbol);
        if(begin >= end || end > symbolObject->blobSize)
            return false;
        auto rest = symbolObject->blobSize-end;
        if(rest > 0)
            bitwiseCopy(symbolObject->blobData.get(), symbolObject->blobData.get(), begin, end, rest);
        reallocateBlob(symbol, rest+begin);
        return true;
    }

    bool insertIntoBlob(Symbol dst, ArchitectureType* src, ArchitectureType begin, ArchitectureType length) {
        SymbolObject* dstSymbolObject = getSymbolObject(dst);
        assert(length > 0);
        auto newBlobSize = dstSymbolObject->blobSize+length, rest = dstSymbolObject->blobSize-begin;
        if(dstSymbolObject->blobSize >= newBlobSize || begin > dstSymbolObject->blobSize)
            return false;
        reallocateBlob(dst, newBlobSize);
        if(rest > 0)
            bitwiseCopy(dstSymbolObject->blobData.get(), dstSymbolObject->blobData.get(), begin+length, begin, rest);
        bitwiseCopy(dstSymbolObject->blobData.get(), src, begin, 0, length);
        return true;
    }

    int compareBlobs(Symbol symbolA, Symbol symbolB) {
        SymbolObject* symbolObjectA = getSymbolObject(symbolA);
        SymbolObject* symbolObjectB = getSymbolObject(symbolB);
        if(symbolObjectA->blobSize < symbolObjectB->blobSize)
            return -1;
        if(symbolObjectA->blobSize > symbolObjectB->blobSize)
            return 1;
        return memcmp(symbolObjectA->blobData.get(), symbolObjectB->blobData.get(), (symbolObjectA->blobSize+7)/8);
    }

    Symbol create();
    void destroy(Symbol symbol);
};
