#include <Ontology/Triple.hpp>

void encodeBvlNatural(BitVector& bitVector, NativeNaturalType& dstOffset, NativeNaturalType src) {
    assert(dstOffset <= bitVector.getSize());
    NativeNaturalType srcLength = (src < 2) ? src : BitMask<NativeNaturalType>::ceilLog2(src),
                      dstLength = BitMask<NativeNaturalType>::ceilLog2(srcLength+1),
                      sliceLength = 1;
    Natural8 flagBit = 1;
    dstLength += 1<<dstLength;
    bitVector.increaseSize(dstOffset, dstLength);
    --src;
    NativeNaturalType endOffset = dstOffset+dstLength-1;
    while(dstOffset < endOffset) {
        bitVector.externalOperate<true>(&flagBit, dstOffset++, 1);
        bitVector.externalOperate<true>(&src, dstOffset, sliceLength);
        src >>= sliceLength;
        dstOffset += sliceLength;
        sliceLength <<= 1;
    }
    flagBit = 0;
    bitVector.externalOperate<true>(&flagBit, dstOffset++, 1);
}

NativeNaturalType decodeBvlNatural(BitVector& bitVector, NativeNaturalType& srcOffset) {
    assert(srcOffset < bitVector.getSize());
    NativeNaturalType dstOffset = 0, sliceLength = 1, dst = 0;
    while(true) {
        Natural8 flagBit = 0;
        bitVector.externalOperate<false>(&flagBit, srcOffset++, 1);
        if(!flagBit)
            return (dstOffset == 0) ? dst : dst+1;
        NativeNaturalType buffer = 0;
        bitVector.externalOperate<false>(&buffer, srcOffset, sliceLength);
        dst |= buffer<<dstOffset;
        srcOffset += sliceLength;
        dstOffset += sliceLength;
        sliceLength <<= 1;
        assert(dstOffset < architectureSize);
    }
}
