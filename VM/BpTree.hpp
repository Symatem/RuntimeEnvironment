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
    typedef uint64_t ReferenceType;
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
                           sizeof(Type)*8, 0, bitOffset+src*sizeof(Type)*8);
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
                           sizeof(Type)*8, bitOffset+dst*sizeof(Type)*8, 0);
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
        void overwriteBranchContent(const KeyType*& srcKeys, const ReferenceType*& srcReferences,
                                    IndexType dst, IndexType n) {
            assert(n > 0 && n+dst <= BranchCapacity);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcKeys -= n,
                            n*KeyBits, KeysBitOffset+dst*KeyBits, 0);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcReferences -= n,
                            (n+(additionalReference?1:0))*ReferenceBits,
                            BranchReferencesBitOffset+dst*ReferenceBits, 0);
        }

        void overwriteLeafContent(const KeyType*& srcKeys, const ValueType*& srcValues,
                                  IndexType dst, IndexType n) {
            assert(n > 0 && n+dst <= LeafCapacity);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcKeys -= n,
                            n*KeyBits, KeysBitOffset+dst*KeyBits, 0);
            bitwiseCopy<-1>(reinterpret_cast<ArchitectureType*>(this), srcValues -= n,
                            n*ValueBits, LeafValuesBitOffset+dst*ValueBits, 0);
        }

        template<bool isLowest>
        void initBranch(KeyType*& keyBufferPos, const KeyType*& keys, const ReferenceType*& references, IndexType n) {
            assert(n > 0 && n <= BranchCapacity);
            count = n;
            overwriteBranchContent<true>(keys, references, 0, count);
            if(!isLowest)
                bitwiseCopy<1>(--keyBufferPos, --keys, KeyBits, 0, 0);
        }

        template<bool isLowest>
        void initLeaf(KeyType*& keyBufferPos, const KeyType*& keys, const ValueType*& values, IndexType n) {
            assert(n > 0 && n <= LeafCapacity);
            count = n;
            overwriteLeafContent(keys, values, 0, n);
            if(!isLowest)
                bitwiseCopy<1>(--keyBufferPos, keys, KeyBits, 0, 0);
        }

        // TODO: Fill Insert

        static void shiftKey(Page* dstPage, IndexType dstIndex,
                             Page* srcPage, IndexType srcIndex) {
            bitwiseCopy<1>(dstPage, srcPage, sizeof(KeyType)*8, KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits);
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
            bitwiseCopy<1>(dstPage, srcPage, (n+(additionalReference?1:0))*ReferenceBits,
                           BranchReferencesBitOffset+dstIndex*ReferenceBits, BranchReferencesBitOffset+srcIndex*ReferenceBits);
            if(!additionalReference) {
                --dstIndex;
                --srcIndex;
            }
            bitwiseCopy<1>(dstPage, srcPage, n*KeyBits,
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits);
        }

        static void shiftLeafElements(Page* dstPage, Page* srcPage,
                                      IndexType dstIndex, IndexType srcIndex,
                                      IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<1>(dstPage, srcPage, n*KeyBits,
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits);
            bitwiseCopy<1>(dstPage, srcPage, n*ValueBits,
                           LeafValuesBitOffset+dstIndex*ValueBits, LeafValuesBitOffset+srcIndex*ValueBits);
        }

        void eraseFromBranch(IndexType start, IndexType end) {
            assert(start < end && end <= count);
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

        static void evacuateBranchDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftBranchElements<true>(lower, higher, lower->count+1, 0, higher->count);
            shiftKey(lower, parent, lower->count, parentIndex);
            lower->count += higher->count+1;
        }

        static void evacuateBranchUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftBranchElements<true>(higher, higher, lower->count+1, 0, higher->count);
            shiftBranchElements<true>(higher, lower, 0, 0, lower->count);
            shiftKey(higher, parent, lower->count, parentIndex);
            higher->count += lower->count+1;
        }

        static void evacuateLeafDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftLeafElements(lower, higher, lower->count, 0, higher->count);
            lower->count += higher->count;
        }

        static void evacuateLeafUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftLeafElements(higher, higher, lower->count, 0, higher->count);
            shiftLeafElements(higher, lower, 0, 0, lower->count);
            higher->count += lower->count;
        }

        static void shiftBranchDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            swapKeyInParent(lower, higher, parent, lower->count, count-1, parentIndex);
            shiftBranchElements<true>(lower, lower->count+1, higher, 0, count-1);
            updateCounts(lower, higher, count);
            shiftBranchElements<true>(higher, 0, higher, count, higher->count);
        }

        static void shiftBranchUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            shiftBranchElements<true>(higher, higher, count, 0, higher->count);
            updateCounts(higher, lower, count);
            shiftBranchElements<true>(higher, lower, 0, lower->count+1, count-1);
            swapKeyInParent(higher, lower, parent, count-1, lower->count, parentIndex);
        }

        static void shiftLeafDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            shiftLeafElements(lower, higher, lower->count, 0, count);
            updateCounts(lower, higher, count);
            shiftLeafElements(higher, higher, 0, count, higher->count);
            shiftKey(parent, higher, parentIndex, 0);
        }

        static void shiftLeafUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            shiftLeafElements(higher, higher, count, 0, higher->count);
            updateCounts(higher, lower, count);
            shiftLeafElements(higher, lower, 0, lower->count, count);
            shiftKey(parent, higher, parentIndex, 0);
        }

        // TODO: redistribute
    };

};
