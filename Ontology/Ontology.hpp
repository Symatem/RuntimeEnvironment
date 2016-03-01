#include "Containers.hpp"

namespace Ontology {
    // TODO: unindexBlob must be called at every blob mutation
    BlobIndex blobIndex;

    std::map<Symbol, std::unique_ptr<SymbolObject>>::iterator SymbolFactory(Symbol symbol) {
        return topIndex.insert(std::make_pair(symbol, std::unique_ptr<SymbolObject>(new SymbolObject()))).first;
    }

    bool link(Triple triple) {
        ArchitectureType indexCount = (Ontology::indexMode == Ontology::MonoIndex) ? 1 : 3;
        bool reverseIndex = (Ontology::indexMode == Ontology::HexaIndex);
        for(ArchitectureType i = 0; i < indexCount; ++i) {
            auto topIter = Ontology::topIndex.find(triple.pos[i]);
            if(topIter == Ontology::topIndex.end())
                topIter = SymbolFactory(triple.pos[i]);
            if(!topIter->second->link(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                return false;
        }
        return true;
    }

    Symbol create() {
        Symbol symbol = nextSymbol++;
        SymbolFactory(symbol);
        return symbol;
    }

    template<typename DataType>
    Symbol createFromData(DataType src) {
        Symbol blobType;
        if(typeid(DataType) == typeid(uint64_t))
            blobType = PreDef_Natural;
        else if(typeid(DataType) == typeid(int64_t))
            blobType = PreDef_Integer;
        else if(typeid(DataType) == typeid(double))
            blobType = PreDef_Float;
        else
            abort();
        Symbol symbol = create();
        link({symbol, PreDef_BlobType, blobType});
        overwriteBlob(symbol, src);
        return symbol;
    }

    Symbol createSymbolFromFile(const char* path) {
        int fd = open(path, O_RDONLY);
        if(fd < 0)
            return PreDef_Void;
        Symbol symbol = create();
        link({symbol, PreDef_BlobType, PreDef_Text});
        ArchitectureType len = lseek(fd, 0, SEEK_END);
        allocateBlob(symbol, len*8);
        lseek(fd, 0, SEEK_SET);
        read(fd, reinterpret_cast<char*>(accessBlobData(symbol)), len);
        close(fd);
        return symbol;
    }

    Symbol createFromData(const char* src, ArchitectureType len) {
        Symbol symbol = create();
        link({symbol, PreDef_BlobType, PreDef_Text});
        allocateBlob(symbol, len*8);
        auto dst = reinterpret_cast<uint8_t*>(accessBlobData(symbol));
        for(ArchitectureType i = 0; i < len; ++i)
            dst[i] = src[i];
        return symbol;
    }

    Symbol createFromData(const char* src) {
        ArchitectureType len = 0;
        while(src[len])
            ++len;
        return createFromData(src, len);
    }

    void checkSymbolLinkCount(Symbol symbol) {
        auto topIter = topIndex.find(symbol);
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        for(ArchitectureType i = 0; i < indexCount; ++i)
            if(!topIter->second->subIndices[i].empty())
                return;
        topIndex.erase(topIter);
    }

    bool unlinkInternal(Triple triple, bool skipEnabled = false, Symbol skip = PreDef_Void) {
        ArchitectureType indexCount = (indexMode == MonoIndex) ? 1 : 3;
        bool reverseIndex = (indexMode == HexaIndex);
        for(ArchitectureType i = 0; i < indexCount; ++i) {
            if(skipEnabled && triple.pos[i] == skip)
                continue;
            auto topIter = topIndex.find(triple.pos[i]);
            if(topIter == topIndex.end() ||
               !topIter->second->unlink(reverseIndex, i, triple.pos[(i+1)%3], triple.pos[(i+2)%3]))
                return false;
        }
        if(triple.pos[1] == PreDef_BlobType)
            blobIndex.eraseElement(triple.pos[0]);
        return true;
    }

    bool unlink(Triple triple) {
        if(!unlinkInternal(triple))
            return false;
        for(ArchitectureType i = 0; i < 3; ++i)
            checkSymbolLinkCount(triple.pos[i]);
        return true;
    }

    void destroy(Symbol alpha) {
        auto topIter = topIndex.find(alpha);
        assert(topIter != topIndex.end());
        Set<Symbol, true> symbols;
        for(ArchitectureType i = EAV; i <= VEA; ++i)
            for(auto& beta : topIter->second->subIndices[i])
                for(auto gamma : beta.second) {
                    unlinkInternal(Triple(alpha, beta.first, gamma).normalized(i), true, alpha);
                    symbols.insertElement(beta.first);
                    symbols.insertElement(gamma);
                }
        topIndex.erase(topIter);
        symbols.iterate([&](Symbol symbol) {
            checkSymbolLinkCount(symbol);
        });
    }

    void fillPreDef() {
        const Symbol preDefSymbolsEnd = sizeof(PreDefSymbols)/sizeof(void*);
        while(nextSymbol < preDefSymbolsEnd) {
            Symbol symbol = createFromData(PreDefSymbols[nextSymbol]);
            link({PreDef_RunTimeEnvironment, PreDef_Holds, symbol});
        }
        for(Symbol symbol = 0; symbol < preDefSymbolsEnd; ++symbol)
            blobIndex.insertElement(symbol);
        Symbol ArchitectureSizeSymbol = createFromData(ArchitectureSize);
        link({PreDef_RunTimeEnvironment, PreDef_Holds, ArchitectureSizeSymbol});
        link({PreDef_RunTimeEnvironment, PreDef_ArchitectureSize, ArchitectureSizeSymbol});
    }
};
