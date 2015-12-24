#include "Blob.hpp"

const ArchitectureType bitsPerPage = 4096*8;

class BasePage {
    public:
    ArchitectureType transaction;
};

BasePage* aquirePage(Context* context) {
    return reinterpret_cast<BasePage*>(malloc(bitsPerPage/8));
}

void releasePage(Context* context, BasePage* ptr) {
    free(ptr);
}

template<class TemplateKeyType, class TemplateValueType>
class BpTree {
    public:
    typedef uint16_t IndexType;
    typedef uint8_t LayerType;
    typedef BasePage* ReferenceType;
    typedef TemplateKeyType KeyType;
    typedef TemplateValueType ValueType;
    typedef std::function<void(KeyType& key, ValueType& value)> AquireData;

    LayerType layerCount;
    ReferenceType rootReference;

    void init() {
        rootReference = 0;
        layerCount = 0;
    }

    inline bool isEmpty() const {
        return (layerCount == 0);
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

        template<bool isLeaf>
        static ArchitectureType pagesNeededFor(ArchitectureType n) {
            if(isLeaf)
                return (n+LeafCapacity-1)/LeafCapacity;
            else
                return (n+BranchCapacity)/(BranchCapacity+1);
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

        void setKey(IndexType src, KeyType content) {
            return set<KeyType, KeysBitOffset>(src, content);
        }

        void setReference(IndexType src, ReferenceType content) {
            return set<ReferenceType, BranchReferencesBitOffset>(src, content);
        }

        void setValue(IndexType src, ValueType content) {
            return set<ValueType, LeafValuesBitOffset>(src, content);
        }

        template<bool isLeaf>
        void debugPrint(std::ostream& stream) const {
            stream << "Page " << (ArchitectureType)(this) << " " << count << std::endl;
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
            assert(dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             BranchReferencesBitOffset+dstIndex*ReferenceBits, BranchReferencesBitOffset+srcIndex*ReferenceBits,
                             (n+(additionalReference?1:0))*ReferenceBits);
            if(!additionalReference) {
                assert(dstIndex > 0 && srcIndex > 0 && n > 0);
                --dstIndex;
                --srcIndex;
            } else if(n == 0) return;
            bitwiseCopy<dir>(reinterpret_cast<ArchitectureType*>(dstPage),
                             reinterpret_cast<const ArchitectureType*>(srcPage),
                             KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                             n*KeyBits);
        }

        template<int dir = -1>
        static void copyLeafElements(Page* dstPage, Page* srcPage,
                                     IndexType dstIndex, IndexType srcIndex,
                                     IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
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
            copyBranchElements<true>(dstPage, srcPage, dstIndex, srcIndex, 0);
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

        void insertInLeaf(KeyType key, ValueType value, IndexType dst) {
            assert(dst <= count);
            setKey(dst, key);
            setValue(dst, value);
        }

        void insertInBranch(KeyType key, ReferenceType reference, IndexType dst) {
            assert(dst > 0 && dst <= count+1);
            setKey(dst-1, key);
            setReference(dst, reference);
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
        static IndexType insert2(Page* lower, Page* higher, IndexType dst, IndexType n) {
            IndexType insertLower, insertHigher, shiftLower = dst+n, shiftHigher = lower->count-dst;
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
            if(lower->count > shiftLower) {
                shiftLower = lower->count-shiftLower;
                shiftHigher -= shiftLower;
            } else
                shiftLower = 0;
            insertLower = lower->count-dst-shiftLower;
            if(!isLeaf) ++insertLower;
            insertHigher = n-insertLower;
            if(isLeaf) {
                copyLeafElements<1>(lower, lower, dst+insertLower, dst, shiftLower);
                copyLeafElements(higher, lower, insertHigher, dst, shiftHigher);
            } else {
                copyBranchElements<false, 1>(lower, lower, dst+insertLower, dst, shiftLower);
                copyBranchElements<false>(higher, lower, insertHigher, dst, shiftHigher);
            }
            return insertLower;
        }

        template<bool isLeaf>
        static void erase1(Page* lower, IndexType start, IndexType end) {
            if(isLeaf) {
                assert(start < end && end <= lower->count);
                copyLeafElements<-1>(lower, lower, start, end, lower->count-end);
            } else {
                assert(start > 0 && start < end && end <= lower->count+1);
                copyBranchElements<false, -1>(lower, lower, start, end, lower->count-end+1);
            }
            lower->count -= end-start;
        }

        template<bool isLeaf>
        static bool erase2(Page* parent, Page* lower, Page* higher,
                           IndexType parentIndex, IndexType startInLower, IndexType endInHigher) {
            if(isLeaf)
                assert(startInLower < lower->count && endInHigher > 0 && endInHigher <= higher->count);
            else
                assert(startInLower > 0 && startInLower <= lower->count && endInHigher <= higher->count+1);
            int count = startInLower+higher->count-endInHigher;
            if(count <= capacity<isLeaf>()) {
                if(isLeaf)
                    copyLeafElements(lower, higher, startInLower, endInHigher, higher->count-endInHigher);
                else
                    copyBranchElements<false>(lower, higher, startInLower, endInHigher, higher->count-endInHigher+1);
                lower->count = count;
                return true;
            } else {
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
                updateCounts(lower, higher, count);
                copyLeafElements<-1>(higher, higher, 0, count, higher->count);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                swapKeyInParent(parent, lower, higher, parentIndex, lower->count, count-1);
                copyBranchElements<true>(lower, higher, lower->count+1, 0, count-1);
                updateCounts(lower, higher, count);
                copyBranchElements<true, -1>(higher, higher, 0, count, higher->count);
            }
        }

        template<bool isLeaf>
        static void shiftUp(Page* parent, Page* lower, Page* higher, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                copyLeafElements<+1>(higher, higher, count, 0, higher->count);
                updateCounts(higher, lower, count);
                copyLeafElements(higher, lower, 0, lower->count, count);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                copyBranchElements<true, +1>(higher, higher, count, 0, higher->count);
                updateCounts(higher, lower, count);
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
        ReferenceType reference, auxReference;
        IndexType index, maxIndex;
        ArchitectureType elementCount, elementsPerPage;
    };

    template<bool readOnly>
    class Iterator {
        public:
        LayerType start, end;
        IteratorFrame stackBottom;

        /* const static constexpr auto getPage = std::conditional_value<readOnly,
            Page*(*)(const Database*, PageID), pageByID<Page>,
            Page*(*)(Database*, PageID&), openPageByID<true, Page>
        >::value;*/

        typedef typename std::conditional<readOnly,
            const Context*,
            Context*
        >::type ContextType;

        typedef typename std::conditional<readOnly,
            const Page*,
            Page*
        >::type PageType;

        typedef typename std::conditional<readOnly,
            const KeyType,
            KeyType
        >::type KeyType;

        typedef typename std::conditional<readOnly,
            const ValueType,
            ValueType
        >::type ValueType;

        typedef std::pair<KeyType*, ValueType*> PairType;

        IteratorFrame* operator[](LayerType layer) const {
            return &stackBottom+layer;
        }

        IteratorFrame* fromEnd(LayerType layer = 1) const {
            return &stackBottom+end-layer;
        }

        static PageType getPage(ContextType context, ReferenceType reference) {
            return reference;
        }

        /* PairType get(DatabaseType db) const {
            auto frame = fromEnd();
            auto page = pageByID<Page>(db, frame->reference);
            return PairType(&page->leaf.keys[frame->index], &page->leaf.values[frame->index]);
        }*/

        bool isValid() const {
            return start < end && fromEnd()->index <= fromEnd()->maxIndex;
        }

        template<bool srcReadOnly>
        void copy(Iterator<srcReadOnly>* src) const {
            static_assert(srcReadOnly && !readOnly, "Can not copy from read only to writeable");
            assert(end-start == src->end-src->start);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>((*this)[start]),
                            reinterpret_cast<const ArchitectureType*>(src[src->start]),
                            0, 0, sizeof(IteratorFrame)*8*(end-start));
        }

        // Compares two iterators (-1 : smaller, 0 equal, 1 greater)
        char compare(const Iterator& other) const {
            LayerType layer = start;
            for(; (*this)[layer]->index == other[layer]->index; ++layer)
                if(layer == end-1)
                    return 0;
            return ((*this)[layer]->index < other[layer]->index) ? -1 : 1;
        }

        bool operator<(const Iterator& other) const {
            return compare(other) == -1;
        }

        bool operator>(const Iterator& other) const {
            return compare(other) == 1;
        }

        bool operator<=(const Iterator& other) const {
            return compare(other) < 1;
        }

        bool operator>=(const Iterator& other) const {
            return compare(other) > -1;
        }

        bool operator==(const Iterator& other) const {
            return *fromEnd() == *other.fromEnd(); // compare(other) == 0;
        }

        bool operator!=(const Iterator& other) const {
            return *fromEnd() != *other.fromEnd(); // compare(other) != 0;
        }

        template<int dir = 1>
        bool advance(ContextType context, LayerType atLayer, ArchitectureType steps = 1) {
            if(start >= end || atLayer >= end)
                return steps;

            while(steps > 0) {
                bool abort = true;
                LayerType layer = atLayer;
                IteratorFrame* frame = (*this)[layer];

                ArchitectureType stepsToTake;
                if(dir == 1) {
                    stepsToTake = std::min((ArchitectureType)(frame->maxIndex-frame->index), steps);
                    frame->index += stepsToTake;
                } else {
                    stepsToTake = std::min((ArchitectureType)frame->index, steps);
                    frame->index -= stepsToTake;
                }
                steps -= stepsToTake;

                if(steps > 0)
                    while(layer > start) {
                        frame = (*this)[--layer];
                        if(dir == 1) {
                            if(frame->index < frame->maxIndex) {
                                ++frame->index;
                                --steps;
                                abort = false;
                                break;
                            }
                        } else {
                            if(frame->index > 0) {
                                --frame->index;
                                --steps;
                                abort = false;
                                break;
                            }
                        }
                    }

                while(layer+1 < end) {
                    Page* page = getPage(context, frame->reference);
                    ReferenceType& reference = page->Branch.References[frame->index];
                    page = getPage(context, reference);
                    frame = (*this)[++layer];
                    frame->reference = reference;
                    frame->maxIndex = (layer+1 == end) ? page->count-1 : page->count;
                    frame->index = (dir == 1) ? 0 : frame->maxIndex;
                }

                if(abort) break;
            }

            return steps;
        }

        void debugPrint(std::ostream& stream) const {
            stream << (end-start) << " (" << start << "/" << end << ")" << std::endl;
            for(LayerType layer = start; layer < end; ++layer) {
                auto frame = (*this)[layer];
                stream << "    " << layer << ": " << frame->reference << " (" << frame->index << "/" << frame->maxIndex << ")" << std::endl;
            }
        }
    };

    template<bool readOnly>
    Iterator<readOnly>* createIterator(LayerType reserve = 0) const {
        LayerType size = layerCount+reserve;
        auto iterator = reinterpret_cast<Iterator<readOnly>*>(malloc(sizeof(LayerType)*2+sizeof(IteratorFrame)*size));
        iterator->start = reserve;
        iterator->end = size;
        return iterator;
    }

    template<bool readOnly, bool border = false, bool lower = false>
	bool at(typename Iterator<readOnly>::ContextType context, Iterator<readOnly>& iterator, KeyType key = 0) const {
		if(isEmpty()) return false;
        LayerType layer = iterator.start;
        ReferenceType* reference = &rootReference;
	    while(true) {
            Page* page = Iterator<readOnly>::getPage(context, *reference);
	        IteratorFrame* frame = iterator[layer];
            frame->reference = *reference;
	        if(++layer == iterator.end) {
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
                reference = &page->Branch.References[frame->index];
	        }
	    }
	}

    template<bool apply, bool isLeaf>
    void insertLayer(Context* context, Iterator<false>* iter, LayerType& layer, ArchitectureType& elementCount) {
        Page *lower, *higher;
        auto frame = (*iter)[--layer];
        if(layer >= iter->start) {
            lower = Iterator<false>::getPage(context, frame->reference);
            elementCount += lower->count;
            if(!isLeaf) ++elementCount;
        }
        ArchitectureType pageCount = Page::pagesNeededFor<isLeaf>(elementCount);
        if(apply) {
            frame->elementsPerPage = elementCount/pageCount;
            frame->elementCount = elementCount;
            elementCount -= pageCount*frame->elementsPerPage;
            frame->elementCount -= elementCount;
        }
        if(layer >= iter->start) {
            if(apply) {
                if(pageCount == 1) {
                    frame->auxReference = 0;
                    Page::insert1<isLeaf>(lower, frame->index, elementCount);
                } else {
                    frame->auxReference = aquirePage(context);
                    higher = Iterator<false>::getPage(context, frame->auxReference);
                    elementCount = Page::insert2<isLeaf>(lower, higher, frame->index, elementCount);
                }
            }
            --pageCount;
        } else if(apply) {
            frame->reference = aquirePage(context);
            lower = Iterator<false>::getPage(context, frame->reference);
            lower->init<isLeaf>(elementCount);
            frame->auxReference = 0;
            frame->index = 0;
        }
        if(apply)
            frame->maxIndex = frame->index+elementCount-1;
        elementCount = pageCount;
    }

    template<bool apply>
    LayerType insertAux(Context* context, Iterator<false>* iter, ArchitectureType elementCount) {
        LayerType layer = iter->end;
        insertLayer<apply, true>(context, iter, layer, elementCount);
        while(elementCount > 0)
            insertLayer<apply, false>(context, iter, layer, elementCount);
        return layer;
    }

    template<bool isLeaf>
    Page* insertAdvanceLayer(Context* context, IteratorFrame* frame) {
        Page* page;
        if(frame->auxReference && frame->elementCount <= frame->elementsPerPage) {
            frame->reference = frame->auxReference;
            frame->maxIndex = frame->elementCount-1;
            page = Iterator<false>::getPage(context, frame->reference);
        } else {
            frame->reference = aquirePage(context);
            frame->maxIndex = frame->elementsPerPage-1;
            page = Iterator<false>::getPage(context, frame->reference);
            page->init<isLeaf>(frame->elementsPerPage);
        }
        frame->index = 0;
        frame->elementCount -= frame->maxIndex+1;
    }

    void insert(Context* context, Iterator<false>* at, AquireData aquireData, ArchitectureType insertCount) {
        assert(insertCount > 0);
        auto iter = createIterator<false>();
        iter->copy(at, insertAux<false>(context, at, insertCount));
        iter->start = insertAux<true>(context, iter, insertCount);
        layerCount = iter.end-iter.start;

        KeyType key;
        ValueType value;
        Page* page = Iterator<false>::getPage(context, (*iter)[iter.end-1]->reference);
        while(true) {
            LayerType layer = iter.end-1;
            auto frame = (*iter)[layer];
            if(frame->index > frame->maxIndex) {
                if(frame->elementCount == 0) break;
                key = page->getKey(0);
                page = insertAdvanceLayer<true>(context, frame);
                while(true) {
                    auto branchFrame = (*iter)[--layer];
                    Page* branchPage = Iterator<false>::getPage(context, branchFrame->reference);
                    if(branchFrame->index <= branchFrame->maxIndex) {
                        if(branchFrame->index == 0)
                            branchPage->setReference((*iter)[layer+1]->reference, branchFrame->index++);
                        else
                            branchPage->insertInBranch(key, (*iter)[layer+1]->reference, branchFrame->index++);
                        break;
                    }
                    branchPage = insertAdvanceLayer<false>(context, branchFrame);
                    branchPage->setReference((*iter)[layer+1]->reference, branchFrame->index++);
                }
            }
            aquireData(key, value);
            page->insertInLeaf(key, value, frame->index++);
        }
    }

    struct EraseData {
        Context* context;
        Iterator<false> *from, *to, *iter;
        bool spareLowerInner, eraseHigherInner;
        LayerType layer;
    };

    template<bool isLeaf>
    bool eraseLayer(EraseData& data) {
        IndexType lowerInnerIndex = (*data.from)[data.layer]->index+data.spareLowerInner,
                  higherInnerIndex = (*data.to)[data.layer]->index+data.eraseHigherInner;
        if(lowerInnerIndex == higherInnerIndex)
            return false;

        Page* lowerInner = Iterator<false>::getPage(data.context, (*data.from)[data.layer]->reference);
        if(data.layer == data.from->start) {
            Page::erase1<isLeaf>(lowerInner, lowerInnerIndex, higherInnerIndex);
            if(lowerInner->count == 0) {
                releasePage(data.context, (*data.from)[data.layer]->reference);
                if(isLeaf) init();
            }
            return false;
        }

        Page* higherInner = Iterator<false>::getPage(data.context, (*data.to)[data.layer]->reference);
        if(lowerInner == higherInner) {
            Page::erase1<isLeaf>(lowerInner, lowerInnerIndex, higherInnerIndex);
            data.eraseHigherInner = false;
        } else {
            IndexType higherInnerParentIndex = (*data.to)[data.layer-1]->index;
            Page* higherInnerParent = Iterator<false>::getPage(data.context, (*data.to)[data.layer-1]->reference);
            if(Page::erase2<isLeaf>(higherInnerParent, lowerInner, higherInner,
                                    higherInnerParentIndex, lowerInnerIndex, higherInnerIndex)) {
                releasePage(data.context, (*data.to)[data.layer]->reference);
                data.eraseHigherInner = true; // TODO Necessary ?
            } else
                data.eraseHigherInner = false;

            data.iter->copy(data.to);
            while(data.iter->template advance<-1>(data.context, data.layer-1) == 0 &&
                  (*data.iter)[data.layer]->reference != (*data.from)[data.layer]->reference)
                releasePage(data.context, (*data.iter)[data.layer]->reference);
        }

        if(lowerInner->count < Page::template capacity<isLeaf>()/2) {
            data.iter->copy(data.from);
            IndexType lowerInnerParentIndex = (*data.from)[data.layer-1]->index, higherOuterParentIndex;
            Page *lowerInnerParent = Iterator<false>::getPage(data.context, (*data.from)[data.layer-1]->reference),
                 *lowerOuter = (data.iter->template advance<-1>(data.context, data.layer-1) == 0)
                               ? Iterator<false>::getPage(data.context, (*data.iter)[data.layer]->reference) : NULL,
                 *higherOuterParent, *higherOuter;

            data.iter->copy(data.to);
            if(data.iter->template advance<1>(data.context, data.layer-1) == 0) {
                higherOuter = Iterator<false>::getPage(data.context, (*data.iter)[data.layer]->reference);
                higherOuterParent = Iterator<false>::getPage(data.context, (*data.iter)[data.layer-1]->reference);
                higherOuterParentIndex = (*data.iter)[data.layer-1]->index;
            } else
                higherOuter = NULL;

            if(!lowerOuter && !higherOuter) {
                if(lowerInner->count == 0) {
                    releasePage(data.context, (*data.from)[data.layer]->reference);
                    data.spareLowerInner = false; // TODO Necessary ?
                } else {
                    rootReference = (*data.from)[data.layer]->reference;
                    layerCount = data.from->end-data.layer;
                    data.spareLowerInner = true;
                }
            } else if(Page::redistribute<isLeaf>(lowerInnerParent, higherOuterParent,
                                                 lowerOuter, lowerInner, higherOuter,
                                                 lowerInnerParentIndex, higherOuterParentIndex)) {
                releasePage(data.context, (*data.from)[data.layer]->reference);
                data.spareLowerInner = false; // TODO Necessary ?
            } else
                data.spareLowerInner = true;
        } else
            data.spareLowerInner = true;

        --data.layer;
        return true;
    }

    void eraseRange(Context* context, Iterator<false>* from, Iterator<false>* to) {
        assert(from->isValid() && to->isValid() && *from <= *to);

        EraseData data;
        data.context = context;
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
