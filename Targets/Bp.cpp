#include "../Targets/POSIX.hpp"

struct TreeType : public Storage::BpTree<NativeNaturalType, NativeNaturalType, architectureSize> {
    typedef Storage::BpTree<NativeNaturalType, NativeNaturalType, architectureSize> Super;
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

    void insert(Iterator<true>& origIter, NativeNaturalType n, typename Super::AcquireData acquireData) {
        Super::insert(origIter, n, acquireData);
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
                    exit(2);
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
        exit(1);
    }
    loadStorage(argv[1]);
    tree.init();

    printf("Capacity %hhd %d %d\n", TreeType::maxLayerCount, TreeType::Page::capacity<true>(), TreeType::Page::capacity<false>());
    printf("Bits %llu %llu %llu %hhu\n", TreeType::keyBits, TreeType::rankBits, TreeType::pageRefBits, architectureSize);
    printf("Offset %llu %llu %llu %llu\n", TreeType::Page::keyOffset, TreeType::Page::rankOffset, TreeType::Page::pageRefOffset, TreeType::Page::valueOffset);

    elementCount = 1024*1024*128;
    const NativeNaturalType insertPermutation[] = {
        5, 13, 113, 67, 112, 12, 15, 78, 1, 7, 38, 71, 84, 121, 93, 33, 36, 0, 47, 73, 72, 106, 22, 120, 18, 30, 20, 127, 27, 60, 101, 90, 69, 105, 87, 126, 124, 51, 59, 56, 44, 123, 68, 35, 65, 14, 82, 17, 118, 111, 11, 76, 53, 86, 92, 4, 108, 37, 114, 19, 46, 116, 104, 98, 110, 77, 64, 66, 58, 97, 62, 28, 2, 25, 83, 29, 85, 100, 54, 94, 43, 48, 107, 81, 79, 31, 23, 32, 57, 115, 119, 99, 91, 50, 117, 41, 21, 89, 75, 52, 24, 8, 16, 34, 9, 74, 102, 6, 39, 10, 80, 42, 96, 55, 61, 122, 40, 3, 95, 26, 88, 49, 45, 125, 63, 109, 103, 70
    };
    permutation = insertPermutation;
    sectionCount = sizeof(insertPermutation)/sizeof(NativeNaturalType);
    testUsingPermutation<true>();

    const NativeNaturalType erasePermutation[] = {
        65, 9, 95, 91, 124, 101, 110, 123, 13, 18, 0, 126, 88, 82, 64, 32, 45, 86, 46, 51, 42, 104, 76, 90, 4, 5, 35, 53, 56, 7, 11, 100, 99, 62, 39, 50, 12, 1, 40, 108, 6, 83, 106, 112, 113, 114, 55, 37, 57, 75, 103, 105, 49, 17, 94, 2, 72, 36, 30, 97, 98, 119, 125, 33, 22, 23, 26, 109, 81, 77, 60, 111, 122, 67, 8, 70, 59, 41, 120, 73, 68, 3, 87, 19, 25, 15, 118, 89, 27, 116, 34, 71, 127, 10, 74, 66, 85, 31, 92, 84, 78, 107, 29, 44, 69, 38, 21, 102, 63, 24, 28, 14, 61, 54, 79, 20, 121, 117, 96, 48, 58, 47, 43, 93, 115, 16, 52, 80
    };
    permutation = erasePermutation;
    sectionCount = sizeof(erasePermutation)/sizeof(NativeNaturalType);
    testUsingPermutation<false>();

    assert(tree.empty() && Storage::superPage->pagesEnd == Storage::countFreePages()+1);
    unloadStorage();
    return 0;
}
