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

    void iterate(Closure<void(ElementType)> callback) const {
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
        assert(Blob(symbol).increaseSize(at*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value)*length);
    }

    void insert(NativeNaturalType at, ElementType element) {
        activate();
        insertRange(at, 1);
        writeElementAt(at, element);
    }

    void eraseRange(NativeNaturalType at, NativeNaturalType length) {
        assert(symbol);
        assert(Blob(symbol).decreaseSize(at*sizeOfInBits<ElementType>::value, sizeOfInBits<ElementType>::value)*length);
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

template<typename FirstType, typename SecondType = VoidType[0]>
struct Pair {
    FirstType first;
    SecondType second;
    Pair() {}
    Pair(FirstType _first) :first(_first) {}
    Pair(FirstType _first, SecondType _second) :first(_first), second(_second) {}
    operator FirstType() {
        return first;
    }
};

template<bool guarded, typename KeyType, typename ValueType>
struct BlobMap : public BlobVector<guarded, Pair<KeyType, ValueType>> {
    typedef Pair<KeyType, ValueType> ElementType;
    typedef BlobVector<guarded, ElementType> Super;

    BlobMap() :Super() {}
    BlobMap(const BlobMap<false, KeyType, ValueType>& other) :Super(other) {}
    BlobMap(const BlobMap<true, KeyType, ValueType>& other) :Super(other) {}

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
struct BlobHeap : public BlobMap<guarded, KeyType, ValueType> {
    typedef BlobMap<guarded, KeyType, ValueType> Super;
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
struct BlobSet : public BlobMap<guarded, KeyType, ValueType> {
    typedef BlobMap<guarded, KeyType, ValueType> Super;
    typedef typename Super::ElementType ElementType;

    BlobSet() :Super() {}
    BlobSet(const BlobSet<false, KeyType, ValueType>& other) :Super(other) {}
    BlobSet(const BlobSet<true, KeyType, ValueType>& other) :Super(other) {}

    NativeNaturalType find(KeyType key) const {
        return binarySearch<NativeNaturalType>(0, Super::size(), [&](NativeNaturalType at) {
            return key > Super::readKeyAt(at);
        });
    }

    bool find(KeyType key, NativeNaturalType& at) const {
        at = find(key);
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
