#include "../Storage/Blob.hpp"

namespace Ontology {
    bool unlink(Symbol symbol);
};

template<bool guarded, typename ElementType>
struct Vector {
    Symbol symbol;

    Vector() :symbol(0) {}

    ~Vector() {
        if(guarded && symbol)
            Ontology::unlink(symbol);
    }

    bool empty() const {
        return (!symbol || Storage::getBlobSize(symbol) == 0);
    }

    NativeNaturalType size() const {
        return (symbol) ? Storage::getBlobSize(symbol)/(sizeof(ElementType)*8) : 0;
    }

    ElementType& operator[](NativeNaturalType at) const {
        assert(symbol && at < size());
        return *(reinterpret_cast<ElementType*>(Storage::accessBlobData(symbol))+at);
    }

    ElementType& front() const {
        return (*this)[0];
    }

    ElementType& back() const {
        return (*this)[size()-1];
    }

    void iterate(Closure<void(ElementType&)> callback) const {
        for(NativeNaturalType at = 0; at < size(); ++at)
            callback((*this)[at]);
    }

    void activate() {
        if(!symbol) {
            assert(guarded);
            symbol = Storage::createSymbol();
        }
    }

    void clear() {
        if(symbol)
            Storage::setBlobSize(symbol, 0);
    }

    void insert(NativeNaturalType at, ElementType element) {
        activate();
        Storage::insertIntoBlob(symbol, reinterpret_cast<NativeNaturalType*>(&element), at*sizeof(ElementType)*8, sizeof(ElementType)*8);
    }

    void erase(NativeNaturalType begin, NativeNaturalType end) {
        assert(symbol);
        assert(begin < end);
        assert(Storage::getBlobSize(symbol) >= end*sizeof(ElementType)*8);
        Storage::eraseFromBlob(symbol, begin*sizeof(ElementType)*8, end*sizeof(ElementType)*8);
    }

    void erase(NativeNaturalType at) {
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
    Pair(KeyType _key) :key(_key) {}
    Pair(KeyType _key, ValueType _value) :key(_key), value(_value) {}
    operator KeyType() {
        return key;
    }
};

template<bool guarded, typename KeyType, typename ValueType = NativeNaturalType[0]>
struct Set : public Vector<guarded, Pair<KeyType, ValueType>> {
    typedef Pair<KeyType, ValueType> ElementType;
    typedef Vector<guarded, ElementType> Super;

    Set() :Super() {}

    NativeNaturalType find(KeyType key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return key > (*this)[at];
        });
    }

    bool find(KeyType key, NativeNaturalType& at) const {
        at = find(key);
        return (at < Super::size() && (*this)[at] == key);
    }

    bool insertElement(ElementType element) {
        NativeNaturalType at;
        if(find(element.key, at))
            return false;
        Super::insert(at, element);
        return true;
    }

    bool eraseElement(ElementType element) {
        NativeNaturalType at;
        if(!find(element.key, at))
            return false;
        Super::erase(at);
        return true;
    }
};

template<bool guarded>
struct BlobIndex : public Set<guarded, Symbol> {
    typedef Set<guarded, Symbol> Super;

    BlobIndex() :Super() {}

    NativeNaturalType find(Symbol key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return Storage::compareBlobs(key, (*this)[at]) < 0;
        });
    }

    bool find(Symbol element, NativeNaturalType& at) const {
        at = find(element);
        if(at == Super::size())
            return false;
        return (Storage::compareBlobs(element, (*this)[at]) == 0);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(find(element, at)) {
            Ontology::unlink(element);
            element = (*this)[at];
        } else
            Super::insert(at, element);
    }

    bool eraseElement(Symbol element) {
        NativeNaturalType at;
        if(!Super::find(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};
