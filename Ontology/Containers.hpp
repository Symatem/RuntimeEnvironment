#include "../Storage/Blob.hpp"

namespace Ontology {
    bool unlink(Identifier symbol);
};

template<bool guarded, typename ElementType>
struct Vector {
    Identifier symbol;

    Vector() :symbol(0) { }

    ~Vector() {
        if(guarded && symbol)
            Ontology::unlink(symbol);
    }

    bool empty() const {
        return (!symbol || Storage::getBlobSize(symbol) == 0);
    }

    ArchitectureType size() const {
        return (symbol) ? Storage::getBlobSize(symbol)/(sizeof(ElementType)*8) : 0;
    }

    ElementType& operator[](ArchitectureType at) const {
        assert(symbol && at < size());
        return *(reinterpret_cast<ElementType*>(Storage::accessBlobData(symbol))+at);
    }

    ElementType& front() const {
        return (*this)[0];
    }

    ElementType& back() const {
        return (*this)[size()-1];
    }

    void iterate(Closure<void, ElementType&> callback) const {
        for(ArchitectureType at = 0; at < size(); ++at)
            callback((*this)[at]);
    }

    void activate() {
        if(!symbol) {
            assert(guarded);
            symbol = Storage::createIdentifier();
        }
    }

    void clear() {
        if(symbol) {
            Storage::setBlobSize(symbol, 0);
            symbol = 0;
        }
    }

    void insert(ArchitectureType at, ElementType element) {
        activate();
        Storage::insertIntoBlob(symbol, reinterpret_cast<ArchitectureType*>(&element), at*sizeof(ElementType)*8, sizeof(ElementType)*8);
    }

    void erase(ArchitectureType begin, ArchitectureType end) {
        assert(symbol);
        assert(begin < end);
        assert(Storage::getBlobSize(symbol) >= end*sizeof(ElementType)*8);
        Storage::eraseFromBlob(symbol, begin*sizeof(ElementType)*8, end*sizeof(ElementType)*8);
    }

    void erase(ArchitectureType at) {
        erase(at, at+1);
    }

    void push_back(ElementType element) {
        insert(size(), element);
    }

    ElementType pop_back() {
        assert(!empty());
        ElementType element = back();
        erase(size()-1);
        return element;
    }
};

template<typename KeyType, typename ValueType>
struct Pair {
    KeyType key;
    ValueType value;
    Pair(KeyType _key) :key(_key) { }
    Pair(KeyType _key, ValueType _value) :key(_key), value(_value) { }
    operator KeyType() {
        return key;
    }
};

template<bool guarded, typename KeyType, typename ValueType = ArchitectureType[0]>
struct Set : public Vector<guarded, Pair<KeyType, ValueType>> {
    typedef Pair<KeyType, ValueType> ElementType;
    typedef Vector<guarded, ElementType> Super;

    Set() :Super() { }

    ArchitectureType find(KeyType key) const {
        return binarySearch<ArchitectureType>(Super::size(), [&](ArchitectureType at) {
            return key > (*this)[at];
        });
    }

    bool find(KeyType key, ArchitectureType& at) const {
        at = find(key);
        return (at < Super::size() && (*this)[at] == key);
    }

    bool insertElement(ElementType element) {
        ArchitectureType at;
        if(find(element.key, at))
            return false;
        Super::insert(at, element);
        return true;
    }

    bool eraseElement(ElementType element) {
        ArchitectureType at;
        if(!find(element.key, at))
            return false;
        Super::erase(at);
        return true;
    }
};

struct BlobIndex : public Set<true, Identifier> {
    typedef Set<true, Identifier> Super;

    BlobIndex() :Super() { }

    ArchitectureType find(Identifier key) const {
        return binarySearch<ArchitectureType>(Super::size(), [&](ArchitectureType at) {
            return Storage::compareBlobs(key, (*this)[at]) < 0;
        });
    }

    bool find(Identifier element, ArchitectureType& at) const {
        at = find(element);
        if(at == Super::size())
            return false;
        return (Storage::compareBlobs(element, (*this)[at]) == 0);
    }

    void insertElement(Identifier& element) {
        ArchitectureType at;
        if(find(element, at)) {
            Ontology::unlink(element);
            element = (*this)[at];
        } else
            Super::insert(at, element);
    }

    bool eraseElement(Identifier element) {
        ArchitectureType at;
        if(!Super::find(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};
