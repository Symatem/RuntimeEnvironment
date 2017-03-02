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


#if 1
template<bool guarded, typename ElementType = Symbol>
struct BlobPairSet : public BlobSet<guarded, ElementType, ElementType> {
    typedef BlobSet<guarded, ElementType, ElementType> Super;

    BlobPairSet() :Super() {}
    BlobPairSet(const BlobPairSet<false, ElementType>& other) :Super(other) {}
    BlobPairSet(const BlobPairSet<true, ElementType>& other) :Super(other) {}

    void initialize() {}

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

#else

template<bool guarded, typename ElementType>
struct BlobPairSet : public BlobVector<guarded, ElementType> {
    typedef BlobVector<guarded, ElementType> Super;

    BlobPairSet() :Super() {}
    BlobPairSet(const BlobPairSet<false, ElementType>& other) :Super(other) {}
    BlobPairSet(const BlobPairSet<true, ElementType>& other) :Super(other) {}

    void initialize() {
        Super::insert(0, 0);
    }

    ElementType getFirstKeyCount() const {
        return Super::readElementAt(0);
    }

    ElementType getSecondKeyCount(NativeNaturalType firstAt) const {
        NativeNaturalType firstKeyCount = getFirstKeyCount(),
                          begin = getSecondKeysBegin(firstAt, firstKeyCount),
                          end = getSecondKeysEnd(firstAt, firstKeyCount);
        return end-begin;
    }

    bool empty() {
        return getFirstKeyCount() == 0;
    }

    NativeNaturalType getSecondKeysBegin(NativeNaturalType firstAt, NativeNaturalType firstKeyCount) const {
        return firstKeyCount*2+1+((firstAt == 1) ? 0 : Super::readElementAt(firstAt-1));
    }

    NativeNaturalType getSecondKeysEnd(NativeNaturalType firstAt, NativeNaturalType firstKeyCount) const {
        return firstKeyCount*2+1+Super::readElementAt(firstAt+1);
    }

    void iterateFirstKeys(Closure<void(ElementType)> callback) const {
        for(NativeNaturalType firstAt = 1; firstAt < getFirstKeyCount()*2+1; firstAt += 2)
            callback(Super::readElementAt(firstAt));
    }

    void iterateSecondKeys(NativeNaturalType firstAt, Closure<void(ElementType)> callback) const {
        NativeNaturalType firstKeyCount = getFirstKeyCount(),
                          begin = getSecondKeysBegin(firstAt, firstKeyCount),
                          end = getSecondKeysEnd(firstAt, firstKeyCount);
        for(NativeNaturalType secondAt = begin; secondAt < end; ++secondAt)
            callback(Super::readElementAt(secondAt));
    }

    void iterate(Closure<void(Pair<ElementType, ElementType>)> callback) const {
        NativeNaturalType firstKeyCount = getFirstKeyCount();
        if(firstKeyCount == 0)
            return;
        NativeNaturalType begin = getSecondKeysBegin(1, firstKeyCount);
        for(NativeNaturalType firstAt = 1; firstAt < firstKeyCount*2+1; firstAt += 2) {
            ElementType firstKey = Super::readElementAt(firstAt);
            NativeNaturalType end = getSecondKeysEnd(firstAt, firstKeyCount);
            for(NativeNaturalType secondAt = begin; secondAt < end; ++secondAt)
                callback({firstKey, Super::readElementAt(secondAt)});
            begin = end;
        }
    }

    bool findFirstKey(NativeNaturalType firstKeyCount, ElementType firstKey, NativeNaturalType& firstAt) const {
        firstAt = binarySearch<NativeNaturalType>(0, firstKeyCount, [&](NativeNaturalType firstAt) {
            return firstKey > Super::readElementAt(firstAt*2+1);
        })*2+1;
        return (firstAt < firstKeyCount*2+1 && Super::readElementAt(firstAt) == firstKey);
    }

    bool findFirstKey(ElementType firstKey, NativeNaturalType& firstAt) const {
        return findFirstKey(getFirstKeyCount(), firstKey, firstAt);
    }

    bool findSecondKey(NativeNaturalType firstKeyCount, NativeNaturalType& begin, NativeNaturalType& end,
                       ElementType secondKey, NativeNaturalType firstAt, NativeNaturalType& secondAt) const {
        begin = getSecondKeysBegin(firstAt, firstKeyCount);
        end = getSecondKeysEnd(firstAt, firstKeyCount);
        secondAt = binarySearch<NativeNaturalType>(begin, end, [&](NativeNaturalType secondAt) {
            return secondKey > Super::readElementAt(secondAt);
        });
        return (secondAt < end && Super::readElementAt(secondAt) == secondKey);
    }

    bool findSecondKey(ElementType secondKey, NativeNaturalType firstAt, NativeNaturalType& secondAt) const {
        NativeNaturalType begin, end;
        return findSecondKey(getFirstKeyCount(), begin, end, secondKey, firstAt, secondAt);
    }

    bool findElement(NativeNaturalType firstKeyCount, NativeNaturalType& begin, NativeNaturalType& end,
                     Pair<ElementType, ElementType> element,
                     NativeNaturalType& firstAt, NativeNaturalType& secondAt) const {
        if(!findFirstKey(firstKeyCount, element.first, firstAt))
            return false;
        return findSecondKey(firstKeyCount, begin, end, element.second, firstAt, secondAt);
    }

    bool findElement(Pair<ElementType, ElementType> element,
                     NativeNaturalType& firstAt, NativeNaturalType& secondAt) const {
        NativeNaturalType begin, end;
        return findElement(getFirstKeyCount(), begin, end, element, firstAt, secondAt);
    }

    bool insertElement(Pair<ElementType, ElementType> element) {
        NativeNaturalType begin, end, firstAt, secondAt;
        auto firstKeyCount = getFirstKeyCount();
        if(!findFirstKey(firstKeyCount, element.first, firstAt)) {
            secondAt = (firstAt == 1) ? 1 : Super::readElementAt(firstAt-1)+1;
            Super::insertRange(firstAt, 2);
            Super::writeElementAt(firstAt, element.first);
            Super::writeElementAt(++firstAt, secondAt);
            Super::writeElementAt(0, ++firstKeyCount);
            secondAt += firstKeyCount*2;
        } else {
            if(findSecondKey(firstKeyCount, begin, end, element.second, firstAt, secondAt))
                return false;
            ++firstAt;
            Super::writeElementAt(firstAt, Super::readElementAt(firstAt)+1);
        }
        Super::insert(secondAt, element.second);
        for(firstAt += 2; firstAt < firstKeyCount*2+1; firstAt += 2)
            Super::writeElementAt(firstAt, Super::readElementAt(firstAt)+1);
        return true;
    }

    bool eraseElement(Pair<ElementType, ElementType> element) {
        NativeNaturalType begin, end, firstAt, secondAt;
        auto firstKeyCount = getFirstKeyCount();
        if(!findElement(firstKeyCount, begin, end,
                        element, firstAt, secondAt))
            return false;
        Super::erase(secondAt);
        if(begin+1 == end) {
            Super::eraseRange(firstAt--, 2);
            Super::writeElementAt(0, --firstKeyCount);
        } else {
            ++firstAt;
            Super::writeElementAt(firstAt, Super::readElementAt(firstAt)-1);
        }
        for(firstAt += 2; firstAt < firstKeyCount*2+1; firstAt += 2)
            Super::writeElementAt(firstAt, Super::readElementAt(firstAt)-1);
        return true;
    }
};
#endif
