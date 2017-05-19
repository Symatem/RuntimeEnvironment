#include <DataStructures/Vector.hpp>

template<typename KeyType, typename ValueType, typename _ParentType = BitVectorContainer>
struct PairVector : public Vector<Pair<KeyType, ValueType>, _ParentType> {
    typedef _ParentType ParentType;
    typedef Pair<KeyType, ValueType> ElementType;
    typedef Vector<ElementType, ParentType> Super;

    PairVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    NativeNaturalType getOffsetOfKey(NativeNaturalType at) {
        return Super::getOffsetOfElement(at);
    }

    NativeNaturalType getOffsetOfValue(NativeNaturalType at) {
        return Super::getOffsetOfElement(at)+sizeOfInBits<KeyType>::value;
    }

    bool setKeyAt(NativeNaturalType at, KeyType key) {
        Super::getBitVector().template externalOperate<true>(&key, getOffsetOfKey(at), sizeOfInBits<KeyType>::value);
        return true;
    }

    bool setValueAt(NativeNaturalType at, ValueType value) {
        Super::getBitVector().template externalOperate<true>(&value, getOffsetOfValue(at), sizeOfInBits<ValueType>::value);
        return true;
    }

    KeyType getKeyAt(NativeNaturalType at) {
        KeyType key;
        Super::getBitVector().template externalOperate<false>(&key, getOffsetOfKey(at), sizeOfInBits<KeyType>::value);
        return key;
    }

    ValueType getValueAt(NativeNaturalType at) {
        ValueType value;
        Super::getBitVector().template externalOperate<false>(&value, getOffsetOfValue(at), sizeOfInBits<ValueType>::value);
        return value;
    }
};
