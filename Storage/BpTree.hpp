#include "Basics.hpp"

namespace Storage {

enum FindMode {
    First,
    Last,
    Key,
    Rank
};

template<typename KeyType, typename RankType, NativeNaturalType valueBits>
struct BpTree {
    typedef Natural32 OffsetType;
    typedef Integer8 LayerType;

    static const NativeNaturalType
        keyBits = sizeOfInBits<KeyType>::value,
        rankBits = sizeOfInBits<RankType>::value,
        pageRefBits = sizeOfInBits<PageRefType>::value;

    struct IteratorFrame {
        RankType rank;
        PageRefType pageRef;
        OffsetType index, endIndex;
    };

    struct InsertIteratorFrame : public IteratorFrame {
        PageRefType lowerInnerPageRef, higherInnerPageRef, higherOuterPageRef;
        OffsetType lowerInnerIndex, higherInnerEndIndex, higherOuterEndIndex, elementsPerPage;
        NativeNaturalType pageCount;
    };

    struct PageHeader : public BasePage {
        OffsetType count;
        LayerType layer;
    };

    struct Page {
        PageHeader header;

        static const NativeNaturalType
            headerBits = sizeOfInBits<PageHeader>::value,
            keyOffset = architecturePadding(headerBits),
            bodyBits = Storage::bitsPerPage-keyOffset,
            branchKeyCount = (bodyBits-rankBits-pageRefBits)/(keyBits+rankBits+pageRefBits),
            leafKeyCount = bodyBits/(keyBits+valueBits),
            rankOffset = keyOffset+keyBits*branchKeyCount,
            pageRefOffset = Storage::bitsPerPage-pageRefBits*(branchKeyCount+1),
            valueOffset = Storage::bitsPerPage-valueBits*leafKeyCount;

        template<bool isLeaf>
        static OffsetType capacity() {
            return (isLeaf) ? leafKeyCount : branchKeyCount+1;
        }

        template<bool isLeaf>
        OffsetType keyCount() const {
            return (isLeaf) ? header.count : header.count-1;
        }

        template<typename Type, NativeNaturalType offset>
        Type get(OffsetType src) const {
            Type result;
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(&result),
                                     reinterpret_cast<const NativeNaturalType*>(this),
                                     0, offset+src*sizeOfInBits<Type>::value, sizeOfInBits<Type>::value);
            return result;
        }

        template<bool isLeaf>
        KeyType getKey(OffsetType src) const {
            return get<KeyType, keyOffset>(src);
        }

        RankType getRank(OffsetType src) const {
            return get<RankType, rankOffset>(src);
        }

        PageRefType getPageRef(OffsetType src) const {
            return get<PageRefType, pageRefOffset>(src);
        }

        template<typename Type, NativeNaturalType offset>
        void set(OffsetType dst, Type content) {
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(this),
                                     reinterpret_cast<const NativeNaturalType*>(&content),
                                     offset+dst*sizeOfInBits<Type>::value, 0, sizeOfInBits<Type>::value);
        }

        template<bool isLeaf>
        void setKey(OffsetType dst, KeyType content) {
            set<KeyType, keyOffset>(dst, content);
        }

        void setRank(OffsetType dst, RankType content) {
            set<RankType, rankOffset>(dst, content);
        }

        void setPageRef(OffsetType dst, PageRefType content) {
            set<PageRefType, pageRefOffset>(dst, content);
        }

        template<bool isLeaf>
        bool isValid() const {
            if(header.count <= !isLeaf || header.count > capacity<isLeaf>())
                return false;
            for(OffsetType i = 1; i < keyCount<isLeaf>(); ++i)
                if(getKey<isLeaf>(i-1) >= getKey<isLeaf>(i))
                    return false;
            return true;
        }

        template<bool frontKey, NativeIntegerType dir = -1>
        static void copyBranchElements(Page* dstPage, Page* srcPage,
                                       OffsetType dstIndex, OffsetType srcIndex,
                                       OffsetType n) {
            if(n == 0)
                return;
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      pageRefOffset+dstIndex*pageRefBits,
                                      pageRefOffset+srcIndex*pageRefBits,
                                      n*pageRefBits);
            if(frontKey) {
                assert(dstIndex > 0 && srcIndex > 0);
                --dstIndex;
                --srcIndex;
            } else if(n > 1)
                --n;
            else
                return;
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      keyOffset+dstIndex*keyBits,
                                      keyOffset+srcIndex*keyBits,
                                      n*keyBits);
        }

        template<NativeIntegerType dir = -1>
        static void copyLeafElements(Page* dstPage, Page* srcPage,
                                     OffsetType dstIndex, OffsetType srcIndex,
                                     OffsetType n) {
            if(n == 0)
                return;
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      keyOffset+dstIndex*keyBits,
                                      keyOffset+srcIndex*keyBits,
                                      n*keyBits);
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      valueOffset+dstIndex*valueBits,
                                      valueOffset+srcIndex*valueBits,
                                      n*valueBits);
        }

        template<bool isLeaf>
        void copy(Page* dstPage, Page* srcPage) {
            assert(dstPage != srcPage);
            dstPage->header.count = srcPage->header.count;
            if(isLeaf)
                copyLeafElements(dstPage, srcPage, 0, 0, srcPage->header.count);
            else
                copyBranchElements<false>(dstPage, srcPage, 0, 0, srcPage->header.count);
        }

        template<bool dstIsLeaf, bool srcIsLeaf>
        static void copyKey(Page* dstPage, Page* srcPage,
                            OffsetType dstIndex, OffsetType srcIndex) {
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                     reinterpret_cast<const NativeNaturalType*>(srcPage),
                                     keyOffset+dstIndex*keyBits, keyOffset+srcIndex*keyBits,
                                     keyBits);
        }

        static void swapKeyInParent(Page* parent, Page* dstPage, Page* srcPage,
                                    OffsetType parentIndex, OffsetType dstIndex, OffsetType srcIndex) {
            copyKey<false, false>(dstPage, parent, dstIndex, parentIndex);
            copyKey<false, false>(parent, srcPage, parentIndex, srcIndex);
        }

        static void shiftCounts(Page* dstPage, Page* srcPage, OffsetType n) {
            dstPage->header.count += n;
            srcPage->header.count -= n;
        }

        static void distributeCount(Page* lower, Page* higher, NativeIntegerType n) {
            lower->header.count = (n+1)/2;
            higher->header.count = n/2;
        }

        template<bool isLeaf>
        static void insert(InsertIteratorFrame* frame, Page* lower, OffsetType count) {
            if(!isLeaf)
                ++frame->index;
            assert(count > lower->header.count && count <= capacity<isLeaf>() && frame->index <= lower->header.count);
            frame->endIndex = frame->index+count-lower->header.count;
            if(isLeaf)
                copyLeafElements<1>(lower, lower, frame->endIndex, frame->index, lower->header.count-frame->index);
            else {
                assert(frame->index > 0);
                copyBranchElements<true, 1>(lower, lower, frame->endIndex, frame->index, lower->header.count-frame->index);
            }
            lower->header.count = count;
        }

        template<bool isLeaf>
        static void insertOverflow(InsertIteratorFrame* frame,
                                   Page* lowerInnerParent, Page* higherOuterParent,
                                   Page* lowerOuter, Page* lowerInner, Page* higherInner, Page* higherOuter,
                                   OffsetType lowerInnerParentIndex, OffsetType higherOuterParentIndex) {
            if(!isLeaf)
                ++frame->index;
            bool insertKeyInParentNow;
            NativeIntegerType shiftHigherInner = frame->index+frame->endIndex-lowerOuter->header.count,
                              shiftHigherOuter = lowerOuter->header.count-frame->index, higherOuterEndIndex;
            assert(shiftHigherInner > 0 && frame->endIndex <= capacity<isLeaf>()*2 && frame->index <= lowerOuter->header.count);
            lowerInner->header.count = higherInner->header.count = frame->elementsPerPage;
            distributeCount(lowerOuter, higherOuter, frame->endIndex);
            if(shiftHigherInner < lowerOuter->header.count) {
                shiftHigherInner = lowerOuter->header.count-shiftHigherInner;
                assert(shiftHigherInner < higherInner->header.count);
                frame->lowerInnerIndex = 0;
                shiftHigherOuter -= shiftHigherInner;
            } else {
                shiftHigherInner = 0;
                frame->lowerInnerIndex = (frame->index > lowerOuter->header.count) ? frame->index-lowerOuter->header.count : 0;
            }
            frame->endIndex = lowerOuter->header.count;
            frame->higherInnerEndIndex = higherInner->header.count-shiftHigherInner;
            frame->higherOuterEndIndex = higherOuter->header.count-shiftHigherOuter;
            if(lowerOuter == higherInner)
                frame->endIndex = frame->higherInnerEndIndex;
            if(isLeaf) {
                copyLeafElements(lowerInner, lowerOuter, 0, lowerOuter->header.count, frame->lowerInnerIndex);
                copyLeafElements(higherOuter, lowerOuter, frame->higherOuterEndIndex, frame->index+shiftHigherInner, shiftHigherOuter);
                copyLeafElements<1>(higherInner, lowerOuter, frame->higherInnerEndIndex, frame->index, shiftHigherInner);
                if(frame->lowerInnerIndex > 0)
                    copyKey<false, true>(lowerInnerParent, lowerInner, lowerInnerParentIndex, 0);
                else if(frame->higherOuterEndIndex == 0)
                    copyKey<false, true>(higherOuterParent, higherOuter, higherOuterParentIndex, 0);
            } else {
                if(frame->lowerInnerIndex > 0) {
                    copyKey<false, false>(lowerInnerParent, lowerOuter, lowerInnerParentIndex, lowerOuter->header.count-1);
                    copyBranchElements<false>(lowerInner, lowerOuter, 0, lowerOuter->header.count, frame->lowerInnerIndex);
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
        static void erase1(Page* lower, OffsetType start, OffsetType end) {
            assert(start < end && end <= lower->header.count);
            if(isLeaf)
                copyLeafElements<-1>(lower, lower, start, end, lower->header.count-end);
            else if(start > 0)
                copyBranchElements<true, -1>(lower, lower, start, end, lower->header.count-end);
            else if(end < lower->header.count)
                copyBranchElements<false, -1>(lower, lower, 0, end, lower->header.count-end);
            lower->header.count -= end-start;
        }

        template<bool isLeaf>
        static bool erase2(Page* parent, Page* lower, Page* higher,
                           OffsetType parentIndex, OffsetType startInLower, OffsetType endInHigher) {
            assert(startInLower <= lower->header.count && endInHigher <= higher->header.count);
            NativeIntegerType count = startInLower+higher->header.count-endInHigher;
            if(count <= capacity<isLeaf>()) {
                lower->header.count = count;
                if(count == 0)
                    return true;
                if(isLeaf)
                    copyLeafElements(lower, higher, startInLower, endInHigher, higher->header.count-endInHigher);
                else if(startInLower == 0)
                    copyBranchElements<false, -1>(lower, higher, 0, endInHigher, lower->header.count);
                else if(endInHigher == 0) {
                    copyKey<true, false>(lower, parent, startInLower-1, parentIndex);
                    copyBranchElements<false, -1>(lower, higher, startInLower, 0, higher->header.count);
                } else
                    copyBranchElements<true, -1>(lower, higher, startInLower, endInHigher, higher->header.count-endInHigher);
                return true;
            } else {
                assert(startInLower > 0);
                distributeCount(lower, higher, count);
                count = startInLower-lower->header.count;
                if(isLeaf) {
                    if(count <= 0) {
                        count *= -1;
                        copyLeafElements(lower, higher, startInLower, endInHigher, count);
                        copyLeafElements<-1>(higher, higher, 0, endInHigher+count, higher->header.count);
                    } else {
                        if(count < endInHigher)
                            copyLeafElements<-1>(higher, higher, count, endInHigher, higher->header.count);
                        else
                            copyLeafElements<1>(higher, higher, count, endInHigher, higher->header.count);
                        copyLeafElements(higher, lower, 0, lower->header.count, count);
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
                        copyBranchElements<false, -1>(higher, higher, 0, endInHigher+count, higher->header.count);
                    } else {
                        if(endInHigher == 0) {
                            copyBranchElements<false, 1>(higher, higher, count, 0, higher->header.count);
                            swapKeyInParent(parent, higher, lower, parentIndex, count-1, lower->header.count-1);
                        } else {
                            if(count < endInHigher)
                                copyBranchElements<true, -1>(higher, higher, count, endInHigher, higher->header.count);
                            else
                                copyBranchElements<true, 1>(higher, higher, count, endInHigher, higher->header.count);
                            copyKey<false, false>(parent, lower, parentIndex, lower->header.count-1);
                        }
                        copyBranchElements<false>(higher, lower, 0, lower->header.count, count);
                    }
                }
                return false;
            }
        }

        template<bool isLeaf>
        static void evacuateDown(Page* parent, Page* lower, Page* higher, OffsetType parentIndex) {
            assert(lower->header.count+higher->header.count <= capacity<isLeaf>());
            if(isLeaf)
                copyLeafElements(lower, higher, lower->header.count, 0, higher->header.count);
            else {
                copyKey<false, false>(lower, parent, lower->header.count-1, parentIndex);
                copyBranchElements<false>(lower, higher, lower->header.count, 0, higher->header.count);
            }
            lower->header.count += higher->header.count;
        }

        template<bool isLeaf>
        static void evacuateUp(Page* parent, Page* lower, Page* higher, OffsetType parentIndex) {
            assert(lower->header.count+higher->header.count <= capacity<isLeaf>());
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, lower->header.count, 0, higher->header.count);
                copyLeafElements(higher, lower, 0, 0, lower->header.count);
            } else {
                copyBranchElements<false, +1>(higher, higher, lower->header.count, 0, higher->header.count);
                copyKey<false, false>(higher, parent, lower->header.count-1, parentIndex);
                copyBranchElements<false>(higher, lower, 0, 0, lower->header.count);
            }
            higher->header.count += lower->header.count;
        }

        template<bool isLeaf>
        static void shiftDown(Page* parent, Page* lower, Page* higher, OffsetType parentIndex, OffsetType count) {
            assert(count > 0 && lower->header.count+count <= capacity<isLeaf>());
            if(isLeaf) {
                copyLeafElements(lower, higher, lower->header.count, 0, count);
                shiftCounts(lower, higher, count);
                copyLeafElements<-1>(higher, higher, 0, count, higher->header.count);
                copyKey<false, true>(parent, higher, parentIndex, 0);
            } else {
                swapKeyInParent(parent, lower, higher, parentIndex, lower->header.count-1, count-1);
                copyBranchElements<false>(lower, higher, lower->header.count, 0, count);
                shiftCounts(lower, higher, count);
                copyBranchElements<false, -1>(higher, higher, 0, count, higher->header.count);
            }
        }

        template<bool isLeaf>
        static void shiftUp(Page* parent, Page* lower, Page* higher, OffsetType parentIndex, OffsetType count) {
            assert(count > 0 && higher->header.count+count <= capacity<isLeaf>());
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, count, 0, higher->header.count);
                shiftCounts(higher, lower, count);
                copyLeafElements(higher, lower, 0, lower->header.count, count);
                copyKey<false, true>(parent, higher, parentIndex, 0);
            } else {
                copyBranchElements<false, +1>(higher, higher, count, 0, higher->header.count);
                shiftCounts(higher, lower, count);
                copyBranchElements<false>(higher, lower, 0, lower->header.count, count);
                swapKeyInParent(parent, higher, lower, parentIndex, count-1, lower->header.count-1);
            }
        }

        template<bool isLeaf, bool lowerIsMiddle>
        static bool redistribute2(Page* parent, Page* lower, Page* higher, OffsetType parentIndex) {
            NativeIntegerType count = lower->header.count+higher->header.count;
            if(count <= capacity<isLeaf>()) {
                if(lowerIsMiddle)
                    evacuateUp<isLeaf>(parent, lower, higher, parentIndex);
                else
                    evacuateDown<isLeaf>(parent, lower, higher, parentIndex);
                return true;
            } else {
                count = ((lowerIsMiddle)?higher:lower)->header.count-count/2;
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
                                  OffsetType middleParentIndex, OffsetType higherParentIndex) {
            NativeIntegerType count = lower->header.count+middle->header.count+higher->header.count;
            if(count <= capacity<isLeaf>()*2) {
                count = count/2-higher->header.count;
                if(count < 0) {
                    count *= -1;
                    evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
                    shiftDown<isLeaf>(higherParent, lower, higher, higherParentIndex, count);
                } else if(count <= middle->header.count) {
                    if(count > 0)
                        shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, count);
                    evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
                } else if(isLeaf) {
                    copyLeafElements<+1>(higher, higher, count, 0, higher->header.count);
                    higher->header.count += count;
                    count -= middle->header.count;
                    lower->header.count -= count;
                    copyLeafElements(higher, middle, count, 0, middle->header.count);
                    copyLeafElements(higher, lower, 0, lower->header.count, count);
                    copyKey<false, true>(higherParent, higher, higherParentIndex, 0);
                } else {
                    copyBranchElements<false, +1>(higher, higher, count, 0, higher->header.count);
                    copyKey<false, false>(higher, higherParent, count-1, higherParentIndex);
                    higher->header.count += count;
                    count -= middle->header.count;
                    lower->header.count -= count;
                    copyBranchElements<false>(higher, middle, count, 0, middle->header.count);
                    copyBranchElements<false>(higher, lower, 0, lower->header.count, count);
                    copyKey<false, false>(higher, middleParent, count-1, middleParentIndex);
                    copyKey<false, false>(higherParent, lower, higherParentIndex, lower->header.count-1);
                }
                count = lower->header.count+higher->header.count;
                assert(lower->header.count == (count+1)/2);
                assert(higher->header.count == count/2);
                return true;
            } else {
                count = count/3;
                NativeIntegerType shiftLower = lower->header.count-count, shiftUpper = higher->header.count-count;
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
                                 OffsetType middleParentIndex, OffsetType higherParentIndex) {
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
    static const LayerType maxLayerCount = 9;

    template<bool enableCopyOnWrite = false>
    static Page* getPage(PageRefType& pageRef) {
        return Storage::dereferencePage<Page>(pageRef);
    }

    template<bool enableCopyOnWrite, typename FrameType = IteratorFrame>
    struct Iterator {
        LayerType end;
        FrameType stack[maxLayerCount];

        FrameType* operator[](LayerType layer) {
            return &stack[layer];
        }

        FrameType* getParentFrame(LayerType layer) {
            while(++layer < end) {
                FrameType* frame = (*this)[layer];
                if(frame->index > 0)
                    return frame;
            }
            return nullptr;
        }

        bool isValid() {
            for(LayerType layer = 0; layer < end; ++layer)
                if((*this)[layer]->index >= (*this)[layer]->endIndex)
                    return false;
            return true;
        }

        template<bool srcEnableCopyOnWrite>
        typename enableIf<!enableCopyOnWrite || srcEnableCopyOnWrite, void>::type copy(Iterator<srcEnableCopyOnWrite>& src) {
            end = src.end;
            for(LayerType layer = 0; layer < src.end; ++layer)
                Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>((*this)[layer]),
                                         reinterpret_cast<const NativeNaturalType*>(src[layer]),
                                         0, 0, sizeOfInBits<IteratorFrame>::value);
        }

        char compare(Iterator& other) {
            LayerType layer = end-1;
            for(; (*this)[layer]->index == other[layer]->index; --layer)
                if(layer < 0)
                    return 0;
            return ((*this)[layer]->index < other[layer]->index) ? -1 : 1;
        }

        void descend(FrameType*& frame, Page*& page, LayerType& layer) {
            auto parentFrame = frame;
            auto parentPage = page;
            frame = (*this)[--layer];
            if(rankBits) {
                frame->rank = parentFrame->rank;
                if(parentFrame->index)
                    frame->rank += page->getRank(parentFrame->index-1);
            }
            frame->pageRef = parentPage->getPageRef(parentFrame->index);
            page = getPage<enableCopyOnWrite>(frame->pageRef);
            // parentPage->setPageRef(parentFrame->index, frame->pageRef); // TODO: Update pageRef in getPage
        }

        template<NativeIntegerType dir = 1>
        NativeNaturalType advance(LayerType atLayer = 0, NativeNaturalType steps = 1, Closure<void(Page*)> pageTouchCallback = nullptr) {
            if(steps == 0 || end == 0 || atLayer < 0 || atLayer >= end)
                return steps;
            bool keepRunning;
            do {
                LayerType layer = atLayer;
                FrameType* frame = (*this)[layer];
                NativeNaturalType stepsTaken;
                if(dir == 1) {
                    stepsTaken = min((NativeNaturalType)(frame->endIndex-1-frame->index), steps);
                    frame->index += stepsTaken;
                } else {
                    stepsTaken = min((NativeNaturalType)frame->index, steps);
                    frame->index -= stepsTaken;
                }
                steps -= stepsTaken;
                bool keepRunning = false;
                if(steps > 0)
                    while(++layer < end) {
                        frame = (*this)[layer];
                        if((dir == -1 && frame->index > 0) ||
                           (dir == 1 && frame->index+1 < frame->endIndex)) {
                            frame->index += dir;
                            --steps;
                            keepRunning = true;
                            break;
                        }
                    }
                if(keepRunning || stepsTaken > 0) {
                    Page* page = getPage(frame->pageRef);
                    LayerType endLayer = atLayer-1;
                    if(endLayer < 0)
                        endLayer = 0;
                    while(layer > endLayer) {
                        descend(frame, page, layer);
                        if(pageTouchCallback)
                            pageTouchCallback(page);
                        frame->endIndex = page->header.count;
                        frame->index = (dir == 1) ? 0 : frame->endIndex-1;
                    }
                }
            } while(keepRunning);
            return steps;
        }

        KeyType getRank() {
            FrameType* frame = (*this)[0];
            return frame->rank+frame->index;
        }

        KeyType getKey() {
            FrameType* frame = (*this)[0];
            return getPage(frame->pageRef)->template getKey<true>(frame->index);
        }

        void setKey(KeyType key) {
            static_assert(enableCopyOnWrite);
            FrameType* frame = (*this)[0];
            getPage(frame->pageRef)->template setKey<true>(frame->index, key);
        }

        void debugPrint() {
            printf("Iterator %hhd\n", end);
            for(LayerType layer = 0; layer < end; ++layer) {
                FrameType* frame = (*this)[layer];
                /*if(rankBits > 0)
                    printf("%llu %llu %d/%d\n", frame->pageRef, frame->rank, frame->index, frame->endIndex);
                else*/
                    printf("%llu %d/%d\n", frame->pageRef, frame->index, frame->endIndex);
            }
        }
    };

    template<FindMode mode, bool enableCopyOnWrite>
	bool find(Iterator<enableCopyOnWrite>& iter,
              typename conditional<mode == Rank, RankType, KeyType>::type keyOrRank = 0,
              Closure<void(Page*)> pageTouchCallback = nullptr) {
		if(empty()) {
            iter.end = 0;
            return false;
        }
        Page* page = getPage<enableCopyOnWrite>(rootPageRef);
        LayerType layer = page->header.layer;
        iter.end = layer+1;
        auto frame = iter[layer];
        frame->rank = 0;
        frame->pageRef = rootPageRef;
	    while(true) {
            if(pageTouchCallback)
                pageTouchCallback(page);
            frame->endIndex = page->header.count;
	        if(layer == 0) {
                switch(mode) {
                    case First:
                        frame->index = 0;
                        return true;
                    case Last:
                        frame->index = frame->endIndex-1;
                        return true;
                    case Key:
                        frame->index = binarySearch<OffsetType>(page->template keyCount<true>(), [&](OffsetType at) {
                            return keyOrRank > page->template getKey<true>(at);
                        });
                        return frame->index < page->header.count && page->template getKey<true>(frame->index) == keyOrRank;
                    case Rank:
                        frame->index = keyOrRank-frame->rank;
                        return frame->index < page->header.count;
                }
	        } else {
                switch(mode) {
                    case First:
                        frame->index = 0;
                        break;
                    case Last:
                        frame->index = frame->endIndex-1;
                        break;
                    case Key:
                        frame->index = binarySearch<OffsetType>(page->template keyCount<false>(), [&](OffsetType at) {
                            return keyOrRank >= page->template getKey<false>(at);
                        });
                        break;
                    case Rank:
                        frame->index = binarySearch<OffsetType>(page->template keyCount<false>(), [&](OffsetType at) {
                            return keyOrRank-frame->rank >= page->getRank(at);
                        });
                        break;
                }
                iter.descend(frame, page, layer);
	        }
	    }
	}

    void iterate(Closure<void(Iterator<false>&)> callback) {
        if(empty())
            return;
        Iterator<false> iter;
        find<false, true, false>(iter);
        do {
            callback(iter);
        } while(iter.advance() == 0);
    }

    void generateStats(struct Stats& stats, Closure<void(Iterator<false>&)> callback = nullptr) {
        if(empty())
            return;
        Iterator<false> iter;
        NativeNaturalType branchPageCount = 0, leafPageCount = 0;
        Closure<void(Page*)> pageTouch = [&](Page* page) {
            if(page->header.layer == 0) {
                ++leafPageCount;
                stats.inhabitedPayload += (keyBits+valueBits)*page->header.count;
                stats.elementCount += iter[0]->endIndex;
                if(callback)
                    for(; iter[0]->index < iter[0]->endIndex; ++iter[0]->index)
                        callback(iter);
            } else {
                ++branchPageCount;
                stats.inhabitedMetaData += (keyBits+rankBits+pageRefBits)*page->header.count+rankBits+pageRefBits;
            }
        };
        find<First>(iter, 0, pageTouch);
        while(iter.template advance<1>(1, 1, pageTouch) == 0);
        NativeNaturalType uninhabitable = Page::valueOffset-Page::headerBits-keyBits*Page::leafKeyCount;
        stats.uninhabitable += uninhabitable*leafPageCount;
        stats.totalPayload += (Storage::bitsPerPage-uninhabitable)*leafPageCount;
        uninhabitable = Page::keyOffset-Page::headerBits+Page::pageRefOffset-Page::rankOffset-rankBits*(Page::branchKeyCount+1);
        stats.uninhabitable += uninhabitable*branchPageCount;
        stats.totalMetaData += (Storage::bitsPerPage-uninhabitable)*branchPageCount;
    }

    void init() {
        rootPageRef = 0;
    }

    bool empty() const {
        return (rootPageRef == 0);
    }

    struct InsertData {
        LayerType layer;
        NativeNaturalType elementCount;
        OffsetType lowerInnerParentIndex, higherOuterParentIndex;
        Page *lowerInnerParent, *higherOuterParent;
    };

    template<bool isLeaf>
    static bool insertPhase1Layer(InsertData& data, Iterator<true, InsertIteratorFrame>& iter) {
        Page* lowerOuter;
        auto frame = reinterpret_cast<InsertIteratorFrame*>(iter[data.layer]);
        if(data.layer < iter.end) {
            if(!isLeaf)
                --data.elementCount;
            if(data.elementCount == 0)
                return false;
            lowerOuter = getPage(frame->pageRef);
            data.elementCount += lowerOuter->header.count;
        } else if(!isLeaf && data.elementCount == 1)
            return false;
        NativeNaturalType pageCount = (data.elementCount+Page::template capacity<isLeaf>()-1)/Page::template capacity<isLeaf>();
        frame->elementsPerPage = (data.elementCount+pageCount-1)/pageCount;
        frame->pageCount = pageCount-1;
        if(data.layer < iter.end) {
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
            lowerOuter = getPage(frame->pageRef);
            lowerOuter->header.layer = data.layer;
            if(!isLeaf)
                lowerOuter->setPageRef(0, iter[data.layer-1]->pageRef);
            if(frame->pageCount == 0) {
                lowerOuter->header.count = data.elementCount-frame->pageCount*frame->elementsPerPage;
                frame->higherOuterPageRef = 0;
            } else {
                frame->lowerInnerPageRef = 0;
                if(frame->pageCount <= 1)
                    frame->higherInnerPageRef = 0;
                else {
                    frame->higherInnerPageRef = Storage::aquirePage();
                    frame->higherInnerEndIndex = frame->elementsPerPage;
                    Page* higherInner = getPage(frame->higherInnerPageRef);
                    higherInner->header.count = frame->elementsPerPage;
                    higherInner->header.layer = data.layer;
                }
                frame->higherOuterPageRef = Storage::aquirePage();
                Page* higherOuter = getPage(frame->higherOuterPageRef);
                higherOuter->header.layer = data.layer;
                Page::distributeCount(lowerOuter, higherOuter, data.elementCount-(frame->pageCount-1)*frame->elementsPerPage);
                frame->higherOuterEndIndex = higherOuter->header.count;
            }
            frame->index = (isLeaf) ? 0 : 1;
            frame->endIndex = lowerOuter->header.count;
        }
        data.elementCount = pageCount;
        ++data.layer;
        return true;
    }

    static LayerType insertPhase1(InsertData& data, Iterator<true, InsertIteratorFrame>& iter) {
        data.layer = 0;
        insertPhase1Layer<true>(data, iter);
        while(insertPhase1Layer<false>(data, iter));
        return data.layer;
    }

    template<bool isLeaf>
    static void insertPhase2Layer(InsertData& data, Iterator<true, InsertIteratorFrame>& iter) {
        InsertIteratorFrame* frame = iter[data.layer];
        assert(frame->higherOuterPageRef);
        Page *lowerOuter = getPage(frame->pageRef),
             *lowerInner = getPage(frame->lowerInnerPageRef),
             *higherInner = getPage(frame->higherInnerPageRef),
             *higherOuter = getPage(frame->higherOuterPageRef);
        lowerInner->header.layer = data.layer;
        higherInner->header.layer = data.layer;
        higherOuter->header.layer = data.layer;
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

    void insertPhase2(InsertData& data, Iterator<true, InsertIteratorFrame>& iter) {
        InsertIteratorFrame* frame;
        while(true) {
            if(data.layer < 0)
                return;
            frame = iter[data.layer];
            if(frame->higherOuterPageRef)
                break;
            --data.layer;
        }
        frame = iter[++data.layer];
        assert(data.layer >= 0);
        assert(frame->endIndex > 1);
        assert(frame->index > 0 && frame->index < frame->endIndex);
        data.lowerInnerParent = data.higherOuterParent = getPage(frame->pageRef);
        data.lowerInnerParentIndex = frame->index;
        data.higherOuterParentIndex = frame->endIndex-1;
        while(--data.layer > 0)
            insertPhase2Layer<false>(data, iter);
        insertPhase2Layer<true>(data, iter);
    }

    template<bool isLeaf>
    Page* insertAdvance(InsertData& data, InsertIteratorFrame* frame) {
        assert(frame->pageCount > 0);
        --frame->pageCount;
        if(frame->higherOuterPageRef) {
            if(frame->lowerInnerPageRef) {
                frame->pageRef = frame->lowerInnerPageRef;
                Page* page = getPage(frame->pageRef);
                frame->index = frame->lowerInnerIndex;
                frame->endIndex = (frame->lowerInnerPageRef == frame->higherOuterPageRef) ? frame->higherOuterEndIndex : frame->elementsPerPage;
                frame->lowerInnerPageRef = 0;
                return page;
            } else if(frame->pageCount == 1) {
                frame->pageRef = frame->higherInnerPageRef;
                Page* page = getPage(frame->pageRef);
                frame->index = 0;
                frame->endIndex = frame->higherInnerEndIndex;
                return page;
            } else if(frame->pageCount == 0) {
                frame->pageRef = frame->higherOuterPageRef;
                Page* page = getPage(frame->pageRef);
                frame->index = 0;
                frame->endIndex = frame->higherOuterEndIndex;
                return page;
            }
        }
        frame->pageRef = Storage::aquirePage();
        frame->index = 0;
        frame->endIndex = frame->elementsPerPage;
        Page* page = getPage(frame->pageRef);
        page->header.count = frame->endIndex;
        page->header.layer = data.layer;
        return page;
    }

    typedef Closure<void(Page*, OffsetType, OffsetType)> AquireData;
    void insert(Iterator<true>& origIter, NativeNaturalType n, AquireData aquireData) {
        assert(n > 0);
        InsertData data;
        data.elementCount = n;
        Iterator<true, InsertIteratorFrame> iter;
        iter.copy(origIter);
        LayerType unmodifiedLayer = insertPhase1(data, iter);
        data.layer = min(origIter.end, unmodifiedLayer)-1;
        iter.end = max(origIter.end, unmodifiedLayer);
        rootPageRef = iter[iter.end-1]->pageRef;
        insertPhase2(data, iter);
        InsertIteratorFrame *parentFrame, *frame = iter[0];
        Page *parentPage, *page = getPage(frame->pageRef);
        if(frame->index < frame->endIndex)
            aquireData(page, frame->index, frame->endIndex);
        while(frame->pageCount > 0) {
            page = insertAdvance<true>(data, frame);
            if(frame->index < frame->endIndex)
                aquireData(page, frame->index, frame->endIndex);
            data.layer = 0;
            bool setKey = true;
            PageRefType childPageRef = frame->pageRef;
            while(data.layer < unmodifiedLayer) {
                parentFrame = iter[++data.layer];
                if(parentFrame->index < parentFrame->endIndex) {
                    parentPage = getPage(parentFrame->pageRef);
                    if(setKey)
                        Page::template copyKey<false, true>(parentPage, page, parentFrame->index-1, 0);
                    parentPage->setPageRef(parentFrame->index++, childPageRef);
                    break;
                }
                parentPage = insertAdvance<false>(data, parentFrame);
                if(parentFrame->index > 0) {
                    Page::template copyKey<false, true>(parentPage, page, parentFrame->index-1, 0);
                    setKey = false;
                }
                parentPage->setPageRef(parentFrame->index++, childPageRef);
                childPageRef = parentFrame->pageRef;
            }
        }
        for(data.layer = 1; data.layer < unmodifiedLayer; ++data.layer) {
            frame = iter[data.layer];
            if(frame->pageCount == 0)
                continue;
            assert(frame->higherOuterPageRef && frame->higherOuterEndIndex == 0);
            insertAdvance<false>(data, frame);
            parentFrame = iter[data.layer+1];
            parentPage = getPage(parentFrame->pageRef);
            parentPage->setPageRef(parentFrame->index++, frame->higherOuterPageRef);
        }
    }

    struct EraseData {
        bool spareLowerInner, eraseHigherInner;
        LayerType layer;
        Iterator<true> &from, &to, iter;
    };

    template<NativeIntegerType dir>
    static Page* eraseAdvance(EraseData& data, OffsetType& parentIndex, Page*& parent) {
        data.iter.copy((dir == -1) ? data.from : data.to);
        if(data.iter.template advance<dir>(data.layer+1) == 0) {
            IteratorFrame* parentFrame = ((dir == -1) ? data.from : data.iter).getParentFrame(data.layer);
            parentIndex = parentFrame->index-1;
            parent = getPage(parentFrame->pageRef);
            return getPage(data.iter[data.layer]->pageRef);
        } else
            return nullptr;
    }

    template<bool isLeaf>
    void eraseEmptyLayer(EraseData& data, Page* lowerInner) {
        if(isLeaf) {
            if(lowerInner->header.count > 0)
                return;
            init();
        } else if(lowerInner->header.count == 1) {
            rootPageRef = lowerInner->getPageRef(0);
        } else if(lowerInner->header.count > 1)
            return;
        Storage::releasePage(data.from[data.layer]->pageRef);
        data.spareLowerInner = false;
        data.eraseHigherInner = true;
    }

    template<bool isLeaf>
    bool eraseLayer(EraseData& data) {
        OffsetType lowerInnerIndex = data.from[data.layer]->index+data.spareLowerInner,
                   higherInnerIndex = data.to[data.layer]->index+data.eraseHigherInner;
        Page *lowerInner = getPage(data.from[data.layer]->pageRef),
             *higherInner = getPage(data.to[data.layer]->pageRef);
        data.spareLowerInner = true;
        data.eraseHigherInner = false;
        if(lowerInner == higherInner) {
            if(lowerInnerIndex >= higherInnerIndex)
                return false;
            Page::template erase1<isLeaf>(lowerInner, lowerInnerIndex, higherInnerIndex);
            if(data.layer == data.from.end-1) {
                eraseEmptyLayer<isLeaf>(data, lowerInner);
                return false;
            }
        } else {
            IteratorFrame* parentFrame = data.to.getParentFrame(data.layer);
            OffsetType higherInnerParentIndex = parentFrame->index-1;
            Page* higherInnerParent = getPage(parentFrame->pageRef);
            if(Page::template erase2<isLeaf>(higherInnerParent, lowerInner, higherInner,
                                             higherInnerParentIndex, lowerInnerIndex, higherInnerIndex)) {
                Storage::releasePage(data.to[data.layer]->pageRef);
                data.eraseHigherInner = true;
            }
            data.iter.copy(data.to);
            while(data.iter.template advance<-1>(data.layer+1) == 0 &&
                  data.iter[data.layer]->pageRef != data.from[data.layer]->pageRef)
                Storage::releasePage(data.iter[data.layer]->pageRef);
        }
        if(lowerInner->header.count < Page::template capacity<isLeaf>()/2) {
            OffsetType lowerInnerParentIndex, higherOuterParentIndex;
            Page *lowerInnerParent, *higherOuterParent,
                 *lowerOuter = eraseAdvance<-1>(data, lowerInnerParentIndex, lowerInnerParent),
                 *higherOuter = eraseAdvance<1>(data, higherOuterParentIndex, higherOuterParent);
            if(!lowerOuter && !higherOuter)
                eraseEmptyLayer<isLeaf>(data, lowerInner);
            else if(Page::template redistribute<isLeaf>(lowerInnerParent, higherOuterParent,
                                                        lowerOuter, lowerInner, higherOuter,
                                                        lowerInnerParentIndex, higherOuterParentIndex)) {
                Storage::releasePage(data.from[data.layer]->pageRef);
                data.spareLowerInner = false;
                data.eraseHigherInner = true;
            }
        }
        ++data.layer;
        return true;
    }

    void erase(Iterator<true>& from, Iterator<true>& to) {
        assert(!empty() && from.isValid() && to.isValid() && from.compare(to) < 1);
        EraseData data = {false, true, static_cast<LayerType>(0), from, to};
        if(eraseLayer<true>(data))
            while(eraseLayer<false>(data));
    }

    void erase(Iterator<true>& iter) {
        assert(iter.isValid());
        erase(iter, iter);
    }

    template<FindMode mode>
    bool erase(typename conditional<mode == Rank, RankType, KeyType>::type keyOrRank = 0) {
        Iterator<true> iter;
        if(!find<mode>(iter, keyOrRank))
            return false;
        erase(iter);
        return true;
    }
};

};
