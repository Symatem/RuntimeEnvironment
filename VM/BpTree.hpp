#include "Page.hpp"

template<class TemplateKeyType, class TemplateValueType>
class BpTree {
    public:
    typedef uint16_t IndexType;
    typedef int8_t LayerType;
    typedef TemplateKeyType KeyType;
    typedef TemplateValueType ValueType;

    LayerType layerCount;
    ReferenceType rootReference;

    void init() {
        rootReference = 0;
        layerCount = 0;
    }

    bool isEmpty() const {
        return (rootReference == 0);
    }

    class Page : public BasePage {
        public:
        static const ArchitectureType
            HeaderBits = sizeof(BasePage)*8+sizeof(IndexType)*8,
            BodyBits = bitsPerPage-HeaderBits,
            KeyBits = sizeof(KeyType)*8,
            ReferenceBits = sizeof(ReferenceType)*8,
            ValueBits = sizeof(ValueType)*8,
            BranchCapacity = (BodyBits-ReferenceBits)/(KeyBits+ReferenceBits),
            LeafCapacity = BodyBits/(KeyBits+ValueBits),
            KeysBitOffset = architecturePadding(HeaderBits),
            BranchReferencesBitOffset = architecturePadding(KeysBitOffset+BranchCapacity*KeyBits),
            LeafValuesBitOffset = architecturePadding(KeysBitOffset+LeafCapacity*KeyBits);

        template<bool isLeaf>
        static ArchitectureType capacity() {
            return (isLeaf) ? LeafCapacity : BranchCapacity;
        }

        IndexType count;

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

        ReferenceType getReference(IndexType src) const {
            return get<ReferenceType, BranchReferencesBitOffset>(src);
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

        void setKey(KeyType content, IndexType src) {
            return set<KeyType, KeysBitOffset>(src, content);
        }

        void setReference(ReferenceType content, IndexType src) {
            return set<ReferenceType, BranchReferencesBitOffset>(src, content);
        }

        void setValue(ValueType content, IndexType src) {
            return set<ValueType, LeafValuesBitOffset>(src, content);
        }

        template<bool isLeaf>
        void debugPrint(std::ostream& stream) const {
            stream << count << std::endl;
            for(IndexType i = 0; i < count; ++i) {
                if(i > 0) stream << " ";
                stream << getKey(i);
            }
            stream << std::endl;
            for(IndexType i = 0; i < count; ++i) {
                if(i > 0) stream << " ";
                if(isLeaf)
                    stream << getValue(i);
                else
                    stream << getReference(i);
            }
            if(!isLeaf)
                stream << " " << getReference(count);
            stream << std::endl;
        }

        template<bool isLeaf>
        IndexType indexOfKey(KeyType key) const {
            IndexType begin = 0, mid, end = count;
            while(begin < end) {
                mid = (begin+end)/2;
                KeyType keyAtMid = getKey(mid);
                //if(key == keyAtMid)
                //    return mid-isLeaf+1;
                if((isLeaf && key > keyAtMid) ||
                   (!isLeaf && key >= keyAtMid))
                    begin = mid+1;
                else
                    end = mid;
            }
            return begin;
        }

        template<bool additionalReference, int dir = -1>
        static void copyBranchElements(Page* dstPage, Page* srcPage,
                                       IndexType dstIndex, IndexType srcIndex,
                                       IndexType n) {
            if(n == 0 && !additionalReference) return;
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             BranchReferencesBitOffset+dstIndex*ReferenceBits,
                             BranchReferencesBitOffset+srcIndex*ReferenceBits,
                             (n+(additionalReference?1:0))*ReferenceBits);
            if(additionalReference) {
                assert(dstIndex <= dstPage->count+1);
                if(n == 0) return;
            } else {
                assert(dstIndex <= dstPage->count && dstIndex > 0 && srcIndex > 0 && n > 0);
                --dstIndex;
                --srcIndex;
            }
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
            if(n == 0) return;
            assert(dstIndex <= dstPage->count);
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

        template<bool isLeaf>
        void init(IndexType n) {
            count = (isLeaf) ? n : n-1;
        }

        template<bool isLeaf>
        static void insert1(Page* lower, IndexType dst, IndexType n) {
            assert(n > 0 && lower->count+n <= capacity<isLeaf>());
            if(isLeaf) {
                assert(dst <= lower->count);
                copyLeafElements<1>(lower, lower, dst+n, dst, lower->count-dst);
            } else {
                assert(dst > 0 && dst <= lower->count+1);
                copyBranchElements<false, 1>(lower, lower, dst+n, dst, lower->count-dst+1);
            }
            lower->count += n;
        }

        template<bool isLeaf>
        static IndexType insert2(Page* parent, Page* lower, Page* higher,
                                 IndexType parentIndex, IndexType dst, IndexType n) {
            IndexType lowerEnd, insertLower, insertHigher, shiftA = dst+n, shiftB = 0, shiftC = lower->count-dst;
            int count = lower->count+n;
            if(isLeaf)
                assert(dst <= lower->count);
            else {
                assert(dst > 0 && dst <= lower->count+1);
                --count;
            }
            assert(n > 0 && count <= capacity<isLeaf>()*2);
            lower->count = (count+1)/2;
            higher->count = count/2;
            lowerEnd = lower->count;
            if(!isLeaf) {
                ++lowerEnd;
                ++shiftC;
            }
            if(shiftA < lowerEnd) {
                shiftA = lowerEnd-shiftA;
                shiftB = 0;
                shiftC -= shiftA;
            } else {
                shiftA = 0;
                shiftB = (dst > lowerEnd) ? dst-lowerEnd : 0;
            }
            insertLower = (dst < lowerEnd) ? lowerEnd-dst-shiftA : 0;
            insertHigher = n-insertLower;
            if(isLeaf) {
                copyLeafElements(higher, lower, 0, lower->count, shiftB);
                copyLeafElements(higher, lower, insertHigher, dst, shiftC);
                copyLeafElements<1>(lower, lower, dst+insertLower, dst, shiftA);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                if(shiftB > 0) {
                    copyKey(parent, lower, parentIndex, lower->count);
                    copyBranchElements<true>(higher, lower, 0, lower->count, shiftB-1);
                }
                if(insertHigher == 0) {
                    copyKey(parent, lower, parentIndex, lower->count-insertLower);
                    copyBranchElements<true>(higher, lower, 0, dst, shiftC-1);
                } else
                    copyBranchElements<false>(higher, lower, insertHigher, dst, shiftC);
                copyBranchElements<false, 1>(lower, lower, dst+insertLower, dst, shiftA);
            }
            return insertLower;
        }

        static void eraseAux(Page* dstPage, Page* srcPage,
                             IndexType dstIndex, IndexType srcIndex,
                             IndexType n) {
            if(dstIndex == 0)
                copyBranchElements<true, -1>(dstPage, srcPage, dstIndex, srcIndex, n);
            else
                copyBranchElements<false, -1>(dstPage, srcPage, dstIndex, srcIndex, n);
        }

        template<bool isLeaf>
        static void erase1(Page* lower, IndexType start, IndexType end) {
            if(isLeaf) {
                assert(start < end && end <= lower->count);
                copyLeafElements<-1>(lower, lower, start, end, lower->count-end);
            } else {
                assert(start < end && end <= lower->count+1);
                eraseAux(lower, lower, start, end, lower->count-end+1);
            }
            lower->count -= (start == 0) ? end-1 : end-start;
        }

        template<bool isLeaf>
        static bool erase2(Page* parent, Page* lower, Page* higher,
                           IndexType parentIndex, IndexType startInLower, IndexType endInHigher) {
            int count = startInLower+higher->count-endInHigher;
            if(isLeaf)
                assert(startInLower < lower->count && endInHigher > 0 && endInHigher <= higher->count);
            else {
                assert(startInLower <= lower->count && endInHigher <= higher->count+1);
                if(startInLower == 0) ++count;
            }
            if(count <= capacity<isLeaf>()) {
                if(isLeaf)
                    copyLeafElements(lower, higher, startInLower, endInHigher, higher->count-endInHigher);
                else
                    eraseAux(lower, higher, startInLower, endInHigher, higher->count-endInHigher+1);
                lower->count = count;
                return true;
            } else {
                assert(startInLower > 0);
                if(!isLeaf) --count;
                lower->count = (count+1)/2;
                higher->count = count/2;
                count = startInLower-lower->count;
                if(isLeaf) {
                    if(count <= 0) {
                        count *= -1;
                        copyLeafElements(lower, higher, startInLower, endInHigher, count);
                        copyLeafElements<-1>(higher, higher, 0, endInHigher+count, higher->count);
                    } else {
                        copyLeafElements(higher, lower, 0, lower->count, count);
                        copyLeafElements<-1>(higher, higher, count, endInHigher, higher->count);
                    }
                    copyKey(parent, higher, parentIndex, 0);
                } else {
                    --count;
                    if(count <= 0) {
                        count *= -1;
                        copyKey(parent, higher, parentIndex, endInHigher+count-1);
                        copyBranchElements<false>(lower, higher, startInLower, endInHigher, count);
                        copyBranchElements<true, -1>(higher, higher, 0, endInHigher+count, higher->count);
                    } else {
                        copyKey(parent, lower, parentIndex, lower->count);
                        copyBranchElements<true>(higher, lower, 0, lower->count+1, count-1);
                        copyBranchElements<false, -1>(higher, higher, count, endInHigher, higher->count);
                    }
                }
                return false;
            }
        }

        template<bool isLeaf>
        static void evacuateDown(Page* parent, Page* lower, Page* higher, IndexType parentIndex) {
            if(isLeaf) {
                copyLeafElements(lower, higher, lower->count, 0, higher->count);
                lower->count += higher->count;
            } else {
                copyBranchElements<true>(lower, higher, lower->count+1, 0, higher->count);
                copyKey(lower, parent, lower->count, parentIndex);
                lower->count += higher->count+1;
            }
        }

        template<bool isLeaf>
        static void evacuateUp(Page* parent, Page* lower, Page* higher, IndexType parentIndex) {
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, lower->count, 0, higher->count);
                copyLeafElements(higher, lower, 0, 0, lower->count);
                higher->count += lower->count;
            } else {
                copyBranchElements<true, +1>(higher, higher, lower->count+1, 0, higher->count);
                copyBranchElements<true>(higher, lower, 0, 0, lower->count);
                copyKey(higher, parent, lower->count, parentIndex);
                higher->count += lower->count+1;
            }
        }

        template<bool isLeaf>
        static void shiftDown(Page* parent, Page* lower, Page* higher, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                copyLeafElements(lower, higher, lower->count, 0, count);
                shiftCounts(lower, higher, count);
                copyLeafElements<-1>(higher, higher, 0, count, higher->count);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                swapKeyInParent(parent, lower, higher, parentIndex, lower->count, count-1);
                copyBranchElements<true>(lower, higher, lower->count+1, 0, count-1);
                shiftCounts(lower, higher, count);
                copyBranchElements<true, -1>(higher, higher, 0, count, higher->count);
            }
        }

        template<bool isLeaf>
        static void shiftUp(Page* parent, Page* lower, Page* higher, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, count, 0, higher->count);
                shiftCounts(higher, lower, count);
                copyLeafElements(higher, lower, 0, lower->count, count);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                copyBranchElements<true, +1>(higher, higher, count, 0, higher->count);
                shiftCounts(higher, lower, count);
                copyBranchElements<true>(higher, lower, 0, lower->count+1, count-1);
                swapKeyInParent(parent, higher, lower, parentIndex, count-1, lower->count);
            }
        }

        template<bool isLeaf, bool focus>
        static bool redistribute2(Page* parent, Page* lower, Page* higher, IndexType parentIndex) {
            int count = lower->count+higher->count;
            if(count <= capacity<isLeaf>()) {
                if(focus)
                    evacuateUp<isLeaf>(parent, lower, higher, parentIndex);
                else
                    evacuateDown<isLeaf>(parent, lower, higher, parentIndex);
                return true;
            } else {
                count = ((focus)?higher:lower)->count-count/2;
                if(focus)
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
            int count = lower->count+middle->count+higher->count;
            if(count <= capacity<isLeaf>()*2) {
                count = count/2-higher->count;
                if(count > 0)
                    shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, count);
                evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
                return true;
            } else {
                count = count/3;
                int shiftLower = lower->count-count, shiftUpper = higher->count-count;
                if(shiftLower < 0) {
                    if(shiftUpper < 0)
                        shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, -shiftUpper);
                    else
                        shiftDown<isLeaf>(higherParent, middle, higher, higherParentIndex, shiftUpper);
                    shiftDown<isLeaf>(middleParent, lower, middle, middleParentIndex, -shiftLower);
                } else {
                    shiftUp<isLeaf>(middleParent, lower, middle, middleParentIndex, shiftLower);
                    if(shiftUpper < 0)
                        shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, -shiftUpper);
                    else
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
                if(higher)
                    return redistribute3<isLeaf>(middleParent, higherParent,
                                                 lower, middle, higher,
                                                 middleParentIndex, higherParentIndex);
                else
                    return redistribute2<isLeaf, true>(middleParent, lower, middle, middleParentIndex);
            } else
                return redistribute2<isLeaf, false>(higherParent, middle, higher, higherParentIndex);
        }
    };

    struct IteratorFrame {
        ReferenceType reference;
        IndexType index, maxIndex;
    };

    struct InsertIteratorFrame : public IteratorFrame {
        ReferenceType auxReference;
        IndexType auxIndex;
        ArchitectureType elementCount, elementsPerPage;
    };

    template<bool readOnly, typename FrameType = IteratorFrame>
    class Iterator {
        public:
        LayerType start, end;
        FrameType stackBottom;

        FrameType* fromBegin(LayerType layer) {
            return &stackBottom+layer;
        }

        FrameType* fromEnd(LayerType layer = 1) {
            return &stackBottom+end-layer;
        }

        static Page* getPage(Storage* storage, ReferenceType reference) {
            return storage->template dereferencePage<Page>(reference);
        }

        bool isValid() {
            return start < end && fromEnd()->index <= fromEnd()->maxIndex;
        }

        template<bool srcReadOnly>
        void copy(Iterator<srcReadOnly>* src) {
            static_assert(readOnly || !srcReadOnly, "Can not copy from read only to writeable");
            assert(end-start == src->end-src->start);
            if(start == end) return;
            for(LayerType layer = start; layer < end; ++layer)
                bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(fromBegin(layer)),
                                reinterpret_cast<const ArchitectureType*>(src->fromBegin(layer)),
                                0, 0, sizeof(IteratorFrame)*8);
        }

        // Compares two iterators (-1 : smaller, 0 equal, 1 greater)
        char compare(Iterator* other) {
            LayerType layer = start;
            for(; fromBegin(layer)->index == other->fromBegin(layer)->index; ++layer)
                if(layer == end-1)
                    return 0;
            return (fromBegin(layer)->index < other->fromBegin(layer)->index) ? -1 : 1;
        }

        template<int dir = 1>
        bool advance(Storage* storage, LayerType atLayer, ArchitectureType steps = 1) {
            assert(storage != 0);
            if(start >= end || atLayer >= end) return steps;
            while(steps > 0) {
                LayerType stopLayer, layer = atLayer;
                FrameType* frame = fromBegin(layer);
                ArchitectureType stepsToTake;
                if(dir == 1) {
                    stepsToTake = std::min((ArchitectureType)(frame->maxIndex-frame->index), steps);
                    frame->index += stepsToTake;
                } else {
                    stepsToTake = std::min((ArchitectureType)frame->index, steps);
                    frame->index -= stepsToTake;
                }
                steps -= stepsToTake;
                if(steps > 0) {
                    stopLayer = atLayer;
                    while(layer > start) {
                        frame = fromBegin(--layer);
                        if(dir == 1) {
                            if(frame->index < frame->maxIndex)
                                ++frame->index;
                            else continue;
                        } else {
                            if(frame->index > 0)
                                --frame->index;
                            else continue;
                        }
                        --steps;
                        stopLayer = end;
                        break;
                    }
                } else
                    stopLayer = end;
                while(layer+1 < stopLayer) {
                    Page* page = getPage(storage, frame->reference);
                    ReferenceType reference = page->getReference(frame->index);
                    page = getPage(storage, reference);
                    frame = fromBegin(++layer);
                    frame->reference = reference;
                    frame->maxIndex = (layer+1 == end) ? page->count-1 : page->count;
                    frame->index = (dir == 1) ? 0 : frame->maxIndex;
                }
                if(stopLayer == atLayer) break;
            }
            return steps;
        }

        void debugPrint(std::ostream& stream) {
            stream << (end-start) << " (" << static_cast<uint16_t>(start) << "/" << static_cast<uint16_t>(end) << ")" << std::endl;
            for(LayerType layer = start; layer < end; ++layer) {
                auto frame = fromBegin(layer);
                stream << "    " << layer << ": " << frame->reference << " (" << frame->index << "/" << frame->maxIndex << ")" << std::endl;
            }
        }
    };

    template<bool readOnly, typename FrameType = IteratorFrame>
    Iterator<readOnly, FrameType>* createIterator(LayerType reserve = 0) const {
        LayerType size = layerCount+reserve;
        auto iterator = reinterpret_cast<Iterator<readOnly, FrameType>*>(malloc(sizeof(LayerType)*2+sizeof(FrameType)*size));
        iterator->start = reserve;
        iterator->end = size;
        return iterator;
    }

    template<bool readOnly, bool border = false, bool lower = false>
	bool at(Storage* storage, Iterator<readOnly>* iterator, KeyType key = 0) {
		if(isEmpty()) return false;
        LayerType layer = iterator->start;
        ReferenceType reference = rootReference; // TODO: Update reference in getPage
	    while(true) {
            Page* page = Iterator<readOnly>::getPage(storage, reference);
	        auto frame = iterator->fromBegin(layer);
            frame->reference = reference;
	        if(++layer == iterator->end) {
                frame->maxIndex = page->count-1;
                if(border)
                    frame->index = (lower) ? 0 : frame->maxIndex;
                else
                    frame->index = page->template indexOfKey<true>(key);
	            return border || (frame->index < page->count);
	        } else {
                frame->maxIndex = page->count;
                if(border)
                    frame->index = (lower) ? 0 : frame->maxIndex;
                else
                    frame->index = page->template indexOfKey<false>(key);
                reference = page->getReference(frame->index);
	        }
	    }
	}

    template<typename FrameType, bool isLeaf>
    bool insertAuxLayer(Storage* storage, Iterator<false, FrameType>* iter, LayerType& layer, ArchitectureType& elementCount) {
        bool apply = std::is_same<FrameType, InsertIteratorFrame>::value;
        Page *lower, *higher;
        auto frame = reinterpret_cast<InsertIteratorFrame*>(iter->fromBegin(layer));
        if(layer >= iter->start) {
            if(elementCount == 0) return false;
            lower = Iterator<false>::getPage(storage, frame->reference);
            elementCount += lower->count;
            if(!isLeaf) ++elementCount;
        } else if(elementCount == 1) return false;
        ArchitectureType pageCount = (isLeaf)
            ? (elementCount+Page::LeafCapacity-1)/Page::LeafCapacity
            : (elementCount+Page::BranchCapacity)/(Page::BranchCapacity+1);
        // TODO: 1048576 element case, leads to 1048576-4112*255=16 elements on first page (less than half of the capacity)
        if(apply) {
            frame->elementsPerPage = (elementCount+pageCount-1)/pageCount;
            frame->elementCount = elementCount;
            elementCount -= (pageCount-1)*frame->elementsPerPage;
            frame->elementCount -= elementCount;
        }
        if(layer >= iter->start) {
            if(apply) {
                if(pageCount == 1) {
                    frame->auxReference = 0;
                    Page::template insert1<isLeaf>(lower, frame->index, elementCount);
                } else {
                    auto parentFrame = iter->fromBegin(layer-1);
                    Page* parent = Iterator<false>::getPage(storage, parentFrame->reference);
                    frame->auxReference = storage->aquirePage();
                    higher = Iterator<false>::getPage(storage, frame->auxReference);
                    elementCount = Page::template insert2<isLeaf>(parent, lower, higher, parentFrame->index, frame->index, elementCount);
                }
            }
            --pageCount;
        } else if(apply) {
            frame->reference = storage->aquirePage();
            lower = Iterator<false>::getPage(storage, frame->reference);
            lower->template init<isLeaf>(elementCount);
            if(!isLeaf)
                lower->setReference(iter->fromBegin(layer+1)->reference, 0);
            frame->auxReference = 0;
            frame->index = (isLeaf) ? 0 : 1;
        }
        if(apply) {
            frame->maxIndex = lower->count;
            if(isLeaf) --frame->maxIndex;
        }
        elementCount = pageCount;
        --layer;
        return true;
    }

    template<typename FrameType>
    LayerType insertAux(Storage* storage, Iterator<false, FrameType>* iter, ArchitectureType elementCount) {
        LayerType layer = iter->end-1;
        insertAuxLayer<FrameType, true>(storage, iter, layer, elementCount);
        while(insertAuxLayer<FrameType, false>(storage, iter, layer, elementCount));
        return layer+1;
    }

    template<bool isLeaf>
    Page* insertAdvanceLayer(Storage* storage, InsertIteratorFrame* frame) {
        Page* page;
        if(frame->auxReference && frame->elementCount <= frame->elementsPerPage) {
            frame->reference = frame->auxReference;
            frame->maxIndex = frame->elementCount-1;
            page = Iterator<false>::getPage(storage, frame->reference);
        } else {
            frame->reference = storage->aquirePage();
            frame->maxIndex = frame->elementsPerPage-1;
            page = Iterator<false>::getPage(storage, frame->reference);
            page->template init<isLeaf>(frame->elementsPerPage);
        }
        frame->index = 0;
        frame->elementCount -= frame->maxIndex+1;
        return page;
    }

    typedef std::function<void(Page*, IndexType, IndexType)> AquireData;
    void insert(Storage* storage, Iterator<false>* at, AquireData aquireData, ArchitectureType insertCount) {
        // TODO: Insert behind "at" in leaf
        assert(insertCount > 0);
        auto iter = createIterator<false, InsertIteratorFrame>(at->start-insertAux<IteratorFrame>(storage, at, insertCount));
        iter->copy(at);
        iter->start = insertAux<InsertIteratorFrame>(storage, iter, insertCount);
        assert(iter->start == 0);
        layerCount = iter->end; //-iter->start;
        rootReference = iter->fromBegin(0)->reference;
        LayerType layer = iter->end-1;
        InsertIteratorFrame *parentFrame, *frame = iter->fromBegin(layer);
        Page *parentPage, *page = Iterator<false>::getPage(storage, frame->reference);
        aquireData(page, frame->index, frame->maxIndex);
        do {
            layer = iter->end-1;
            frame = iter->fromBegin(layer);
            page = insertAdvanceLayer<true>(storage, frame);
            aquireData(page, frame->index, frame->maxIndex);
            ReferenceType childReference = frame->reference;
            while(true) {
                parentFrame = iter->fromBegin(--layer);
                if(parentFrame->index <= parentFrame->maxIndex) {
                    parentPage = Iterator<false>::getPage(storage, parentFrame->reference);
                    parentPage->copyKey(parentPage, page, parentFrame->index-1, 0);
                    parentPage->setReference(childReference, parentFrame->index++);
                    break;
                } else {
                    parentPage = insertAdvanceLayer<false>(storage, parentFrame);
                    parentPage->setReference(childReference, parentFrame->index++);
                    childReference = parentFrame->reference;
                }
            }
        } while(frame->elementCount > 0);
    }

    struct EraseData {
        Storage* storage;
        Iterator<false> *from, *to, *iter;
        bool spareLowerInner, eraseHigherInner;
        LayerType layer;
    };

    template<bool isLeaf>
    bool eraseLayer(EraseData& data) {
        IndexType lowerInnerIndex = data.from->fromBegin(data.layer)->index+data.spareLowerInner,
                  higherInnerIndex = data.to->fromBegin(data.layer)->index+data.eraseHigherInner;
        if(lowerInnerIndex == higherInnerIndex)
            return false;
        Page* lowerInner = Iterator<false>::getPage(data.storage, data.from->fromBegin(data.layer)->reference);
        if(data.layer == data.from->start) {
            Page::template erase1<isLeaf>(lowerInner, lowerInnerIndex, higherInnerIndex);
            if(lowerInner->count == 0) {
                data.storage->releasePage(data.from->fromBegin(data.layer)->reference);
                init();
            }
            return false;
        }
        Page* higherInner = Iterator<false>::getPage(data.storage, data.to->fromBegin(data.layer)->reference);
        if(lowerInner == higherInner) {
            Page::template erase1<isLeaf>(lowerInner, lowerInnerIndex, higherInnerIndex);
            data.eraseHigherInner = false;
        } else {
            IndexType higherInnerParentIndex = data.to->fromBegin(data.layer-1)->index;
            Page* higherInnerParent = Iterator<false>::getPage(data.storage, data.to->fromBegin(data.layer-1)->reference);
            if(Page::template erase2<isLeaf>(higherInnerParent, lowerInner, higherInner,
                                             higherInnerParentIndex, lowerInnerIndex, higherInnerIndex)) {
                data.storage->releasePage(data.to->fromBegin(data.layer)->reference);
                data.eraseHigherInner = true;
            } else
                data.eraseHigherInner = false;
            data.iter->copy(data.to);
            while(data.iter->template advance<-1>(data.storage, data.layer-1) == 0 &&
                  data.iter->fromBegin(data.layer)->reference != data.from->fromBegin(data.layer)->reference)
                data.storage->releasePage(data.iter->fromBegin(data.layer)->reference);
        }
        if(lowerInner->count < Page::template capacity<isLeaf>()/2) {
            data.iter->copy(data.from);
            IndexType lowerInnerParentIndex = data.from->fromBegin(data.layer-1)->index, higherOuterParentIndex;
            Page *lowerInnerParent = Iterator<false>::getPage(data.storage, data.from->fromBegin(data.layer-1)->reference),
                 *lowerOuter = (data.iter->template advance<-1>(data.storage, data.layer-1) == 0)
                               ? Iterator<false>::getPage(data.storage, data.iter->fromBegin(data.layer)->reference) : NULL,
                 *higherOuterParent, *higherOuter;
            data.iter->copy(data.to);
            if(data.iter->template advance<1>(data.storage, data.layer-1) == 0) {
                higherOuter = Iterator<false>::getPage(data.storage, data.iter->fromBegin(data.layer)->reference);
                higherOuterParent = Iterator<false>::getPage(data.storage, data.iter->fromBegin(data.layer-1)->reference);
                higherOuterParentIndex = data.iter->fromBegin(data.layer-1)->index;
            } else
                higherOuter = NULL;
            if(!lowerOuter && !higherOuter) {
                if(lowerInner->count == 0) {
                    data.storage->releasePage(data.from->fromBegin(data.layer)->reference);
                    data.spareLowerInner = false;
                } else {
                    rootReference = data.from->fromBegin(data.layer)->reference;
                    layerCount = data.from->end-data.layer;
                    data.spareLowerInner = true;
                }
            } else if(Page::template redistribute<isLeaf>(lowerInnerParent, higherOuterParent,
                                                          lowerOuter, lowerInner, higherOuter,
                                                          lowerInnerParentIndex, higherOuterParentIndex)) {
                data.storage->releasePage(data.from->fromBegin(data.layer)->reference);
                data.spareLowerInner = false;
            } else
                data.spareLowerInner = true;
        } else
            data.spareLowerInner = true;
        --data.layer;
        return true;
    }

    void erase(Storage* storage, Iterator<false>* from, Iterator<false>* to) {
        assert(from->isValid() && to->isValid() && from->compare(to) < 1);
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
