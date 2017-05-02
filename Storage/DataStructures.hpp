#include <Storage/Blob.hpp>

template<bool guarded, typename ElementType>
struct BlobVector {
    Symbol symbol;

    BlobVector() :symbol(0) {}
    BlobVector(const BlobVector<false, ElementType>& vector) :symbol(vector.symbol) {}
    BlobVector(const BlobVector<true, ElementType>& vector) :symbol(vector.symbol) {
        static_assert(!guarded);
    }

    ~BlobVector() {
        if(guarded && symbol)
            releaseSymbol(symbol);
    }

    bool empty() const {
        return (!symbol || Blob(symbol).getSize() == 0);
    }

    NativeNaturalType size() const {
        return (symbol) ? Blob(symbol).getSize()/sizeOfInBits<ElementType>::value : 0;
    }

    ElementType readElementAt(NativeNaturalType at) const {
        assert(symbol);
        return Blob(symbol).readAt<ElementType>(at);
    }

    void writeElementAt(NativeNaturalType at, ElementType element) const {
        assert(symbol);
        Blob(symbol).writeAt<ElementType>(at, element);
    }

    void swapElementsAt(NativeNaturalType a, NativeNaturalType b) const {
        ElementType A = readElementAt(a),
                    B = readElementAt(b);
        writeElementAt(a, B);
        writeElementAt(b, A);
    }

    ElementType front() const {
        return readElementAt(0);
    }

    ElementType back() const {
        return readElementAt(size()-1);
    }

    void iterateElements(Closure<void(ElementType)> callback) const {
        for(NativeNaturalType at = 0; at < size(); ++at)
            callback(readElementAt(at));
    }

    void activate() {
        if(!symbol) {
            symbol = createSymbol();
            assert(guarded && symbol);
        }
    }

    void reserve(NativeNaturalType size) {
        activate();
        Blob(symbol).setSize(size*sizeOfInBits<ElementType>::value);
    }

    void clear() {
        reserve(0);
    }

    void insertRange(NativeNaturalType at, NativeNaturalType length) {
        activate();
        Blob(symbol).increaseSize(at*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value*length);
    }

    void insert(NativeNaturalType at, ElementType element) {
        activate();
        insertRange(at, 1);
        writeElementAt(at, element);
    }

    void eraseRange(NativeNaturalType at, NativeNaturalType length) {
        assert(symbol);
        Blob(symbol).decreaseSize(at*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value*length);
    }

    void erase(NativeNaturalType at) {
        eraseRange(at, 1);
    }

    void push_front(ElementType element) {
        insert(0, element);
    }

    void push_back(ElementType element) {
        insert(size(), element);
    }

    ElementType pop_front() {
        ElementType element = front();
        erase(0);
        return element;
    }

    ElementType pop_back() {
        ElementType element = back();
        erase(size()-1);
        return element;
    }
};

template<bool guarded, typename KeyType, typename ValueType>
struct BlobPairVector : public BlobVector<guarded, Pair<KeyType, ValueType>> {
    typedef Pair<KeyType, ValueType> ElementType;
    typedef BlobVector<guarded, ElementType> Super;

    BlobPairVector() :Super() {}
    BlobPairVector(const BlobPairVector<false, KeyType, ValueType>& other) :Super(other) {}
    BlobPairVector(const BlobPairVector<true, KeyType, ValueType>& other) :Super(other) {}

    void iterateKeys(Closure<void(KeyType)> callback) const {
        for(NativeNaturalType at = 0; at < Super::size(); ++at)
            callback(readKeyAt(at));
    }

    void iterateValues(Closure<void(ValueType)> callback) const {
        for(NativeNaturalType at = 0; at < Super::size(); ++at)
            callback(readValueAt(at));
    }

    KeyType&& readKeyAt(NativeNaturalType at) const {
        assert(Super::symbol);
        KeyType key;
        Blob(Super::symbol).externalOperate<false>(&key, at*sizeOfInBits<ElementType>::value, sizeOfInBits<KeyType>::value);
        return reinterpret_cast<KeyType&&>(key);
    }

    ValueType&& readValueAt(NativeNaturalType at) const {
        assert(Super::symbol);
        ValueType value;
        Blob(Super::symbol).externalOperate<false>(&value, at*sizeOfInBits<ElementType>::value+sizeOfInBits<KeyType>::value, sizeOfInBits<ValueType>::value);
        return reinterpret_cast<ValueType&&>(value);
    }

    bool writeKeyAt(NativeNaturalType at, KeyType key) const {
        assert(Super::symbol);
        Blob(Super::symbol).externalOperate<true>(&key, at*sizeOfInBits<ElementType>::value, sizeOfInBits<KeyType>::value);
        return true;
    }

    bool writeValueAt(NativeNaturalType at, ValueType value) const {
        assert(Super::symbol);
        Blob(Super::symbol).externalOperate<true>(&value, at*sizeOfInBits<ElementType>::value+sizeOfInBits<KeyType>::value, sizeOfInBits<ValueType>::value);
        return true;
    }
};

template<bool guarded, typename KeyType, typename ValueType = VoidType[0]>
struct BlobHeap : public BlobPairVector<guarded, KeyType, ValueType> {
    typedef BlobPairVector<guarded, KeyType, ValueType> Super;
    typedef typename Super::ElementType ElementType;

    BlobHeap() :Super() {}
    BlobHeap(const BlobHeap<false, KeyType, ValueType>& other) :Super(other) {}
    BlobHeap(const BlobHeap<true, KeyType, ValueType>& other) :Super(other) {}

    void siftToLeaves(NativeNaturalType at, NativeNaturalType size) {
        while(true) {
            NativeNaturalType left = 2*at+1,
                              right = 2*at+2,
                              min = at;
            if(left < size && Super::readKeyAt(left) < Super::readKeyAt(min))
                min = left;
            if(right < size && Super::readKeyAt(right) < Super::readKeyAt(min))
                min = right;
            if(min == at)
                break;
            Super::swapElementsAt(at, min);
            at = min;
        }
    }

    void siftToLeaves(NativeNaturalType at) {
        siftToLeaves(at, Super::size());
    }

    void build() {
        for(NativeIntegerType i = Super::size()/2-1; i >= 0; --i)
            siftToLeaves(i);
    }

    void sort() {
        build();
        NativeNaturalType size = Super::size();
        while(size > 1) {
            --size;
            Super::swapElementsAt(0, size);
            siftToLeaves(0, size);
        }
    }

    void siftToRoot(NativeNaturalType at) {
        while(at > 0) {
            NativeNaturalType parent = (at-1)/2;
            if(Super::readKeyAt(parent) <= Super::readKeyAt(at))
                break;
            Super::swapElementsAt(at, parent);
            at = parent;
        }
    }

    void insertElement(ElementType element) {
        Super::push_back(element);
        siftToRoot(Super::size()-1);
    }

    void erase(NativeNaturalType at) {
        NativeNaturalType last = Super::size()-1;
        if(at != last)
            Super::writeElementAt(at, Super::readElementAt(last));
        Super::pop_back();
        if(at != last) {
            if(at == 0 || Super::readKeyAt((at-1)/2) < Super::readKeyAt(at))
                siftToLeaves(at);
            else
                siftToRoot(at);
        }
    }

    bool writeKeyAt(NativeNaturalType at, KeyType key) const {
        Super::writeKeyAt(at, key);
        siftToRoot(at);
        return true;
    }

    ElementType pop_front() {
        ElementType element = Super::front();
        erase(0);
        return element;
    }
};

template<bool guarded, typename KeyType, typename ValueType = VoidType[0]>
struct BlobSet : public BlobPairVector<guarded, KeyType, ValueType> {
    typedef BlobPairVector<guarded, KeyType, ValueType> Super;
    typedef typename Super::ElementType ElementType;

    BlobSet() :Super() {}
    BlobSet(const BlobSet<false, KeyType, ValueType>& other) :Super(other) {}
    BlobSet(const BlobSet<true, KeyType, ValueType>& other) :Super(other) {}

    bool find(KeyType key, NativeNaturalType& at) const {
        at = binarySearch<NativeNaturalType>(0, Super::size(), [&](NativeNaturalType at) {
            return key > Super::readKeyAt(at);
        });
        return (at < Super::size() && Super::readKeyAt(at) == key);
    }

    bool insertElement(ElementType element) {
        NativeNaturalType at;
        if(find(element.first, at))
            return false;
        Super::insert(at, element);
        return true;
    }

    bool eraseElement(ElementType element) {
        NativeNaturalType at;
        if(!find(element.first, at))
            return false;
        Super::erase(at);
        return true;
    }

    bool writeKeyAt(NativeNaturalType at, KeyType key) const {
        assert(Super::symbol);
        NativeNaturalType newAt;
        ValueType value = Super::readValueAt(at);
        if(find(key, newAt))
            return false;
        Super::erase(at);
        Super::insert(newAt, {key, value});
        return true;
    }
};

#if 0
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

    void iterateElements(Closure<void(Pair<ElementType, ElementType>)> callback) const {
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
// #include <stdio.h>

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
        return /*(firstAt == firstKeyCount*2-1) ? Super::size() :*/ firstKeyCount*2+1+Super::readElementAt(firstAt+1);
    }

    /*void debugPrint() const {
        auto firstKeyCount = getFirstKeyCount();
        printf("%llu", firstKeyCount);
        for(NativeNaturalType i = 1; i < firstKeyCount*2+1; i += 2)
            printf(" | %llu %llu", Super::readElementAt(i), Super::readElementAt(i+1));
        for(NativeNaturalType i = firstKeyCount*2+1; i < Super::size(); ++i)
            printf(" : %llu", Super::readElementAt(i));
        printf("\n");
    }*/

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

    void iterateElements(Closure<void(Pair<ElementType, ElementType>)> callback) const {
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
        // printf("insertElement <%llu> %llu %llu %llu\n", Super::symbol, firstKeyCount, element.first, element.second);
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
        // debugPrint();
        return true;
    }

    bool eraseElement(Pair<ElementType, ElementType> element) {
        NativeNaturalType begin, end, firstAt, secondAt;
        auto firstKeyCount = getFirstKeyCount();
        // printf("eraseElement <%llu> %llu %llu %llu\n", Super::symbol, firstKeyCount, element.first, element.second);
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
        // debugPrint();
        return true;
    }

    // Key, ChildIndex0, ChildIndex1, Value
    // AVL: 2bit, 1key/value, 2ptr
    // RB: 1bit, 1key/value, 2ptr
    // 2-3: 2key/value, 3ptr
    // 2-3-4: 3key/value, 4ptr
};
#endif
