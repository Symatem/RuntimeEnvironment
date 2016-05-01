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
TreeType::Iterator<true> iterA, iterB;
NativeNaturalType elementCount, sectionCount, sectionElementCount, sectionIndex, permutationIndex, elementKey;
const NativeNaturalType* permutation;

template<bool insert>
void testUsingPermutation() {
    assert(sectionCount <= elementCount);

    Natural8* flagField = reinterpret_cast<Natural8*>(alloca(sectionCount));
    for(NativeNaturalType sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
        flagField[sectionIndex] = !insert;

    NativeNaturalType sectionElementCountMin = elementCount/sectionCount,
                      sectionElementCountMax = elementCount-(sectionCount-1)*sectionElementCountMin;
    for(NativeNaturalType sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex) {
        permutationIndex = permutation[sectionIndex];
        elementKey = sectionElementCountMin*permutationIndex;
        sectionElementCount = (permutationIndex < sectionCount-1) ? sectionElementCountMin : sectionElementCountMax;
        tree.find<Storage::Key>(iterA, elementKey);
        flagField[permutationIndex] = insert;

        printf("[%010llu, %010llu] %04llu %3.2f%% %hhd\n", elementKey, elementKey+sectionElementCount, permutationIndex, 100.0*sectionIndex/sectionCount, tree.getLayerCount());

        if(insert) {
            tree.insert(iterA, sectionElementCount, [&](TreeType::Page* page, TreeType::OffsetType index, TreeType::OffsetType endIndex) {
                for(; index < endIndex; ++index) {
                    page->template setKey<true>(index, elementKey);
                    page->template set<NativeNaturalType, TreeType::Page::valueOffset>(index, elementKey);
                    ++elementKey;
                }
            });
        } else {
            tree.find<Storage::Key>(iterB, elementKey+sectionElementCount-1);
            tree.erase(iterA, iterB);
        }

        NativeNaturalType elementRank = 0;
        for(NativeNaturalType sectionToCheck = 0; sectionToCheck < sectionCount; ++sectionToCheck) {
            elementKey = sectionElementCountMin*sectionToCheck;
            sectionElementCount = (sectionToCheck < sectionCount-1) ? sectionElementCountMin : sectionElementCountMax;
            tree.find<Storage::Key>(iterA, elementKey);
            if(!flagField[sectionToCheck])
                continue;
            printf("[%010llu, %010llu]\n", elementKey, elementKey+sectionElementCount);
            for(NativeNaturalType endKey = elementKey+sectionElementCount; elementKey < endKey; ++elementKey) {
                if(elementKey != iterA.getKey() || elementKey != iterA.getValue() || elementRank != iterA.getRank()) {
                    printf("ERROR: %lld:%lld:%lld %lld:%lld\n", elementKey, iterA.getKey(), iterA.getValue(), elementRank, iterA.getRank());
                    exit(1);
                }
                iterA.advance();
                ++elementRank;
            }
        }
        assert(tree.getElementCount() == elementRank);
    }
}

Integer32 main(Integer32 argc, Integer8** argv) {
    if(argc < 2) {
        printf("Expected path argument.\n");
        exit(4);
    }
    loadStorage(argv[1]);
    tree.init();

    elementCount = 1024*1024;
    const NativeNaturalType insertPermutation[] = {
        0
    };
    permutation = insertPermutation;
    sectionCount = sizeof(insertPermutation)/sizeof(NativeNaturalType);
    testUsingPermutation<true>();

    const NativeNaturalType erasePermutation[] = {
        0
    };
    permutation = erasePermutation;
    sectionCount = sizeof(erasePermutation)/sizeof(NativeNaturalType);
    testUsingPermutation<false>();

    unloadStorage();
    return 0;
}
