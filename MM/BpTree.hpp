#include "Storage.hpp"

template<class TemplateKeyType, class TemplateValueType>
class BpTree {
    public:
    typedef TemplateKeyType KeyType;
    typedef TemplateValueType ValueType;
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
        ArchitectureType pageCount;
    };

    class Page : public BasePage {
        public:
        static const ArchitectureType
            HeaderBits = sizeof(BasePage)*8+sizeof(IndexType)*8,
            BodyBits = bitsPerPage-HeaderBits,
            KeyBits = sizeof(KeyType)*8,
            PageRefBits = sizeof(PageRefType)*8,
            ValueBits = sizeof(ValueType)*8,
            BranchKeyCount = (BodyBits-PageRefBits)/(KeyBits+PageRefBits),
            LeafKeyCount = BodyBits/(KeyBits+ValueBits),
            KeysBitOffset = architecturePadding(HeaderBits),
            BranchPageRefsBitOffset = architecturePadding(KeysBitOffset+BranchKeyCount*KeyBits),
            LeafValuesBitOffset = architecturePadding(KeysBitOffset+LeafKeyCount*KeyBits);

        template<bool isLeaf>
        static ArchitectureType capacity() {
            return (isLeaf) ? LeafKeyCount : BranchKeyCount+1;
        }

        IndexType count;

        template<bool isLeaf>
        IndexType keyCount() const {
            return (isLeaf) ? count : count-1;
        }

        template<typename Type, ArchitectureType bitOffset>
        Type get(IndexType src) const {
            Type result;
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(&result),
                            reinterpret_cast<const ArchitectureType*>(this),
                            0, bitOffset+src*sizeof(Type)*8, sizeof(Type)*8);
            return result;
        }

        KeyType getKey(IndexType src) const {
            return get<KeyType, KeysBitOffset>(src);
        }

        PageRefType getPageRef(IndexType src) const {
            return get<PageRefType, BranchPageRefsBitOffset>(src);
        }

        ValueType getValue(IndexType src) const {
            return get<ValueType, LeafValuesBitOffset>(src);
        }

        template<typename Type, ArchitectureType bitOffset>
        void set(IndexType dst, Type content) {
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this),
                            reinterpret_cast<const ArchitectureType*>(&content),
                            bitOffset+dst*sizeof(Type)*8, 0, sizeof(Type)*8);
        }

        void setKey(IndexType dst, KeyType content) {
            return set<KeyType, KeysBitOffset>(dst, content);
        }

        void setPageRef(IndexType dst, PageRefType content) {
            return set<PageRefType, BranchPageRefsBitOffset>(dst, content);
        }

        void setValue(IndexType dst, ValueType content) {
            return set<ValueType, LeafValuesBitOffset>(dst, content);
        }

        template<bool isLeaf>
        void debugPrint(std::ostream& stream) const {
            for(IndexType i = 0; i < keyCount<isLeaf>(); ++i) {
                if(i > 0) stream << " ";
                stream << std::setfill('0') << std::setw(8) << getKey(i);
            }
            stream << std::endl;
            for(IndexType i = 0; i < count; ++i) {
                if(i > 0) stream << " ";
                if(isLeaf)
                    stream << std::setfill('0') << std::setw(8) << getValue(i);
                else
                    stream << std::setfill('0') << std::setw(4) << getPageRef(i);
            }
            stream << std::endl;
        }

        template<bool isLeaf>
        bool isValid() const {
            if(count <= !isLeaf || count > capacity<isLeaf>())
                return false;
            for(IndexType i = 1; i < keyCount<isLeaf>(); ++i)
                if(getKey(i-1) >= getKey(i))
                    return false;
            return true;
        }

        template<bool isLeaf>
        IndexType indexOfKey(KeyType key) const {
            IndexType begin = 0, mid, end = keyCount<isLeaf>();
            while(begin < end) {
                mid = (begin+end)/2;
                if((isLeaf && key > getKey(mid)) ||
                   (!isLeaf && key >= getKey(mid)))
                    begin = mid+1;
                else
                    end = mid;
            }
            return begin;
        }

        template<bool frontKey, int dir = -1>
        static void copyBranchElements(Page* dstPage, Page* srcPage,
                                       IndexType dstIndex, IndexType srcIndex,
                                       IndexType n) {
            if(n == 0)
                return;
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             BranchPageRefsBitOffset+dstIndex*PageRefBits,
                             BranchPageRefsBitOffset+srcIndex*PageRefBits,
                             n*PageRefBits);
            if(frontKey) {
                assert(dstIndex > 0 && srcIndex > 0);
                --dstIndex;
                --srcIndex;
            } else if(n > 1)
                --n;
            else
                return;
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             KeysBitOffset+dstIndex*KeyBits,
                             KeysBitOffset+srcIndex*KeyBits,
                             n*KeyBits);
        }

        template<int dir = -1>
        static void copyLeafElements(Page* dstPage, Page* srcPage,
                                     IndexType dstIndex, IndexType srcIndex,
                                     IndexType n) {
            if(n == 0)
                return;
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                             n*KeyBits);
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             LeafValuesBitOffset+dstIndex*ValueBits, LeafValuesBitOffset+srcIndex*ValueBits,
                             n*ValueBits);
        }

        static void copyKey(Page* dstPage, Page* srcPage,
                            IndexType dstIndex, IndexType srcIndex) {
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(dstPage),
                            reinterpret_cast<const ArchitectureType*>(srcPage),
                            KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                            KeyBits);
        }

        static void swapKeyInParent(Page* parent, Page* dstPage, Page* srcPage,
                                    IndexType parentIndex, IndexType dstIndex, IndexType srcIndex) {
            copyKey(dstPage, parent, dstIndex, parentIndex);
            copyKey(parent, srcPage, parentIndex, srcIndex);
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
        void init(IndexType n, InsertIteratorFrame* frame) {
            frame->endIndex = count = n;
        }

        template<bool isLeaf>
        static void insert(Page* lower, IndexType count, InsertIteratorFrame* frame) {
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
        static void insertOverflow(Page* lowerInnerParent, Page* higherOuterParent,
                                   Page* lowerOuter, Page* lowerInner, Page* higherInner, Page* higherOuter,
                                   IndexType lowerInnerParentIndex, IndexType higherOuterParentIndex,
                                   InsertIteratorFrame* frame) {
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
                    copyKey(lowerInnerParent, lowerInner, lowerInnerParentIndex, 0);
                else if(frame->higherOuterEndIndex == 0)
                    copyKey(higherOuterParent, higherOuter, higherOuterParentIndex, 0);
            } else {
                if(frame->lowerInnerIndex > 0) {
                    copyKey(lowerInnerParent, lowerOuter, lowerInnerParentIndex, lowerOuter->count-1);
                    copyBranchElements<false>(lowerInner, lowerOuter, 0, lowerOuter->count, frame->lowerInnerIndex);
                    copyBranchElements<true>(higherOuter, lowerOuter, frame->higherOuterEndIndex, frame->index+shiftHigherInner, shiftHigherOuter);
                } else if(frame->higherOuterEndIndex == 0) {
                    copyKey(higherOuterParent, lowerOuter, higherOuterParentIndex, frame->index+shiftHigherInner-1);
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
                    copyKey(lower, parent, startInLower-1, parentIndex);
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
                    copyKey(parent, higher, parentIndex, 0);
                } else {
                    if(count <= 0) {
                        count *= -1;
                        if(endInHigher == 0 && count > 0) {
                            copyBranchElements<false>(lower, higher, startInLower, 0, count);
                            swapKeyInParent(parent, lower, higher, parentIndex, startInLower-1, count-1);
                        } else {
                            copyBranchElements<true>(lower, higher, startInLower, endInHigher, count);
                            copyKey(parent, higher, parentIndex, endInHigher+count-1);
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
                            copyKey(parent, lower, parentIndex, lower->count-1);
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
                copyKey(lower, parent, lower->count-1, parentIndex);
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
                copyKey(higher, parent, lower->count-1, parentIndex);
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
                copyKey(parent, higher, parentIndex, 0);
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
                copyKey(parent, higher, parentIndex, 0);
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
                    copyKey(higherParent, higher, higherParentIndex, 0);
                } else {
                    copyBranchElements<false, +1>(higher, higher, count, 0, higher->count);
                    copyKey(higher, higherParent, count-1, higherParentIndex);
                    higher->count += count;
                    count -= middle->count;
                    lower->count -= count;
                    copyBranchElements<false>(higher, middle, count, 0, middle->count);
                    copyBranchElements<false>(higher, lower, 0, lower->count, count);
                    copyKey(higher, middleParent, count-1, middleParentIndex);
                    copyKey(higherParent, lower, higherParentIndex, lower->count-1);
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

    template<bool writeable>
    static Page* getPage(Storage* storage, PageRefType& pageRef) {
        return storage->template dereferencePage<Page>(pageRef);
    }

    template<bool writeable, typename FrameType = IteratorFrame>
    class Iterator {
        public:
        LayerType end;
        FrameType stackBottom;

        FrameType* fromBegin(LayerType layer) {
            return &stackBottom+layer;
        }

        FrameType* fromEnd(LayerType layer = 1) {
            return &stackBottom+end-layer;
        }

        FrameType* getParentFrame(LayerType layer) {
            while(layer > 0) {
                FrameType* frame = fromBegin(--layer);
                if(frame->index > 0)
                    return frame;
            }
            return NULL;
        }

        bool isValid() {
            for(LayerType layer = 0; layer < end; ++layer)
                if(fromEnd()->index >= fromEnd()->endIndex)
                    return false;
            return true;
        }

        template<bool srcWriteable>
        void copy(Iterator<srcWriteable>* src) {
            static_assert(!writeable || srcWriteable, "Can not copy from read only to writeable");
            LayerType offset = end-src->end;
            for(LayerType layer = 0; layer < src->end; ++layer)
                bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(fromBegin(layer+offset)),
                                reinterpret_cast<const ArchitectureType*>(src->fromBegin(layer)),
                                0, 0, sizeof(IteratorFrame)*8);
        }

        char compare(Iterator* other) {
            LayerType layer = 0;
            for(; fromBegin(layer)->index == other->fromBegin(layer)->index; ++layer)
                if(layer == end-1)
                    return 0;
            return (fromBegin(layer)->index < other->fromBegin(layer)->index) ? -1 : 1;
        }

        template<int dir = 1>
        bool advance(Storage* storage, LayerType atLayer, ArchitectureType steps = 1) {
            assert(storage);
            if(steps == 0 || end == 0 || atLayer < 0 || atLayer >= end)
                return steps;
            bool updateLower, keepRunning;
            do {
                LayerType layer = atLayer;
                FrameType* frame = fromBegin(layer);
                ArchitectureType stepsToTake;
                if(dir == 1) {
                    stepsToTake = std::min((ArchitectureType)(frame->endIndex-1-frame->index), steps);
                    frame->index += stepsToTake;
                } else {
                    stepsToTake = std::min((ArchitectureType)frame->index, steps);
                    frame->index -= stepsToTake;
                }
                steps -= stepsToTake;
                updateLower = (stepsToTake > 0);
                keepRunning = false;
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
                        updateLower = true;
                        keepRunning = true;
                        break;
                    }
                if(updateLower) {
                    LayerType endLayer = atLayer+1;
                    if(endLayer > end-1)
                        endLayer = end-1;
                    Page* parent = getPage<false>(storage, frame->pageRef);
                    while(layer < endLayer) {
                        IndexType parentIndex = frame->index;
                        frame = fromBegin(++layer);
                        frame->pageRef = parent->getPageRef(parentIndex);
                        Page* page = getPage<writeable>(storage, frame->pageRef);
                        // parent->setPageRef(parentIndex, frame->pageRef); // TODO: Update pageRef in getPage
                        frame->endIndex = page->count;
                        frame->index = (dir == 1) ? 0 : frame->endIndex-1;
                        parent = page;
                    }
                }
            } while(keepRunning);
            return steps;
        }

        void debugPrint(Storage* storage, std::ostream& stream) {
            stream << "Iterator " << static_cast<uint16_t>(end) << std::endl;
            for(LayerType layer = 0; layer < end; ++layer) {
                auto frame = fromBegin(layer);
                stream << "    " << layer << ": " << frame->pageRef << " (" << frame->index << "/" << frame->endIndex << ")" << std::endl;
                Page* page = getPage<false>(storage, frame->pageRef);
                if(layer == end-1)
                    page->template debugPrint<true>(std::cout);
                else
                    page->template debugPrint<false>(std::cout);
            }
        }
    };

    template<bool writeable, typename FrameType = IteratorFrame>
    inline Iterator<writeable, FrameType>* createIterator(LayerType size = 0) const {
        // TODO: Alloca this on stack not heap
        size += layerCount;
        auto iterator = reinterpret_cast<Iterator<writeable, FrameType>*>(malloc(sizeof(LayerType)*2+sizeof(FrameType)*size));
        iterator->end = size;
        return iterator;
    }

    template<bool writeable, bool border = false, bool lower = false>
	bool at(Storage* storage, Iterator<writeable>* iterator, KeyType key = 0) {
        assert(storage && iterator && iterator->end == layerCount);
		if(isEmpty())
            return false;
        LayerType layer = 0;
        PageRefType pageRef = rootPageRef;
	    while(true) {
	        auto frame = iterator->fromBegin(layer);
            frame->pageRef = pageRef;
            Page* page = getPage<writeable>(storage, frame->pageRef);
            // parent->setPageRef(parentIndex, frame->pageRef); // TODO: Update pageRef in getPage
            frame->endIndex = page->count;
	        if(++layer == iterator->end) {
                if(border)
                    frame->index = (lower) ? 0 : page->count;
                else
                    frame->index = page->template indexOfKey<true>(key);
                return border || (frame->index < page->count);
	        } else {
                if(border)
                    frame->index = (lower) ? 0 : page->count-1;
                else
                    frame->index = page->template indexOfKey<false>(key);
                pageRef = page->getPageRef(frame->index);
	        }
	    }
	}

    PageRefType rootPageRef;
    LayerType layerCount;

    void init() {
        rootPageRef = 0;
        layerCount = 0;
    }

    bool isEmpty() const {
        return (rootPageRef == 0);
    }

    struct InsertData {
        Storage* storage;
        LayerType startLayer, layer;
        ArchitectureType elementCount;
        IndexType lowerInnerParentIndex, higherOuterParentIndex;
        Page *lowerInnerParent, *higherOuterParent;
    };

    template<typename FrameType, bool isLeaf>
    static bool insertAuxLayer(InsertData& data, Iterator<false, FrameType>* iter) {
        bool apply = std::is_same<FrameType, InsertIteratorFrame>::value;
        Page* page;
        auto frame = reinterpret_cast<InsertIteratorFrame*>(iter->fromBegin(data.layer));
        if(data.layer >= data.startLayer) {
            if(!isLeaf)
                --data.elementCount;
            if(data.elementCount == 0)
                return false;
            page = getPage<false>(data.storage, frame->pageRef);
            data.elementCount += page->count;
        } else if(data.elementCount == 1)
            return false;
        ArchitectureType pageCount = (data.elementCount+Page::template capacity<isLeaf>()-1)/Page::template capacity<isLeaf>();
        // TODO: 1048576 element case, leads to 1048576-4112*255=16 elements on first page (less than half of the capacity)
        if(apply) {
            frame->elementsPerPage = (data.elementCount+pageCount-1)/pageCount;
            frame->pageCount = pageCount-1;
            if(data.layer >= data.startLayer) {
                if(frame->pageCount == 0) {
                    frame->higherOuterPageRef = 0;
                    Page::template insert<isLeaf>(page, data.elementCount, frame);
                } else {
                    frame->endIndex = data.elementCount-(pageCount-2)*frame->elementsPerPage;
                    frame->higherOuterPageRef = data.storage->aquirePage();
                    switch(frame->pageCount) {
                        case 1:
                            frame->lowerInnerPageRef = frame->higherOuterPageRef;
                            frame->higherInnerPageRef = frame->pageRef;
                        break;
                        case 2:
                            frame->lowerInnerPageRef = data.storage->aquirePage();
                            frame->higherInnerPageRef = frame->lowerInnerPageRef;
                        break;
                        default:
                            frame->lowerInnerPageRef = data.storage->aquirePage();
                            frame->higherInnerPageRef = data.storage->aquirePage();
                        break;
                    }
                }
            } else {
                frame->higherOuterPageRef = 0;
                frame->pageRef = data.storage->aquirePage();
                frame->index = (isLeaf) ? 0 : 1;
                page = getPage<false>(data.storage, frame->pageRef);
                page->template init<isLeaf>(data.elementCount-frame->pageCount*frame->elementsPerPage, frame);
                if(!isLeaf)
                    page->setPageRef(0, iter->fromBegin(data.layer+1)->pageRef);
            }
        }
        data.elementCount = pageCount;
        --data.layer;
        return true;
    }

    template<typename FrameType>
    static LayerType insertAux(InsertData& data, Iterator<false, FrameType>* iter, ArchitectureType elementCount) {
        data.layer = iter->end-1;
        data.elementCount = elementCount;
        insertAuxLayer<FrameType, true>(data, iter);
        while(insertAuxLayer<FrameType, false>(data, iter));
        return data.layer+1;
    }

    template<bool isLeaf>
    static void insertSplitLayer(InsertData& data, Iterator<false, InsertIteratorFrame>* iter) {
        InsertIteratorFrame* frame = iter->fromBegin(data.layer);
        if(frame->higherOuterPageRef) {
            InsertIteratorFrame* parentFrame = iter->fromBegin(data.layer-1);
            if(!parentFrame->higherOuterPageRef) {
                assert(parentFrame->endIndex > 1);
                assert(parentFrame->index > 0 && parentFrame->index+1 <= parentFrame->endIndex);
                data.lowerInnerParent = data.higherOuterParent = getPage<false>(data.storage, parentFrame->pageRef);
                data.lowerInnerParentIndex = parentFrame->index;
                data.higherOuterParentIndex = parentFrame->endIndex-1;
            }
            Page *lowerOuter = getPage<false>(data.storage, frame->pageRef),
                 *lowerInner = getPage<false>(data.storage, frame->lowerInnerPageRef),
                 *higherInner = getPage<false>(data.storage, frame->higherInnerPageRef),
                 *higherOuter = getPage<false>(data.storage, frame->higherOuterPageRef);
            Page::template insertOverflow<isLeaf>(data.lowerInnerParent, data.higherOuterParent,
                    lowerOuter, lowerInner, higherInner, higherOuter, data.lowerInnerParentIndex-1, data.higherOuterParentIndex-1, frame);
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
                    default: // TODO: Testing
                        printf("TTP\n");
                        data.higherOuterParent = higherInner;
                        data.higherOuterParentIndex = frame->higherInnerEndIndex-1;
                    break;
                }
            } else if(frame->higherOuterEndIndex > 1) {
                data.higherOuterParent = higherOuter;
                data.higherOuterParentIndex = frame->higherOuterEndIndex-1;
            }
        }
        ++data.layer;
    }

    template<bool isLeaf>
    static Page* insertAdvance(Storage* storage, InsertIteratorFrame* frame) {
        Page* page;
        if(frame->lowerInnerPageRef) {
            frame->pageRef = frame->lowerInnerPageRef;
            page = getPage<false>(storage, frame->pageRef);
            frame->index = frame->lowerInnerIndex;
            frame->endIndex = (frame->lowerInnerPageRef == frame->higherOuterPageRef) ? frame->higherOuterEndIndex : frame->elementsPerPage;
            frame->lowerInnerPageRef = 0;
        } else if(frame->higherOuterPageRef && frame->pageCount == 2) {
            frame->pageRef = frame->higherInnerPageRef;
            page = getPage<false>(storage, frame->pageRef);
            frame->index = 0;
            frame->endIndex = frame->higherInnerEndIndex;
        } else if(frame->higherOuterPageRef && frame->pageCount == 1) {
            frame->pageRef = frame->higherOuterPageRef;
            page = getPage<false>(storage, frame->pageRef);
            frame->index = 0;
            frame->endIndex = frame->higherOuterEndIndex;
        } else {
            frame->pageRef = storage->aquirePage();
            page = getPage<false>(storage, frame->pageRef);
            frame->index = 0;
            page->template init<isLeaf>(frame->elementsPerPage, frame);
        }
        assert(frame->pageCount > 0);
        --frame->pageCount;
        return page;
    }

    typedef std::function<void(Page*, IndexType, IndexType)> AquireData;
    void insert(Storage* storage, Iterator<false>* at, AquireData aquireData, ArchitectureType elementCount) {
        assert(storage && at && elementCount > 0);
        InsertData data = { storage, 0 };
        data.startLayer = insertAux<IteratorFrame>(data, at, elementCount);
        data.startLayer = (data.startLayer < 0) ? -data.startLayer : 0;
        auto iter = createIterator<false, InsertIteratorFrame>(data.startLayer );
        iter->copy(at);
        data.layer = data.startLayer = insertAux<InsertIteratorFrame>(data, iter, elementCount);
        while(data.layer < iter->end-1)
            insertSplitLayer<false>(data, iter);
        if(data.layer < iter->end)
            insertSplitLayer<true>(data, iter);
        layerCount = iter->end;
        rootPageRef = iter->fromBegin(0)->pageRef;
        InsertIteratorFrame *parentFrame, *frame = iter->fromBegin(iter->end-1);
        Page *parentPage, *page = getPage<false>(storage, frame->pageRef);
        if(frame->index < frame->endIndex)
            aquireData(page, frame->index, frame->endIndex);
        while(frame->pageCount > 0) {
            page = insertAdvance<true>(storage, frame);
            if(frame->index < frame->endIndex)
                aquireData(page, frame->index, frame->endIndex);
            data.layer = iter->end-1;
            bool setKey = true;
            PageRefType childPageRef = frame->pageRef;
            while(data.layer > data.startLayer) {
                parentFrame = iter->fromBegin(--data.layer);
                if(parentFrame->index < parentFrame->endIndex) {
                    parentPage = getPage<false>(storage, parentFrame->pageRef);
                    if(setKey)
                        Page::copyKey(parentPage, page, parentFrame->index-1, 0);
                    parentPage->setPageRef(parentFrame->index++, childPageRef);
                    break;
                } else {
                    parentPage = insertAdvance<false>(storage, parentFrame);
                    if(parentFrame->index > 0) {
                        Page::copyKey(parentPage, page, parentFrame->index-1, 0);
                        setKey = false;
                    }
                    parentPage->setPageRef(parentFrame->index++, childPageRef);
                    childPageRef = parentFrame->pageRef;
                }
            }
        }
        for(data.layer = iter->end-2; data.layer > data.startLayer; --data.layer) {
            frame = iter->fromBegin(data.layer);
            if(frame->pageCount == 0)
                continue;
            assert(frame->higherOuterPageRef && frame->higherOuterEndIndex == 0);
            insertAdvance<false>(storage, frame);
            parentFrame = iter->fromBegin(data.layer-1);
            parentPage = getPage<false>(storage, parentFrame->pageRef);
            parentPage->setPageRef(parentFrame->index++, frame->higherOuterPageRef);
        }
    }

    struct EraseData {
        Storage* storage;
        Iterator<false> *from, *to, *iter;
        bool spareLowerInner, eraseHigherInner;
        LayerType layer;
    };

    template<int dir>
    static Page* eraseAdvance(EraseData& data, IndexType& parentIndex, Page*& parent) {
        data.iter->copy((dir == -1) ? data.from : data.to);
        if(data.iter->template advance<dir>(data.storage, data.layer-1) == 0) {
            IteratorFrame* parentFrame = ((dir == -1) ? data.from : data.iter)->getParentFrame(data.layer);
            parentIndex = parentFrame->index-1;
            parent = getPage<false>(data.storage, parentFrame->pageRef);
            return getPage<false>(data.storage, data.iter->fromBegin(data.layer)->pageRef);
        } else
            return NULL;
    }

    template<bool isLeaf>
    void eraseEmptyLayer(EraseData& data, Page* lowerInner) {
        if(isLeaf) {
            if(lowerInner->count > 0)
                return;
            init();
        } else if(lowerInner->count == 1) {
            rootPageRef = lowerInner->getPageRef(0);
            layerCount = data.from->end-data.layer-1;
        } else if(lowerInner->count > 1)
            return;
        data.storage->releasePage(data.from->fromBegin(data.layer)->pageRef);
        data.spareLowerInner = false;
        data.eraseHigherInner = true;
    }

    template<bool isLeaf>
    bool eraseLayer(EraseData& data) {
        IndexType lowerInnerIndex = data.from->fromBegin(data.layer)->index+data.spareLowerInner,
                  higherInnerIndex = data.to->fromBegin(data.layer)->index+data.eraseHigherInner;
        Page *lowerInner = getPage<false>(data.storage, data.from->fromBegin(data.layer)->pageRef),
             *higherInner = getPage<false>(data.storage, data.to->fromBegin(data.layer)->pageRef);
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
            IteratorFrame* parentFrame = data.to->getParentFrame(data.layer);
            IndexType higherInnerParentIndex = parentFrame->index-1;
            Page* higherInnerParent = getPage<false>(data.storage, parentFrame->pageRef);
            if(Page::template erase2<isLeaf>(higherInnerParent, lowerInner, higherInner,
                                             higherInnerParentIndex, lowerInnerIndex, higherInnerIndex)) {
                data.storage->releasePage(data.to->fromBegin(data.layer)->pageRef);
                data.eraseHigherInner = true;
            }
            data.iter->copy(data.to);
            while(data.iter->template advance<-1>(data.storage, data.layer-1) == 0 &&
                  data.iter->fromBegin(data.layer)->pageRef != data.from->fromBegin(data.layer)->pageRef)
                data.storage->releasePage(data.iter->fromBegin(data.layer)->pageRef);
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
                data.storage->releasePage(data.from->fromBegin(data.layer)->pageRef);
                data.spareLowerInner = false;
                data.eraseHigherInner = true;
            }
        }
        --data.layer;
        return true;
    }

    void erase(Storage* storage, Iterator<false>* from, Iterator<false>* to) {
        assert(storage && from->isValid() && to->isValid() && from->compare(to) < 1);
        EraseData data;
        data.storage = storage;
        data.from = from;
        data.to = to;
        data.iter = createIterator<false>();
        data.spareLowerInner = false;
        data.eraseHigherInner = true;
        data.layer = to->end-1;
        if(eraseLayer<true>(data))
            while(eraseLayer<false>(data));
    }
};
