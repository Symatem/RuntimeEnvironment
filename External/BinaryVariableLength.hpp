#include <Ontology/Triple.hpp>

void encodeBvlNatural(Blob& blob, NativeNaturalType& dstOffset, NativeNaturalType src) {
    assert(dstOffset <= blob.getSize());
    NativeNaturalType srcLength = BitMask<NativeNaturalType>::ceilLog2((src < 2) ? src : src-1),
                      dstLength = BitMask<NativeNaturalType>::ceilLog2(srcLength),
                      sliceLength = 1;
    Natural8 flagBit = 1;
    dstLength += 1<<dstLength;
    blob.increaseSize(dstOffset, dstLength);
    --src;
    NativeNaturalType endOffset = dstOffset+dstLength-1;
    while(dstOffset < endOffset) {
        blob.externalOperate<true>(&flagBit, dstOffset++, 1);
        blob.externalOperate<true>(&src, dstOffset, sliceLength);
        src >>= sliceLength;
        dstOffset += sliceLength;
        sliceLength <<= 1;
    }
    flagBit = 0;
    blob.externalOperate<true>(&flagBit, dstOffset++, 1);
}

NativeNaturalType decodeBvlNatural(Blob& blob, NativeNaturalType& srcOffset) {
    assert(srcOffset < blob.getSize());
    NativeNaturalType dstOffset = 0, sliceLength = 1, dst = 0;
    while(true) {
        Natural8 flagBit = 0;
        blob.externalOperate<false>(&flagBit, srcOffset++, 1);
        if(!flagBit)
            return (dstOffset == 0) ? dst : dst+1;
        NativeNaturalType buffer = 0;
        blob.externalOperate<false>(&buffer, srcOffset, sliceLength);
        dst |= buffer<<dstOffset;
        srcOffset += sliceLength;
        dstOffset += sliceLength;
        sliceLength <<= 1;
        assert(dstOffset < architectureSize);
    }
}
