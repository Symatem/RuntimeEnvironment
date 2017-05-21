#include <DataStructures/MetaVector.hpp>

template<typename KeyType, typename ValueType = VoidType, typename _ParentType = BitVectorContainer>
struct MetaSet : public SetTemplate<MetaVector<KeyType, _ParentType>, KeyType, ValueType, _ParentType> {
    typedef SetTemplate<MetaVector<KeyType, _ParentType>, KeyType, ValueType, _ParentType> Super;
    using Super::Super;
};

template<typename FirstKeyType, typename SecondKeyType, typename _ParentType = BitVectorContainer>
struct PairSet : public MetaSet<FirstKeyType, _ParentType> {
    typedef _ParentType ParentType;
    typedef Set<SecondKeyType, VoidType, PairSet<FirstKeyType, SecondKeyType, ParentType>> ValueType;
    typedef MetaSet<FirstKeyType, ParentType> Super;
    typedef Pair<FirstKeyType, SecondKeyType> ElementType;

    PairSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    auto getValueAt(NativeNaturalType firstAt) {
        return Super::template getValueAt<ValueType>(firstAt);
    }

    FirstKeyType getFirstKeyCount() {
        return Super::getElementCount();
    }

    SecondKeyType getSecondKeyCount(NativeNaturalType firstAt) {
        return getValueAt(firstAt).getElementCount();
    }

    void iterateFirstKeys(Closure<void(FirstKeyType)> callback) {
        for(NativeNaturalType at = 0; at < Super::getElementCount(); ++at)
            callback(Super::getKeyAt(at));
    }

    void iterateSecondKeys(NativeNaturalType firstAt, Closure<void(SecondKeyType)> callback) {
        auto innerSet = getValueAt(firstAt);
        for(NativeNaturalType at = 0; at < innerSet.getElementCount(); ++at)
            callback(innerSet.getKeyAt(at));
    }

    void iterateElements(Closure<void(ElementType)> callback) {
        for(NativeNaturalType firstAt = 0; firstAt < getFirstKeyCount(); ++firstAt) {
            FirstKeyType firstKey = Super::getKeyAt(firstAt);
            auto innerSet = getValueAt(firstAt);
            for(NativeNaturalType at = 0; at < innerSet.getElementCount(); ++at)
                callback({firstKey, innerSet.getKeyAt(at)});
        }
    }

    bool findFirstKey(FirstKeyType firstKey, NativeNaturalType& firstAt) {
        return Super::findKey(firstKey, firstAt);
    }

    bool findSecondKey(SecondKeyType secondKey, NativeNaturalType firstAt, NativeNaturalType& secondAt) {
        return getValueAt(firstAt).findKey(secondKey, secondAt);
    }

    bool findElement(ElementType element, NativeNaturalType& firstAt, NativeNaturalType& secondAt) {
        return findFirstKey(element.first, firstAt) && findSecondKey(element.second, firstAt, secondAt);
    }

    bool insertElement(ElementType element) {
        NativeNaturalType firstAt;
        if(!findFirstKey(element.first, firstAt))
            Super::insertElementAt(firstAt, element.first);
        ValueType innerSet = getValueAt(firstAt);
        return innerSet.insertElement(element.second);
    }

    bool eraseElement(ElementType element) {
        NativeNaturalType firstAt;
        if(!findFirstKey(element.first, firstAt))
            return false;
        ValueType innerSet = getValueAt(firstAt);
        if(!innerSet.eraseElementByKey(element.second))
            return false;
        if(innerSet.getElementCount() == 0)
            Super::eraseElementAt(firstAt);
        return true;
    }
};
