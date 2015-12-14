#include "Blob.hpp"

ArchitectureType bitsPerPage = 4096*8;

class BasePage {
    public:
    ArchitectureType transaction;
};

template<class TemplateKeyType, class TemplateValueType>
class BpTree {
    public:
    typedef uint16_t IndexType;
    typedef uint8_t LayerType;
    typedef void* ReferenceType;
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
        static const ArchitectureType // TODO: Padding
            HeaderBits = sizeof(BasePage)*8+sizeof(IndexType)*8,
            BodyBits = bitsPerPage-HeaderBits,
            KeyBits = sizeof(KeyType)*8,
            ReferenceBits = sizeof(ReferenceType)*8,
            ValueBits = sizeof(ValueType)*8,
            BranchCapacity = (BodyBits-ReferenceBits)/(KeyBits+ReferenceBits),
            LeafCapacity = BodyBits/(KeyBits+ValueBits),
            BranchKeysBits = BranchCapacity*KeyBits,
            BranchReferencesBits = (BranchCapacity+1)*ReferenceBits,
            LeafKeysBits = LeafCapacity*KeyBits,
            LeafValuesBits = LeafCapacity*ValueBits,
            KeysBitOffset = architecturePadding(HeaderBits),
            BranchReferencesBitOffset = architecturePadding(KeysBitOffset+BranchKeysBits),
            LeafValuesBitOffset = architecturePadding(KeysBitOffset+LeafKeysBits);

        // Both (Branch and Leaf)
        IndexType count;

        template<typename Type, ArchitectureType bitOffset>
        Type get(IndexType src) {
            Type result;
            bitwiseCopy<1>(&result, this, sizeof(Type)*8, 0, bitOffset+src*sizeof(Type)*8);
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
            bitwiseCopy<1>(this, &content, sizeof(Type)*8, bitOffset+dst*sizeof(Type)*8, 0);
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

        // Insert

        static inline ArchitectureType branchesNeededForReferences(ArchitectureType n) {
            return (n+BranchCapacity)/(BranchCapacity+1);
        }

        static inline ArchitectureType leavesNeededForElements(ArchitectureType n) {
            return (n+LeafCapacity-1)/LeafCapacity;
        }

        template<bool additionalReference>
        void overwriteBranchContent(const KeyType*& srcKeys, const ReferenceType*& srcReferences,
                                    IndexType n, IndexType dst) {
            assert(n > 0 && n+dst < BranchCapacity);
            bitwiseCopy<-1>(this, srcKeys -= n, n*KeyBits, KeysBitOffset+dst*KeyBits, 0);
            bitwiseCopy<-1>(this, srcReferences -= n, (n+(additionalReference?1:0))*ReferenceBits,
                            BranchReferencesBitOffset+dst*ReferenceBits, 0);
        }

        void overwriteLeafContent(const KeyType*& srcKeys, const ValueType*& srcValues,
                                  IndexType n, IndexType dst) {
            assert(n > 0 && n+dst < LeafCapacity);
            bitwiseCopy<-1>(this, srcKeys -= n, n*KeyBits, KeysBitOffset+dst*KeyBits, 0);
            bitwiseCopy<-1>(this, srcValues -= n, n*ValueBits, LeafValuesBitOffset+dst*ValueBits, 0);
        }

        template<bool isLowest>
        void initBranch(KeyType*& keyBufferPos, const KeyType*& keys, const ReferenceType*& references, IndexType n) {
            assert(n > 0 && n <= BranchCapacity);
            count = n;
            overwriteBranchContent<true>(keys, references, count, 0);
            if(!isLowest)
                bitwiseCopy<1>(--keyBufferPos, --keys, KeyBits, 0, 0);
        }

        template<bool isLowest>
        void initLeaf(KeyType*& keyBufferPos, const KeyType*& keys, const ValueType*& values, IndexType n) {
            assert(n > 0 && n <= LeafCapacity);
            count = n;
            overwriteLeafContent(keys, values, n, 0);
            if(!isLowest)
                bitwiseCopy<1>(--keyBufferPos, keys, KeyBits, 0, 0);
        }

        // TODO: Fill Insert

        // Erase

        static void shiftKey(Page* dstPage, IndexType dstIndex,
                             Page* srcPage, IndexType srcIndex) {
            dstPage->setKey(dstIndex, srcPage->getKey(srcIndex));
        }

        static void swapKeyInParent(Page* dstPage, IndexType dstIndex,
                                    Page* srcPage, IndexType srcIndex,
                                    Page* parent, IndexType parentIndex) {
            shiftKey(dstPage, parent, dstIndex, parentIndex);
            shiftKey(parent, srcPage, parentIndex, srcIndex);
        }

        template<bool additionalReference>
        static void shiftBranchElements(Page* dstPage, IndexType dstIndex,
                                        Page* srcPage, IndexType srcIndex,
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

        static void shiftLeafElements(Page* dstPage, IndexType dstIndex,
                                      Page* srcPage, IndexType srcIndex,
                                      IndexType n) {
            assert(n > 0 && dstIndex <= dstPage->count && srcIndex+n <= srcPage->count);
            bitwiseCopy<1>(dstPage, srcPage, n*KeyBits,
                           KeysBitOffset+dstIndex*KeyBits, KeysBitOffset+srcIndex*KeyBits);
            bitwiseCopy<1>(dstPage, srcPage, n*ValueBits,
                           LeafValuesBitOffset+dstIndex*ValueBits, LeafValuesBitOffset+srcIndex*ValueBits);
        }

        void eraseFromBranch1(IndexType start, IndexType end) {
            auto shiftCount = count-end+1;
            shiftElementsBranch(this, start, this, end, shiftCount);
            count -= end-start;
        }

        void eraseFromLeaf1(IndexType start, IndexType end) {
            auto shiftCount = count-end;
            shiftElementsLeaf(this, start, this, end, shiftCount);
            count -= end-start;
        }

        // TODO: eraseFromBranch2, eraseFromLeaf2

        // Redistribute

        static void evacuateBranchDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftKeysBranch(lower, lower->count+1, higher, 0, higher->count);
            shiftKey(lower, parent, lower->count, parentIndex);
            lower->count += higher->count+1;
        }

        static void evacuateBranchUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftKeysBranch(higher, lower->count+1, higher, 0, higher->count);
            shiftKeysBranch(higher, 0, lower, 0, lower->count);
            shiftKey(higher, parent, lower->count, parentIndex);
            higher->count += lower->count+1;
        }

        static void evacuateLeafDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftElementsLeaf(lower, lower->count, higher, 0, higher->count);
            lower->count += higher->count;
        }

        static void evacuateLeafUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex) {
            shiftElementsLeaf(higher, lower->count, higher, 0, higher->count);
            shiftElementsLeaf(higher, 0, lower, 0, lower->count);
            higher->count += lower->count;
        }

        static void shiftBranchDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            swapKeyInParent(lower, lower->count, higher, count-1, parent, parentIndex);
            shiftKeysBranch(lower, lower->count+1, higher, 0, count-1);
            updateCounts(lower, higher, count);
            shiftKeysBranch(higher, 0, higher, count, higher->count);
        }

        static void shiftBranchUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            shiftKeysBranch(higher, count, higher, 0, higher->count);
            updateCounts(higher, lower, count);
            shiftKeysBranch(higher, 0, lower, lower->count+1, count-1);
            swapKeyInParent(higher, count-1, lower, lower->count, parent, parentIndex);
        }

        static void shiftLeafDown(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            shiftElementsLeaf(lower, lower->count, higher, 0, count);
            updateCounts(lower, higher, count);
            shiftElementsLeaf(higher, 0, higher, count, higher->count);
            shiftKey(parent, higher, parentIndex, 0);
        }

        static void shiftLeafUp(Page* lower, Page* higher, Page* parent, IndexType parentIndex, IndexType count) {
            shiftElementsLeaf(higher, count, higher, 0, higher->count);
            updateCounts(higher, lower, count);
            shiftElementsLeaf(higher, 0, lower, lower->count, count);
            shiftKey(parent, higher, parentIndex, 0);
        }

        // TODO: redistribute
    };

};
