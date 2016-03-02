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

namespace Ontology {
    enum IndexMode {
        MonoIndex = 1,
        TriIndex = 3,
        HexaIndex = 6
    } indexMode = HexaIndex;
    Symbol nextSymbol = 0;
    struct SymbolObject {
        ArchitectureType blobSize, *blobData;
        std::map<Symbol, std::set<Symbol>> subIndices[6]; // TODO: Use Blob instead

        SymbolObject() :blobSize(0), blobData(NULL) { }

        ~SymbolObject() {
            if(blobSize)
                free(blobData);
        }

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
    std::map<Symbol, std::unique_ptr<SymbolObject>> topIndex; // TODO: Use B+Tree instead

    Symbol create();
    void destroy(Symbol symbol);
    void modifiedBlob(Symbol symbol);

    SymbolObject* getSymbolObject(Symbol symbol) {
        auto topIter = topIndex.find(symbol);
        assert(topIter != topIndex.end());
        return topIter->second.get();
    }

    void* accessBlobData(Symbol symbol) {
        return getSymbolObject(symbol)->blobData;
    }

    ArchitectureType accessBlobSize(Symbol symbol) {
        return getSymbolObject(symbol)->blobSize;
    }

    void allocateBlob(Symbol symbol, ArchitectureType size, ArchitectureType preserve = 0) {
        SymbolObject* symbolObject = getSymbolObject(symbol);
        if(symbolObject->blobSize == size)
            return;
        ArchitectureType* data;
        if(size) {
            data = reinterpret_cast<ArchitectureType*>(malloc((size+ArchitectureSize-1)/ArchitectureSize*ArchitectureSize));
            if(size%ArchitectureSize > 0)
                data[size/ArchitectureSize] &= BitMask<ArchitectureType>::fillLSBs(size%ArchitectureSize);
        } else
            data = NULL;
        if(symbolObject->blobSize) {
            ArchitectureType length = std::min(std::min(symbolObject->blobSize, size), preserve);
            if(length > 0)
                bitwiseCopy<-1>(data, symbolObject->blobData, 0, 0, length);
            free(symbolObject->blobData);
        }
        symbolObject->blobSize = size;
        symbolObject->blobData = data;
        modifiedBlob(symbol);
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
        bitwiseCopy(dstSymbolObject->blobData, srcSymbolObject->blobData, dstOffset, srcOffset, length);
        modifiedBlob(dst);
        return true;
    }

    void cloneBlob(Symbol dst, Symbol src) {
        SymbolObject* dstSymbolObject = getSymbolObject(dst);
        SymbolObject* srcSymbolObject = getSymbolObject(src);
        allocateBlob(dst,  srcSymbolObject->blobSize);
        bitwiseCopy(dstSymbolObject->blobData, srcSymbolObject->blobData, 0, 0, srcSymbolObject->blobSize);
        modifiedBlob(dst);
    }

    template<typename DataType>
    void overwriteBlob(Symbol dst, DataType src) {
        SymbolObject* dstSymbolObject = getSymbolObject(dst);
        allocateBlob(dst, sizeof(src)*8);
        *reinterpret_cast<DataType*>(dstSymbolObject->blobData) = src;
        modifiedBlob(dst);
    }

    bool eraseFromBlob(Symbol symbol, ArchitectureType begin, ArchitectureType end) {
        SymbolObject* symbolObject = getSymbolObject(symbol);
        if(begin >= end || end > symbolObject->blobSize)
            return false;
        auto rest = symbolObject->blobSize-end;
        if(rest > 0)
            bitwiseCopy(symbolObject->blobData, symbolObject->blobData, begin, end, rest);
        reallocateBlob(symbol, rest+begin);
        modifiedBlob(symbol);
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
            bitwiseCopy(dstSymbolObject->blobData, dstSymbolObject->blobData, begin+length, begin, rest);
        bitwiseCopy(dstSymbolObject->blobData, src, begin, 0, length);
        modifiedBlob(dst);
        return true;
    }

    int compareBlobs(Symbol symbolA, Symbol symbolB) {
        SymbolObject* symbolObjectA = getSymbolObject(symbolA);
        SymbolObject* symbolObjectB = getSymbolObject(symbolB);
        if(symbolObjectA->blobSize < symbolObjectB->blobSize)
            return -1;
        if(symbolObjectA->blobSize > symbolObjectB->blobSize)
            return 1;
        return memcmp(symbolObjectA->blobData, symbolObjectB->blobData, (symbolObjectA->blobSize+7)/8);
    }
};
