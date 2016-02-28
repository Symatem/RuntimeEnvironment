#include "Ontology.hpp"

namespace Context {
    void destroy(Symbol alpha);
    SymbolObject* getSymbolObject(Symbol symbol);
    Symbol create();
};

template<typename T, bool guarded>
struct Vector {
    Symbol symbol;
    SymbolObject* symbolObject;

    Vector() :symbol(PreDef_Void), symbolObject(NULL) { }

    ~Vector() {
        if(guarded && symbolObject)
            Context::destroy(symbol);
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
        symbolObject = Context::getSymbolObject(symbol);
    }

    void activate() {
        if(!symbolObject)
            setSymbol(Context::create());
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
        SymbolObject* keySymbolObject = Context::getSymbolObject(key);
        return binarySearch<ArchitectureType>(Super::size(), [&](ArchitectureType at) {
            SymbolObject* atSymbolObject = Context::getSymbolObject((*this)[at]);
            return keySymbolObject->compareBlob(*atSymbolObject) < 0;
        });
    }

    bool findElement(Symbol element, ArchitectureType& at) const {
        at = blobFindIndexFor(element);
        if(at == Super::size())
            return false;
        SymbolObject *elementSymbolObject = Context::getSymbolObject(element),
                     *atSymbolObject = Context::getSymbolObject((*this)[at]);
        return (elementSymbolObject->compareBlob(*atSymbolObject) == 0);
    }

    void insertElement(Symbol& element) {
        ArchitectureType at;
        if(findElement(element, at)) {
            Context::destroy(element);
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
