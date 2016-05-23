template<bool enableCopyOnWrite, typename FrameType = IteratorFrame>
struct Iterator {
    LayerType end;
    FrameType stack[maxLayerCount];

    FrameType* operator[](LayerType layer) {
        return &stack[layer];
    }

    bool getParentFrame(LayerType layer, Page*& parent, OffsetType& parentIndex) {
        while(++layer < end) {
            FrameType* frame = (*this)[layer];
            if(frame->index > 0) {
                parent = getPage(frame->pageRef);
                parentIndex = frame->index-1;
                return true;
            }
        }
        parent = nullptr;
        parentIndex = 0;
        return false;
    }

    bool isValid() {
        for(LayerType layer = 0; layer < end; ++layer)
            if((*this)[layer]->index >= (*this)[layer]->endIndex)
                return false;
        return true;
    }

    template<bool srcEnableCopyOnWrite>
    void copy(Iterator<srcEnableCopyOnWrite>& src) {
        static_assert(!enableCopyOnWrite || srcEnableCopyOnWrite);
        end = src.end;
        for(LayerType layer = 0; layer < src.end; ++layer)
            Storage::bitwiseCopy<-1>(reinterpret_cast<NativeNaturalType*>((*this)[layer]),
                                     reinterpret_cast<const NativeNaturalType*>(src[layer]),
                                     0, 0, sizeOfInBits<IteratorFrame>::value);
    }

    NativeIntegerType compare(Iterator& other) {
        LayerType layer = end-1;
        while(true) {
            if((*this)[layer]->index < other[layer]->index)
                return -1;
            if((*this)[layer]->index < other[layer]->index)
                return 1;
            if(layer == 0)
                return 0;
            --layer;
        }
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
        parentPage->setPageRef(parentFrame->index, frame->pageRef);
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
            keepRunning = false;
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
                layer = page->header.layer;
                LayerType endLayer = (atLayer) ? atLayer-1 : 0;
                while(layer > endLayer) {
                    descend(frame, page, layer);
                    frame->endIndex = page->header.count;
                    frame->index = (dir == 1) ? 0 : frame->endIndex-1;
                    if(pageTouchCallback)
                        pageTouchCallback(page);
                }
            }
        } while(keepRunning);
        return steps;
    }

    KeyType getRank() {
        static_assert(rankBits);
        FrameType* frame = (*this)[0];
        return frame->rank+frame->index;
    }

    KeyType getKey() {
        static_assert(keyBits);
        FrameType* frame = (*this)[0];
        return getPage(frame->pageRef)->template getKey<true>(frame->index);
    }

    void setKey(KeyType key) {
        static_assert(keyBits && enableCopyOnWrite);
        FrameType* frame = (*this)[0];
        getPage(frame->pageRef)->template setKey<true>(frame->index, key);
    }
};

template<FindMode mode, bool enableCopyOnWrite>
bool find(Iterator<enableCopyOnWrite>& iter,
          typename conditional<mode == Rank, RankType, KeyType>::type keyOrRank = 0,
          Closure<void(Page*)> pageTouchCallback = nullptr) {
    static_assert(mode != Key || keyBits);
    static_assert(mode != Rank || rankBits);
    if(empty()) {
        iter.end = 0;
        return false;
    }
    bool result;
    Page* page = getPage<enableCopyOnWrite>(rootPageRef);
    LayerType layer = page->header.layer;
    iter.end = layer+1;
    auto frame = iter[layer];
    frame->rank = 0;
    frame->pageRef = rootPageRef;
    while(true) {
        frame->endIndex = page->header.count;
        if(layer == 0) {
            switch(mode) {
                case First:
                    frame->index = 0;
                    result = true;
                    break;
                case Last:
                    frame->index = frame->endIndex-1;
                    result = true;
                    break;
                case Key:
                    frame->index = binarySearch<OffsetType>(page->template keyCount<true>(), [&](OffsetType at) {
                        return static_cast<KeyType>(keyOrRank) > page->template getKey<true>(at);
                    });
                    result = frame->index < page->header.count && static_cast<KeyType>(keyOrRank) == page->template getKey<true>(frame->index);
                    break;
                case Rank:
                    frame->index = static_cast<RankType>(keyOrRank)-frame->rank;
                    result = frame->index < page->header.count;
                    break;
            }
            if(pageTouchCallback)
                pageTouchCallback(page);
            return result;
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
                        return static_cast<KeyType>(keyOrRank) >= page->template getKey<false>(at);
                    });
                    break;
                case Rank:
                    frame->index = binarySearch<OffsetType>(page->template keyCount<false>(), [&](OffsetType at) {
                        return static_cast<RankType>(keyOrRank)-frame->rank >= page->getRank(at);
                    });
                    break;
            }
            if(pageTouchCallback)
                pageTouchCallback(page);
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
