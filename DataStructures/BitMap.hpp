#include <DataStructures/PairSet.hpp>

template<typename _ParentType = BitVectorContainer>
struct BitMap : public MetaSet<NativeNaturalType, _ParentType> {
    typedef _ParentType ParentType;
    typedef MetaSet<NativeNaturalType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitMap(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    NativeNaturalType getSliceBeginAddress(NativeNaturalType sliceIndex) {
        return Super::getKeyAt(sliceIndex);
    }

    NativeNaturalType getSliceEndAddress(NativeNaturalType sliceIndex) {
        return getSliceBeginAddress(sliceIndex)+Super::getChildLength(sliceIndex);
    }

    template<bool includeFront = false>
    bool getSliceContaining(NativeNaturalType address, NativeNaturalType& sliceIndex) {
        if(Super::findKey(address, sliceIndex) && includeFront)
            return true;
        if(!Super::isEmpty() && sliceIndex > 0 && address < getSliceEndAddress(sliceIndex-1)) {
            --sliceIndex;
            return true;
        }
        return false;
    }

    bool getSliceContaining(NativeNaturalType address, NativeNaturalType length, NativeNaturalType& sliceIndex) {
        if(!getSliceContaining<false>(address, sliceIndex))
            return false;
        return getSliceEndAddress(sliceIndex) <= address+length;
    }

    bool mergeSlices(NativeNaturalType sliceIndex) {
        if(getSliceBeginAddress(sliceIndex+1)-getSliceBeginAddress(sliceIndex) != Super::getChildLength(sliceIndex))
            return false;
        Super::template eraseRange<false>(sliceIndex+1, 1);
        return true;
    }

    NativeNaturalType fillSlice(NativeNaturalType address, NativeNaturalType length) {
        NativeNaturalType endAddress = address+length, sliceIndex,
                          frontSliceIndex, sliceOffset, sliceLength, sliceAddress;
        bool frontSlice = getSliceContaining(address, sliceIndex), backSlice = false;
        if(frontSlice) {
            sliceOffset = address-getSliceBeginAddress(sliceIndex);
            sliceLength = Super::getChildLength(sliceIndex)-sliceOffset;
            sliceOffset += Super::getChildBegin(sliceIndex);
            if(length <= sliceLength)
                return sliceOffset;
            frontSliceIndex = sliceIndex++;
        }
        while(sliceIndex < Super::getElementCount()) {
            if(endAddress < getSliceEndAddress(sliceIndex)) {
                sliceAddress = getSliceBeginAddress(sliceIndex);
                backSlice = (endAddress > sliceAddress);
                break;
            }
            Super::eraseElementAt(sliceIndex);
            // TODO: Optimize, recyle this slice if !frontSlice
        }
        if(frontSlice) {
            length -= sliceLength;
            if(backSlice)
                length -= endAddress-sliceAddress;
            Super::increaseChildLength(frontSliceIndex, sliceOffset+sliceLength, length);
            if(backSlice)
                Super::template eraseRange<false>(sliceIndex, 1);
        } else {
            if(backSlice) {
                length = endAddress-sliceAddress;
                Super::setKeyAt(sliceIndex, address);
            } else
                Super::insertElementAt(sliceIndex, address);
            sliceOffset = Super::getChildBegin(sliceIndex);
            Super::increaseChildLength(sliceIndex, sliceOffset, length);
        }
        if(!backSlice && sliceIndex+1 < Super::getElementCount())
            mergeSlices(sliceIndex);
        if(!frontSlice && sliceIndex > 0)
            mergeSlices(sliceIndex-1);
        return sliceOffset;
    }

    void clearSlice(NativeNaturalType address, NativeNaturalType length) {
        NativeNaturalType endAddress = address+length, sliceIndex;
        if(getSliceContaining(address, sliceIndex)) {
            NativeNaturalType sliceOffset = address-getSliceBeginAddress(sliceIndex),
                              sliceLength = Super::getChildLength(sliceIndex)-sliceOffset;
            bool splitFrontSlice = (sliceLength > length);
            if(splitFrontSlice) {
                sliceLength = length;
                Super::insertElementAt(sliceIndex+1, endAddress);
            }
            sliceOffset += Super::getChildBegin(sliceIndex);
            Super::decreaseChildLength(sliceIndex, sliceOffset, sliceLength);
            if(splitFrontSlice) {
                Super::setValueAt(sliceIndex+1, sliceOffset);
                return;
            }
            ++sliceIndex;
        }
        while(sliceIndex < Super::getElementCount()) {
            if(endAddress < getSliceEndAddress(sliceIndex)) {
                NativeNaturalType sliceAddress = getSliceBeginAddress(sliceIndex);
                if(endAddress > sliceAddress) {
                    Super::decreaseChildLength(sliceIndex, Super::getChildBegin(sliceIndex), endAddress-sliceAddress);
                    Super::setKeyAt(sliceIndex, endAddress);
                }
                break;
            }
            Super::eraseElementAt(sliceIndex);
        }
    }

    bool moveSlice(NativeNaturalType dstAddress, NativeNaturalType srcAddress, NativeNaturalType length) {
        bool downward = (dstAddress < srcAddress);
        NativeNaturalType sliceIndex, eraseLength = length, eraseAddress = dstAddress, addressDiff = downward ? srcAddress-dstAddress : dstAddress-srcAddress;
        if(length < addressDiff) {
            NativeNaturalType beginAddress = downward ? dstAddress : srcAddress,
                              endAddress = beginAddress+addressDiff;
            beginAddress += length;
            if(getSliceContaining<true>(beginAddress, sliceIndex) || (sliceIndex < Super::getElementCount() && getSliceBeginAddress(sliceIndex) < endAddress))
                return false;
        } else if(length > addressDiff) {
            eraseLength = addressDiff;
            if(!downward)
                eraseAddress = dstAddress+addressDiff;
        }
        clearSlice(eraseAddress, eraseLength);
        if(downward) {
            Super::findKey(srcAddress, sliceIndex);
            if(getSliceEndAddress(sliceIndex) > srcAddress+length) {
                NativeNaturalType spareLength = getSliceEndAddress(sliceIndex)-(srcAddress+length);
                Super::insertElementAt(sliceIndex+1, getSliceBeginAddress(sliceIndex)+spareLength);
                Super::setValueAt(sliceIndex+1, Super::getChildBegin(sliceIndex)+spareLength);
            }
        } else {
            if(getSliceContaining<true>(srcAddress, sliceIndex) && getSliceBeginAddress(sliceIndex) < srcAddress) {
                NativeNaturalType spareLength = srcAddress-getSliceBeginAddress(sliceIndex);
                Super::insertElementAt(sliceIndex+1, getSliceEndAddress(sliceIndex)-spareLength);
                Super::setValueAt(sliceIndex+1, Super::getChildEnd(sliceIndex)-spareLength);
                ++sliceIndex;
            }
        }
        NativeNaturalType lowestSliceIndex = sliceIndex;
        while(sliceIndex < Super::getElementCount()) {
            NativeNaturalType sliceAddress = getSliceBeginAddress(sliceIndex);
            if(sliceAddress >= srcAddress+length)
                break;
            Super::setKeyAt(sliceIndex++, downward ? sliceAddress-addressDiff : sliceAddress+addressDiff);
        }
        if(downward) {
            if(lowestSliceIndex > 0)
                mergeSlices(lowestSliceIndex-1);
        } else {
            if(sliceIndex < Super::getElementCount())
                mergeSlices(sliceIndex-1);
        }
        return true;
    }
};
