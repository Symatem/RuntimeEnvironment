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

    template<typename DataType, NativeNaturalType offset>
    DataType get(OffsetType src) const {
        static_assert(offset%8 == 0);
        return *reinterpret_cast<const DataType*>(reinterpret_cast<const char*>(this)+(offset+src*sizeOfInBits<DataType>::value)/8);
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

    template<typename DataType, NativeNaturalType offset>
    void set(OffsetType dst, DataType content) {
        static_assert(offset%8 == 0);
        *reinterpret_cast<DataType*>(reinterpret_cast<char*>(this)+(offset+dst*sizeOfInBits<DataType>::value)/8) = content;
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

    void integrateRanks(OffsetType at, OffsetType end) {
        if(!rankBits || at >= end)
            return;
        RankType sum;
        if(at == 0) {
            ++at;
            sum = getRank(0);
        } else
            sum = getRank(at-1);
        for(; at < end; ++at) {
            sum += getRank(at);
            setRank(at, sum);
        }
    }

    void disintegrateRanks(OffsetType begin, OffsetType at) {
        if(!rankBits || begin >= at)
            return;
        RankType higher = getRank(--at);
        for(; at > begin; --at) {
            RankType lower = getRank(at-1);
            setRank(at, higher-lower);
            higher = lower;
        }
        if(begin == 0)
            return;
        setRank(begin, higher-getRank(begin-1));
    }

    RankType getIntegratedRank() {
        return (header.layer == 0) ? static_cast<RankType>(header.count) : getRank(header.count-1);
    }

    template<bool frontKey, NativeIntegerType dir = -1>
    static void copyBranchElements(Page* dstPage, Page* srcPage,
                                   OffsetType dstIndex, OffsetType srcIndex,
                                   OffsetType n) {
        if(n == 0)
            return;
        if(rankBits)
            Storage::bitwiseCopy<dir>(reinterpret_cast<NativeNaturalType*>(dstPage),
                                      reinterpret_cast<const NativeNaturalType*>(srcPage),
                                      rankOffset+dstIndex*rankBits,
                                      rankOffset+srcIndex*rankBits,
                                      n*rankBits);
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
        if(keyBits > 0)
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
        if(keyBits > 0)
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

    template<bool dstIsLeaf, bool srcIsLeaf>
    static void copyKey(Page* dstPage, Page* srcPage,
                        OffsetType dstIndex, OffsetType srcIndex) {
        if(keyBits == 0)
            return;
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

    static void distributeCount(Page* lower, Page* higher, OffsetType n) {
        lower->header.count = (n+1)/2;
        higher->header.count = n/2;
    }

    template<bool isLeaf>
    static void insert(InsertIteratorFrame* frame, Page* lowerOuter, OffsetType count) {
        assert(count > lowerOuter->header.count && count <= capacity<isLeaf>() && frame->index <= lowerOuter->header.count);
        frame->endIndex = frame->index+count-lowerOuter->header.count;
        if(isLeaf)
            copyLeafElements<1>(lowerOuter, lowerOuter, frame->endIndex, frame->index, lowerOuter->header.count-frame->index);
        else
            copyBranchElements<true, 1>(lowerOuter, lowerOuter, frame->endIndex, frame->index, lowerOuter->header.count-frame->index);
        lowerOuter->header.count = count;
    }

    template<bool isLeaf>
    static void insertOverflow(InsertIteratorFrame* frame,
                               Page* lowerInnerParent, Page* higherOuterParent,
                               Page* lowerOuter, Page* lowerInner, Page* higherInner, Page* higherOuter,
                               OffsetType lowerInnerParentIndex, OffsetType higherOuterParentIndex) {
        OffsetType shiftHigherInner = frame->index+frame->endIndex-lowerOuter->header.count,
                   shiftHigherOuter = lowerOuter->header.count-frame->index;
        assert(frame->index+frame->endIndex >= lowerOuter->header.count);
        assert(lowerOuter->header.count >= frame->index);
        assert(frame->endIndex <= capacity<isLeaf>()*2);
        lowerInner->header.count = higherInner->header.count = frame->elementsPerPage;
        distributeCount(lowerOuter, higherOuter, frame->endIndex);
        if(shiftHigherInner < lowerOuter->header.count) {
            shiftHigherInner = lowerOuter->header.count-shiftHigherInner;
            assert(shiftHigherInner < higherInner->header.count);
            frame->lowerInnerIndex = 0;
            assert(shiftHigherOuter >= shiftHigherInner);
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
                lowerOuter->disintegrateRanks(lowerOuter->header.count, frame->rank);
                copyKey<false, false>(lowerInnerParent, lowerOuter, lowerInnerParentIndex, lowerOuter->header.count-1);
                copyBranchElements<false>(lowerInner, lowerOuter, 0, lowerOuter->header.count, frame->lowerInnerIndex);
                copyBranchElements<true>(higherOuter, lowerOuter, frame->higherOuterEndIndex, frame->index+shiftHigherInner, shiftHigherOuter);
                lowerInner->integrateRanks(0, frame->lowerInnerIndex-1);
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
        else {
            if(start > 0)
                copyBranchElements<true, -1>(lower, lower, start, end, lower->header.count-end);
            else if(end < lower->header.count)
                copyBranchElements<false, -1>(lower, lower, 0, end, lower->header.count-end);
        }
        lower->header.count -= end-start;
    }

    template<bool isLeaf>
    static bool erase2(Page* lowerParent, Page* higherParent, Page* lower, Page* higher,
                       OffsetType lowerParentIndex, OffsetType higherParentIndex,
                       OffsetType startInLower, OffsetType endInHigher) {
        assert(startInLower <= lower->header.count && endInHigher <= higher->header.count);
        NativeIntegerType count = startInLower;
        count += higher->header.count;
        count -= endInHigher;
        if(static_cast<OffsetType>(count) <= capacity<isLeaf>()) {
            lower->header.count = count;
            if(count == 0)
                return true;
            // TODO: Optimize startInLower == 0: Move pageRef instead of elements
            if(isLeaf)
                copyLeafElements(lower, higher, startInLower, endInHigher, higher->header.count-endInHigher);
            else if(startInLower == 0 && endInHigher == 0) {
                copyKey<false, false>(lowerParent, higherParent, lowerParentIndex, higherParentIndex);
                copyBranchElements<false, -1>(lower, higher, 0, 0, higher->header.count);
            } else if(startInLower == 0) {
                copyKey<false, false>(lowerParent, higher, lowerParentIndex, endInHigher-1);
                copyBranchElements<false, -1>(lower, higher, 0, endInHigher, lower->header.count);
            } else if(endInHigher == 0) {
                copyKey<false, false>(lower, higherParent, startInLower-1, higherParentIndex);
                copyBranchElements<false, -1>(lower, higher, startInLower, 0, higher->header.count);
            } else
                copyBranchElements<true, -1>(lower, higher, startInLower, endInHigher, higher->header.count-endInHigher);
            return true;
        } else {
            assert(startInLower > 0);
            distributeCount(lower, higher, count);
            count = startInLower;
            count -= lower->header.count;
            if(isLeaf) {
                if(count <= 0) {
                    count *= -1;
                    copyLeafElements(lower, higher, startInLower, endInHigher, count);
                    copyLeafElements<-1>(higher, higher, 0, endInHigher+count, higher->header.count);
                } else {
                    if(static_cast<OffsetType>(count) < endInHigher)
                        copyLeafElements<-1>(higher, higher, count, endInHigher, higher->header.count);
                    else
                        copyLeafElements<1>(higher, higher, count, endInHigher, higher->header.count);
                    copyLeafElements(higher, lower, 0, lower->header.count, count);
                }
                copyKey<false, true>(higherParent, higher, higherParentIndex, 0);
            } else {
                if(count <= 0) {
                    count *= -1;
                    if(endInHigher+count == 0)
                        copyKey<false, false>(lower, higherParent, startInLower-1, higherParentIndex);
                    else if(endInHigher == 0 && count > 0) {
                        copyBranchElements<false>(lower, higher, startInLower, 0, count);
                        swapKeyInParent(higherParent, lower, higher, higherParentIndex, startInLower-1, count-1);
                    } else {
                        copyBranchElements<true>(lower, higher, startInLower, endInHigher, count);
                        copyKey<false, false>(higherParent, higher, higherParentIndex, endInHigher+count-1);
                    }
                    copyBranchElements<false, -1>(higher, higher, 0, endInHigher+count, higher->header.count);
                } else {
                    if(endInHigher == 0) {
                        copyBranchElements<false, 1>(higher, higher, count, 0, higher->header.count);
                        swapKeyInParent(higherParent, higher, lower, higherParentIndex, count-1, lower->header.count-1);
                    } else {
                        if(static_cast<OffsetType>(count) < endInHigher)
                            copyBranchElements<true, -1>(higher, higher, count, endInHigher, higher->header.count);
                        else
                            copyBranchElements<true, 1>(higher, higher, count, endInHigher, higher->header.count);
                        copyKey<false, false>(higherParent, lower, higherParentIndex, lower->header.count-1);
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
        assert(count > 0 && count < higher->header.count && lower->header.count+count <= capacity<isLeaf>());
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
        assert(count > 0 && count < lower->header.count && higher->header.count+count <= capacity<isLeaf>());
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
        OffsetType count = lower->header.count+higher->header.count;
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
        if(static_cast<OffsetType>(count) <= capacity<isLeaf>()*2) {
            count /= 2;
            count -= higher->header.count;
            if(count < 0) {
                count *= -1;
                evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
                shiftDown<isLeaf>(higherParent, lower, higher, higherParentIndex, count);
            } else if(static_cast<OffsetType>(count) < middle->header.count) {
                if(count > 0)
                    shiftUp<isLeaf>(higherParent, middle, higher, higherParentIndex, count);
                evacuateDown<isLeaf>(middleParent, lower, middle, middleParentIndex);
            } else if(static_cast<OffsetType>(count) == middle->header.count) {
                evacuateUp<isLeaf>(higherParent, middle, higher, higherParentIndex);
                copyKey<false, false>(higherParent, middleParent, higherParentIndex, middleParentIndex);
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
            assert(lower->header.count == static_cast<OffsetType>((count+1)/2));
            assert(higher->header.count == static_cast<OffsetType>(count/2));
            return true;
        } else {
            count /= 3;
            NativeIntegerType shiftLower = lower->header.count, shiftUpper = higher->header.count;
            shiftLower -= count;
            shiftUpper -= count;
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
