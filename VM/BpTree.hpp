#include "Blob.hpp"

const ArchitectureType bitsPerPage = 4096*8;

class BasePage {
    public:
    ArchitectureType transaction;
};

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

        static ArchitectureType branchesNeededForReferences(ArchitectureType n) {
            return (n+BranchCapacity)/(BranchCapacity+1);
        }

        static ArchitectureType leavesNeededForElements(ArchitectureType n) {
            return (n+LeafCapacity-1)/LeafCapacity;
        }

        IndexType count;

        template<typename Type, ArchitectureType bitOffset>
        Type get(IndexType src) {
            Type result;
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(&result), reinterpret_cast<ArchitectureType*>(this),
                           0, bitOffset+src*sizeof(Type)*8, sizeof(Type)*8);
            return result;
        }

        KeyType getKey(IndexType src) {
            return get<KeyType, KeysBitOffset>(src);
        }

        ReferenceType getReference(IndexType src) {
            return get<ReferenceType, BranchReferencesBitOffset>(src);
        }

        ValueType getValue(IndexType src) {
            return get<ValueType, LeafValuesBitOffset>(src);
        }

        template<typename Type, ArchitectureType bitOffset>
        void set(IndexType dst, Type content) {
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(this), reinterpret_cast<ArchitectureType*>(&content),
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
        void debugPrint(std::ostream& stream) {
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
                // if(key >= getKey(mid)+isLeaf) // TODO: overflow ?
                if((isLeaf && key >= getKey(mid)) ||
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
        void overwriteBranchContent(const ArchitectureType* srcKeys, const ArchitectureType* srcReferences,
                                    IndexType dst, IndexType n) {
            assert(n > 0 && n+dst <= BranchCapacity);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcKeys,
                            KeysBitOffset+dst*KeyBits, 0, n*KeyBits);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcReferences,
                            BranchReferencesBitOffset+dst*ReferenceBits, 0,
                            (n+(additionalReference?1:0))*ReferenceBits);
        }

        void overwriteLeafContent(const ArchitectureType* srcKeys, const ArchitectureType* srcValues,
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
            overwriteBranchContent<true>(keys, references, 0, count);
        }

        template<bool isLowest>
        void initLeaf(const ArchitectureType* keys, const ArchitectureType* values,
                      Page* parent, IndexType parentIndex, IndexType n) {
            assert(n > 0 && n <= LeafCapacity);
            count = n;
            if(!isLowest)
                parent->setKey(parentIndex, keys);
            overwriteLeafContent(keys, values, 0, n);
        }

        // TODO: Fill Insert

        static void shiftKey(Page* dstPage, Page* srcPage,
                             IndexType dstIndex, IndexType srcIndex) {
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage), reinterpret_cast<ArchitectureType*>(srcPage),
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                           sizeof(KeyType)*8);
        }

        static void swapKeyInParent(Page* dstPage, Page* srcPage, Page* parent,
                                    IndexType dstIndex, IndexType srcIndex, IndexType parentIndex) {
            shiftKey(dstPage, parent, dstIndex, parentIndex);
            shiftKey(parent, srcPage, parentIndex, srcIndex);
        }

        template<bool additionalReference>
        static void shiftBranchElements(Page* dstPage, Page* srcPage,
                                        IndexType dstIndex, IndexType srcIndex,
                                        IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage), reinterpret_cast<ArchitectureType*>(srcPage),
                           BranchReferencesBitOffset+dstIndex*ReferenceBits, BranchReferencesBitOffset+srcIndex*ReferenceBits,
                           (n+(additionalReference?1:0))*ReferenceBits);
            if(!additionalReference) {
                --dstIndex;
                --srcIndex;
            }
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage), reinterpret_cast<ArchitectureType*>(srcPage),
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                           n*KeyBits);
        }

        static void shiftLeafElements(Page* dstPage, Page* srcPage,
                                      IndexType dstIndex, IndexType srcIndex,
                                      IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage), reinterpret_cast<ArchitectureType*>(srcPage),
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits,
                           n*KeyBits);
            bitwiseCopy<1>(reinterpret_cast<ArchitectureType*>(dstPage), reinterpret_cast<ArchitectureType*>(srcPage),
                           LeafValuesBitOffset+dstIndex*ValueBits, LeafValuesBitOffset+srcIndex*ValueBits,
                           n*ValueBits);
        }

        void eraseFromBranch(IndexType start, IndexType end) {
            assert(start > 0 && start < end && end <= count);
            auto shiftCount = count-end;
            shiftBranchElements<false>(this, this, start, end, shiftCount);
            count -= end-start;
        }

        void eraseFromLeaf(IndexType start, IndexType end) {
            assert(start < end && end <= count);
            auto shiftCount = count-end;
            shiftLeafElements(this, this, start, end, shiftCount);
            count -= end-start;
        }

        static bool eraseFromBranches(Page* lower, Page* higher, Page* parent,
                                      IndexType parentIndex, IndexType start, IndexType end) {
            assert(start < end && end <= lower->count+higher->count);
            IndexType count = higher->count-end+start;
            if(count <= BranchCapacity) {
                shiftBranchElements<true>(lower, higher, start, end, higher->count-end);
                shiftKey(lower, parent, start-1, parentIndex);
                lower->count = count;
                return true;
            } else {
                lower->count = count/2;
                higher->count = (count-1)/2;
                auto shiftCount = start-lower->count-1;
                if(shiftCount < 0) {
                    shiftCount *= -1;
                    swapKeyInParent(lower, higher, parent, lower->count, end, parentIndex);
                    shiftBranchElements<true>(lower, higher, lower->count+1, end, shiftCount-1);
                    shiftBranchElements<true>(higher, higher, 0, end+shiftCount, higher->count);
                } else if(shiftCount > 0) {
                    shiftBranchElements<true>(higher, higher, shiftCount, end, higher->count);
                    shiftBranchElements<true>(higher, lower, 0, lower->count+1, shiftCount-1);
                    swapKeyInParent(higher, lower, parent, shiftCount-1, lower->count, parentIndex);
                } else
                    shiftBranchElements<true>(higher, higher, 0, end, higher->count);
                return false;
            }
        }

        static bool eraseFromLeaves(Page* lower, Page* higher, Page* parent,
                                    IndexType parentIndex, IndexType start, IndexType end) {
            assert(start < end && end <= lower->count+higher->count);
            IndexType count = higher->count-end+start;
            if(count <= LeafCapacity) {
                shiftLeafElements(lower, higher, start, end, higher->count-end);
                lower->count = count;
                return true;
            } else {
                lower->count = (count+1)/2;
                higher->count = count/2;
                auto shiftCount = start-lower->count;
                if(shiftCount < 0) {
                    shiftCount *= -1;
                    shiftLeafElements(lower, higher, start, end, shiftCount);
                    shiftLeafElements(higher, higher, 0, end+shiftCount, higher->count);
                    shiftKey(parent, higher, parentIndex, 0);
                } else {
                    shiftLeafElements(higher, higher, shiftCount, end, higher->count);
                    shiftLeafElements(higher, lower, 0, lower->count, shiftCount);
                    shiftKey(parent, higher, parentIndex, 0);
                }
                return false;
            }
        }

        template<bool isLeaf>
        static void evacuateDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            if(isLeaf) {
                shiftLeafElements(lower, higher, lower->count, 0, higher->count);
                lower->count += higher->count;
            } else {
                shiftBranchElements<true>(lower, higher, lower->count+1, 0, higher->count);
                shiftKey(lower, parent, lower->count, parentIndex);
                lower->count += higher->count+1;
            }
        }

        template<bool isLeaf>
        static void evacuateUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            if(isLeaf) {
                shiftLeafElements(higher, higher, lower->count, 0, higher->count);
                shiftLeafElements(higher, lower, 0, 0, lower->count);
                higher->count += lower->count;
            } else {
                shiftBranchElements<true>(higher, higher, lower->count+1, 0, higher->count);
                shiftBranchElements<true>(higher, lower, 0, 0, lower->count);
                shiftKey(higher, parent, lower->count, parentIndex);
                higher->count += lower->count+1;
            }
        }

        template<bool isLeaf>
        static void shiftDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                shiftLeafElements(lower, higher, lower->count, 0, count);
                updateCounts(lower, higher, count);
                shiftLeafElements(higher, higher, 0, count, higher->count);
                shiftKey(parent, higher, parentIndex, 0);
            } else {
                swapKeyInParent(lower, higher, parent, lower->count, count-1, parentIndex);
                shiftBranchElements<true>(lower, lower->count+1, higher, 0, count-1);
                updateCounts(lower, higher, count);
                shiftBranchElements<true>(higher, 0, higher, count, higher->count);
            }
        }

        template<bool isLeaf>
        static void shiftUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            if(isLeaf) {
                shiftLeafElements(higher, higher, count, 0, higher->count);
                updateCounts(higher, lower, count);
                shiftLeafElements(higher, lower, 0, lower->count, count);
                shiftKey(parent, higher, parentIndex, 0);
            } else {
                shiftBranchElements<true>(higher, higher, count, 0, higher->count);
                updateCounts(higher, lower, count);
                shiftBranchElements<true>(higher, lower, 0, lower->count+1, count-1);
                swapKeyInParent(higher, lower, parent, count-1, lower->count, parentIndex);
            }
        }

        template<bool isLeaf, bool focus>
        static bool redistribute2(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            auto count = lower->count+higher->count;
            if(count <= ((isLeaf) ? LeafCapacity : BranchCapacity)) {
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
            auto count = lower->count+middle->count+higher->count;
            if(count <= ((isLeaf) ? LeafCapacity : BranchCapacity)*2) {
                count = count/2-higher->count;
                if(count > 0)
                    shiftUp<isLeaf>(middle, higher, higherParent, higherParentIndex, count);
                evacuateDown<isLeaf>(lower, middle, middleParent, middleParentIndex);
                return true;
            } else {
                count = count/3;
                auto shiftLower = lower->count-count, shiftUpper = higher->count-count;
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
};
