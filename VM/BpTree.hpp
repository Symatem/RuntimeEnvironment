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

        static ArchitectureType branchesNeededForReferences(ArchitectureType n) {
            return (n+BranchCapacity)/(BranchCapacity+1);
        }

        static ArchitectureType leavesNeededForElements(ArchitectureType n) {
            return (n+LeafCapacity-1)/LeafCapacity;
        }

        IndexType count;

        template<typename Type, ArchitectureType bitOffset>
        Type get(IndexType src) const {
            Type result;
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(&result),
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
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(this),
                           reinterpret_cast<const ArchitectureType*>(&content),
                           bitOffset+dst*sizeof(Type)*8, 0, sizeof(Type)*8);
        }

        void setKey(IndexType src, KeyType content) {
            return set<KeyType, KeysBitOffset>(src, content);
        }

        void getReference(IndexType src, ReferenceType content) {
            return set<ReferenceType, BranchReferencesBitOffset>(src, content);
        }

        void getValue(IndexType src, ValueType content) {
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
                if((isLeaf && key > getKey(mid)) ||
                   (!isLeaf && key >= getKey(mid)))
                    begin = mid+1;
                else
                    end = mid;
            }
            return begin;
        }

        static void shiftCounts(Page* dstPage, Page* srcPage, IndexType n) {
            dstPage->count += n;
            srcPage->count -= n;
        }

        template<bool additionalReference>
        void overwriteBranch(const ArchitectureType* srcKeys, const ArchitectureType* srcReferences,
                                    IndexType dst, IndexType n) {
            assert(n > 0 && n+dst <= BranchCapacity);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcKeys,
                            KeysBitOffset+dst*KeyBits, 0, n*KeyBits);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcReferences,
                            BranchReferencesBitOffset+dst*ReferenceBits, 0,
                            (n+(additionalReference?1:0))*ReferenceBits);
        }

        void overwriteLeaf(const ArchitectureType* srcKeys, const ArchitectureType* srcValues,
                                  IndexType dst, IndexType n) {
            assert(n > 0 && n+dst <= LeafCapacity);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcKeys,
                            KeysBitOffset+dst*KeyBits, 0, n*KeyBits);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcValues,
                            LeafValuesBitOffset+dst*ValueBits, 0, n*ValueBits);
        }

        template<bool isLowest>
        void initBranch(const ArchitectureType* keys, const ArchitectureType* references,
                        Page* parent, IndexType parentIndex, IndexType n) {
            assert(n > 0 && n <= BranchCapacity);
            count = n;
            if(!isLowest) {
                parent->setKey(parentIndex, keys);
                keys = reinterpret_cast<const ArchitectureType*>(reinterpret_cast<const uint8_t*>(keys)+sizeof(KeyType));
            }
            overwriteBranch<true>(keys, references, 0, count);
        }

        template<bool isLowest>
        void initLeaf(const ArchitectureType* keys, const ArchitectureType* values,
                      Page* parent, IndexType parentIndex, IndexType n) {
            assert(n > 0 && n <= LeafCapacity);
            count = n;
            if(!isLowest)
                parent->setKey(parentIndex, keys);
            overwriteLeaf(keys, values, 0, n);
        }

        // TODO: Fill Insert

        template<bool additionalReference>
        static void copyBranchElements(Page* dstPage, Page* srcPage,
                                        IndexType dstIndex, IndexType srcIndex,
                                        IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage),
                           reinterpret_cast<const ArchitectureType*>(srcPage),
                           BranchReferencesBitOffset+dstIndex*ReferenceBits, BranchReferencesBitOffset+srcIndex*ReferenceBits,
                           (n+(additionalReference?1:0))*ReferenceBits);
            if(!additionalReference) {
                --dstIndex;
                --srcIndex;
            }
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage),
                           reinterpret_cast<const ArchitectureType*>(srcPage),
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                           n*KeyBits);
        }

        static void copyLeafElements(Page* dstPage, Page* srcPage,
                                      IndexType dstIndex, IndexType srcIndex,
                                      IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage),
                           reinterpret_cast<const ArchitectureType*>(srcPage),
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                           n*KeyBits);
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage),
                           reinterpret_cast<const ArchitectureType*>(srcPage),
                           LeafValuesBitOffset+dstIndex*ValueBits, LeafValuesBitOffset+srcIndex*ValueBits,
                           n*ValueBits);
        }

        static void copyKey(Page* dstPage, Page* srcPage,
                             IndexType dstIndex, IndexType srcIndex) {
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage),
                           reinterpret_cast<const ArchitectureType*>(srcPage),
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                           sizeof(KeyType)*8);
        }

        static void swapKeyInParent(Page* dstPage, Page* srcPage, Page* parent,
                                    IndexType dstIndex, IndexType srcIndex, IndexType parentIndex) {
            copyKey(dstPage, parent, dstIndex, parentIndex);
            copyKey(parent, srcPage, parentIndex, srcIndex);
        }

        template<bool isLeaf>
        void erase1(IndexType start, IndexType end) {
            assert(start < end && end <= count);
            auto shiftCount = count-end;
            if(isLeaf)
                copyLeafElements(this, this, start, end, shiftCount);
            else {
                assert(start > 0);
                copyBranchElements<false>(this, this, start, end, shiftCount);
            }
            count -= end-start;
        }

        template<bool isLeaf>
        static bool erase2(Page* lower, Page* higher, Page* parent,
                           IndexType parentIndex, IndexType start, IndexType end) {
            assert(start < end && end <= lower->count+higher->count);
            int count = higher->count-end+start;
            if(count <= capacity<isLeaf>()) {
                if(isLeaf)
                    copyLeafElements(lower, higher, start, end, higher->count-end);
                else {
                    copyBranchElements<true>(lower, higher, start, end, higher->count-end);
                    copyKey(lower, parent, start-1, parentIndex);
                }
                lower->count = count;
                return true;
            } else {
                if(!isLeaf) --count;
                lower->count = (count+1)/2;
                higher->count = count/2;
                count = start-lower->count;
                if(isLeaf) {
                    if(count < 0) {
                        count *= -1;
                        copyLeafElements(lower, higher, start, end, count);
                        copyLeafElements(higher, higher, 0, end+count, higher->count);
                    } else {
                        copyLeafElements(higher, higher, count, end, higher->count);
                        copyLeafElements(higher, lower, 0, lower->count, count);
                    }
                    copyKey(parent, higher, parentIndex, 0);
                } else {
                    --count;
                    if(count < 0) {
                        count *= -1;
                        swapKeyInParent(lower, higher, parent, lower->count, end, parentIndex);
                        copyBranchElements<true>(lower, higher, lower->count+1, end, count-1);
                        copyBranchElements<true>(higher, higher, 0, end+count, higher->count);
                    } else if(count > 0) {
                        copyBranchElements<true>(higher, higher, count, end, higher->count);
                        copyBranchElements<true>(higher, lower, 0, lower->count+1, count-1);
                        swapKeyInParent(higher, lower, parent, count-1, lower->count, parentIndex);
                    } else
                        copyBranchElements<true>(higher, higher, 0, end, higher->count);
                }
                return false;
            }
        }

        template<bool isLeaf>
        static void evacuateDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
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
        static void evacuateUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            if(isLeaf) {
                copyLeafElements(higher, higher, lower->count, 0, higher->count);
                copyLeafElements(higher, lower, 0, 0, lower->count);
                higher->count += lower->count;
            } else {
                copyBranchElements<true>(higher, higher, lower->count+1, 0, higher->count);
                copyBranchElements<true>(higher, lower, 0, 0, lower->count);
                copyKey(higher, parent, lower->count, parentIndex);
                higher->count += lower->count+1;
            }
        }

        template<bool isLeaf>
        static void shiftDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                copyLeafElements(lower, higher, lower->count, 0, count);
                updateCounts(lower, higher, count);
                copyLeafElements(higher, higher, 0, count, higher->count);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                swapKeyInParent(lower, higher, parent, lower->count, count-1, parentIndex);
                copyBranchElements<true>(lower, lower->count+1, higher, 0, count-1);
                updateCounts(lower, higher, count);
                copyBranchElements<true>(higher, 0, higher, count, higher->count);
            }
        }

        template<bool isLeaf>
        static void shiftUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                copyLeafElements(higher, higher, count, 0, higher->count);
                updateCounts(higher, lower, count);
                copyLeafElements(higher, lower, 0, lower->count, count);
                copyKey(parent, higher, parentIndex, 0);
            } else {
                copyBranchElements<true>(higher, higher, count, 0, higher->count);
                updateCounts(higher, lower, count);
                copyBranchElements<true>(higher, lower, 0, lower->count+1, count-1);
                swapKeyInParent(higher, lower, parent, count-1, lower->count, parentIndex);
            }
        }

        template<bool isLeaf, bool focus>
        static bool redistribute2(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            int count = lower->count+higher->count;
            if(count <= capacity<isLeaf>()) {
                if(focus)
                    evacuateUp<isLeaf>(lower, higher, parent, parentIndex);
                else
                    evacuateDown<isLeaf>(lower, higher, parent, parentIndex);
                return true;
            } else {
                count = ((focus) ? higher : lower)->count-count/2;
                if(focus)
                    shiftDown<isLeaf>(lower, higher, parent, parentIndex, count);
                else
                    shiftUp<isLeaf>(lower, higher, parent, parentIndex, count);
                return false;
            }
        }

        template<bool isLeaf>
        static bool redistribute3(Page* lower, Page* middle, Page* higher,
                                  Page* middleParent, Page* higherParent,
                                  IndexType middleParentIndex, IndexType higherParentIndex) {
            int count = lower->count+middle->count+higher->count;
            if(count <= capacity<isLeaf>()*2) {
                count = count/2-higher->count;
                if(count > 0)
                    shiftUp<isLeaf>(middle, higher, higherParent, higherParentIndex, count);
                evacuateDown<isLeaf>(lower, middle, middleParent, middleParentIndex);
                return true;
            } else {
                count = count/3;
                int shiftLower = lower->count-count, shiftUpper = higher->count-count;
                if(shiftLower < 0) {
                    if(shiftUpper < 0)
                        shiftUp<isLeaf>(middle, higher, higherParent, higherParentIndex, -shiftUpper);
                    else
                        shiftDown<isLeaf>(middle, higher, higherParent, higherParentIndex, shiftUpper);
                    shiftDown<isLeaf>(lower, middle, middleParent, middleParentIndex, -shiftLower);
                } else {
                    shiftUp<isLeaf>(lower, middle, middleParent, middleParentIndex, shiftLower);
                    if(shiftUpper < 0)
                        shiftUp<isLeaf>(middle, higher, higherParent, higherParentIndex, -shiftUpper);
                    else
                        shiftDown<isLeaf>(middle, higher, higherParent, higherParentIndex, shiftUpper);
                }
                return false;
            }
        }

        template<bool isLeaf>
        static bool redistribute(Page* lower, Page* middle, Page* higher,
                                 Page* middleParent, Page* higherParent,
                                 IndexType middleParentIndex, IndexType higherParentIndex) {
            if(lower) {
                if(higher)
                    return redistribute3<isLeaf>(lower, middle, higher,
                                                 middleParent, higherParent,
                                                 middleParentIndex, higherParentIndex);
                else
                    return redistribute2<isLeaf, true>(lower, middle, middleParent, middleParentIndex);
            } else
                return redistribute2<isLeaf, false>(middle, higher, higherParent, higherParentIndex);
        }
    };

    // Iterator

    struct IteratorFrame {
        ReferenceType stackBottom;
        IndexType index, maxIndex;
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
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>((*this)[dst->start]),
                           reinterpret_cast<const ArchitectureType*>(src[src->start]),
                           0, 0, sizeof(IteratorFrame)*8*layerCount);
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

    void insert(Context* context, Iterator<false>& at, ArchitectureType insertCounter,
                const KeyType* keys, const ValueType* values, float density = 1.0F) {
        // TODO
    }

    template<bool isLeaf>
    bool eraseAux(Context* context, Iterator<false>& from, Iterator<false>& to, Iterator<false>& aux, LayerType& layer) {
        bool spareLowerInner = !isLeaf, eraseHigherInner = true;
        IndexType lowerInnerIndex = from[layer]->index+spareLowerInner,
                  higherInnerIndex = to[layer]->index+eraseHigherInner;
        if(lowerInnerIndex == higherInnerIndex)
            return false;

        Page* lowerInner = Iterator<false>::getPage(context, from[layer]->reference);
        if(layer == from.start) {
            lowerInner->template erase1<isLeaf>(lowerInnerIndex, higherInnerIndex);
            if(lowerInner->count == 0) {
                releasePage(context, from[layer]->reference);
                if(isLeaf) init();
            }
            return false;
        }

        Page* higherInner = Iterator<false>::getPage(context, to[layer]->reference);
        if(lowerInner == higherInner)
            lowerInner->template erase1<isLeaf>(lowerInnerIndex, higherInnerIndex);
        else {
            IndexType higherInnerParentIndex = to[layer-1]->index;
            Page* higherInnerParent = Iterator<false>::getPage(context, to[layer-1]->reference);
            if(Page::erase2<isLeaf>(lowerInner, higherInner, higherInnerParent,
                                    higherInnerParentIndex, lowerInnerIndex, higherInnerIndex))
                releasePage(context, to[layer]->reference);
            /*else
                eraseHigherInner = false; TODO */

            aux.copy(&to);
            while(aux.template advance<-1>(context, layer-1) == 0 &&
                  aux[layer]->reference != from[layer]->reference)
                releasePage(context, aux[layer]->reference);
        }

        if(lowerInner->count < Page::template capacity<isLeaf>()/2) {
            aux.copy(&from);
            IndexType lowerInnerParentIndex = from[layer-1]->index, higherOuterParentIndex;
            Page *lowerInnerParent = Iterator<false>::getPage(context, from[layer-1]->reference), *higherOuterParent;
            Page *lowerOuter = (aux.template advance<-1>(context, layer-1) == 0)
                                ? Iterator<false>::getPage(context, aux[layer]->reference) : NULL, *higherOuter;

            aux.copy(&to);
            if(aux.template advance<1>(context, layer-1) == 0) {
                higherOuter = Iterator<false>::getPage(context, aux[layer]->reference);
                higherOuterParent = Iterator<false>::getPage(context, aux[layer-1]->reference);
                higherOuterParentIndex = aux[layer-1]->index;
            } else
                higherOuter = NULL;

            if(!lowerOuter && !higherOuter) {
                if(lowerInner->count == 0) {
                    releasePage(context, from[layer]->reference);
                    if(isLeaf) init();
                } else {
                    rootReference = from[layer]->reference;
                    layerCount = layer-from.start;
                }
            } else if(Page::redistribute<isLeaf>(lowerOuter, lowerInner, higherOuter,
                                                 lowerInnerParent, higherOuterParent,
                                                 lowerInnerParentIndex, higherOuterParentIndex)) {
                releasePage(context, from[layer]->reference);
                spareLowerInner = false;
            }
        }

        --layer;
        return true;
    }

    void erase(Context* context, Iterator<false>& from, Iterator<false>& to) {
        assert(from.isValid() && to.isValid() && from <= to);

        Iterator<false> aux;
        LayerType layer = to.end-1;
        if(eraseAux<true>(context, from, to, aux, layer))
            while(eraseAux<false>(context, from, to, aux, layer));
    }
};
