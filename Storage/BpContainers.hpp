#include <Storage/BpTree.hpp>

template<typename KeyType, typename ValueType>
struct BpTreeMap : public BpTree<KeyType, VoidType, sizeOfInBits<ValueType>::value> {
    typedef BpTree<KeyType, VoidType, sizeOfInBits<ValueType>::value> Super;
    typedef typename Super::IteratorFrame FrameType;

    template<bool enableModification>
    struct Iterator : public Super::template Iterator<enableModification, FrameType> {
        typedef typename Super::template Iterator<enableModification, FrameType> SuperIterator;

        Iterator() {}
        Iterator(SuperIterator iter) {
            SuperIterator::copy(iter);
        }

        ValueType getValue() {
            FrameType* frame = (*this)[0];
            return Super::getPage(frame->pageRef)->template get<ValueType, Super::Page::valueOffset>(frame->index);
        }

        void setValue(ValueType value) {
            static_assert(enableModification);
            FrameType* frame = (*this)[0];
            Super::getPage(frame->pageRef)->template set<ValueType, Super::Page::valueOffset>(frame->index, value);
        }
    };

    void insert(Iterator<true>& iter, KeyType key, ValueType value) {
        Super::insert(iter, 1, [&](typename Super::Page* page, typename Super::OffsetType index, typename Super::OffsetType endIndex) {
            assert(index+1 == endIndex);
            page->template setKey<true>(index, key);
            page->template set<ValueType, Super::Page::valueOffset>(index, value);
        });
    }

    bool insert(KeyType key, ValueType value) {
        Iterator<true> iter;
        if(Super::template find<Key>(iter, key))
            return false;
        insert(iter, key, value);
        return true;
    }
};

template<typename KeyType>
struct BpTreeSet : public BpTree<KeyType, VoidType, 0> {
    typedef BpTree<KeyType, VoidType, 0> Super;
    typedef typename Super::template Iterator<true> SuperIterator;

    template<FindMode mode, bool erase>
    KeyType getOne() {
        static_assert(mode == First || mode == Last);
        SuperIterator iter;
        Super::template find<mode>(iter);
        KeyType key = iter.getKey();
        if(erase)
            Super::erase(iter);
        return key;
    }

    void insert(SuperIterator& iter, KeyType key) {
        Super::insert(iter, 1, [&](typename Super::Page* page, typename Super::OffsetType index, typename Super::OffsetType endIndex) {
            assert(index+1 == endIndex);
            page->template setKey<true>(index, key);
        });
    }

    bool insert(KeyType key) {
        SuperIterator iter;
        if(Super::template find<Key>(iter, key))
            return false;
        insert(iter, key);
        return true;
    }
};

struct BpTreeBitVector : public BpTree<VoidType, NativeNaturalType, 1> {
    typedef BpTree<VoidType, NativeNaturalType, 1> Super;
};
