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

    template<bool includeBegin, bool includeEnd>
    bool getSliceContaining(NativeNaturalType address, NativeNaturalType& sliceIndex) {
        if(Super::findKey(address, sliceIndex) && includeBegin)
            return true;
        if(!Super::isEmpty() && sliceIndex > 0) {
            NativeNaturalType endAddress = getSliceEndAddress(sliceIndex-1);
            if(includeEnd)
                ++endAddress;
            if(address < endAddress) {
                --sliceIndex;
                return true;
            }
        }
        return false;
    }

    bool getSliceContaining(NativeNaturalType address, NativeNaturalType length, NativeNaturalType& sliceIndex) {
        if(!getSliceContaining<true, false>(address, sliceIndex))
            return false;
        return getSliceEndAddress(sliceIndex) <= address+length;
    }

    NativeIntegerType fillSlice(NativeNaturalType address, NativeNaturalType length, NativeNaturalType& sliceOffset) {
        NativeIntegerType slicesAdded = 0;
        NativeNaturalType endAddress = address+length, sliceIndex,
                          frontSliceIndex, sliceLength, sliceAddress;
        bool frontSlice = getSliceContaining<false, true>(address, sliceIndex), backSlice = false;
        if(frontSlice) {
            sliceOffset = address-getSliceBeginAddress(sliceIndex);
            sliceLength = Super::getChildLength(sliceIndex)-sliceOffset;
            sliceOffset += Super::getChildBegin(sliceIndex);
            if(length <= sliceLength)
                return slicesAdded;
            frontSliceIndex = sliceIndex++;
        }
        while(sliceIndex < Super::getElementCount()) {
            if(endAddress < getSliceEndAddress(sliceIndex)) {
                sliceAddress = getSliceBeginAddress(sliceIndex);
                backSlice = (sliceAddress <= endAddress);
                break;
            }
            Super::eraseElementAt(sliceIndex);
            --slicesAdded;
            // TODO: Optimize, recyle this slice if !frontSlice
        }
        if(frontSlice) {
            length -= sliceLength;
            if(backSlice)
                length -= endAddress-sliceAddress;
            Super::increaseSize(sliceOffset+sliceLength, length, frontSliceIndex);
            if(backSlice) {
                Super::template eraseRange<false>(sliceIndex, 1);
                --slicesAdded;
            }
        } else {
            if(backSlice) {
                length -= endAddress-sliceAddress;
                Super::setKeyAt(sliceIndex, address);
            } else {
                Super::insertElementAt(sliceIndex, address);
                ++slicesAdded;
            }
            sliceOffset = Super::getChildBegin(sliceIndex);
            Super::increaseSize(sliceOffset, length, sliceIndex);
        }
        return slicesAdded;
    }

    NativeIntegerType clearSlice(NativeNaturalType address, NativeNaturalType length) {
        NativeIntegerType slicesAdded = 0;
        NativeNaturalType endAddress = address+length, sliceIndex;
        if(getSliceContaining<false, false>(address, sliceIndex)) {
            NativeNaturalType sliceOffset = address-getSliceBeginAddress(sliceIndex),
                              sliceLength = Super::getChildLength(sliceIndex)-sliceOffset;
            bool splitFrontSlice = (sliceLength > length);
            if(splitFrontSlice) {
                sliceLength = length;
                Super::insertElementAt(sliceIndex+1, endAddress);
                ++slicesAdded;
            }
            sliceOffset += Super::getChildBegin(sliceIndex);
            Super::decreaseSize(sliceOffset, sliceLength, sliceIndex);
            if(splitFrontSlice) {
                Super::setValueAt(sliceIndex+1, sliceOffset);
                return slicesAdded;
            }
            ++sliceIndex;
        }
        while(sliceIndex < Super::getElementCount()) {
            if(endAddress < getSliceEndAddress(sliceIndex)) {
                NativeNaturalType sliceAddress = getSliceBeginAddress(sliceIndex);
                if(endAddress > sliceAddress) {
                    Super::decreaseSize(Super::getChildBegin(sliceIndex), endAddress-sliceAddress, sliceIndex);
                    Super::setKeyAt(sliceIndex, endAddress);
                }
                break;
            }
            Super::eraseElementAt(sliceIndex);
            --slicesAdded;
        }
        return slicesAdded;
    }

    void copySlice(BitMap& src, NativeNaturalType dstAddress, NativeNaturalType srcAddress, NativeNaturalType length) {
        if(dstAddress < srcAddress) {
            NativeNaturalType sliceIndex, lastEndAddress = srcAddress;
            getSliceContaining<true, false>(lastEndAddress, sliceIndex);
            while(sliceIndex < Super::getElementCount()) {
                NativeNaturalType beginAddress = getSliceBeginAddress(sliceIndex);
                if(beginAddress > srcAddress+length)
                    break;
                NativeNaturalType fillLength = Super::getChildLength(sliceIndex),
                                  endAddress = beginAddress+fillLength;
                NativeIntegerType slicesAdded = 0;
                if(beginAddress > lastEndAddress)
                    slicesAdded += clearSlice(dstAddress-srcAddress+lastEndAddress, beginAddress-lastEndAddress);
                lastEndAddress = endAddress;
                if(srcAddress+length < endAddress)
                    fillLength -= endAddress-srcAddress-length;
                NativeNaturalType fillOffset = (beginAddress < srcAddress) ? srcAddress-beginAddress : 0;
                fillLength -= fillOffset;
                NativeNaturalType dstOffset;
                slicesAdded += fillSlice(dstAddress-srcAddress+beginAddress+fillOffset, fillLength, dstOffset);
                if(src == *this)
                    sliceIndex += slicesAdded;
                Super::getBitVector().interoperation(Super::getBitVector(), dstOffset, Super::getChildOffset(sliceIndex)+fillOffset, fillLength);
                ++sliceIndex;
            }
            if(srcAddress+length > lastEndAddress)
                clearSlice(dstAddress-srcAddress+lastEndAddress, srcAddress+length-lastEndAddress);
        } else {
            NativeNaturalType sliceIndex, lastBeginAddress = srcAddress+length;
            Super::findKey(lastBeginAddress-1, sliceIndex);
            while(sliceIndex > 0) {
                --sliceIndex;
                NativeNaturalType fillLength = Super::getChildLength(sliceIndex),
                                  beginAddress = getSliceBeginAddress(sliceIndex),
                                  endAddress = beginAddress+fillLength;
                if(endAddress < srcAddress)
                    break;
                if(lastBeginAddress > endAddress)
                    clearSlice(dstAddress-srcAddress+endAddress, lastBeginAddress-endAddress);
                lastBeginAddress = beginAddress;
                if(srcAddress+length < endAddress)
                    fillLength -= endAddress-srcAddress-length;
                NativeNaturalType fillOffset = (beginAddress < srcAddress) ? srcAddress-beginAddress : 0;
                fillLength -= fillOffset;
                NativeNaturalType dstOffset;
                fillSlice(dstAddress-srcAddress+beginAddress+fillOffset, fillLength, dstOffset);
                Super::getBitVector().interoperation(Super::getBitVector(), dstOffset, Super::getChildOffset(sliceIndex)+fillOffset, fillLength);
            }
            if(lastBeginAddress > srcAddress)
                clearSlice(dstAddress, lastBeginAddress-srcAddress);
        }
    }

    bool mergeSlices(NativeNaturalType sliceIndex) {
        if(getSliceBeginAddress(sliceIndex+1)-getSliceBeginAddress(sliceIndex) != Super::getChildLength(sliceIndex))
            return false;
        Super::template eraseRange<false>(sliceIndex+1, 1);
        return true;
    }

    bool moveSlice(NativeNaturalType dstAddress, NativeNaturalType srcAddress, NativeNaturalType length) {
        bool downward = (dstAddress < srcAddress);
        NativeNaturalType sliceIndex, eraseLength = length, eraseAddress = dstAddress, addressDiff = downward ? srcAddress-dstAddress : dstAddress-srcAddress;
        if(length < addressDiff) {
            NativeNaturalType beginAddress = downward ? dstAddress : srcAddress,
                              endAddress = beginAddress+addressDiff;
            beginAddress += length;
            if(getSliceContaining<true, false>(beginAddress, sliceIndex) || (sliceIndex < Super::getElementCount() && getSliceBeginAddress(sliceIndex) < endAddress))
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
            if(getSliceContaining<true, false>(srcAddress, sliceIndex) && getSliceBeginAddress(sliceIndex) < srcAddress) {
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
