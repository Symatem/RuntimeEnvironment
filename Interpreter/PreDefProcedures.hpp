#include "Deserialize.hpp"

#define PreDefProcedure(Name) void PreDefProcedure_##Name(Thread& thread)

PreDefProcedure(Search) {
    Triple triple;
    uint8_t modes[3] = {2, 2, 2};
    Identifier posNames[3] = {PreDef_Entity, PreDef_Attribute, PreDef_Value};
    Ontology::query(9, {thread.block, PreDef_Varying, PreDef_Void}, [&](Triple result, ArchitectureType) {
        for(ArchitectureType index = 0; index < 3; ++index)
            if(result.pos[0] == posNames[index]) {
                modes[index] = 1;
                return;
            }
        thread.throwException("Invalid Varying");
    });
    for(ArchitectureType index = 0; index < 3; ++index)
        if(thread.getUncertain(thread.block, posNames[index], triple.pos[index])) {
            if(modes[index] != 2)
                thread.throwException("Invalid Input");
                modes[index] = 0;
            }
    getSymbolByName(Output)

    Vector<false, Identifier> output;
    output.symbol = OutputSymbol;
    auto count = Ontology::query(modes[0] + modes[1]*3 + modes[2]*9, triple, [&](Triple result, ArchitectureType size) {
        for(ArchitectureType i = 0; i < size; ++i)
            output.push_back(result.pos[i]);
    });

    Identifier CountSymbol;
    if(thread.getUncertain(thread.block, PreDef_Count, CountSymbol)) {
        thread.setSolitary({CountSymbol, PreDef_BlobType, PreDef_Natural});
        Storage::overwriteBlob(CountSymbol, count);
    }

    thread.popCallStack();
}

struct PreDefProcedure_Link {
    static bool e(Thread& thread, Triple triple) { return thread.link(triple); };
};

struct PreDefProcedure_Unlink {
    static bool e(Thread& thread, Triple triple) { return thread.unlink(triple); };
};

template<class op>
PreDefProcedure(Triple) {
    getSymbolByName(Entity)
    getSymbolByName(Attribute)
    getSymbolByName(Value)
    ArchitectureType result = op::e(thread, {EntitySymbol, AttributeSymbol, ValueSymbol});
    Identifier OutputSymbol;
    if(thread.getUncertain(thread.block, PreDef_Output, OutputSymbol)) {
        thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
        Storage::overwriteBlob(OutputSymbol, result);
    }
    thread.popCallStack();
}

PreDefProcedure(Create) {
    Identifier InputSymbol, ValueSymbol;
    bool input = thread.getUncertain(thread.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = thread.accessBlobAs<ArchitectureType>(InputSymbol);

    Identifier TargetSymbol = thread.getTargetSymbol();
    if(Ontology::query(9, {thread.block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Identifier OutputSymbol = result.pos[0];
        if(!input)
            ValueSymbol = Storage::createIdentifier();
        thread.setSolitary({TargetSymbol, OutputSymbol, ValueSymbol});
        thread.link({TargetSymbol, PreDef_Holds, ValueSymbol});
    }) == 0)
        thread.throwException("Expected Output");
    thread.popCallStack();
}

PreDefProcedure(Destroy) {
    Set<false, Identifier> symbols;
    symbols.symbol = Storage::createIdentifier();
    Ontology::link({thread.block, PreDef_Holds, symbols.symbol});
    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        symbols.insertElement(result.pos[0]);
    }) == 0)
        thread.throwException("Expected more Inputs");
    symbols.iterate([&](Identifier symbol) {
        Ontology::destroy(symbol);
    });
    thread.popCallStack();
}

PreDefProcedure(Push) {
    getSymbolByName(Execute)
    thread.block = thread.getTargetSymbol();
    thread.popCallStack();
    thread.pushCallStack();
    thread.link({thread.frame, PreDef_Execute, ExecuteSymbol});
}

PreDefProcedure(Pop) {
    getSymbolByName(Count)
    checkBlobType(Count, PreDef_Natural)
    auto CountValue = thread.accessBlobAs<uint64_t>(CountSymbol);
    if(CountValue < 2)
        thread.throwException("Invalid Count Value");
    for(; CountValue > 0 && thread.popCallStack(); --CountValue);
}

PreDefProcedure(Branch) {
    getUncertainValueByName(Input, 1)
    getSymbolByName(Branch)
    thread.popCallStack();
    if(InputValue != 0)
        thread.setSolitary({thread.frame, PreDef_Execute, BranchSymbol});
}

PreDefProcedure(Exception) {
    Identifier ExceptionSymbol = thread.frame;
    if(thread.getUncertain(thread.block, PreDef_Exception, ExceptionSymbol)) {
        Identifier currentFrame = ExceptionSymbol;
        while(thread.getUncertain(currentFrame, PreDef_Parent, currentFrame));
        thread.link({currentFrame, PreDef_Parent, thread.frame});
    }

    Identifier execute, currentFrame = thread.frame, prevFrame = currentFrame;
    do {
        if(currentFrame != prevFrame && thread.getUncertain(currentFrame, PreDef_Catch, execute)) {
            thread.setSolitary({prevFrame, PreDef_Parent, PreDef_Void});
            thread.setFrame(currentFrame, true);
            thread.link({thread.block, PreDef_Holds, ExceptionSymbol});
            thread.link({thread.block, PreDef_Exception, ExceptionSymbol});
            thread.setSolitary({thread.frame, PreDef_Execute, execute});
            return;
        }
        prevFrame = currentFrame;
    } while(thread.getUncertain(currentFrame, PreDef_Parent, currentFrame));
    thread.setStatus(PreDef_Exception);
}

PreDefProcedure(Serialize) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Serialize serialize(thread, OutputSymbol);
    serialize.serializeBlob(InputSymbol);
    serialize.finalizeSymbol();
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
    Storage::setBlobSize(TargetSymbol, thread.accessBlobAs<ArchitectureType>(InputSymbol), PreserveValue);
    thread.popCallStack();
}

PreDefProcedure(CloneBlob) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Storage::cloneBlob(OutputSymbol, InputSymbol);
    Identifier type = PreDef_Void;
    thread.getUncertain(InputSymbol, PreDef_BlobType, type);
    thread.setSolitary({OutputSymbol, PreDef_BlobType, type});
    thread.popCallStack();
}

PreDefProcedure(GetBlobLength) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    Storage::overwriteBlob(OutputSymbol, Storage::getBlobSize(InputSymbol));
    thread.popCallStack();
}

template<typename T>
void PreDefProcedure_NumericCastTo(Thread& thread, Identifier type, Identifier OutputSymbol, Identifier InputSymbol) {
    switch(type) {
        case PreDef_Natural:
            Storage::overwriteBlob(OutputSymbol, static_cast<T>(thread.accessBlobAs<uint64_t>(InputSymbol)));
            break;
        case PreDef_Integer:
            Storage::overwriteBlob(OutputSymbol, static_cast<T>(thread.accessBlobAs<int64_t>(InputSymbol)));
            break;
        case PreDef_Float:
            Storage::overwriteBlob(OutputSymbol, static_cast<T>(thread.accessBlobAs<double>(InputSymbol)));
            break;
        default:
            thread.throwException("Invalid Input");
    }
}

PreDefProcedure(NumericCast) {
    getSymbolByName(Input)
    getSymbolByName(To)
    getSymbolByName(Output)

    Identifier type;
    if(!thread.getUncertain(InputSymbol, PreDef_BlobType, type))
        thread.throwException("Invalid Input");
    thread.setSolitary({OutputSymbol, PreDef_BlobType, ToSymbol});
    switch(ToSymbol) {
        case PreDef_Natural:
            PreDefProcedure_NumericCastTo<uint64_t>(thread, type, OutputSymbol, InputSymbol);
            break;
        case PreDef_Integer:
            PreDefProcedure_NumericCastTo<int64_t>(thread, type, OutputSymbol, InputSymbol);
            break;
        case PreDef_Float:
            PreDefProcedure_NumericCastTo<double>(thread, type, OutputSymbol, InputSymbol);
            break;
        default:
            thread.throwException("Invalid To Value");
    }
    thread.popCallStack();
}

PreDefProcedure(Equal) {
    Identifier type, FirstSymbol;
    uint64_t OutputValue = 1;
    bool first = true;

    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(OutputValue == 0)
            return;
        Identifier InputSymbol = result.pos[0], _type = PreDef_Void;
        thread.getUncertain(InputSymbol, PreDef_BlobType, _type);
        if(first) {
            first = false;
            type = _type;
            FirstSymbol = InputSymbol;
        } else if(type == _type) {
            if(type == PreDef_Float) {
                if(thread.accessBlobAs<double>(InputSymbol) != thread.accessBlobAs<double>(FirstSymbol))
                    OutputValue = 0;
            } else if(Storage::compareBlobs(InputSymbol, FirstSymbol) != 0)
                OutputValue = 0;
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");

    getSymbolByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    Storage::overwriteBlob(OutputSymbol, OutputValue);
    thread.popCallStack();
}

struct PreDefProcedure_LessThan {
    static bool n(uint64_t i, uint64_t c) { return i < c; };
    static bool i(int64_t i, int64_t c) { return i < c; };
    static bool f(double i, double c) { return i < c; };
    static bool s(Identifier i, Identifier c) { return Storage::compareBlobs(i, c) < 0; };
};

struct PreDefProcedure_LessEqual {
    static bool n(uint64_t i, uint64_t c) { return i <= c; };
    static bool i(int64_t i, int64_t c) { return i <= c; };
    static bool f(double i, double c) { return i <= c; };
    static bool s(Identifier i, Identifier c) { return Storage::compareBlobs(i, c) <= 0; };
};

template<class op>
PreDefProcedure(CompareLogic) {
    getSymbolByName(Input)
    getSymbolByName(Comparandum)
    getSymbolByName(Output)

    Identifier type, _type;
    thread.getUncertain(InputSymbol, PreDef_BlobType, type);
    thread.getUncertain(ComparandumSymbol, PreDef_BlobType, _type);
    if(type != _type)
        thread.throwException("Input and Comparandum have different types");

    uint64_t result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(thread.accessBlobAs<uint64_t>(InputSymbol), thread.accessBlobAs<uint64_t>(ComparandumSymbol));
            break;
        case PreDef_Integer:
            result = op::i(thread.accessBlobAs<int64_t>(InputSymbol), thread.accessBlobAs<int64_t>(ComparandumSymbol));
            break;
        case PreDef_Float:
            result = op::f(thread.accessBlobAs<double>(InputSymbol), thread.accessBlobAs<double>(ComparandumSymbol));
            break;
        default:
            result = op::s(InputSymbol, ComparandumSymbol);
            break;
    }
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    Storage::overwriteBlob(OutputSymbol, result);
    thread.popCallStack();
}

struct PreDefProcedure_BitShiftEmpty {
    static void d(ArchitectureType& dst, uint64_t count) { dst >>= count; };
    static void m(ArchitectureType& dst, uint64_t count) { dst <<= count; };
};

struct PreDefProcedure_BitShiftReplicate {
    static void d(ArchitectureType& dst, uint64_t count) {
        *reinterpret_cast<int64_t*>(&dst) >>= count;
    };
    static void m(ArchitectureType& dst, uint64_t count) {
        ArchitectureType lowestBit = dst&BitMask<uint64_t>::one;
        dst <<= count;
        dst |= lowestBit*BitMask<uint64_t>::fillLSBs(count);
    };
};

struct PreDefProcedure_BitShiftBarrel {
    static void d(ArchitectureType& dst, uint64_t count) {
        ArchitectureType aux = dst&BitMask<uint64_t>::fillLSBs(count);
        dst >>= count;
        dst |= aux<<(ArchitectureSize-count);
    };
    static void m(ArchitectureType& dst, uint64_t count) {
        ArchitectureType aux = dst&BitMask<uint64_t>::fillMSBs(count);
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

    auto result = thread.accessBlobAs<uint64_t>(InputSymbol);
    auto CountValue = thread.accessBlobAs<uint64_t>(CountSymbol);
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

    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    Storage::overwriteBlob(OutputSymbol, result);
    thread.popCallStack();
}

PreDefProcedure(BitwiseComplement) {
    getSymbolByName(Input)
    getSymbolByName(Output)

    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    Storage::overwriteBlob(OutputSymbol, ~thread.accessBlobAs<uint64_t>(InputSymbol));
    thread.popCallStack();
}

struct PreDefProcedure_BitwiseAnd {
    static void n(uint64_t& dst, uint64_t src) { dst &= src; };
};

struct PreDefProcedure_BitwiseOr {
    static void n(uint64_t& dst, uint64_t src) { dst |= src; };
};

struct PreDefProcedure_BitwiseXor {
    static void n(uint64_t& dst, uint64_t src) { dst ^= src; };
};

template<class op>
PreDefProcedure(AssociativeCommutativeBitwise) {
    uint64_t OutputValue;
    bool first = true;

    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Identifier InputSymbol = result.pos[0];
        if(first) {
            first = false;
            OutputValue = thread.accessBlobAs<uint64_t>(InputSymbol);
        } else
            op::n(OutputValue, thread.accessBlobAs<uint64_t>(InputSymbol));
    }) < 2)
        thread.throwException("Expected more Input");

    getSymbolByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    Storage::overwriteBlob(OutputSymbol, OutputValue);
    thread.popCallStack();
}

struct PreDefProcedure_Add {
    static void n(uint64_t& dst, uint64_t src) { dst += src; };
    static void i(int64_t& dst, int64_t src) { dst += src; };
    static void f(double& dst, double src) { dst += src; };
};

struct PreDefProcedure_Multiply {
    static void n(uint64_t& dst, uint64_t src) { dst *= src; };
    static void i(int64_t& dst, int64_t src) { dst *= src; };
    static void f(double& dst, double src) { dst *= src; };
};

template<class op>
PreDefProcedure(AssociativeCommutativeArithmetic) {
    Identifier type;
    union {
        uint64_t n;
        int64_t i;
        double f;
    } aux;
    bool first = true;

    if(Ontology::query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Identifier InputSymbol = result.pos[0];
        Identifier _type = thread.getGuaranteed(InputSymbol, PreDef_BlobType);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = thread.accessBlobAs<uint64_t>(InputSymbol);
                    break;
                case PreDef_Integer:
                    aux.i = thread.accessBlobAs<int64_t>(InputSymbol);
                    break;
                case PreDef_Float:
                    aux.f = thread.accessBlobAs<double>(InputSymbol);
                    break;
                default:
                    thread.throwException("Invalid Input");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, thread.accessBlobAs<uint64_t>(InputSymbol));
                    break;
                case PreDef_Integer:
                    op::i(aux.i, thread.accessBlobAs<int64_t>(InputSymbol));
                    break;
                case PreDef_Float:
                    op::f(aux.f, thread.accessBlobAs<double>(InputSymbol));
                    break;
            }
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");

    getSymbolByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, type});
    Storage::overwriteBlob(OutputSymbol, aux.n);
    thread.popCallStack();
}

PreDefProcedure(Subtract) {
    getSymbolByName(Minuend)
    getSymbolByName(Subtrahend)
    getSymbolByName(Output)

    Identifier type = thread.getGuaranteed(MinuendSymbol, PreDef_BlobType);
    Identifier _type = thread.getGuaranteed(SubtrahendSymbol, PreDef_BlobType);
    if(type != _type)
        thread.throwException("Minuend and Subtrahend have different types");

    thread.setSolitary({OutputSymbol, PreDef_BlobType, type});
    switch(type) {
        case PreDef_Natural:
            Storage::overwriteBlob(OutputSymbol, thread.accessBlobAs<uint64_t>(MinuendSymbol)-thread.accessBlobAs<uint64_t>(SubtrahendSymbol));
            break;
        case PreDef_Integer:
            Storage::overwriteBlob(OutputSymbol, thread.accessBlobAs<int64_t>(MinuendSymbol)-thread.accessBlobAs<int64_t>(SubtrahendSymbol));
            break;
        case PreDef_Float:
            Storage::overwriteBlob(OutputSymbol, thread.accessBlobAs<double>(MinuendSymbol)-thread.accessBlobAs<double>(SubtrahendSymbol));
            break;
        default:
            thread.throwException("Invalid Minuend or Subtrahend");
    }
    thread.popCallStack();
}

PreDefProcedure(Divide) {
    getSymbolByName(Dividend)
    getSymbolByName(Divisor)

    Identifier type = thread.getGuaranteed(DividendSymbol, PreDef_BlobType);
    Identifier _type = thread.getGuaranteed(DivisorSymbol, PreDef_BlobType);
    if(type != _type)
        thread.throwException("Dividend and Divisor have different types");

    Identifier RestSymbol, QuotientSymbol;
    bool rest = thread.getUncertain(thread.block, PreDef_Rest, RestSymbol),
         quotient = thread.getUncertain(thread.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        thread.setSolitary({RestSymbol, PreDef_BlobType, type});
        if(Storage::getBlobSize(RestSymbol) != ArchitectureSize)
            thread.throwException("Invalid Rest");
    }
    if(quotient) {
        thread.setSolitary({QuotientSymbol, PreDef_BlobType, type});
        if(Storage::getBlobSize(QuotientSymbol) != ArchitectureSize)
            thread.throwException("Invalid Quotient");
    }

    if(!rest && !quotient)
        thread.throwException("Expected Rest or Quotient");

    switch(type) {
        case PreDef_Natural: {
            auto DividendValue =  thread.accessBlobAs<uint64_t>(DividendSymbol),
                 DivisorValue = thread.accessBlobAs<uint64_t>(DivisorSymbol);
            if(DivisorValue == 0)
                thread.throwException("Division by Zero");
            if(rest)
                Storage::overwriteBlob(RestSymbol, DividendValue%DivisorValue);
            if(quotient)
                Storage::overwriteBlob(QuotientSymbol, DividendValue/DivisorValue);
        }   break;
        case PreDef_Integer: {
            auto DividendValue = thread.accessBlobAs<int64_t>(DividendSymbol),
                 DivisorValue = thread.accessBlobAs<int64_t>(DivisorSymbol);
            if(DivisorValue == 0)
                thread.throwException("Division by Zero");
            if(rest)
                Storage::overwriteBlob(RestSymbol, DividendValue%DivisorValue);
            if(quotient)
                Storage::overwriteBlob(QuotientSymbol, DividendValue/DivisorValue);
        }   break;
        case PreDef_Float: {
            auto DividendValue = thread.accessBlobAs<double>(DividendSymbol),
                 DivisorValue = thread.accessBlobAs<double>(DivisorSymbol),
                 QuotientValue = DividendValue/DivisorValue;
            if(DivisorValue == 0.0)
                thread.throwException("Division by Zero");
            if(rest) {
                double integerPart = static_cast<int64_t>(QuotientValue);
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

bool executePreDefProcedure(Thread& thread, Identifier procedure) {
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
