#include "BpTree.hpp"

namespace Storage {

template<typename KeyType, typename ValueType>
struct BpTreeMap : public BpTree<KeyType, VoidType, sizeof(ValueType)*8> {
    typedef BpTree<KeyType, VoidType, sizeof(ValueType)*8> Super;
    typedef typename Super::IteratorFrame FrameType;

    template<bool enableCopyOnWrite>
    struct Iterator : public Super::template Iterator<enableCopyOnWrite, FrameType> {
        typedef typename Super::template Iterator<enableCopyOnWrite, FrameType> SuperIterator;

        ValueType getValue() {
            FrameType* frame = SuperIterator::fromEnd();
            return Super::getPage(frame->pageRef)->template get<ValueType, Super::Page::valueOffset>(frame->index);
        }

        void setValue(ValueType value) {
            static_assert(enableCopyOnWrite);
            FrameType* frame = SuperIterator::fromEnd();
            Super::getPage(frame->pageRef)->template set<ValueType, Super::Page::valueOffset>(frame->index, value);
        }
    };

    void insert(Iterator<true>& iter, KeyType key, ValueType value) {
        Super::insert(iter, 1, [&](typename Super::Page* page, typename Super::OffsetType index, typename Super::OffsetType endIndex) {
            page->template setKey<true>(index, key);
            page->template set<ValueType, Super::Page::valueOffset>(index, value);
        });
    }

    bool insert(KeyType key, ValueType value) {
        Iterator<false> iter;
        if(Super::find(iter, key))
            return false;
        insert(iter, key, value);
        return true;
    }
};

template<typename KeyType>
struct BpTreeSet : public BpTree<KeyType, VoidType, 0> {
    typedef BpTree<KeyType, VoidType, 0> Super;
    typedef typename Super::template Iterator<true> SuperIterator;

    template<FindMode mode>
    typename enableIf<mode == First || mode == Last, KeyType>::type pullOneOut() {
        SuperIterator iter;
        Super::template find<mode>(iter);
        KeyType key = iter.getKey();
        Super::erase(iter);
        return key;
    }

    void insert(SuperIterator& iter, KeyType key) {
        Super::insert(iter, 1, [&](typename Super::Page* page, typename Super::OffsetType index, typename Super::OffsetType endIndex) {
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

struct BpTreeBlob : public BpTree<VoidType, NativeNaturalType, 1> {
    typedef BpTree<VoidType, NativeNaturalType, 1> Super;
    
};

};