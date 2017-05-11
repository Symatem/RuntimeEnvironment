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
