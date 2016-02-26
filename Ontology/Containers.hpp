#include "Context.hpp"

template<typename T, bool guarded>
struct Vector {
    Symbol symbol;
    SymbolObject* symbolObject;

    Vector() :symbol(PreDef_Void), symbolObject(NULL) { }

    ~Vector() {
        if(guarded && symbolObject)
            context.destroy(symbol);
    }

    bool empty() const {
        return (!symbolObject || symbolObject->blobSize == 0);
    }

    ArchitectureType size() const {
        return (symbolObject) ? symbolObject->blobSize/(sizeof(T)*8) : 0;
    }

    T& operator[](ArchitectureType at) const {
        assert(symbolObject);
        return symbolObject->template accessBlobAt<T>(at);
    }

    T& front() const {
        return (*this)[0];
    }

    T& back() const {
        return (*this)[size()-1];
    }

    void iterate(std::function<void(T)> callback) const {
        for(ArchitectureType at = 0; at < size(); ++at)
            callback((*this)[at]);
    }

    void setSymbol(Symbol _symbol) {
        symbol = _symbol;
        symbolObject = context.getSymbolObject(symbol);
    }

    void activate() {
        if(!symbolObject)
            setSymbol(context.create());
    }

    void clear() {
        if(symbolObject)
            symbolObject->allocateBlob(0);
    }

    void push_back(T element) {
        activate();
        symbolObject->reallocateBlob(symbolObject->blobSize+sizeof(T)*8);
        back() = element;
    }

    T pop_back() {
        activate();
        assert(symbolObject->blobSize >= sizeof(T)*8);
        T element = back();
        symbolObject->reallocateBlob(symbolObject->blobSize-sizeof(T)*8);
        return element;
    }

    void insert(ArchitectureType at, T element) {
        activate();
        symbolObject->insertIntoBlob(reinterpret_cast<ArchitectureType*>(&element), at*sizeof(T)*8, sizeof(T)*8);
    }

    void erase(ArchitectureType begin, ArchitectureType end) {
        assert(symbolObject);
        symbolObject->eraseFromBlob(begin*sizeof(T)*8, end*sizeof(T)*8);
    }

    void erase(ArchitectureType at) {
        assert(symbolObject);
        symbolObject->eraseFromBlob(at, at+1);
    }
};

template<typename T, bool guarded>
struct Set : public Vector<T, guarded> {
    typedef Vector<T, guarded> Super;

    Set() :Super() { }

    ArchitectureType blobFindIndexFor(T key) const {
        return binarySearch<ArchitectureType>(Super::size(), [&](ArchitectureType at) {
            return key > (*this)[at];
        });
    }

    bool findElement(T element, ArchitectureType& at) const {
        at = blobFindIndexFor(element);
        return (at < Super::size() && (*this)[at] == element);
    }

    /*bool containsElement(T element) const {
        ArchitectureType at = blobFindIndexFor(element);
        return (at < Super:size() && (*this)[at] == element);
    }*/

    bool insertElement(T element) {
        ArchitectureType at;
        if(findElement(element, at))
            return false;
        Super::insert(at, element);
        return true;
    }

    bool eraseElement(T element) {
        ArchitectureType at;
        if(!findElement(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};

struct BlobIndex : public Set<Symbol, true> {
    typedef Set<Symbol, true> Super;

    BlobIndex() :Super() { }

    ArchitectureType blobFindIndexFor(Symbol key) const {
        SymbolObject* keySymbolObject = context.getSymbolObject(key);
        return binarySearch<ArchitectureType>(Super::size(), [&](ArchitectureType at) {
            SymbolObject* atSymbolObject = context.getSymbolObject((*this)[at]);
            return keySymbolObject->compareBlob(*atSymbolObject) < 0;
        });
    }

    bool findElement(Symbol element, ArchitectureType& at) const {
        at = blobFindIndexFor(element);
        if(at == Super::size())
            return false;
        SymbolObject *elementSymbolObject = context.getSymbolObject(element),
                     *atSymbolObject = context.getSymbolObject((*this)[at]);
        return (elementSymbolObject->compareBlob(*atSymbolObject) == 0);
    }

    void insertElement(Symbol& element) {
        ArchitectureType at;
        if(findElement(element, at)) {
            context.destroy(element);
            element = (*this)[at];
        } else
            Super::insert(at, element);
    }

    bool eraseElement(Symbol element) {
        ArchitectureType at;
        if(!Super::findElement(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};

BlobIndex blobIndex;

ArchitectureType Context::searchGIV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
    auto topIter = topIndex.find(triple.pos[0]);
    if(topIter == topIndex.end())
        throw Exception("Symbol is Nonexistent");
    auto& subIndex = topIter->second->subIndices[index];
    Set<Symbol, true> result;
    for(auto& beta : subIndex)
        for(auto& gamma : beta.second)
            result.insertElement(gamma);
    if(callback)
        result.iterate([&](Symbol gamma) {
            triple.pos[2] = gamma;
            callback();
        });
    return result.size();
}

bool Context::unlinkInternal(Triple triple, bool skipEnabled, Symbol skip) {
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

void Context::destroy(Symbol alpha) {
    auto topIter = topIndex.find(alpha);
    if(topIter == topIndex.end())
        throw Exception("Already destroyed", {
            {PreDef_Entity, alpha}
        });
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
        scrutinizeSymbol(symbol);
    });
}

void Context::scrutinizeHeldBy(Symbol symbol) {
    Set<Symbol, true> symbols;
    symbols.insertElement(symbol);
    while(!symbols.empty()) {
        symbol = symbols.pop_back();
        if(topIndex.find(symbol) == topIndex.end() ||
           query(1, {PreDef_Void, PreDef_Holds, symbol}) > 0)
            continue;
        query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result, ArchitectureType) {
            symbols.insertElement(result.pos[0]);
        });
        destroy(symbol);
    }
}

void Context::setSolitary(Triple triple, bool linkVoid) {
    bool toLink = (linkVoid || triple.value != PreDef_Void);
    Set<Symbol, true> symbols;
    query(9, triple, [&](Triple result, ArchitectureType) {
        if((triple.pos[2] == result.pos[0]) && (linkVoid || result.pos[0] != PreDef_Void))
            toLink = false;
        else
            symbols.insertElement(result.pos[0]);
    });
    if(toLink)
        link(triple);
    symbols.iterate([&](Symbol symbol) {
        unlinkInternal({triple.pos[0], triple.pos[1], symbol});
    });
    if(!linkVoid)
        symbols.insertElement(triple.pos[0]);
    symbols.insertElement(triple.pos[1]);
    symbols.iterate([&](Symbol symbol) {
        scrutinizeSymbol(symbol);
    });
}

void Context::init() {
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
