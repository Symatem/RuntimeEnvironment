#include "Blob.hpp"

template<typename T, bool guarded>
struct Vector {
    Symbol symbol;

    Vector() :symbol(PreDef_Void) { }

    ~Vector() {
        if(guarded && symbol != PreDef_Void)
            Ontology::destroy(symbol);
    }

    bool empty() const {
        return (symbol == PreDef_Void || Ontology::accessBlobSize(symbol) == 0);
    }

    ArchitectureType size() const {
        return (symbol != PreDef_Void) ? Ontology::accessBlobSize(symbol)/(sizeof(T)*8) : 0;
    }

    T& operator[](ArchitectureType at) const {
        assert(symbol != PreDef_Void && at < size());
        return *(reinterpret_cast<T*>(Ontology::accessBlobData(symbol))+at);
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

    void activate() {
        if(symbol == PreDef_Void) {
            assert(guarded);
            symbol = Ontology::create();
        }
    }

    void clear() {
        if(symbol != PreDef_Void)
            Ontology::allocateBlob(symbol, 0);
    }

    void push_back(T element) {
        activate();
        Ontology::reallocateBlob(symbol, Ontology::accessBlobSize(symbol)+sizeof(T)*8);
        back() = element;
    }

    T pop_back() {
        assert(symbol != PreDef_Void);
        assert(Ontology::accessBlobSize(symbol) >= sizeof(T)*8);
        T element = back();
        Ontology::reallocateBlob(symbol, Ontology::accessBlobSize(symbol)-sizeof(T)*8);
        return element;
    }

    void insert(ArchitectureType at, T element) {
        activate();
        Ontology::insertIntoBlob(symbol, reinterpret_cast<ArchitectureType*>(&element), at*sizeof(T)*8, sizeof(T)*8);
    }

    void erase(ArchitectureType begin, ArchitectureType end) {
        assert(symbol != PreDef_Void);
        assert(begin < end);
        assert(Ontology::accessBlobSize(symbol) >= (end-begin)*sizeof(T)*8);
        Ontology::eraseFromBlob(symbol, begin*sizeof(T)*8, end*sizeof(T)*8);
    }

    void erase(ArchitectureType at) {
        erase(at, at+1);
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
        return binarySearch<ArchitectureType>(Super::size(), [&](ArchitectureType at) {
            return Ontology::compareBlobs(key, (*this)[at]) < 0;
        });
    }

    bool findElement(Symbol element, ArchitectureType& at) const {
        at = blobFindIndexFor(element);
        if(at == Super::size())
            return false;
        return (Ontology::compareBlobs(element, (*this)[at]) == 0);
    }

    void insertElement(Symbol& element) {
        ArchitectureType at;
        if(findElement(element, at)) {
            Ontology::destroy(element);
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
