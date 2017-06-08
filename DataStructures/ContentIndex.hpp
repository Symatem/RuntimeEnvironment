#include <DataStructures/BitMap.hpp>

bool unlink(Symbol symbol);

template<typename _ParentType = BitVectorContainer>
struct ContentIndex : public Set<Symbol, VoidType, _ParentType> {
    typedef _ParentType ParentType;
    typedef Set<Symbol, VoidType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    ContentIndex(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    bool findKey(Symbol key, NativeNaturalType& at) {
        SymbolSpace* symbolSpace = Super::parent.getBitVector().symbolSpace;
        at = binarySearch<NativeNaturalType>(0, Super::getElementCount(), [&](NativeNaturalType at) {
            return BitVector(symbolSpace, key).compare(BitVector(Super::getElementAt(at))) < 0;
        });
        return (at < Super::getElementCount() && Super::getKeyAt(at) == key);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(findKey(element, at)) {
            unlink(element);
            element = Super::getElementAt(at);
        } else
            Super::insertElementAt(at, element);
    }
};
