#include "Deserialize.hpp"

#define PreDefProcedure(Name) void PreDefProcedure_##Name(Thread& thread)

PreDefProcedure(Search) {
    Triple triple;
    NativeNaturalType modes[3] = {2, 2, 2}, varyingCount = 0;
    Symbol posNames[3] = {PreDef_Entity, PreDef_Attribute, PreDef_Value};
    Ontology::query(9, {thread.block, PreDef_Varying, PreDef_Void}, [&](Triple result) {
        for(NativeNaturalType index = 0; index < 3; ++index)
            if(result.pos[0] == posNames[index]) {
                modes[index] = 1;
                ++varyingCount;
                return;
            }
        thread.throwException("Invalid Varying");
    });
    for(NativeNaturalType index = 0; index < 3; ++index)
        if(Ontology::getUncertain(thread.block, posNames[index], triple.pos[index])) {
            if(modes[index] != 2)
                thread.throwException("Invalid Input");
            modes[index] = 0;
        }
    getSymbolByName(Output)
    Vector<false, Symbol> output;
    output.symbol = OutputSymbol;
    auto count = Ontology::query(modes[0] + modes[1]*3 + modes[2]*9, triple, [&](Triple result) {
        for(NativeNaturalType i = 0; i < varyingCount; ++i)
            output.push_back(result.pos[i]);
    });
    Symbol CountSymbol;
    if(Ontology::getUncertain(thread.block, PreDef_Count, CountSymbol)) {
        Ontology::setSolitary({CountSymbol, PreDef_BlobType, PreDef_Natural});
        Storage::overwriteBlob(CountSymbol, count);
    }
    thread.popCallStack();
}

struct PreDefProcedure_Link {
    static bool e(Thread& thread, Triple triple) { return Ontology::link(triple); };
};

struct PreDefProcedure_Unlink {
    static bool e(Thread& thread, Triple triple) { return Ontology::unlink(triple); };
};

template<class op>
PreDefProcedure(Triple) {
    getSymbolByName(Entity)
    getSymbolByName(Attribute)
    getSymbolByName(Value)
    NativeNaturalType result = op::e(thread, {EntitySymbol, AttributeSymbol, ValueSymbol});
    Symbol OutputSymbol;
    if(Ontology::getUncertain(thread.block, PreDef_Output, OutputSymbol)) {
        Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
        Storage::overwriteBlob(OutputSymbol, result);
    }
    thread.popCallStack();
}

PreDefProcedure(Create) {
    Symbol InputSymbol, ValueSymbol;
    bool input = Ontology::getUncertain(thread.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = thread.accessBlobAs<NativeNaturalType>(InputSymbol);
    Symbol TargetSymbol = thread.getTargetSymbol();
    if(Ontology::query(9, {thread.block, PreDef_Output, PreDef_Void}, [&](Triple result) {
        Symbol OutputSymbol = result.pos[0];
        if(!input)
            ValueSymbol = Storage::createSymbol();
        Ontology::setSolitary({TargetSymbol, OutputSymbol, ValueSymbol});
        thread.link({TargetSymbol, PreDef_Holds, ValueSymbol});
    }) == 0)
        thread.throwException("Expected Output");
    thread.popCallStack();
}

PreDefProcedure(Destroy) {
    Set<false, Symbol> symbols;
    symbols.symbol = Storage::createSymbol();
    Ontology::link({thread.block, PreDef_Holds, symbols.symbol});
    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result) {
        symbols.insertElement(result.pos[0]);
    }) == 0)
        thread.throwException("Expected more Inputs");
    symbols.iterate([&](Symbol symbol) {
        Ontology::unlink(symbol);
    });
    thread.popCallStack();
}

PreDefProcedure(Push) {
    getSymbolByName(Execute)
    thread.link({thread.frame, PreDef_Execute, ExecuteSymbol});
    thread.setBlock(thread.getTargetSymbol());
}

PreDefProcedure(Pop) {
    getSymbolByName(Count)
    checkBlobType(Count, PreDef_Natural)
    auto CountValue = thread.accessBlobAs<NativeNaturalType>(CountSymbol);
    if(CountValue < 2)
        thread.throwException("Invalid Count Value");
    for(; CountValue > 0 && thread.popCallStack(); --CountValue);
}

PreDefProcedure(Branch) {
    getUncertainValueByName(Input, 1)
    getSymbolByName(Branch)
    thread.popCallStack();
    if(InputValue != 0)
        Ontology::setSolitary({thread.frame, PreDef_Execute, BranchSymbol});
}

PreDefProcedure(Exception) {
    Symbol ExceptionSymbol = thread.frame;
    if(Ontology::getUncertain(thread.block, PreDef_Exception, ExceptionSymbol)) {
        Symbol currentFrame = ExceptionSymbol;
        while(Ontology::getUncertain(currentFrame, PreDef_Parent, currentFrame));
        thread.link({currentFrame, PreDef_Parent, thread.frame});
    }
    Symbol execute, currentFrame = thread.frame, prevFrame = currentFrame;
    do {
        if(currentFrame != prevFrame && Ontology::getUncertain(currentFrame, PreDef_Catch, execute)) {
            Ontology::setSolitary({prevFrame, PreDef_Parent, PreDef_Void});
            thread.setFrame(currentFrame, true);
            thread.link({thread.block, PreDef_Holds, ExceptionSymbol});
            thread.link({thread.block, PreDef_Exception, ExceptionSymbol});
            Ontology::setSolitary({thread.frame, PreDef_Execute, execute});
            return;
        }
        prevFrame = currentFrame;
    } while(Ontology::getUncertain(currentFrame, PreDef_Parent, currentFrame));
    thread.setStatus(PreDef_Exception);
}

PreDefProcedure(Serialize) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Serialize serialize(thread, OutputSymbol);
    serialize.serializeBlob(InputSymbol);
    thread.popCallStack();
}

PreDefProcedure(Deserialize) {
    Deserialize{thread};
}

PreDefProcedure(SliceBlob) {
    getSymbolByName(Input)
    getSymbolByName(Target)
    getUncertainValueByName(Count, Storage::getBlobSize(InputSymbol))
    getUncertainValueByName(Destination, 0)
    getUncertainValueByName(Source, 0)
    if(!Storage::overwriteBlobPartial(TargetSymbol, InputSymbol, DestinationValue, SourceValue, CountValue))
        thread.throwException("Invalid Count, Destination or SrcOffset Value");
    thread.popCallStack();
}

PreDefProcedure(AllocateBlob) {
    getSymbolByName(Input)
    getUncertainValueByName(Preserve, 0)
    getSymbolByName(Target)
    checkBlobType(Input, PreDef_Natural)
    Storage::setBlobSize(TargetSymbol, thread.accessBlobAs<NativeNaturalType>(InputSymbol), PreserveValue);
    thread.popCallStack();
}

PreDefProcedure(CloneBlob) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Storage::cloneBlob(OutputSymbol, InputSymbol);
    Symbol type = PreDef_Void;
    Ontology::getUncertain(InputSymbol, PreDef_BlobType, type);
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, type});
    thread.popCallStack();
}

PreDefProcedure(GetBlobLength) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    Storage::overwriteBlob(OutputSymbol, Storage::getBlobSize(InputSymbol));
    thread.popCallStack();
}

template<typename T>
void PreDefProcedure_NumericCastTo(Thread& thread, Symbol type, Symbol OutputSymbol, Symbol InputSymbol) {
    switch(type) {
        case PreDef_Natural:
            Storage::overwriteBlob(OutputSymbol, static_cast<T>(thread.accessBlobAs<NativeNaturalType>(InputSymbol)));
            break;
        case PreDef_Integer:
            Storage::overwriteBlob(OutputSymbol, static_cast<T>(thread.accessBlobAs<NativeIntegerType>(InputSymbol)));
            break;
        case PreDef_Float:
            Storage::overwriteBlob(OutputSymbol, static_cast<T>(thread.accessBlobAs<NativeFloatType>(InputSymbol)));
            break;
        default:
            thread.throwException("Invalid Input");
    }
}

PreDefProcedure(NumericCast) {
    getSymbolByName(Input)
    getSymbolByName(To)
    getSymbolByName(Output)
    Symbol type;
    if(!Ontology::getUncertain(InputSymbol, PreDef_BlobType, type))
        thread.throwException("Invalid Input");
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, ToSymbol});
    switch(ToSymbol) {
        case PreDef_Natural:
            PreDefProcedure_NumericCastTo<NativeNaturalType>(thread, type, OutputSymbol, InputSymbol);
            break;
        case PreDef_Integer:
            PreDefProcedure_NumericCastTo<NativeIntegerType>(thread, type, OutputSymbol, InputSymbol);
            break;
        case PreDef_Float:
            PreDefProcedure_NumericCastTo<NativeFloatType>(thread, type, OutputSymbol, InputSymbol);
            break;
        default:
            thread.throwException("Invalid To Value");
    }
    thread.popCallStack();
}

PreDefProcedure(Equal) {
    Symbol type, FirstSymbol;
    NativeNaturalType OutputValue = 1;
    bool first = true;
    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result) {
        if(OutputValue == 0)
            return;
        Symbol InputSymbol = result.pos[0], _type = PreDef_Void;
        Ontology::getUncertain(InputSymbol, PreDef_BlobType, _type);
        if(first) {
            first = false;
            type = _type;
            FirstSymbol = InputSymbol;
        } else if(type == _type) {
            if(type == PreDef_Float) {
                if(thread.accessBlobAs<NativeFloatType>(InputSymbol) != thread.accessBlobAs<NativeFloatType>(FirstSymbol))
                    OutputValue = 0;
            } else if(Storage::compareBlobs(InputSymbol, FirstSymbol) != 0)
                OutputValue = 0;
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");
    getSymbolByName(Output)
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    Storage::overwriteBlob(OutputSymbol, OutputValue);
    thread.popCallStack();
}

struct PreDefProcedure_LessThan {
    static bool n(NativeNaturalType i, NativeNaturalType c) { return i < c; };
    static bool i(NativeIntegerType i, NativeIntegerType c) { return i < c; };
    static bool f(NativeFloatType i, NativeFloatType c) { return i < c; };
    static bool s(Symbol i, Symbol c) { return Storage::compareBlobs(i, c) < 0; };
};

struct PreDefProcedure_LessEqual {
    static bool n(NativeNaturalType i, NativeNaturalType c) { return i <= c; };
    static bool i(NativeIntegerType i, NativeIntegerType c) { return i <= c; };
    static bool f(NativeFloatType i, NativeFloatType c) { return i <= c; };
    static bool s(Symbol i, Symbol c) { return Storage::compareBlobs(i, c) <= 0; };
};

template<class op>
PreDefProcedure(CompareLogic) {
    getSymbolByName(Input)
    getSymbolByName(Comparandum)
    getSymbolByName(Output)
    Symbol type, _type;
    Ontology::getUncertain(InputSymbol, PreDef_BlobType, type);
    Ontology::getUncertain(ComparandumSymbol, PreDef_BlobType, _type);
    if(type != _type)
        thread.throwException("Input and Comparandum have different types");
    NativeNaturalType result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(thread.accessBlobAs<NativeNaturalType>(InputSymbol), thread.accessBlobAs<NativeNaturalType>(ComparandumSymbol));
            break;
        case PreDef_Integer:
            result = op::i(thread.accessBlobAs<NativeIntegerType>(InputSymbol), thread.accessBlobAs<NativeIntegerType>(ComparandumSymbol));
            break;
        case PreDef_Float:
            result = op::f(thread.accessBlobAs<NativeFloatType>(InputSymbol), thread.accessBlobAs<NativeFloatType>(ComparandumSymbol));
            break;
        default:
            result = op::s(InputSymbol, ComparandumSymbol);
            break;
    }
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    Storage::overwriteBlob(OutputSymbol, result);
    thread.popCallStack();
}

struct PreDefProcedure_BitShiftEmpty {
    static void d(NativeNaturalType& dst, NativeNaturalType count) { dst >>= count; };
    static void m(NativeNaturalType& dst, NativeNaturalType count) { dst <<= count; };
};

struct PreDefProcedure_BitShiftReplicate {
    static void d(NativeNaturalType& dst, NativeNaturalType count) {
        *reinterpret_cast<NativeIntegerType*>(&dst) >>= count;
    };
    static void m(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType lowestBit = dst&BitMask<NativeNaturalType>::one;
        dst <<= count;
        dst |= lowestBit*BitMask<NativeNaturalType>::fillLSBs(count);
    };
};

struct PreDefProcedure_BitShiftBarrel {
    static void d(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType aux = dst&BitMask<NativeNaturalType>::fillLSBs(count);
        dst >>= count;
        dst |= aux<<(ArchitectureSize-count);
    };
    static void m(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType aux = dst&BitMask<NativeNaturalType>::fillMSBs(count);
        dst <<= count;
        dst |= aux>>(ArchitectureSize-count);
    };
};

template<class op>
PreDefProcedure(BitShift) {
    getSymbolByName(Input)
    getSymbolByName(Count)
    getSymbolByName(Output)
    checkBlobType(Count, PreDef_Natural)
    auto result = thread.accessBlobAs<NativeNaturalType>(InputSymbol);
    auto CountValue = thread.accessBlobAs<NativeNaturalType>(CountSymbol);
    switch(thread.getGuaranteed(thread.block, PreDef_Direction)) {
        case PreDef_Divide:
            op::d(result, CountValue);
            break;
        case PreDef_Multiply:
            op::m(result, CountValue);
            break;
        default:
            thread.throwException("Invalid Direction");
    }
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    Storage::overwriteBlob(OutputSymbol, result);
    thread.popCallStack();
}

PreDefProcedure(BitwiseComplement) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    Storage::overwriteBlob(OutputSymbol, ~thread.accessBlobAs<NativeNaturalType>(InputSymbol));
    thread.popCallStack();
}

struct PreDefProcedure_BitwiseAnd {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst &= src; };
};

struct PreDefProcedure_BitwiseOr {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst |= src; };
};

struct PreDefProcedure_BitwiseXor {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst ^= src; };
};

template<class op>
PreDefProcedure(AssociativeCommutativeBitwise) {
    NativeNaturalType OutputValue;
    bool first = true;
    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result) {
        Symbol InputSymbol = result.pos[0];
        if(first) {
            first = false;
            OutputValue = thread.accessBlobAs<NativeNaturalType>(InputSymbol);
        } else
            op::n(OutputValue, thread.accessBlobAs<NativeNaturalType>(InputSymbol));
    }) < 2)
        thread.throwException("Expected more Input");
    getSymbolByName(Output)
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    Storage::overwriteBlob(OutputSymbol, OutputValue);
    thread.popCallStack();
}

struct PreDefProcedure_Add {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst += src; };
    static void i(NativeIntegerType& dst, NativeIntegerType src) { dst += src; };
    static void f(NativeFloatType& dst, NativeFloatType src) { dst += src; };
};

struct PreDefProcedure_Multiply {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst *= src; };
    static void i(NativeIntegerType& dst, NativeIntegerType src) { dst *= src; };
    static void f(NativeFloatType& dst, NativeFloatType src) { dst *= src; };
};

template<class op>
PreDefProcedure(AssociativeCommutativeArithmetic) {
    Symbol type;
    union {
        NativeNaturalType n;
        NativeIntegerType i;
        NativeFloatType f;
    } aux;
    bool first = true;
    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result) {
        Symbol InputSymbol = result.pos[0];
        Symbol _type = thread.getGuaranteed(InputSymbol, PreDef_BlobType);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = thread.accessBlobAs<NativeNaturalType>(InputSymbol);
                    break;
                case PreDef_Integer:
                    aux.i = thread.accessBlobAs<NativeIntegerType>(InputSymbol);
                    break;
                case PreDef_Float:
                    aux.f = thread.accessBlobAs<NativeFloatType>(InputSymbol);
                    break;
                default:
                    thread.throwException("Invalid Input");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, thread.accessBlobAs<NativeNaturalType>(InputSymbol));
                    break;
                case PreDef_Integer:
                    op::i(aux.i, thread.accessBlobAs<NativeIntegerType>(InputSymbol));
                    break;
                case PreDef_Float:
                    op::f(aux.f, thread.accessBlobAs<NativeFloatType>(InputSymbol));
                    break;
            }
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");
    getSymbolByName(Output)
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, type});
    Storage::overwriteBlob(OutputSymbol, aux.n);
    thread.popCallStack();
}

PreDefProcedure(Subtract) {
    getSymbolByName(Minuend)
    getSymbolByName(Subtrahend)
    getSymbolByName(Output)
    Symbol type = thread.getGuaranteed(MinuendSymbol, PreDef_BlobType);
    Symbol _type = thread.getGuaranteed(SubtrahendSymbol, PreDef_BlobType);
    if(type != _type)
        thread.throwException("Minuend and Subtrahend have different types");
    Ontology::setSolitary({OutputSymbol, PreDef_BlobType, type});
    switch(type) {
        case PreDef_Natural:
            Storage::overwriteBlob(OutputSymbol, thread.accessBlobAs<NativeNaturalType>(MinuendSymbol)-thread.accessBlobAs<NativeNaturalType>(SubtrahendSymbol));
            break;
        case PreDef_Integer:
            Storage::overwriteBlob(OutputSymbol, thread.accessBlobAs<NativeIntegerType>(MinuendSymbol)-thread.accessBlobAs<NativeIntegerType>(SubtrahendSymbol));
            break;
        case PreDef_Float:
            Storage::overwriteBlob(OutputSymbol, thread.accessBlobAs<NativeFloatType>(MinuendSymbol)-thread.accessBlobAs<NativeFloatType>(SubtrahendSymbol));
            break;
        default:
            thread.throwException("Invalid Minuend or Subtrahend");
    }
    thread.popCallStack();
}

PreDefProcedure(Divide) {
    getSymbolByName(Dividend)
    getSymbolByName(Divisor)
    Symbol type = thread.getGuaranteed(DividendSymbol, PreDef_BlobType);
    Symbol _type = thread.getGuaranteed(DivisorSymbol, PreDef_BlobType);
    if(type != _type)
        thread.throwException("Dividend and Divisor have different types");
    Symbol RestSymbol, QuotientSymbol;
    bool rest = Ontology::getUncertain(thread.block, PreDef_Rest, RestSymbol),
         quotient = Ontology::getUncertain(thread.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        Ontology::setSolitary({RestSymbol, PreDef_BlobType, type});
        if(Storage::getBlobSize(RestSymbol) != ArchitectureSize)
            thread.throwException("Invalid Rest");
    }
    if(quotient) {
        Ontology::setSolitary({QuotientSymbol, PreDef_BlobType, type});
        if(Storage::getBlobSize(QuotientSymbol) != ArchitectureSize)
            thread.throwException("Invalid Quotient");
    }
    if(!rest && !quotient)
        thread.throwException("Expected Rest or Quotient");
    switch(type) {
        case PreDef_Natural: {
            auto DividendValue =  thread.accessBlobAs<NativeNaturalType>(DividendSymbol),
                 DivisorValue = thread.accessBlobAs<NativeNaturalType>(DivisorSymbol);
            if(DivisorValue == 0)
                thread.throwException("Division by Zero");
            if(rest)
                Storage::overwriteBlob(RestSymbol, DividendValue%DivisorValue);
            if(quotient)
                Storage::overwriteBlob(QuotientSymbol, DividendValue/DivisorValue);
        }   break;
        case PreDef_Integer: {
            auto DividendValue = thread.accessBlobAs<NativeIntegerType>(DividendSymbol),
                 DivisorValue = thread.accessBlobAs<NativeIntegerType>(DivisorSymbol);
            if(DivisorValue == 0)
                thread.throwException("Division by Zero");
            if(rest)
                Storage::overwriteBlob(RestSymbol, DividendValue%DivisorValue);
            if(quotient)
                Storage::overwriteBlob(QuotientSymbol, DividendValue/DivisorValue);
        }   break;
        case PreDef_Float: {
            auto DividendValue = thread.accessBlobAs<NativeFloatType>(DividendSymbol),
                 DivisorValue = thread.accessBlobAs<NativeFloatType>(DivisorSymbol),
                 QuotientValue = DividendValue/DivisorValue;
            if(DivisorValue == 0.0)
                thread.throwException("Division by Zero");
            if(rest) {
                NativeFloatType integerPart = static_cast<NativeIntegerType>(QuotientValue);
                Storage::overwriteBlob(RestSymbol, QuotientValue-integerPart);
                QuotientValue = integerPart;
            }
            if(quotient)
                Storage::overwriteBlob(QuotientSymbol, QuotientValue);
        }   break;
        default:
            thread.throwException("Invalid Dividend or Divisor");
    }
    thread.popCallStack();
}

#define PreDefProcedureEntry(Name) \
    case PreDef_##Name: \
        PreDefProcedure_##Name(thread); \
        return true;

#define PreDefProcedureGroup(GroupName, Name) \
    case PreDef_##Name: \
        PreDefProcedure_##GroupName<PreDefProcedure_##Name>(thread); \
        return true;

bool executePreDefProcedure(Thread& thread, Symbol procedure) {
    switch(procedure) {
        PreDefProcedureEntry(Search)
        PreDefProcedureGroup(Triple, Link)
        PreDefProcedureGroup(Triple, Unlink)
        PreDefProcedureEntry(Create)
        PreDefProcedureEntry(Destroy)
        PreDefProcedureEntry(Push)
        PreDefProcedureEntry(Pop)
        PreDefProcedureEntry(Branch)
        PreDefProcedureEntry(Exception)
        PreDefProcedureEntry(Serialize)
        PreDefProcedureEntry(Deserialize)
        PreDefProcedureEntry(SliceBlob)
        PreDefProcedureEntry(AllocateBlob)
        PreDefProcedureEntry(CloneBlob)
        PreDefProcedureEntry(GetBlobLength)
        PreDefProcedureEntry(NumericCast)
        PreDefProcedureEntry(Equal)
        PreDefProcedureGroup(CompareLogic, LessThan)
        PreDefProcedureGroup(CompareLogic, LessEqual)
        PreDefProcedureGroup(BitShift, BitShiftEmpty)
        PreDefProcedureGroup(BitShift, BitShiftReplicate)
        PreDefProcedureGroup(BitShift, BitShiftBarrel)
        PreDefProcedureEntry(BitwiseComplement)
        PreDefProcedureGroup(AssociativeCommutativeBitwise, BitwiseAnd)
        PreDefProcedureGroup(AssociativeCommutativeBitwise, BitwiseOr)
        PreDefProcedureGroup(AssociativeCommutativeBitwise, BitwiseXor)
        PreDefProcedureGroup(AssociativeCommutativeArithmetic, Add)
        PreDefProcedureGroup(AssociativeCommutativeArithmetic, Multiply)
        PreDefProcedureEntry(Subtract)
        PreDefProcedureEntry(Divide)
    }
    return false;
}
