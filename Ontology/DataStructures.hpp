#include <Storage/DataStructures.hpp>

bool unlink(Symbol symbol);

template<bool guarded>
struct BlobIndex : public BlobSet<guarded, Symbol> {
    typedef BlobSet<guarded, Symbol> Super;

    BlobIndex() :Super() {}
    BlobIndex(const BlobIndex<false>& other) :Super(other) {}
    BlobIndex(const BlobIndex<true>& other) :Super(other) {}

    NativeNaturalType find(Symbol key) const {
        return binarySearch<NativeNaturalType>(0, Super::size(), [&](NativeNaturalType at) {
            return Blob(key).compare(Blob(Super::readElementAt(at))) < 0;
        });
    }

    bool find(Symbol element, NativeNaturalType& at) const {
        at = find(element);
        if(at == Super::size())
            return false;
        return (Blob(element).compare(Blob(Super::readElementAt(at))) == 0);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(find(element, at)) {
            unlink(element);
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

BlobIndex<false> blobIndex;



template<bool guarded, typename ElementType = Symbol>
struct BlobPairSet : public BlobSet<guarded, ElementType, ElementType> {
    typedef BlobSet<guarded, ElementType, ElementType> Super;

    BlobPairSet() :Super() {}
    BlobPairSet(const BlobPairSet<false, ElementType>& other) :Super(other) {}
    BlobPairSet(const BlobPairSet<true, ElementType>& other) :Super(other) {}

    ElementType getFirstKeyCount() const {
        return Super::size();
    }

    ElementType getSecondKeyCount(NativeNaturalType firstAt) const {
        BlobSet<false, Symbol> second;
        second.symbol = Super::readValueAt(firstAt);
        return second.size();
    }

    void iterateFirstKeys(Closure<void(ElementType)> callback) const {
        Super::iterateKeys([&](Symbol firstKey) {
            callback(firstKey);
        });
    }

    void iterateSecondKeys(NativeNaturalType firstAt, Closure<void(ElementType)> callback) const {
        BlobSet<false, Symbol> second;
        second.symbol = Super::readValueAt(firstAt);
        second.iterateKeys([&](Symbol secondKey) {
            callback(secondKey);
        });
    }

    void iterate(Closure<void(Pair<ElementType, ElementType>)> callback) const {
        for(NativeNaturalType firstAt = 0; firstAt < Super::size(); ++firstAt) {
            ElementType firstKey = Super::readKeyAt(firstAt);
            BlobSet<false, Symbol> second;
            second.symbol = Super::readValueAt(firstAt);
            second.iterateKeys([&](Symbol secondKey) {
                callback({firstKey, secondKey});
            });
        }
    }

    bool findFirstKey(ElementType firstKey, NativeNaturalType& firstAt) const {
        return Super::find(firstKey, firstAt);
    }

    bool findSecondKey(ElementType secondKey, NativeNaturalType firstAt, NativeNaturalType& secondAt) const {
        BlobSet<false, Symbol> second;
        second.symbol = Super::readValueAt(firstAt);
        return second.find(secondKey, secondAt);
    }

    bool findElement(Pair<ElementType, ElementType> element,
                     NativeNaturalType& firstAt, NativeNaturalType& secondAt) const {
        if(!Super::find(element.first, firstAt))
            return false;
        BlobSet<false, Symbol> second;
        second.symbol = Super::readValueAt(firstAt);
        return second.find(element.second, secondAt);
    }

    bool insertElement(Pair<ElementType, ElementType> element) {
        NativeNaturalType firstAt;
        BlobSet<false, Symbol> second;
        if(Super::find(element.first, firstAt))
            second.symbol = Super::readValueAt(firstAt);
        else {
            second.symbol = createSymbol();
            Super::insert(firstAt, {element.first, second.symbol});
        }
        return second.insertElement(element.second);
    }

    bool eraseElement(Pair<ElementType, ElementType> element) {
        NativeNaturalType firstAt, secondAt;
        if(!Super::find(element.first, firstAt))
            return false;
        BlobSet<false, Symbol> second;
        second.symbol = Super::readValueAt(firstAt);
        if(!second.find(element.second, secondAt))
            return false;
        second.erase(secondAt);
        if(second.empty()) {
            Super::erase(firstAt);
            releaseSymbol(second.symbol);
        }
        return true;
    }

    bool empty() {
        return getFirstKeyCount() == 0;
    }

    void clear() {
        for(NativeNaturalType secondAt = 0; secondAt < getFirstKeyCount(); ++secondAt)
            releaseSymbol(Super::readValueAt(secondAt));
        Super::clear();
    }
};
