#include "../Storage/Blob.hpp"

namespace Ontology {

bool unlink(Symbol symbol);

template<bool guarded, typename ElementType>
struct BlobVector {
    Symbol symbol;

    BlobVector() :symbol(0) {}

    ~BlobVector() {
        if(guarded && symbol)
            Ontology::unlink(symbol);
    }

    bool empty() const {
        return (!symbol || Storage::Blob(symbol).getSize() == 0);
    }

    NativeNaturalType size() const {
        return (symbol) ? Storage::Blob(symbol).getSize()/sizeOfInBits<ElementType>::value : 0;
    }

    ElementType readElementAt(NativeNaturalType offset) const {
        assert(symbol && offset < size());
        return Storage::Blob(symbol).readAt<ElementType>(offset);
    }

    void writeElementAt(NativeNaturalType offset, ElementType element) const {
        assert(symbol && offset < size());
        Storage::Blob(symbol).writeAt<ElementType>(offset, element);
    }

    ElementType front() const {
        return readElementAt(0);
    }

    ElementType back() const {
        return readElementAt(size()-1);
    }

    void iterate(Closure<void(ElementType)> callback) const {
        for(NativeNaturalType at = 0; at < size(); ++at)
            callback(readElementAt(at));
    }

    void activate() {
        if(!symbol) {
            symbol = Storage::createSymbol();
            assert(guarded && symbol);
        }
    }

    void clear() {
        if(symbol)
            Storage::Blob(symbol).setSize(0);
    }

    void insert(NativeNaturalType offset, ElementType element) {
        activate();
        Storage::Blob dstBlob(symbol);
        assert(dstBlob.increaseSize(offset*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value));
        dstBlob.writeAt<ElementType>(offset, element);
    }

    void erase(NativeNaturalType offset, NativeNaturalType length) {
        assert(symbol);
        assert(Storage::Blob(symbol).decreaseSize(offset*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value));
    }

    void erase(NativeNaturalType at) {
        erase(at, 1);
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
    Pair() {}
    Pair(KeyType _key) :key(_key) {}
    Pair(KeyType _key, ValueType _value) :key(_key), value(_value) {}
    operator KeyType() {
        return key;
    }
};

template<bool guarded, typename KeyType, typename ValueType = NativeNaturalType[0]>
struct BlobSet : public BlobVector<guarded, Pair<KeyType, ValueType>> {
    typedef Pair<KeyType, ValueType> ElementType;
    typedef BlobVector<guarded, ElementType> Super;

    BlobSet() :Super() {}

    NativeNaturalType find(KeyType key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return key > Super::readElementAt(at).key;
        });
    }

    bool find(KeyType key, NativeNaturalType& at) const {
        at = find(key);
        return (at < Super::size() && Super::readElementAt(at).key == key);
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
struct BlobIndex : public BlobSet<guarded, Symbol> {
    typedef BlobSet<guarded, Symbol> Super;

    BlobIndex() :Super() {}

    NativeNaturalType find(Symbol key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return Storage::Blob(key).compare(Storage::Blob(Super::readElementAt(at))) < 0;
        });
    }

    bool find(Symbol element, NativeNaturalType& at) const {
        at = find(element);
        if(at == Super::size())
            return false;
        return (Storage::Blob(element).compare(Storage::Blob(Super::readElementAt(at))) == 0);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(find(element, at)) {
            Ontology::unlink(element);
            element = Super::readElementAt(at);
        } else
            Super::insert(at, element);
    }

    bool eraseElement(Symbol element) {
        NativeNaturalType at;
        if(!find(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};

};
