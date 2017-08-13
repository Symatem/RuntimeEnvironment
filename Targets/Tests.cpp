#include <Targets/POSIX.hpp>

extern "C" {

#define CURSOR_TO_LEFT_LIMIT "\n\e[A"
#define CLEAR_LINE "\e[K"
#define TEXT_BOLD "\e[1m"
#define TEXT_NORMAL "\e[0m"
#define TEXT_RED "\e[0;31m"
#define TEXT_GREEN "\e[0;32m"
#define test(name) beginTest(__COUNTER__, name);
static const char* currentTest = nullptr;
static NativeNaturalType testIndex = 0;
extern NativeNaturalType testCount;

void assertFailed(const char* message) {
    printf(TEXT_BOLD "[FAIL]" TEXT_NORMAL " %s: %s\n", currentTest, message);
    printf(TEXT_RED "\u2716 Test %" PrintFormatNatural "/%" PrintFormatNatural " failed\n" TEXT_NORMAL, testIndex, testCount);
    abort();
    // signal(SIGTRAP);
}

void beginTest(NativeNaturalType index, const char* name) {
    if(currentTest)
        printf(TEXT_BOLD "[ OK ]" TEXT_NORMAL " %s\n", currentTest);
    testIndex = index;
    currentTest = name;
}

void endTests() {
    beginTest(testCount, nullptr);
    printf(TEXT_GREEN "\u2714 All %" PrintFormatNatural " tests succeeded\n" TEXT_NORMAL, testCount);
}

}



#ifdef BP_TREE_TEST
struct TestBpTree : public BpTree<NativeNaturalType, NativeNaturalType, architectureSize> {
    typedef BpTree<NativeNaturalType, NativeNaturalType, architectureSize> Super;
    typedef typename Super::template Iterator<true> SuperIterator;

    template<bool enableModification>
    struct Iterator : public Super::template Iterator<enableModification, Super::IteratorFrame> {
        typedef typename Super::template Iterator<enableModification, Super::IteratorFrame> SuperIterator;

        NativeNaturalType getValue() {
            Super::IteratorFrame* frame = (*this)[0];
            return Super::getPage(frame->pageRef)->template get<NativeNaturalType, Super::Page::valueOffset>(frame->index);
        }

        void setValue(NativeNaturalType value) {
            static_assert(enableModification);
            Super::IteratorFrame* frame = (*this)[0];
            Super::getPage(frame->pageRef)->template set<NativeNaturalType, Super::Page::valueOffset>(frame->index, value);
        }
    };

    void insert(Iterator<true>& origIter, NativeNaturalType n, typename Super::AcquireData acquireData) {
        Super::insert(origIter, n, acquireData);
    }
};

template<bool insert>
void testBpTreeUsingPermutation(TestBpTree& tree, NativeNaturalType elementCount, NativeNaturalType sectionCount, const NativeNaturalType* permutation) {
    assert(sectionCount <= elementCount);
    TestBpTree::Iterator<true> iterA, iterB;

    Natural8* flagField = reinterpret_cast<Natural8*>(alloca(sectionCount));
    for(NativeNaturalType sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
        flagField[sectionIndex] = !insert;

    NativeNaturalType sectionElementCount, permutationIndex, elementKey,
                      sectionElementCountMin = elementCount/sectionCount,
                      sectionElementCountMax = elementCount-(sectionCount-1)*sectionElementCountMin;
    for(NativeNaturalType sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex) {
        permutationIndex = permutation[sectionIndex];
        elementKey = sectionElementCountMin*permutationIndex;
        sectionElementCount = (permutationIndex < sectionCount-1) ? sectionElementCountMin : sectionElementCountMax;
        tree.find<Key>(iterA, elementKey);
        flagField[permutationIndex] = insert;

        printf(CURSOR_TO_LEFT_LIMIT CLEAR_LINE "[%010llu, %010llu] %04llu %3.2f%% %hhd", elementKey, elementKey+sectionElementCount, permutationIndex, 100.0*sectionIndex/sectionCount, tree.getLayerCount());

        if(insert) {
            tree.insert(iterA, sectionElementCount, [&](TestBpTree::Page* page, TestBpTree::OffsetType index, TestBpTree::OffsetType endIndex) {
                for(; index < endIndex; ++index) {
                    page->template setKey<true>(index, elementKey);
                    page->template set<NativeNaturalType, TestBpTree::Page::valueOffset>(index, elementKey);
                    ++elementKey;
                }
            });
        } else {
            tree.find<Key>(iterB, elementKey+sectionElementCount-1);
            tree.erase(iterA, iterB);
        }

        NativeNaturalType elementRank = 0;
        for(NativeNaturalType sectionToCheck = 0; sectionToCheck < sectionCount; ++sectionToCheck) {
            elementKey = sectionElementCountMin*sectionToCheck;
            sectionElementCount = (sectionToCheck < sectionCount-1) ? sectionElementCountMin : sectionElementCountMax;
            tree.find<Key>(iterA, elementKey);
            if(!flagField[sectionToCheck])
                continue;
            printf(CURSOR_TO_LEFT_LIMIT "[%010llu, %010llu]", elementKey, elementKey+sectionElementCount);
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
#endif

extern "C" {

/*void printBitVector(BitVector& bitVector) {
    NativeNaturalType length = bitVector.getSize(), offset = 0;
    while(offset < length) {
        NativeNaturalType buffer, sliceLength = min(static_cast<NativeNaturalType>(architectureSize), length-offset);
        bitVector.externalOperate<false>(&buffer, offset, sliceLength);
        offset += sliceLength;
        for(NativeNaturalType i = 0; i < sliceLength; ++i)
            printf("%" PrintFormatNatural, (buffer>>i)&1);
    }
    printf("\n");
}*/

Integer32 main(Integer32 argc, Integer8** argv) {
    test("loadStorage") {
        assert(argc == 2);
        loadStorage(argv[1]);
    }

#ifdef BP_TREE_TEST
    test("B+ Tree") {
        NativeNaturalType elementCount = 1024*1024*128;
        TestBpTree tree;
        tree.init();
        // printf("  Capacity %hhd %d %d\n", TestBpTree::maxLayerCount, TestBpTree::Page::capacity<true>(), TestBpTree::Page::capacity<false>());
        // printf("  Bits %llu %llu %llu %hhu\n", TestBpTree::keyBits, TestBpTree::rankBits, TestBpTree::pageRefBits, architectureSize);
        // printf("  Offset %llu %llu %llu %llu\n", TestBpTree::Page::keyOffset, TestBpTree::Page::rankOffset, TestBpTree::Page::pageRefOffset, TestBpTree::Page::valueOffset);

        const NativeNaturalType insertPermutation[] = {
            5, 13, 113, 67, 112, 12, 15, 78, 1, 7, 38, 71, 84, 121, 93, 33, 36, 0, 47, 73, 72, 106, 22, 120, 18, 30, 20, 127, 27, 60, 101, 90, 69, 105, 87, 126, 124, 51, 59, 56, 44, 123, 68, 35, 65, 14, 82, 17, 118, 111, 11, 76, 53, 86, 92, 4, 108, 37, 114, 19, 46, 116, 104, 98, 110, 77, 64, 66, 58, 97, 62, 28, 2, 25, 83, 29, 85, 100, 54, 94, 43, 48, 107, 81, 79, 31, 23, 32, 57, 115, 119, 99, 91, 50, 117, 41, 21, 89, 75, 52, 24, 8, 16, 34, 9, 74, 102, 6, 39, 10, 80, 42, 96, 55, 61, 122, 40, 3, 95, 26, 88, 49, 45, 125, 63, 109, 103, 70
        };
        testBpTreeUsingPermutation<true>(tree, elementCount, sizeof(insertPermutation)/sizeof(NativeNaturalType), insertPermutation);

        const NativeNaturalType erasePermutation[] = {
            65, 9, 95, 91, 124, 101, 110, 123, 13, 18, 0, 126, 88, 82, 64, 32, 45, 86, 46, 51, 42, 104, 76, 90, 4, 5, 35, 53, 56, 7, 11, 100, 99, 62, 39, 50, 12, 1, 40, 108, 6, 83, 106, 112, 113, 114, 55, 37, 57, 75, 103, 105, 49, 17, 94, 2, 72, 36, 30, 97, 98, 119, 125, 33, 22, 23, 26, 109, 81, 77, 60, 111, 122, 67, 8, 70, 59, 41, 120, 73, 68, 3, 87, 19, 25, 15, 118, 89, 27, 116, 34, 71, 127, 10, 74, 66, 85, 31, 92, 84, 78, 107, 29, 44, 69, 38, 21, 102, 63, 24, 28, 14, 61, 54, 79, 20, 121, 117, 96, 48, 58, 47, 43, 93, 115, 16, 52, 80
        };
        testBpTreeUsingPermutation<false>(tree, elementCount, sizeof(erasePermutation)/sizeof(NativeNaturalType), erasePermutation);

        printf(CURSOR_TO_LEFT_LIMIT CLEAR_LINE);
        assert(tree.isEmpty() && superPage->pagesEnd == countRecyclablePages()+2);
    }
#endif

    test("BitVectorGuard<DataStructure>") {
        Symbol symbol;
        {
            BitVectorGuard<DataStructure<Vector<VoidType>>> container;
            symbol = container.getBitVector().location.symbol;
            container.getBitVector().setSize(32);
            assert(container.getBitVector().getSize() == 32);
        }
        assert(BitVector(BitVectorLocation(&heapSymbolSpace, symbol)).getSize() == 0);
    }

    test("Vector") {
        BitVectorGuard<DataStructure<Vector<NativeNaturalType>>> vector;
        vector.insertAsLastElement(2);
        vector.insertAsFirstElement(1);
        vector.insertElementAt(1, 0);
        vector.swapElementsAt(0, 1);
        assert(vector.getElementCount() == 3);
        NativeNaturalType index = 0;
        vector.iterateElements([&](NativeNaturalType element) {
            assert(element == index++);
        });
        vector.moveElementAt(2, 0);
        vector.moveElementAt(0, 1);
        vector.eraseElementAt(1);
        assert(vector.eraseLastElement() == 0
            && vector.eraseFirstElement() == 2
            && vector.getElementCount() == 0);
        vector.setElementCount(3);
        assert(vector.getElementCount() == 3);
        vector.setElementCount(0);
        assert(vector.getElementCount() == 0);
    }

    test("PairVector") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<PairVector<ElementType::FirstType, ElementType::SecondType>>> pairVector;
        pairVector.insertAsLastElement(ElementType{2, 0});
        pairVector.insertAsFirstElement(ElementType{3, 1});
        pairVector.setKeyAt(0, 0);
        pairVector.setValueAt(1, 3);
        assert(pairVector.getElementCount() == 2);
        NativeNaturalType index = 0;
        pairVector.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index+1);
            index += 2;
        });
        index = 0;
        pairVector.iterate([&](NativeNaturalType at) {
            assert(pairVector.getKeyAt(at) == index);
            index += 2;
        });
        index = 1;
        pairVector.iterate([&](NativeNaturalType at) {
            assert(pairVector.getValueAt(at) == index);
            index += 2;
        });
    }

    test("Heap") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<Heap<Ascending, ElementType::FirstType, ElementType::SecondType>>> heap;
        heap.insertElement(ElementType{2, 4});
        heap.insertElement(ElementType{0, 0});
        heap.insertElement(ElementType{4, 8});
        heap.insertElement(ElementType{1, 2});
        heap.insertElement(ElementType{3, 6});
        heap.build();
        assert(heap.getElementCount() == 5);
        NativeNaturalType index = 0;
        for(; index < 5; ++index) {
            Pair<NativeNaturalType, NativeNaturalType> element = heap.eraseFirstElement();
            assert(element.first == index && element.second == index*2);
        }
        heap.insertElement(ElementType{2, 4});
        heap.insertElement(ElementType{0, 0});
        heap.insertElement(ElementType{4, 8});
        heap.insertElement(ElementType{1, 2});
        heap.insertElement(ElementType{3, 6});
        heap.reverseSort();
        assert(heap.getElementCount() == 5);
        heap.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            --index;
            assert(element.first == index && element.second == index*2);
        });
    }

    test("Set") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<Set<ElementType::FirstType, ElementType::SecondType>>> set;
        assert(set.insertElement(ElementType{1, 2}) == true
            && set.insertElement(ElementType{5, 0}) == true
            && set.insertElement(ElementType{3, 6}) == true
            && set.insertElement(ElementType{0, 8}) == true
            && set.insertElement(ElementType{2, 4}) == true
            && set.getElementCount() == 5
            && set.setKeyAt(4, 1) == false
            && set.setKeyAt(0, 4) == true
            && set.setKeyAt(4, 0) == true);
        NativeNaturalType index = 0;
        set.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index*2);
            ++index;
        });
        assert(set.findKey(7, index) == false
            && set.findKey(2, index) == true && index == 2
            && set.eraseElementByKey(7) == false
            && set.eraseElementByKey(2) == true
            && set.eraseElementByKey(4) == true
            && set.eraseElementByKey(3) == true
            && set.getElementCount() == 2);
        index = 0;
        set.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index*2);
            ++index;
        });
    }

    test("MetaVector") {
        BitVectorGuard<DataStructure<MetaVector<NativeNaturalType>>> containerVector;
        containerVector.insertElementAt(0, 7);
        containerVector.increaseSize(containerVector.getChildOffset(0), 64, 0);
        containerVector.insertElementAt(0, 5);
        containerVector.increaseSize(containerVector.getChildOffset(0), 32, 0);
        containerVector.insertElementAt(1, 9);
        containerVector.increaseSize(containerVector.getChildOffset(1), 96, 1);
        assert(containerVector.moveElementAt(2, 1) == true
            && containerVector.moveElementAt(0, 1) == true
            && containerVector.getElementCount() == 3
            && containerVector.getKeyAt(0) == 7 && containerVector.getChildLength(0) == 64
            && containerVector.getKeyAt(1) == 5 && containerVector.getChildLength(1) == 32
            && containerVector.getKeyAt(2) == 9 && containerVector.getChildLength(2) == 96);
        containerVector.eraseElementAt(1);
        assert(containerVector.getElementCount() == 2
            && containerVector.getKeyAt(0) == 7 && containerVector.getChildLength(0) == 64
            && containerVector.getKeyAt(1) == 9 && containerVector.getChildLength(1) == 96);
    }

    test("MetaSet") {
        BitVectorGuard<DataStructure<MetaSet<NativeNaturalType>>> containerSet;
        assert(containerSet.insertElement(7) == true);
        containerSet.increaseSize(containerSet.getChildOffset(0), 64, 0);
        assert(containerSet.insertElement(5) == true);
        containerSet.increaseSize(containerSet.getChildOffset(0), 32, 0);
        assert(containerSet.insertElement(8) == true);
        containerSet.increaseSize(containerSet.getChildOffset(2), 96, 2);
        assert(containerSet.getElementCount() == 3
            && containerSet.getChildLength(0) == 32
            && containerSet.getChildLength(1) == 64
            && containerSet.getChildLength(2) == 96
            && containerSet.setKeyAt(1, 4)
            && containerSet.setKeyAt(1, 9)
            && containerSet.eraseElementByKey(8) == true
            && containerSet.getElementCount() == 2
            && containerSet.getChildLength(0) == 64
            && containerSet.getChildLength(1) == 32);
    }

    test("PairSet") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<PairSet<ElementType::FirstType, ElementType::SecondType>>> pairSet;
        assert(pairSet.insertElement(ElementType{3, 1})
            && pairSet.insertElement(ElementType{3, 3})
            && pairSet.insertElement(ElementType{5, 5})
            && pairSet.insertElement(ElementType{5, 7})
            && pairSet.insertElement(ElementType{5, 9})
            && pairSet.getFirstKeyCount() == 2
            && pairSet.getSecondKeyCount(0) == 2
            && pairSet.getSecondKeyCount(1) == 3);
        NativeNaturalType firstAt, secondAt;
        assert(pairSet.findFirstKey(1, firstAt) == false
            && pairSet.findFirstKey(5, firstAt) == true && firstAt == 1
            && pairSet.findSecondKey(1, firstAt, secondAt) == false
            && pairSet.findSecondKey(5, firstAt, secondAt) == true && secondAt == 0
            && pairSet.findElement({1, 1}, firstAt, secondAt) == false
            && pairSet.findElement({3, 5}, firstAt, secondAt) == false
            && pairSet.findElement({3, 3}, firstAt, secondAt) == true && firstAt == 0 && secondAt == 1);
        assert(pairSet.eraseElement({5, 9}) == true);
        NativeNaturalType counter = 1;
        pairSet.iterateFirstKeys([&](NativeNaturalType first) {
            counter += 2;
            assert(first == counter);
        });
        pairSet.iterateSecondKeys(1, [&](NativeNaturalType second) {
            assert(second == counter);
            counter += 2;
        });
        counter = 1;
        pairSet.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == (counter-1)/4*2+3 && element.second == counter);
            counter += 2;
        });
    }

    test("BitMap fillSlice and clearSlice") {
        BitVectorGuard<DataStructure<BitMap<>>> bitMap;
        NativeNaturalType sliceIndex, dstOffset;
        assert(bitMap.getElementCount() == 0);
        assert(bitMap.fillSlice(64, 16, dstOffset) == 1
            && (bitMap.getSliceContaining<false, false>(0, sliceIndex)) == false && sliceIndex == 0
            && (bitMap.getSliceContaining<false, false>(64, sliceIndex)) == false && sliceIndex == 0
            && (bitMap.getSliceContaining<false, false>(65, sliceIndex)) == true && sliceIndex == 0
            && (bitMap.getSliceContaining<false, false>(80, sliceIndex)) == false && sliceIndex == 1);
        assert(bitMap.fillSlice(0, 16, dstOffset) == 1
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 16
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
        assert(bitMap.fillSlice(8, 16, dstOffset) == 0
            && bitMap.fillSlice(56, 16, dstOffset) == 0
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 24
            && bitMap.getSliceBeginAddress(1) == 56 && bitMap.getChildLength(1) == 24);
        assert(bitMap.fillSlice(24, 8, dstOffset) == 0
            && bitMap.fillSlice(48, 8, dstOffset) == 0
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 32
            && bitMap.getSliceBeginAddress(1) == 48 && bitMap.getChildLength(1) == 32);
        assert(bitMap.fillSlice(24, 32, dstOffset) == -1
            && bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 80);
        assert(bitMap.clearSlice(32, 16) == 1
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 32
            && bitMap.getSliceBeginAddress(1) == 48 && bitMap.getChildLength(1) == 32);
        assert(bitMap.clearSlice(24, 8) == 0
            && bitMap.clearSlice(48, 8) == 0
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 24
            && bitMap.getSliceBeginAddress(1) == 56 && bitMap.getChildLength(1) == 24);
        assert(bitMap.clearSlice(16, 16) == 0
            && bitMap.clearSlice(48, 16) == 0
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 16
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
        assert(bitMap.clearSlice(64, 16) == -1
            && bitMap.clearSlice(0, 16) == -1
            && bitMap.getElementCount() == 0);
    }

    test("BitMap copySlice") {
        BitVectorGuard<DataStructure<BitMap<>>> bitMap;
        NativeNaturalType dstOffset;
        assert(bitMap.fillSlice(16, 16, dstOffset) == 1);
        bitMap.copySlice(bitMap, 16, 64, 16);
        assert(bitMap.getElementCount() == 0);
        assert(bitMap.fillSlice(16, 16, dstOffset) == 1
            && bitMap.fillSlice(64, 8, dstOffset) == 1);
        bitMap.copySlice(bitMap, 64, 8, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 16
            && bitMap.getSliceBeginAddress(1) == 72 && bitMap.getChildLength(1) == 8);
        bitMap.copySlice(bitMap, 64, 24, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 16
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 8);
        bitMap.copySlice(bitMap, 24, 32, 8);
        assert(bitMap.fillSlice(72, 8, dstOffset) == 0
            && bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
        bitMap.copySlice(bitMap, 16, 56, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 24 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
        bitMap.copySlice(bitMap, 16, 72, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
    }

    test("BitMap moveSlice") {
        BitVectorGuard<DataStructure<BitMap<>>> bitMap;
        NativeNaturalType dstOffset;
        assert(bitMap.fillSlice(16, 16, dstOffset) == 1
            && bitMap.moveSlice(32, 16, 8) == false
            && bitMap.moveSlice(32, 24, 8) == true);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 32 && bitMap.getChildLength(1) == 8
            && bitMap.moveSlice(24, 16, 8) == true);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 24 && bitMap.getChildLength(0) == 16
            && bitMap.moveSlice(32, 24, 8) == true);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 32 && bitMap.getChildLength(0) == 8);
        assert(bitMap.fillSlice(24, 8, dstOffset) == 0
            && bitMap.moveSlice(16, 32, 8) == false
            && bitMap.moveSlice(16, 24, 8) == true);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 32 && bitMap.getChildLength(1) == 8
            && bitMap.moveSlice(24, 32, 8)== true);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 16
            && bitMap.moveSlice(16, 24, 8) == true);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.fillSlice(32, 8, dstOffset) == 1);
        bitMap.moveSlice(bitMap, 48, 16, 8);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 32 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 48 && bitMap.getChildLength(1) == 8);
        bitMap.moveSlice(bitMap, 28, 48, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 28 && bitMap.getChildLength(0) == 12);
        bitMap.moveSlice(bitMap, 32, 28, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 32 && bitMap.getChildLength(0) == 8);
    }

    test("BitMap insertSlice and eraseSlice") {
        BitVectorGuard<DataStructure<BitMap<>>> bitMap;
        NativeNaturalType dstOffset;
        bitMap.insertSlice(24, 8, dstOffset);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 24 && bitMap.getChildLength(0) == 8);
        bitMap.eraseSlice(24, 8);
        assert(bitMap.getElementCount() == 0
            && bitMap.fillSlice(16, 16, dstOffset) == 1);
        bitMap.insertSlice(24, 8, dstOffset);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 24);
        bitMap.clearSlice(24, 8);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 32 && bitMap.getChildLength(1) == 8);
        bitMap.eraseSlice(24, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 16);
        bitMap.eraseSlice(20, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8);
    }

    test("ArithmeticCodec Symbol") {
        BitVectorGuard<BitVector> bitVector;
        NativeNaturalType offset = 0;
        ArithmeticEncoder encoder(bitVector, offset, 3);
        for(NativeNaturalType i = 0; i < 8; ++i)
            encoder.encodeSymbol(i%3);
        encoder.encodeTermination();
        offset = 0;
        ArithmeticDecoder decoder(bitVector, offset, encoder.model.symbolCount);
        for(NativeNaturalType i = 0; i < 8; ++i)
            assert(decoder.decodeSymbol() == (i%3));
    }

    test("ArithmeticCodec BitVector") {
        BitVectorGuard<BitVector> plain, compressed, decompressed;
        plain.setSize(strlen(gitRef)*8);
        plain.template externalOperate<true>(gitRef, 0, plain.getSize());
        decompressed.setSize(plain.getSize());
        NativeNaturalType offset = 0;
        arithmeticEncodeBitVector(compressed, offset, plain, 0, plain.getSize());
        offset = 0;
        arithmeticDecodeBitVector(decompressed, 0, decompressed.getSize(), compressed, offset);
        assert(compressed.compare(plain) != 0 && decompressed.compare(plain) == 0);
    }

    test("Triple") {
        Ontology ontologyA(1), ontologyB(2);
        Triple triple = {ontologyA.createSymbol(), ontologyA.createSymbol(), ontologyA.createSymbol()};
        assert(ontologyA.query(QueryMask::MMM, triple) == 0);
        assert(ontologyB.query(QueryMask::MMM, triple) == 0);
        assert(ontologyA.link(triple) == true);
        assert(ontologyA.query(QueryMask::MMM, triple) == 1);
        assert(ontologyB.query(QueryMask::MMM, triple) == 0);
        triple = {ontologyB.createSymbol(), ontologyB.createSymbol(), ontologyB.createSymbol()};
        assert(ontologyB.link(triple) == true);
        assert(ontologyA.query(QueryMask::MMM, triple) == 1);
        assert(ontologyB.query(QueryMask::MMM, triple) == 1);
        assert(ontologyA.unlink(triple) == true);
        assert(ontologyA.query(QueryMask::MMM, triple) == 0);
        assert(ontologyB.query(QueryMask::MMM, triple) == 1);
        assert(ontologyB.unlink(triple) == true);
        assert(ontologyA.query(QueryMask::MMM, triple) == 0);
        assert(ontologyB.query(QueryMask::MMM, triple) == 0);
        assert(ontologyA.state.bitVectorCount == 0);
    }

    test("unloadStorage") {
        unloadStorage();
    }

    endTests();
    return 0;
}

NativeNaturalType testCount = __COUNTER__;

}
