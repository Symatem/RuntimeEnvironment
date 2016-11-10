#include <Storage/Basics.hpp>

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
    typedef Natural8 LayerType;

    static const NativeNaturalType
        keyBits = sizeOfInBits<KeyType>::value,
        rankBits = sizeOfInBits<RankType>::value,
        pageRefBits = sizeOfInBits<PageRefType>::value;
    static_assert(keyBits || rankBits);
    static_assert(pageRefBits);
    static_assert(!rankBits || rankBits >= sizeOfInBits<OffsetType>::value);

    PageRefType rootPageRef;

    void init() {
        rootPageRef = 0;
    }

    bool empty() const {
        return (rootPageRef == 0);
    }

    LayerType getLayerCount() {
        if(empty())
            return 0;
        return getPage(rootPageRef)->header.layer+1;
    }

    RankType getElementCount() {
        static_assert(rankBits);
        if(empty())
            return 0;
        return getPage(rootPageRef)->getIntegratedRank();
    }

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

#include <Storage/BpPage.hpp>
    static const LayerType maxLayerCount = Page::layersNeeded();
#include <Storage/BpIterator.hpp>

    template<bool enableModification = false>
    static Page* getPage(typename conditional<enableModification, PageRefType&, PageRefType>::type pageRef) {
        return Storage::dereferencePage<Page>(pageRef);
    }

    void generateStats(struct Stats& stats, Closure<void(Iterator<false>&)> callback = nullptr) {
        if(empty())
            return;
        Iterator<false> iter;
        NativeNaturalType branchPageCount = 0, leafPageCount = 0;
        auto pageTouch = [&](Page* page) {
            if(page->header.layer == 0) {
                ++leafPageCount;
                stats.inhabitedPayload += (keyBits+valueBits)*page->header.count;
                if(callback)
                    for(iter[0]->index = 0; iter[0]->index < iter[0]->endIndex; ++iter[0]->index)
                        callback(iter);
            } else {
                ++branchPageCount;
                stats.inhabitedMetaData += (keyBits+rankBits+pageRefBits)*page->header.count+rankBits+pageRefBits;
            }
        };
        find<First>(iter, 0, pageTouch);
        while(iter.template advance<1>(1, 1, pageTouch) == 0);
        NativeNaturalType uninhabitable = Page::valueOffset-keyBits*Page::leafKeyCount-Page::headerBits;
        stats.uninhabitable += uninhabitable*leafPageCount;
        stats.totalPayload += (Storage::bitsPerPage-uninhabitable-Page::headerBits)*leafPageCount;
        stats.totalMetaData += Page::headerBits*leafPageCount;
        stats.inhabitedMetaData += Page::headerBits*leafPageCount;
        uninhabitable = Page::keyOffset+Page::pageRefOffset-Page::rankOffset-rankBits*(Page::branchKeyCount+1)-Page::headerBits;
        stats.uninhabitable += uninhabitable*branchPageCount;
        stats.totalMetaData += (Storage::bitsPerPage-uninhabitable)*branchPageCount;
        stats.inhabitedMetaData += Page::headerBits*branchPageCount;
    }

    struct InsertData {
        LayerType layer;
        NativeNaturalType elementCount;
        OffsetType lowerInnerParentIndex, higherOuterParentIndex;
        Page *lowerInnerParent, *higherOuterParent;
    };

    template<bool isLeaf>
    static bool insertPhase1(InsertData& data, Iterator<true, InsertIteratorFrame>& iter) {
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
            if(!isLeaf) {
                lowerOuter->disintegrateRanks(frame->index, lowerOuter->header.count);
                frame->rank = frame->index++;
            }
            if(frame->pageCount == 0) {
                frame->higherOuterPageRef = 0;
                Page::template insert<isLeaf>(frame, lowerOuter, data.elementCount);
            } else {
                frame->endIndex = data.elementCount-(frame->pageCount-1)*frame->elementsPerPage;
                frame->higherOuterPageRef = Storage::acquirePage();
                switch(frame->pageCount) {
                    case 1:
                        frame->lowerInnerPageRef = frame->higherOuterPageRef;
                        frame->higherInnerPageRef = frame->pageRef;
                        break;
                    case 2:
                        frame->lowerInnerPageRef = Storage::acquirePage();
                        frame->higherInnerPageRef = frame->lowerInnerPageRef;
                        break;
                    default:
                        frame->lowerInnerPageRef = Storage::acquirePage();
                        frame->higherInnerPageRef = Storage::acquirePage();
                        break;
                }
            }
        } else {
            frame->pageRef = Storage::acquirePage();
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
                    frame->higherInnerPageRef = Storage::acquirePage();
                    frame->higherInnerEndIndex = frame->elementsPerPage;
                    Page* higherInner = getPage(frame->higherInnerPageRef);
                    higherInner->header.count = frame->elementsPerPage;
                    higherInner->header.layer = data.layer;
                }
                frame->higherOuterPageRef = Storage::acquirePage();
                Page* higherOuter = getPage(frame->higherOuterPageRef);
                higherOuter->header.layer = data.layer;
                Page::distributeCount(lowerOuter, higherOuter, data.elementCount-(frame->pageCount-1)*frame->elementsPerPage);
                frame->higherOuterEndIndex = higherOuter->header.count;
            }
            frame->rank = 0;
            frame->index = (isLeaf) ? 0 : 1;
            frame->endIndex = lowerOuter->header.count;
        }
        data.elementCount = pageCount;
        ++data.layer;
        assert(data.layer < maxLayerCount);
        return true;
    }

    template<bool isLeaf>
    static void insertPhase2(InsertData& data, Iterator<true, InsertIteratorFrame>& iter) {
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

    template<bool isLeaf>
    Page* insertAdvance(InsertData& data, InsertIteratorFrame* frame) {
        assert(frame->pageCount > 0);
        --frame->pageCount;
        if(frame->higherOuterPageRef) {
            if(frame->lowerInnerPageRef) {
                frame->pageRef = frame->lowerInnerPageRef;
                Page* page = getPage(frame->pageRef);
                frame->index = frame->lowerInnerIndex;
                frame->rank = (frame->index) ? frame->index-1 : 0;
                frame->endIndex = (frame->lowerInnerPageRef == frame->higherOuterPageRef) ? frame->higherOuterEndIndex : frame->elementsPerPage;
                frame->lowerInnerPageRef = 0;
                return page;
            } else if(frame->pageCount == 1) {
                frame->pageRef = frame->higherInnerPageRef;
                Page* page = getPage(frame->pageRef);
                frame->rank = frame->index = 0;
                frame->endIndex = frame->higherInnerEndIndex;
                return page;
            } else if(frame->pageCount == 0) {
                frame->pageRef = frame->higherOuterPageRef;
                Page* page = getPage(frame->pageRef);
                frame->rank = frame->index = 0;
                frame->endIndex = frame->higherOuterEndIndex;
                return page;
            }
        }
        frame->pageRef = Storage::acquirePage();
        frame->rank = frame->index = 0;
        frame->endIndex = frame->elementsPerPage;
        Page* page = getPage(frame->pageRef);
        page->header.count = frame->endIndex;
        page->header.layer = data.layer;
        return page;
    }

    static void insertIntegrateRanks(InsertIteratorFrame* frame, Page* page) {
        if(!rankBits)
            return;
        for(OffsetType i = frame->rank; i < frame->endIndex; ++i)
            page->setRank(i, getPage(page->getPageRef(i))->getIntegratedRank());
        page->integrateRanks(frame->rank, page->header.count);
    }

    typedef Closure<void(Page*, OffsetType, OffsetType)> AcquireData;
    void insert(Iterator<true>& _iter, NativeNaturalType n, AcquireData acquireData) {
        assert(n > 0);
        InsertData data = {0, n};
        Iterator<true, InsertIteratorFrame> iter;
        iter.copy(_iter);
        insertPhase1<true>(data, iter);
        while(insertPhase1<false>(data, iter));
        LayerType unmodifiedLayer = data.layer;
        data.layer = min(iter.end, unmodifiedLayer);
        iter.end = max(iter.end, unmodifiedLayer);
        rootPageRef = iter[iter.end-1]->pageRef;
        while(data.layer > 0) {
            InsertIteratorFrame* frame = iter[--data.layer];
            if(frame->higherOuterPageRef) {
                frame = iter[++data.layer];
                assert(frame->endIndex > 1);
                assert(frame->index > 0 && frame->index < frame->endIndex);
                data.lowerInnerParent = data.higherOuterParent = getPage(frame->pageRef);
                data.lowerInnerParentIndex = frame->index;
                data.higherOuterParentIndex = frame->endIndex-1;
                while(--data.layer > 0)
                    insertPhase2<false>(data, iter);
                insertPhase2<true>(data, iter);
                break;
            }
        }
        PageRefType pageRef;
        Page* leafPage = getPage(iter[0]->pageRef);
        if(acquireData && iter[0]->index < iter[0]->endIndex)
            acquireData(leafPage, iter[0]->index, iter[0]->endIndex);
        while(iter[0]->pageCount > 0) {
            data.layer = 0;
            leafPage = insertAdvance<true>(data, iter[0]);
            if(acquireData && iter[0]->index < iter[0]->endIndex)
                acquireData(leafPage, iter[0]->index, iter[0]->endIndex);
            bool setKey = true;
            pageRef = iter[0]->pageRef;
            while(data.layer < unmodifiedLayer) {
                InsertIteratorFrame* frame = iter[++data.layer];
                Page* page = getPage(frame->pageRef);
                if(frame->index < frame->endIndex) {
                    if(setKey)
                        Page::template copyKey<false, true>(page, leafPage, frame->index-1, 0);
                    page->setPageRef(frame->index++, pageRef);
                    break;
                }
                insertIntegrateRanks(frame, page);
                page = insertAdvance<false>(data, frame);
                if(setKey && frame->index > 0) {
                    Page::template copyKey<false, true>(page, leafPage, frame->index-1, 0);
                    setKey = false;
                }
                page->setPageRef(frame->index++, pageRef);
                pageRef = frame->pageRef;
            }
        }
        pageRef = 0;
        for(data.layer = 1; data.layer < unmodifiedLayer; ++data.layer) {
            InsertIteratorFrame* frame = iter[data.layer];
            Page* page = getPage(frame->pageRef);
            if(pageRef && frame->index < frame->endIndex) {
                page->setPageRef(frame->index++, pageRef);
                pageRef = 0;
            }
            insertIntegrateRanks(frame, page);
            if(frame->pageCount > 0) {
                assert(frame->higherOuterPageRef);
                Page* page = insertAdvance<false>(data, frame);
                if(pageRef)
                    page->setPageRef(frame->index++, pageRef);
                insertIntegrateRanks(frame, page);
                pageRef = frame->pageRef;
            } else
                pageRef = 0;
        }
        if(rankBits)
            for(data.layer = unmodifiedLayer; data.layer < iter.end; ++data.layer) {
                InsertIteratorFrame* frame = iter[data.layer];
                frame->rank = frame->index;
                frame->endIndex = frame->index+1;
                Page* page = getPage(frame->pageRef);
                page->disintegrateRanks(frame->rank, page->header.count);
                insertIntegrateRanks(frame, page);
            }
    }

    struct EraseData {
        bool spareLowerInner, eraseHigherInner;
        LayerType layer;
        Iterator<true> &from, &to, iter;
        OffsetType outerParentIndex[2];
        Page* outerParent[2];
        RankType rank[4];
    };

    static void eraseUpdateRank(EraseData& data, NativeNaturalType rankIndex, Page* candidate) {
        if(data.rank[rankIndex] && candidate == data.outerParent[rankIndex])
            data.outerParent[rankIndex]->setRank(data.outerParentIndex[rankIndex], data.rank[rankIndex]);
    }

    template<bool isLeaf, NativeIntegerType dir>
    static Page* eraseAdvance(EraseData& data, Page*& parent, OffsetType& parentIndex, bool condition) {
        if(!condition)
            return nullptr;
        NativeNaturalType rankIndex = (dir+1)/2;
        data.iter.copy((dir == -1) ? data.from : data.to);
        if(data.iter.template advance<dir>(data.layer+1) == 0) {
            ((dir == -1) ? data.from : data.iter).getParentFrame(data.layer, parent, parentIndex);
            Page* page = getPage(data.iter[data.layer]->pageRef);
            if(rankBits && !isLeaf) {
                page->disintegrateRanks(0, page->header.count);
                if(data.rank[rankIndex] && page == data.outerParent[rankIndex])
                    page->setRank(data.outerParentIndex[rankIndex], data.rank[rankIndex]);
                eraseUpdateRank(data, rankIndex, page);
            }
            data.outerParent[rankIndex] = getPage(data.iter[data.layer+1]->pageRef);
            data.outerParentIndex[rankIndex] = data.iter[data.layer+1]->index;
            return page;
        }
        return nullptr;
    }

    template<bool isLeaf>
    void eraseEmptyLayer(EraseData& data, Page*& lowerInner) {
        if(isLeaf) {
            if(lowerInner->header.count > 0)
                return;
            init();
        } else if(lowerInner->header.count == 1)
            rootPageRef = lowerInner->getPageRef(0);
        else if(lowerInner->header.count > 1)
            return;
        Storage::releasePage(data.from[data.layer]->pageRef);
        data.spareLowerInner = false;
        data.eraseHigherInner = true;
        lowerInner = nullptr;
    }

    template<bool isLeaf>
    bool eraseLayer(EraseData& data) {
        OffsetType lowerInnerIndex = data.from[data.layer]->index+data.spareLowerInner, lowerInnerParentIndex,
                   higherInnerIndex = data.to[data.layer]->index+data.eraseHigherInner, higherInnerParentIndex;
        Page *lowerInner = getPage(data.from[data.layer]->pageRef), *lowerInnerParent,
             *higherInner = getPage(data.to[data.layer]->pageRef), *higherInnerParent;
        bool keepRunning = true, ranksFromBelow = rankBits && !isLeaf;
        if(ranksFromBelow) {
            lowerInner->disintegrateRanks(0, lowerInner->header.count);
            if(lowerInner != higherInner)
                higherInner->disintegrateRanks(0, higherInner->header.count);
            eraseUpdateRank(data, 0, lowerInner);
            eraseUpdateRank(data, 1, higherInner);
            if(data.rank[2])
                lowerInner->setRank(data.from[data.layer]->index, data.rank[2]);
            if(data.rank[3])
                higherInner->setRank(data.to[data.layer]->index, data.rank[3]);
        }
        data.spareLowerInner = true;
        data.eraseHigherInner = false;
        data.from.getParentFrame(data.layer, lowerInnerParent, lowerInnerParentIndex);
        if(lowerInner == higherInner) {
            higherInner = nullptr;
            if(lowerInnerIndex < higherInnerIndex) {
                Page::template erase1<isLeaf>(lowerInnerParent, lowerInner, lowerInnerParentIndex, lowerInnerIndex, higherInnerIndex);
                if(data.layer == data.from.end-1) {
                    eraseEmptyLayer<isLeaf>(data, lowerInner);
                    keepRunning = false;
                }
            } else
                keepRunning = false;
        } else {
            data.to.getParentFrame(data.layer, higherInnerParent, higherInnerParentIndex);
            if(Page::template erase2<isLeaf>(lowerInnerParent, higherInnerParent, lowerInner, higherInner,
                                             lowerInnerParentIndex, higherInnerParentIndex, lowerInnerIndex, higherInnerIndex)) {
                Storage::releasePage(data.to[data.layer]->pageRef);
                data.eraseHigherInner = true;
                higherInner = nullptr;
            }
            data.iter.copy(data.to);
            while(data.iter.template advance<-1>(data.layer+1) == 0 &&
                  data.iter[data.layer]->pageRef != data.from[data.layer]->pageRef)
                Storage::releasePage(data.iter[data.layer]->pageRef);
        }
        OffsetType lowerInnerKeyParentIndex, higherOuterKeyParentIndex;
        Page *lowerInnerKeyParent, *higherOuterKeyParent, *lowerOuter, *higherOuter;
        bool redistribution = keepRunning && lowerInner->header.count < Page::template capacity<isLeaf>()/2;
        lowerOuter = eraseAdvance<isLeaf, -1>(data, lowerInnerKeyParent, lowerInnerKeyParentIndex, redistribution || (ranksFromBelow && data.rank[0]));
        higherOuter = eraseAdvance<isLeaf, 1>(data, higherOuterKeyParent, higherOuterKeyParentIndex, redistribution || (ranksFromBelow && data.rank[1]));
        if(redistribution) {
            if(lowerOuter || higherOuter) {
                if(lowerInner->header.count == 0 ||
                   Page::template redistribute<isLeaf>(lowerInnerKeyParent, higherOuterKeyParent,
                                                       lowerOuter, lowerInner, higherOuter,
                                                       lowerInnerKeyParentIndex, higherOuterKeyParentIndex)) {
                    Storage::releasePage(data.from[data.layer]->pageRef);
                    data.spareLowerInner = false;
                    data.eraseHigherInner = true;
                    lowerInner = nullptr;
                }
            } else
                eraseEmptyLayer<isLeaf>(data, lowerInner);
        }
        ++data.layer;
        if(!rankBits)
            return keepRunning;
        Page* pages[] = {lowerOuter, higherOuter, lowerInner, higherInner};
        for(NativeNaturalType rankIndex = 0; rankIndex < 4; ++rankIndex)
            if(pages[rankIndex]) {
                if(!isLeaf)
                    pages[rankIndex]->integrateRanks(0, pages[rankIndex]->header.count);
                data.rank[rankIndex] = pages[rankIndex]->getIntegratedRank();
            } else
                data.rank[rankIndex] = static_cast<RankType>(0);
        return data.layer < data.from.end;
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

    void erase() {
        if(empty())
            return;
        Iterator<true> from, to;
        find<First>(from);
        find<Last>(to);
        erase(from, to);
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
