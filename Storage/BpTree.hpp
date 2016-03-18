#include "Basics.hpp"

template<class KeyType, class ValueType>
class BpTree {
    public:
    typedef int16_t DistributionType;
    typedef uint16_t IndexType;
    typedef int8_t LayerType;

    struct IteratorFrame {
        PageRefType pageRef;
        IndexType index, endIndex;
    };

    struct InsertIteratorFrame : public IteratorFrame {
        PageRefType lowerInnerPageRef, higherInnerPageRef, higherOuterPageRef;
        IndexType lowerInnerIndex, higherInnerEndIndex, higherOuterEndIndex, elementsPerPage;
        NativeNaturalType pageCount;
    };

    class Page : public BasePage {
        public:
        static const NativeNaturalType
            HeaderBits = sizeof(BasePage)*8+sizeof(IndexType)*8,
            BodyOffset = architecturePadding(HeaderBits),
            BodyBits = Storage::bitsPerPage-BodyOffset,
            KeyBits = sizeof(KeyType)*8,
            PageRefBits = sizeof(PageRefType)*8,
            ValueBits = sizeof(ValueType)*8,
            BranchElementBits = PageRefBits+KeyBits,
            LeafElementBits = KeyBits+ValueBits,
            BranchKeyCount = (BodyBits-PageRefBits)/BranchElementBits,
            LeafKeyCount = BodyBits/LeafElementBits;

        template<bool isLeaf>
        static NativeNaturalType capacity() {
            return (isLeaf) ? LeafKeyCount : BranchKeyCount+1;
        }

        IndexType count;

        template<bool isLeaf>
        IndexType keyCount() const {
            return (isLeaf) ? count : count-1;
        }

        template<typename Type, NativeNaturalType offset, NativeNaturalType stride>
        Type get(IndexType src) const {
            Type result;
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&result),
                                     reinterpret_cast<const NativeNaturalType*>(this),
                                     0, BodyOffset+offset+stride*src, sizeof(Type)*8);
            return result;
        }

        template<bool isLeaf>
        KeyType getKey(IndexType src) const {
            return (isLeaf)
                ? get<KeyType, 0, LeafElementBits>(src)
                : get<KeyType, PageRefBits, BranchElementBits>(src);
        }

        PageRefType getPageRef(IndexType src) const {
            return get<PageRefType, 0, BranchElementBits>(src);
        }

        ValueType getValue(IndexType src) const {
            return get<ValueType, KeyBits, LeafElementBits>(src);
        }

        template<typename Type, NativeNaturalType offset, NativeNaturalType stride>
        void set(IndexType dst, Type content) {
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                     reinterpret_cast<const NativeNaturalType*>(&content),
                                     BodyOffset+offset+stride*dst, 0, sizeof(Type)*8);
        }

        template<bool isLeaf>
        void setKey(IndexType dst, KeyType content) {
            if(isLeaf)
                set<KeyType, 0, LeafElementBits>(dst, content);
            else
                set<KeyType, PageRefBits, BranchElementBits>(dst, content);
        }

        void setPageRef(IndexType dst, PageRefType content) {
            set<PageRefType, 0, BranchElementBits>(dst, content);
        }

        void setValue(IndexType dst, ValueType content) {
            set<ValueType, KeyBits, LeafElementBits>(dst, content);
        }

        template<bool isLeaf>
        void debugPrint() const {
            if(!isLeaf)
                printf("%04llu ", getPageRef(0));
            for(IndexType i = 0; i < keyCount<isLeaf>(); ++i) {
                if(i > 0)
                    printf(" ");
                printf("[%08llu] ", getKey<isLeaf>(i));
                if(isLeaf)
                    printf("%08llu", getValue(i));
                else
                    printf("%04llu", getPageRef(i+1));
            }
            printf("\n");
        }

        template<bool isLeaf>
        bool isValid() const {
            if(count <= !isLeaf || count > capacity<isLeaf>())
                return false;
            for(IndexType i = 1; i < keyCount<isLeaf>(); ++i)
                if(getKey<isLeaf>(i-1) >= getKey<isLeaf>(i))
                    return false;
            return true;
        }

        template<bool isLeaf>
        IndexType indexOfKey(KeyType key) const {
            return binarySearch<IndexType>(keyCount<isLeaf>(), [&](IndexType at) {
                return ((isLeaf && key > getKey<true>(at)) ||
                        (!isLeaf && key >= getKey<false>(at)));
            });
        }

        template<bool frontKey, int dir = -1>
        static void copyBranchElements(Page* dstPage, Page* srcPage,
                                       IndexType dstIndex, IndexType srcIndex,
                                       IndexType n) {
            if(n == 0)
                return;
            NativeNaturalType dst = BranchElementBits*dstIndex,
                             src = BranchElementBits*srcIndex,
                             len = BranchElementBits*n;
            if(frontKey) {
                assert(dstIndex > 0 && srcIndex > 0);
                dst -= KeyBits;
                src -= KeyBits;
            } else
                len -= KeyBits;
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      BodyOffset+dst, BodyOffset+src, len);
        }

        template<int dir = -1>
        static void copyLeafElements(Page* dstPage, Page* srcPage,
                                     IndexType dstIndex, IndexType srcIndex,
                                     IndexType n) {
            if(n == 0)
                return;
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      BodyOffset+LeafElementBits*dstIndex,
                                      BodyOffset+LeafElementBits*srcIndex,
                                      (KeyBits+ValueBits)*n);
        }

        template<bool isLeaf>
        void copy(Page* dstPage, Page* srcPage) {
            assert(dstPage != srcPage);
            dstPage->count = srcPage->count;
            if(isLeaf)
                copyLeafElements(dstPage, srcPage, 0, 0, srcPage->count);
            else
                copyBranchElements<false>(dstPage, srcPage, 0, 0, srcPage->count);
        }

        template<bool dstIsLeaf, bool srcIsLeaf>
        static void copyKey(Page* dstPage, Page* srcPage,
                            IndexType dstIndex, IndexType srcIndex) {
            // dstPage->template setKey<dstIsLeaf>(dstIndex, srcPage->template getKey<srcIsLeaf>(srcIndex));
            NativeNaturalType dst, src;
            if(dstIsLeaf)
                dst = BodyOffset+LeafElementBits*dstIndex;
            else
                dst = BodyOffset+PageRefBits+BranchElementBits*dstIndex;
            if(srcIsLeaf)
                src = BodyOffset+LeafElementBits*srcIndex;
            else
                src = BodyOffset+PageRefBits+BranchElementBits*srcIndex;
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                     reinterpret_cast<const NativeNaturalType*>(srcPage),
                                     dst, src, sizeof(KeyType)*8);
        }

        static void swapKeyInParent(Page* parent, Page* dstPage, Page* srcPage,
                                    IndexType parentIndex, IndexType dstIndex, IndexType srcIndex) {
            copyKey<false, false>(dstPage, parent, dstIndex, parentIndex);
            copyKey<false, false>(parent, srcPage, parentIndex, srcIndex);
        }

        static void shiftCounts(Page* dstPage, Page* srcPage, IndexType n) {
            dstPage->count += n;
            srcPage->count -= n;
        }

        static void distributeCount(Page* lower, Page* higher, DistributionType n) {
            lower->count = (n+1)/2;
            higher->count = n/2;
        }

        template<bool isLeaf>
        static void insert(InsertIteratorFrame* frame, Page* lower, IndexType count) {
            if(!isLeaf)
                ++frame->index;
            assert(count > lower->count && count <= capacity<isLeaf>() && frame->index <= lower->count);
            frame->endIndex = frame->index+count-lower->count;
            if(isLeaf)
                copyLeafElements<1>(lower, lower, frame->endIndex, frame->index, lower->count-frame->index);
            else {
                assert(frame->index > 0);
                copyBranchElements<true, 1>(lower, lower, frame->endIndex, frame->index, lower->count-frame->index);
            }
            lower->count = count;
        }

        template<bool isLeaf>
        static void insertOverflow(InsertIteratorFrame* frame,
                                   Page* lowerInnerParent, Page* higherOuterParent,
                                   Page* lowerOuter, Page* lowerInner, Page* higherInner, Page* higherOuter,
                                   IndexType lowerInnerParentIndex, IndexType higherOuterParentIndex) {
            if(!isLeaf)
                ++frame->index;
            bool insertKeyInParentNow;
            DistributionType shiftHigherInner = frame->index+frame->endIndex-lowerOuter->count,
                             shiftHigherOuter = lowerOuter->count-frame->index, higherOuterEndIndex;
            assert(shiftHigherInner > 0 && frame->endIndex <= capacity<isLeaf>()*2 && frame->index <= lowerOuter->count);
            lowerInner->count = higherInner->count = frame->elementsPerPage;
            distributeCount(lowerOuter, higherOuter, frame->endIndex);
            if(shiftHigherInner < lowerOuter->count) {
                shiftHigherInner = lowerOuter->count-shiftHigherInner;
                assert(shiftHigherInner < higherInner->count);
                frame->lowerInnerIndex = 0;
                shiftHigherOuter -= shiftHigherInner;
            } else {
                shiftHigherInner = 0;
                frame->lowerInnerIndex = (frame->index > lowerOuter->count) ? frame->index-lowerOuter->count : 0;
            }
            frame->endIndex = lowerOuter->count;
            frame->higherInnerEndIndex = higherInner->count-shiftHigherInner;
            frame->higherOuterEndIndex = higherOuter->count-shiftHigherOuter;
            if(lowerOuter == higherInner)
                frame->endIndex = frame->higherInnerEndIndex;
            if(isLeaf) {
                copyLeafElements(lowerInner, lowerOuter, 0, lowerOuter->count, frame->lowerInnerIndex);
                copyLeafElements(higherOuter, lowerOuter, frame->higherOuterEndIndex, frame->index+shiftHigherInner, shiftHigherOuter);
                copyLeafElements<1>(higherInner, lowerOuter, frame->higherInnerEndIndex, frame->index, shiftHigherInner);
                if(frame->lowerInnerIndex > 0)
                    copyKey<false, true>(lowerInnerParent, lowerInner, lowerInnerParentIndex, 0);
                else if(frame->higherOuterEndIndex == 0)
                    copyKey<false, true>(higherOuterParent, higherOuter, higherOuterParentIndex, 0);
            } else {
                if(frame->lowerInnerIndex > 0) {
                    copyKey<false, false>(lowerInnerParent, lowerOuter, lowerInnerParentIndex, lowerOuter->count-1);
                    copyBranchElements<false>(lowerInner, lowerOuter, 0, lowerOuter->count, frame->lowerInnerIndex);
                    copyBranchElements<true>(higherOuter, lowerOuter, frame->higherOuterEndIndex, frame->index+shiftHigherInner, shiftHigherOuter);
                } else if(frame->higherOuterEndIndex == 0) {
                    copyKey<false, false>(higherOuterParent, lowerOuter, higherOuterParentIndex, frame->index+shiftHigherInner-1);
                    copyBranchElements<false>(higherOuter, lowerOuter, 0, frame->index+shiftHigherInner, shiftHigherOuter);
                } else
                    copyBranchElements<true>(higherOuter, lowerOuter, frame->higherOuterEndIndex, frame->index+shiftHigherInner, shiftHigherOuter);
                copyBranchElements<true, 1>(higherInner, lowerOuter, frame->higherInnerEndIndex, frame->index, shiftHigherInner);
            }
        }

        template<bool isLeaf>
        static void erase1(Page* lower, IndexType start, IndexType end) {
            assert(start < end && end <= lower->count);
            if(isLeaf)
                copyLeafElements<-1>(lower, lower, start, end, lower->count-end);
            else if(start > 0)
                copyBranchElements<true, -1>(lower, lower, start, end, lower->count-end);
            else if(end < lower->count)
                copyBranchElements<false, -1>(lower, lower, 0, end, lower->count-end);
            lower->count -= end-start;
        }

        template<bool isLeaf>
        static bool erase2(Page* parent, Page* lower, Page* higher,
                           IndexType parentIndex, IndexType startInLower, IndexType endInHigher) {
            assert(startInLower <= lower->count && endInHigher <= higher->count);
            DistributionType count = startInLower+higher->count-endInHigher;
            if(count <= capacity<isLeaf>()) {
                lower->count = count;
                if(count == 0)
                    return true;
                if(isLeaf)
                    copyLeafElements(lower, higher, startInLower, endInHigher, higher->count-endInHigher);
                else if(startInLower == 0)
                    copyBranchElements<false, -1>(lower, higher, 0, endInHigher, lower->count);
                else if(endInHigher == 0) {
                    copyKey<true, false>(lower, parent, startInLower-1, parentIndex);
                    copyBranchElements<false, -1>(lower, higher, startInLower, 0, higher->count);
                } else
                    copyBranchElements<true, -1>(lower, higher, startInLower, endInHigher, higher->count-endInHigher);
                return true;
            } else {
                assert(startInLower > 0);
                distributeCount(lower, higher, count);
                count = startInLower-lower->count;
                if(isLeaf) {
                    if(count <= 0) {
                        count *= -1;
                        copyLeafElements(lower, higher, startInLower, endInHigher, count);
                        copyLeafElements<-1>(higher, higher, 0, endInHigher+count, higher->count);
                    } else {
                        if(count < endInHigher)
                            copyLeafElements<-1>(higher, higher, count, endInHigher, higher->count);
                        else
                            copyLeafElements<1>(higher, higher, count, endInHigher, higher->count);
                        copyLeafElements(higher, lower, 0, lower->count, count);
                    }
                    copyKey<false, true>(parent, higher, parentIndex, 0);
                } else {
                    if(count <= 0) {
                        count *= -1;
                        if(endInHigher == 0 && count > 0) {
                            copyBranchElements<false>(lower, higher, startInLower, 0, count);
                            swapKeyInParent(parent, lower, higher, parentIndex, startInLower-1, count-1);
                        } else {
                            copyBranchElements<true>(lower, higher, startInLower, endInHigher, count);
                            copyKey<false, false>(parent, higher, parentIndex, endInHigher+count-1);
                        }
                        copyBranchElements<false, -1>(higher, higher, 0, endInHigher+count, higher->count);
                    } else {
                        if(endInHigher == 0) {
                            copyBranchElements<false, 1>(higher, higher, count, 0, higher->count);
                            swapKeyInParent(parent, higher, lower, parentIndex, count-1, lower->count-1);
                        } else {
                            if(count < endInHigher)
                                copyBranchElements<true, -1>(higher, higher, count, endInHigher, higher->count);
                            else
                                copyBranchElements<true, 1>(higher, higher, count, endInHigher, higher->count);
                            copyKey<false, false>(parent, lower, parentIndex, lower->count-1);
                        }
                        copyBranchElements<false>(higher, lower, 0, lower->count, count);
                    }
                }
                return false;
            }
        }

        template<bool isLeaf>
        static void evacuateDown(Page* parent, Page* lower, Page* higher, IndexType parentIndex) {
            assert(lower->count+higher->count <= capacity<isLeaf>());
            if(isLeaf)
                copyLeafElements(lower, higher, lower->count, 0, higher->count);
            else {
                copyKey<false, false>(lower, parent, lower->count-1, parentIndex);
                copyBranchElements<false>(lower, higher, lower->count, 0, higher->count);
            }
            lower->count += higher->count;
        }

        template<bool isLeaf>
        static void evacuateUp(Page* parent, Page* lower, Page* higher, IndexType parentIndex) {
            assert(lower->count+higher->count <= capacity<isLeaf>());
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, lower->count, 0, higher->count);
                copyLeafElements(higher, lower, 0, 0, lower->count);
            } else {
                copyBranchElements<false, +1>(higher, higher, lower->count, 0, higher->count);
                copyKey<false, false>(higher, parent, lower->count-1, parentIndex);
                copyBranchElements<false>(higher, lower, 0, 0, lower->count);
            }
            higher->count += lower->count;
        }

        template<bool isLeaf>
        static void shiftDown(Page* parent, Page* lower, Page* higher, IndexType parentIndex, IndexType count) {
            assert(count > 0 && lower->count+count <= capacity<isLeaf>());
            if(isLeaf) {
                copyLeafElements(lower, higher, lower->count, 0, count);
                shiftCounts(lower, higher, count);
                copyLeafElements<-1>(higher, higher, 0, count, higher->count);
                copyKey<false, true>(parent, higher, parentIndex, 0);
            } else {
                swapKeyInParent(parent, lower, higher, parentIndex, lower->count-1, count-1);
                copyBranchElements<false>(lower, higher, lower->count, 0, count);
                shiftCounts(lower, higher, count);
                copyBranchElements<false, -1>(higher, higher, 0, count, higher->count);
            }
        }

        template<bool isLeaf>
        static void shiftUp(Page* parent, Page* lower, Page* higher, IndexType parentIndex, IndexType count) {
            assert(count > 0 && higher->count+count <= capacity<isLeaf>());
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, count, 0, higher->count);
                shiftCounts(higher, lower, count);
                copyLeafElements(higher, lower, 0, lower->count, count);
                copyKey<false, true>(parent, higher, parentIndex, 0);
            } else {
                copyBranchElements<false, +1>(higher, higher, count, 0, higher->count);
                shiftCounts(higher, lower, count);
                copyBranchElements<false>(higher, lower, 0, lower->count, count);
                swapKeyInParent(parent, higher, lower, parentIndex, count-1, lower->count-1);
            }
        }

        template<bool isLeaf, bool lowerIsMiddle>
        static bool redistribute2(Page* parent, Page* lower, Page* higher, IndexType parentIndex) {
            DistributionType count = lower->count+higher->count;
            if(count <= capacity<isLeaf>()) {
                if(lowerIsMiddle)
                    evacuateUp<isLeaf>(parent, lower, higher, parentIndex);
                else
                    evacuateDown<isLeaf>(parent, lower, higher, parentIndex);
                return true;
            } else {
                count = ((lowerIsMiddle)?higher:lower)->count-count/2;
                if(lowerIsMiddle)
                    shiftDown<isLeaf>(parent, lower, higher, parentIndex, count);
                else
                    shiftUp<isLeaf>(parent, lower, higher, parentIndex, count);
                return false;
            }
        }

        template<bool isLeaf>
        static bool redistribute3(Page* middleParent, Page* higherParent,
                                  Page* lower, Page* middle, Page* higher,
                                  IndexType middleParentIndex, IndexType higherParentIndex) {
            DistributionType count = lower->count+middle->count+higher->count;
            if(count <= capacity<isLeaf>()*2) {
                count = count/2-higher->count;
                if(count < 0) {
                    count *= -1;
                    evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
                    shiftDown<isLeaf>(higherParent, lower, higher, higherParentIndex, count);
                } else if(count <= middle->count) {
                    if(count > 0)
                        shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, count);
                    evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
                } else if(isLeaf) {
                    copyLeafElements<+1>(higher, higher, count, 0, higher->count);
                    higher->count += count;
                    count -= middle->count;
                    lower->count -= count;
                    copyLeafElements(higher, middle, count, 0, middle->count);
                    copyLeafElements(higher, lower, 0, lower->count, count);
                    copyKey<false, true>(higherParent, higher, higherParentIndex, 0);
                } else {
                    copyBranchElements<false, +1>(higher, higher, count, 0, higher->count);
                    copyKey<false, false>(higher, higherParent, count-1, higherParentIndex);
                    higher->count += count;
                    count -= middle->count;
                    lower->count -= count;
                    copyBranchElements<false>(higher, middle, count, 0, middle->count);
                    copyBranchElements<false>(higher, lower, 0, lower->count, count);
                    copyKey<false, false>(higher, middleParent, count-1, middleParentIndex);
                    copyKey<false, false>(higherParent, lower, higherParentIndex, lower->count-1);
                }
                count = lower->count+higher->count;
                assert(lower->count == (count+1)/2);
                assert(higher->count == count/2);
                return true;
            } else {
                count = count/3;
                DistributionType shiftLower = lower->count-count, shiftUpper = higher->count-count;
                if(shiftLower < 0) {
                    if(shiftUpper < 0)
                        shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, -shiftUpper);
                    else if(shiftUpper > 0)
                        shiftDown<isLeaf>(higherParent, middle, higher, higherParentIndex, shiftUpper);
                    shiftDown<isLeaf>(middleParent, lower, middle, middleParentIndex, -shiftLower);
                } else {
                    if(shiftLower > 0)
                        shiftUp<isLeaf>(middleParent, lower, middle, middleParentIndex, shiftLower);
                    if(shiftUpper < 0)
                        shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, -shiftUpper);
                    else if(shiftUpper > 0)
                        shiftDown<isLeaf>(higherParent, middle, higher, higherParentIndex, shiftUpper);
                }
                return false;
            }
        }

        template<bool isLeaf>
        static bool redistribute(Page* middleParent, Page* higherParent,
                                 Page* lower, Page* middle, Page* higher,
                                 IndexType middleParentIndex, IndexType higherParentIndex) {
            if(lower) {
                if(higher) {
                    return redistribute3<isLeaf>(middleParent, higherParent,
                                                 lower, middle, higher,
                                                 middleParentIndex, higherParentIndex);
                } else
                    return redistribute2<isLeaf, false>(middleParent, lower, middle, middleParentIndex);
            } else
                return redistribute2<isLeaf, true>(higherParent, middle, higher, higherParentIndex);
        }
    };

    PageRefType rootPageRef;
    LayerType layerCount;
    static const LayerType maxLayerCount = 9;

    template<bool writeable>
    static Page* getPage(PageRefType& pageRef) {
        return Storage::dereferencePage<Page>(pageRef);
    }

    template<bool writeable, typename FrameType = IteratorFrame>
    struct Iterator {
        LayerType end;
        FrameType stack[maxLayerCount];

        FrameType* fromBegin(LayerType layer) {
            assert(layer < end);
            return &stack[layer];
        }

        FrameType* fromEnd(LayerType layer = 1) {
            assert(layer > 0 && layer <= end);
            return &stack[end-layer];
        }

        FrameType* getParentFrame(LayerType layer) {
            while(layer > 0) {
                FrameType* frame = fromBegin(--layer);
                if(frame->index > 0)
                    return frame;
            }
            return nullptr;
        }

        bool isValid() {
            for(LayerType layer = 0; layer < end; ++layer)
                if(fromEnd()->index >= fromEnd()->endIndex)
                    return false;
            return true;
        }

        template<bool srcWriteable>
        void copy(Iterator<srcWriteable>& src) {
            static_assert(!writeable || srcWriteable, "Can not copy from read only to writeable");
            LayerType offset = end-src.end;
            for(LayerType layer = 0; layer < src.end; ++layer)
                Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(fromBegin(layer+offset)),
                                         reinterpret_cast<const NativeNaturalType*>(src.fromBegin(layer)),
                                         0, 0, sizeof(IteratorFrame)*8);
        }

        char compare(Iterator& other) {
            LayerType layer = 0;
            for(; fromBegin(layer)->index == other.fromBegin(layer)->index; ++layer)
                if(layer == end-1)
                    return 0;
            return (fromBegin(layer)->index < other.fromBegin(layer)->index) ? -1 : 1;
        }

        template<int dir = 1>
        NativeNaturalType advanceAtLayer(LayerType atLayer, NativeNaturalType steps = 1) {
            if(steps == 0 || end == 0 || atLayer < 0 || atLayer >= end)
                return steps;
            bool keepRunning;
            do {
                LayerType layer = atLayer;
                FrameType* frame = fromBegin(layer);
                NativeNaturalType stepsToTake;
                if(dir == 1) {
                    stepsToTake = min((NativeNaturalType)(frame->endIndex-1-frame->index), steps);
                    frame->index += stepsToTake;
                } else {
                    stepsToTake = min((NativeNaturalType)frame->index, steps);
                    frame->index -= stepsToTake;
                }
                steps -= stepsToTake;
                bool keepRunning = false;
                if(steps > 0)
                    while(layer > 0) {
                        frame = fromBegin(--layer);
                        if(dir == 1) {
                            if(frame->index+1 < frame->endIndex)
                                ++frame->index;
                            else
                                continue;
                        } else {
                            if(frame->index > 0)
                                --frame->index;
                            else
                                continue;
                        }
                        --steps;
                        keepRunning = true;
                        break;
                    }
                if(keepRunning || stepsToTake > 0) {
                    LayerType endLayer = atLayer+1;
                    if(endLayer > end-1)
                        endLayer = end-1;
                    Page* parent = getPage<false>(frame->pageRef);
                    while(layer < endLayer) {
                        IndexType parentIndex = frame->index;
                        frame = fromBegin(++layer);
                        frame->pageRef = parent->getPageRef(parentIndex);
                        Page* page = getPage<writeable>(frame->pageRef);
                        // parent->setPageRef(parentIndex, frame->pageRef); // TODO: Update pageRef in getPage
                        frame->endIndex = page->count;
                        frame->index = (dir == 1) ? 0 : frame->endIndex-1;
                        parent = page;
                    }
                }
            } while(keepRunning);
            return steps;
        }

        template<int dir = 1>
        NativeNaturalType advance(NativeNaturalType steps = 1) {
            return advanceAtLayer(end-1, steps);
        }

        KeyType getKey() {
            FrameType* frame = fromEnd();
            return getPage<writeable>(frame->pageRef)->template getKey<true>(frame->index);
        }

        ValueType getValue() {
            FrameType* frame = fromEnd();
            return getPage<writeable>(frame->pageRef)->getValue(frame->index);
        }

        void setKey(KeyType key) {
            FrameType* frame = fromEnd();
            getPage<writeable>(frame->pageRef)->template setKey<true>(frame->index, key);
        }

        void setValue(ValueType value) {
            FrameType* frame = fromEnd();
            getPage<writeable>(frame->pageRef)->setValue(frame->index, value);
        }

        void debugPrint() {
            printf("Iterator %hd\n", static_cast<uint16_t>(end));
            for(LayerType layer = 0; layer < end; ++layer) {
                auto frame = fromBegin(layer);
                printf("    %hhd: %04llu (%hd/%hd)\n", layer, frame->pageRef, frame->index, frame->endIndex);
                Page* page = getPage<false>(frame->pageRef);
                if(layer == end-1)
                    page->template debugPrint<true>();
                else
                    page->template debugPrint<false>();
            }
        }
    };

    template<bool writeable, bool border = false, bool lower = false>
	bool at(Iterator<writeable>& iter, KeyType key = 0) {
        iter.end = layerCount;
		if(empty())
            return false;
        LayerType layer = 0;
        PageRefType pageRef = rootPageRef;
	    while(true) {
	        auto frame = iter.fromBegin(layer);
            frame->pageRef = pageRef;
            Page* page = getPage<writeable>(frame->pageRef);
            // parent->setPageRef(parentIndex, frame->pageRef); // TODO: Update pageRef in getPage
            frame->endIndex = page->count;
	        if(++layer == iter.end) {
                if(border)
                    frame->index = (lower) ? 0 : page->count;
                else
                    frame->index = page->template indexOfKey<true>(key);
                return border || (frame->index < page->count && page->template getKey<true>(frame->index) == key);
	        } else {
                if(border)
                    frame->index = (lower) ? 0 : page->count-1;
                else
                    frame->index = page->template indexOfKey<false>(key);
                pageRef = page->getPageRef(frame->index);
	        }
	    }
	}

    void init() {
        rootPageRef = 0;
        layerCount = 0;
    }

    bool empty() const {
        return (rootPageRef == 0);
    }

    struct InsertData {
        LayerType startLayer, layer;
        NativeNaturalType elementCount;
        IndexType lowerInnerParentIndex, higherOuterParentIndex;
        Page *lowerInnerParent, *higherOuterParent;
    };

    template<typename FrameType, bool isLeaf>
    static bool insertPhase1Layer(InsertData& data, Iterator<false, FrameType>& iter) {
        bool apply = isSame<FrameType, InsertIteratorFrame>();
        Page* lowerOuter;
        auto frame = reinterpret_cast<InsertIteratorFrame*>(iter.fromBegin(data.layer));
        if(data.layer >= data.startLayer) {
            if(!isLeaf)
                --data.elementCount;
            if(data.elementCount == 0)
                return false;
            lowerOuter = getPage<false>(frame->pageRef);
            data.elementCount += lowerOuter->count;
        } else if(!isLeaf && data.elementCount == 1)
            return false;
        NativeNaturalType pageCount = (data.elementCount+Page::template capacity<isLeaf>()-1)/Page::template capacity<isLeaf>();
        if(apply) {
            frame->elementsPerPage = (data.elementCount+pageCount-1)/pageCount;
            frame->pageCount = pageCount-1;
            if(data.layer >= data.startLayer) {
                if(frame->pageCount == 0) {
                    frame->higherOuterPageRef = 0;
                    Page::template insert<isLeaf>(frame, lowerOuter, data.elementCount);
                } else {
                    frame->endIndex = data.elementCount-(frame->pageCount-1)*frame->elementsPerPage;
                    frame->higherOuterPageRef = Storage::aquirePage();
                    switch(frame->pageCount) {
                        case 1:
                            frame->lowerInnerPageRef = frame->higherOuterPageRef;
                            frame->higherInnerPageRef = frame->pageRef;
                            break;
                        case 2:
                            frame->lowerInnerPageRef = Storage::aquirePage();
                            frame->higherInnerPageRef = frame->lowerInnerPageRef;
                            break;
                        default:
                            frame->lowerInnerPageRef = Storage::aquirePage();
                            frame->higherInnerPageRef = Storage::aquirePage();
                            break;
                    }
                }
            } else {
                frame->pageRef = Storage::aquirePage();
                lowerOuter = getPage<false>(frame->pageRef);
                if(!isLeaf)
                    lowerOuter->setPageRef(0, iter.fromBegin(data.layer+1)->pageRef);
                if(frame->pageCount == 0) {
                    lowerOuter->count = data.elementCount-frame->pageCount*frame->elementsPerPage;
                    frame->higherOuterPageRef = 0;
                } else {
                    frame->lowerInnerPageRef = 0;
                    if(frame->pageCount <= 1)
                        frame->higherInnerPageRef = 0;
                    else {
                        frame->higherInnerPageRef = Storage::aquirePage();
                        frame->higherInnerEndIndex = frame->elementsPerPage;
                        getPage<false>(frame->higherInnerPageRef)->count = frame->elementsPerPage;
                    }
                    frame->higherOuterPageRef = Storage::aquirePage();
                    Page* higherOuter = getPage<false>(frame->higherOuterPageRef);
                    Page::distributeCount(lowerOuter, higherOuter, data.elementCount-(frame->pageCount-1)*frame->elementsPerPage);
                    frame->higherOuterEndIndex = higherOuter->count;
                }
                frame->index = (isLeaf) ? 0 : 1;
                frame->endIndex = lowerOuter->count;
            }
        }
        data.elementCount = pageCount;
        --data.layer;
        return true;
    }

    template<typename FrameType>
    static LayerType insertPhase1(InsertData& data, Iterator<false, FrameType>& iter) {
        data.layer = iter.end-1;
        insertPhase1Layer<FrameType, true>(data, iter);
        while(insertPhase1Layer<FrameType, false>(data, iter));
        return data.layer+1;
    }

    template<bool isLeaf>
    static void insertPhase2Layer(InsertData& data, Iterator<false, InsertIteratorFrame>& iter) {
        InsertIteratorFrame* frame = iter.fromBegin(data.layer);
        assert(frame->higherOuterPageRef);
        Page *lowerOuter = getPage<false>(frame->pageRef),
             *lowerInner = getPage<false>(frame->lowerInnerPageRef),
             *higherInner = getPage<false>(frame->higherInnerPageRef),
             *higherOuter = getPage<false>(frame->higherOuterPageRef);
        Page::template insertOverflow<isLeaf>(frame, data.lowerInnerParent, data.higherOuterParent,
                lowerOuter, lowerInner, higherInner, higherOuter, data.lowerInnerParentIndex-1, data.higherOuterParentIndex-1);
        if(isLeaf)
            return;
        if(frame->index < frame->endIndex) {
            data.lowerInnerParent = lowerOuter;
            data.lowerInnerParentIndex = frame->index;
        } else if(frame->lowerInnerIndex > 0) {
            data.lowerInnerParent = lowerInner;
            data.lowerInnerParentIndex = frame->lowerInnerIndex;
        }
        if(frame->higherOuterEndIndex == 0) {
            assert(frame->lowerInnerIndex == 0);
            switch(frame->pageCount) {
                case 1:
                    data.higherOuterParent = lowerOuter;
                    data.higherOuterParentIndex = frame->endIndex-1;
                    break;
                case 2:
                    data.higherOuterParent = lowerInner;
                    data.higherOuterParentIndex = frame->elementsPerPage-1;
                    break;
                default:
                    data.higherOuterParent = higherInner;
                    data.higherOuterParentIndex = frame->higherInnerEndIndex-1;
                    break;
            }
        } else if(frame->higherOuterEndIndex > 1) {
            data.higherOuterParent = higherOuter;
            data.higherOuterParentIndex = frame->higherOuterEndIndex-1;
        }
    }

    void insertPhase2(InsertData& data, Iterator<false, InsertIteratorFrame>& iter) {
        InsertIteratorFrame* frame;
        while(true) {
            if(data.layer >= layerCount)
                return;
            frame = iter.fromBegin(data.layer);
            if(frame->higherOuterPageRef)
                break;
            ++data.layer;
        }
        frame = iter.fromBegin(--data.layer);
        assert(frame->endIndex > 1);
        assert(frame->index > 0 && frame->index+1 <= frame->endIndex);
        data.lowerInnerParent = data.higherOuterParent = getPage<false>(frame->pageRef);
        data.lowerInnerParentIndex = frame->index;
        data.higherOuterParentIndex = frame->endIndex-1;
        while(++data.layer < layerCount-1)
            insertPhase2Layer<false>(data, iter);
        if(data.layer < layerCount)
            insertPhase2Layer<true>(data, iter);
    }

    template<bool isLeaf>
    static Page* insertAdvance(InsertIteratorFrame* frame) {
        assert(frame->pageCount > 0);
        --frame->pageCount;
        if(frame->higherOuterPageRef) {
            if(frame->lowerInnerPageRef) {
                frame->pageRef = frame->lowerInnerPageRef;
                Page* page = getPage<false>(frame->pageRef);
                frame->index = frame->lowerInnerIndex;
                frame->endIndex = (frame->lowerInnerPageRef == frame->higherOuterPageRef) ? frame->higherOuterEndIndex : frame->elementsPerPage;
                frame->lowerInnerPageRef = 0;
                return page;
            } else if(frame->pageCount == 1) {
                frame->pageRef = frame->higherInnerPageRef;
                Page* page = getPage<false>(frame->pageRef);
                frame->index = 0;
                frame->endIndex = frame->higherInnerEndIndex;
                return page;
            } else if(frame->pageCount == 0) {
                frame->pageRef = frame->higherOuterPageRef;
                Page* page = getPage<false>(frame->pageRef);
                frame->index = 0;
                frame->endIndex = frame->higherOuterEndIndex;
                return page;
            }
        }
        frame->pageRef = Storage::aquirePage();
        frame->index = 0;
        frame->endIndex = frame->elementsPerPage;
        Page* page = getPage<false>(frame->pageRef);
        page->count = frame->endIndex;
        return page;
    }

    typedef Closure<void(Page*, IndexType, IndexType)> AquireData;
    void insert(Iterator<false>& at, NativeNaturalType elementCount, AquireData aquireData) {
        assert(elementCount > 0);
        InsertData data;
        data.startLayer = 0;
        data.elementCount = elementCount;
        LayerType addedLayers = insertPhase1<IteratorFrame>(data, at);
        addedLayers = (addedLayers < 0) ? -addedLayers : 0;
        layerCount += addedLayers;
        Iterator<false, InsertIteratorFrame> iter;
        iter.end = layerCount;
        iter.copy(at);
        data.startLayer = addedLayers;
        data.elementCount = elementCount;
        LayerType unmodifiedLayerCount = insertPhase1<InsertIteratorFrame>(data, iter);
        assert(addedLayers == 0 || unmodifiedLayerCount == 0);
        rootPageRef = iter.fromBegin(0)->pageRef;
        data.layer = max(addedLayers, unmodifiedLayerCount);
        insertPhase2(data, iter);
        InsertIteratorFrame *parentFrame, *frame = iter.fromBegin(layerCount-1);
        Page *parentPage, *page = getPage<false>(frame->pageRef);
        if(frame->index < frame->endIndex)
            aquireData(page, frame->index, frame->endIndex);
        while(frame->pageCount > 0) {
            page = insertAdvance<true>(frame);
            if(frame->index < frame->endIndex)
                aquireData(page, frame->index, frame->endIndex);
            data.layer = layerCount-1;
            bool setKey = true;
            PageRefType childPageRef = frame->pageRef;
            while(data.layer > unmodifiedLayerCount) {
                parentFrame = iter.fromBegin(--data.layer);
                if(parentFrame->index < parentFrame->endIndex) {
                    parentPage = getPage<false>(parentFrame->pageRef);
                    if(setKey)
                        Page::template copyKey<false, true>(parentPage, page, parentFrame->index-1, 0);
                    parentPage->setPageRef(parentFrame->index++, childPageRef);
                    break;
                }
                parentPage = insertAdvance<false>(parentFrame);
                if(parentFrame->index > 0) {
                    Page::template copyKey<false, true>(parentPage, page, parentFrame->index-1, 0);
                    setKey = false;
                }
                parentPage->setPageRef(parentFrame->index++, childPageRef);
                childPageRef = parentFrame->pageRef;
            }
        }
        for(data.layer = layerCount-2; data.layer > unmodifiedLayerCount; --data.layer) {
            frame = iter.fromBegin(data.layer);
            if(frame->pageCount == 0)
                continue;
            assert(frame->higherOuterPageRef && frame->higherOuterEndIndex == 0);
            insertAdvance<false>(frame);
            parentFrame = iter.fromBegin(data.layer-1);
            parentPage = getPage<false>(parentFrame->pageRef);
            parentPage->setPageRef(parentFrame->index++, frame->higherOuterPageRef);
        }
    }

    void insert(Iterator<false>& at, KeyType key, ValueType value) {
        insert(at, 1, [&](Page* page, IndexType index, IndexType endIndex) {
            page->template setKey<true>(index, key);
            page->setValue(index, value);
        });
    }

    struct EraseData {
        bool spareLowerInner, eraseHigherInner;
        LayerType layer;
        Iterator<false> &from, &to, iter;
    };

    template<int dir>
    static Page* eraseAdvance(EraseData& data, IndexType& parentIndex, Page*& parent) {
        data.iter.copy((dir == -1) ? data.from : data.to);
        if(data.iter.template advanceAtLayer<dir>(data.layer-1) == 0) {
            IteratorFrame* parentFrame = ((dir == -1) ? data.from : data.iter).getParentFrame(data.layer);
            parentIndex = parentFrame->index-1;
            parent = getPage<false>(parentFrame->pageRef);
            return getPage<false>(data.iter.fromBegin(data.layer)->pageRef);
        } else
            return nullptr;
    }

    template<bool isLeaf>
    void eraseEmptyLayer(EraseData& data, Page* lowerInner) {
        if(isLeaf) {
            if(lowerInner->count > 0)
                return;
            init();
        } else if(lowerInner->count == 1) {
            rootPageRef = lowerInner->getPageRef(0);
            layerCount = data.from.end-data.layer-1;
        } else if(lowerInner->count > 1)
            return;
        Storage::releasePage(data.from.fromBegin(data.layer)->pageRef);
        data.spareLowerInner = false;
        data.eraseHigherInner = true;
    }

    template<bool isLeaf>
    bool eraseLayer(EraseData& data) {
        IndexType lowerInnerIndex = data.from.fromBegin(data.layer)->index+data.spareLowerInner,
                  higherInnerIndex = data.to.fromBegin(data.layer)->index+data.eraseHigherInner;
        Page *lowerInner = getPage<false>(data.from.fromBegin(data.layer)->pageRef),
             *higherInner = getPage<false>(data.to.fromBegin(data.layer)->pageRef);
        data.spareLowerInner = true;
        data.eraseHigherInner = false;
        if(lowerInner == higherInner) {
            if(lowerInnerIndex >= higherInnerIndex)
                return false;
            Page::template erase1<isLeaf>(lowerInner, lowerInnerIndex, higherInnerIndex);
            if(data.layer == 0) {
                eraseEmptyLayer<isLeaf>(data, lowerInner);
                return false;
            }
        } else {
            IteratorFrame* parentFrame = data.to.getParentFrame(data.layer);
            IndexType higherInnerParentIndex = parentFrame->index-1;
            Page* higherInnerParent = getPage<false>(parentFrame->pageRef);
            if(Page::template erase2<isLeaf>(higherInnerParent, lowerInner, higherInner,
                                             higherInnerParentIndex, lowerInnerIndex, higherInnerIndex)) {
                Storage::releasePage(data.to.fromBegin(data.layer)->pageRef);
                data.eraseHigherInner = true;
            }
            data.iter.copy(data.to);
            while(data.iter.template advanceAtLayer<-1>(data.layer-1) == 0 &&
                  data.iter.fromBegin(data.layer)->pageRef != data.from.fromBegin(data.layer)->pageRef)
                Storage::releasePage(data.iter.fromBegin(data.layer)->pageRef);
        }
        if(lowerInner->count < Page::template capacity<isLeaf>()/2) {
            IndexType lowerInnerParentIndex, higherOuterParentIndex;
            Page *lowerInnerParent, *higherOuterParent,
                 *lowerOuter = eraseAdvance<-1>(data, lowerInnerParentIndex, lowerInnerParent),
                 *higherOuter = eraseAdvance<1>(data, higherOuterParentIndex, higherOuterParent);
            if(!lowerOuter && !higherOuter)
                eraseEmptyLayer<isLeaf>(data, lowerInner);
            else if(Page::template redistribute<isLeaf>(lowerInnerParent, higherOuterParent,
                                                        lowerOuter, lowerInner, higherOuter,
                                                        lowerInnerParentIndex, higherOuterParentIndex)) {
                Storage::releasePage(data.from.fromBegin(data.layer)->pageRef);
                data.spareLowerInner = false;
                data.eraseHigherInner = true;
            }
        }
        --data.layer;
        return true;
    }

    void erase(Iterator<false>& from, Iterator<false>& to) {
        assert(from.isValid() && to.isValid() && from.compare(to) < 1);
        EraseData data = {false, true, static_cast<LayerType>(to.end-1), from, to};
        data.iter.end = layerCount;
        if(eraseLayer<true>(data))
            while(eraseLayer<false>(data));
    }

    void erase(Iterator<false>& at) {
        assert(at.isValid());
        erase(at, at);
    }
};
