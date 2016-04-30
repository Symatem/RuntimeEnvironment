#include "../Targets/POSIX.hpp"

struct TreeType : public Storage::BpTree<NativeNaturalType, NativeNaturalType, 64> {
    typedef Storage::BpTree<NativeNaturalType, NativeNaturalType, 64> Super;
    typedef typename Super::template Iterator<true> SuperIterator;

    template<bool enableCopyOnWrite>
    struct Iterator : public Super::template Iterator<enableCopyOnWrite, Super::IteratorFrame> {
        typedef typename Super::template Iterator<enableCopyOnWrite, Super::IteratorFrame> SuperIterator;

        NativeNaturalType getValue() {
            Super::IteratorFrame* frame = (*this)[0];
            return Super::getPage(frame->pageRef)->template get<NativeNaturalType, Super::Page::valueOffset>(frame->index);
        }

        void setValue(NativeNaturalType value) {
            static_assert(enableCopyOnWrite);
            Super::IteratorFrame* frame = (*this)[0];
            Super::getPage(frame->pageRef)->template set<NativeNaturalType, Super::Page::valueOffset>(frame->index, value);
        }
    };

    void insert(Iterator<true>& origIter, NativeNaturalType n, typename Super::AquireData aquireData) {
        Super::insert(origIter, n, aquireData);
    }
};
TreeType tree;

template<bool insert>
bool testUsingPermutation(const NativeNaturalType permutation[], NativeNaturalType sectionCount, NativeNaturalType elementCount) {
    TreeType::Iterator<true> iter, iterB;
    assert(sectionCount <= elementCount);
    const NativeNaturalType sectionSize = elementCount/sectionCount;
    NativeNaturalType counter = (insert) ? 0 : elementCount;

    for(NativeNaturalType s = 0; s < sectionCount; ++s) {
        NativeNaturalType p = permutation[s], key = sectionSize*p,
                          size = (p < sectionCount-1) ? sectionSize : elementCount-(sectionCount-1)*sectionSize;
        tree.find<Storage::Key>(iter, key);
        printf("%04llu [%010llu, %010llu] %3.2f%% %hhd\n", p, key, key+size, 100.0*s/sectionCount, tree.getLayerCount());

        if(insert) {
            tree.insert(iter, size, [&](TreeType::Page* page, TreeType::OffsetType index, TreeType::OffsetType endIndex) {
                for(; index < endIndex; ++index) {
                    page->template setKey<true>(index, key);
                    page->template set<NativeNaturalType, TreeType::Page::valueOffset>(index, key);
                    ++key;
                }
            });
            counter += size;
        } else {
            tree.find<Storage::Key>(iterB, key+size-1);
            tree.erase(iter, iterB);
            counter -= size;
        }
        assert(tree.getElementCount() == counter);
    }

    if(insert) {
        NativeNaturalType key = 0;
        tree.find<Storage::First>(iter);
        do {
            if(key != iter.getKey() || key != iter.getValue() || key != iter.getRank()) {
                printf("GLOBAL ERROR: %lld %lld %lld %lld (%llu)\n", key, iter.getKey(), iter.getValue(), iter.getRank(), tree.rootPageRef);
                return false;
            }
            iter.advance();
        } while(++key < elementCount);
    } else
        assert(tree.empty());
    printf("[%010llu, %010llu] 100%%\n", 0ULL, elementCount);
    return true;
}

Integer32 main(Integer32 argc, Integer8** argv) {
    if(argc < 2) {
        printf("Expected path argument.\n");
        exit(4);
    }
    loadStorage(argv[1]);
    tree.init();

    NativeNaturalType elementCount = 1024*1024*100;
    const NativeNaturalType insertPermutation[] = {
        0
    };
    testUsingPermutation<true>(insertPermutation, sizeof(insertPermutation)/sizeof(NativeNaturalType), elementCount);
    const NativeNaturalType erasePermutation[] = {
        0
    };
    testUsingPermutation<false>(erasePermutation, sizeof(erasePermutation)/sizeof(NativeNaturalType), elementCount);

    unloadStorage();
    return 0;
}
