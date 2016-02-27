#include "Deserialize.hpp"

#define PreDefProcedure(Name) void PreDefProcedure_##Name(Task& task)

PreDefProcedure(Search) {
    Triple triple;
    uint8_t modes[3] = {2, 2, 2};
    Symbol posNames[3] = {PreDef_Entity, PreDef_Attribute, PreDef_Value};
    context.query(9, {task.block, PreDef_Varying, PreDef_Void}, [&](Triple result, ArchitectureType) {
        for(ArchitectureType index = 0; index < 3; ++index)
            if(result.pos[0] == posNames[index]) {
                modes[index] = 1;
                return;
            }
        throw Exception("Invalid Varying");
    });
    for(ArchitectureType index = 0; index < 3; ++index)
        if(context.getUncertain(task.block, posNames[index], triple.pos[index])) {
            if(modes[index] != 2)
                throw Exception("Invalid Input");
                modes[index] = 0;
            }
    getSymbolObjectByName(Output)

    ArchitectureType blobSize = 0, index = 0;
    OutputSymbolObject->allocateBlob(ArchitectureSize);
    auto count = context.query(modes[0] + modes[1]*3 + modes[2]*9, triple,
    [&](Triple result, ArchitectureType size) {
        blobSize = ArchitectureSize*size*(index+1);
        if(blobSize > OutputSymbolObject->blobSize)
            OutputSymbolObject->reallocateBlob(std::max(blobSize, OutputSymbolObject->blobSize*2));
        bitwiseCopy<-1>(OutputSymbolObject->blobData.get(), result.pos, ArchitectureSize*size*index, 0, ArchitectureSize*size);
        ++index;
    });
    OutputSymbolObject->reallocateBlob(blobSize);

    Symbol CountSymbol;
    if(context.getUncertain(task.block, PreDef_Count, CountSymbol)) {
        context.setSolitary({CountSymbol, PreDef_BlobType, PreDef_Natural});
        context.getSymbolObject(CountSymbol)->overwriteBlob(count);
    }

    task.popCallStack();
}

struct PreDefProcedure_Link {
    static bool e(Triple triple) { return context.link(triple); };
};

struct PreDefProcedure_Unlink {
    static bool e(Triple triple) { return context.unlink(triple); };
};

template<class op>
PreDefProcedure(Triple) {
    getSymbolByName(Entity)
    getSymbolByName(Attribute)
    getSymbolByName(Value)
    ArchitectureType result = op::e({EntitySymbol, AttributeSymbol, ValueSymbol});
    Symbol OutputSymbol;
    if(context.getUncertain(task.block, PreDef_Output, OutputSymbol)) {
        context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
        context.getSymbolObject(OutputSymbol)->overwriteBlob(result);
    }
    task.popCallStack();
}

PreDefProcedure(Create) {
    Symbol InputSymbol, ValueSymbol;
    bool input = context.getUncertain(task.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = context.getSymbolObject(InputSymbol)->accessBlobAt<ArchitectureType>();

    Symbol TargetSymbol = task.getTargetSymbol();
    if(context.query(9, {task.block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Symbol OutputSymbol = result.pos[0];
        if(!input)
            ValueSymbol = context.create();
        context.setSolitary({TargetSymbol, OutputSymbol, ValueSymbol});
        context.link({TargetSymbol, PreDef_Holds, ValueSymbol});
    }) == 0)
        throw Exception("Expected Output");
    task.popCallStack();
}

PreDefProcedure(Destroy) {
    Set<Symbol, true> symbols;
    if(context.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        symbols.insertElement(result.pos[0]);
    }) == 0)
        throw Exception("Expected more Inputs");
    symbols.iterate([&](Symbol symbol) {
        context.destroy(symbol);
    });
    task.popCallStack();
}

PreDefProcedure(Push) {
    getSymbolByName(Execute)
    task.block = task.getTargetSymbol();
    task.popCallStack();
    task.setFrame(true, false, context.create({
        {PreDef_Holds, task.frame},
        {PreDef_Parent, task.frame},
        {PreDef_Holds, task.block},
        {PreDef_Block, task.block},
        {PreDef_Execute, ExecuteSymbol}
    }));
}

PreDefProcedure(Pop) {
    getSymbolObjectByName(Count)
    checkBlobType(Count, PreDef_Natural)
    auto CountValue = CountSymbolObject->accessBlobAt<uint64_t>();
    if(CountValue < 2)
        throw Exception("Invalid Count Value");
    for(; CountValue > 0 && task.popCallStack(); --CountValue);
}

PreDefProcedure(Branch) {
    getUncertainValueByName(Input, 1)
    getSymbolByName(Branch)
    task.popCallStack();
    if(InputValue != 0)
        context.setSolitary({task.frame, PreDef_Execute, BranchSymbol});
}

PreDefProcedure(Exception) {
    Symbol ExceptionSymbol = task.frame;
    if(context.getUncertain(task.block, PreDef_Exception, ExceptionSymbol)) {
        Symbol currentFrame = ExceptionSymbol;
        while(context.getUncertain(currentFrame, PreDef_Parent, currentFrame));
        context.link({currentFrame, PreDef_Parent, task.frame});
    }

    Symbol execute, currentFrame = task.frame, prevFrame = currentFrame;
    do {
        if(currentFrame != prevFrame && context.getUncertain(currentFrame, PreDef_Catch, execute)) {
            context.setSolitary({prevFrame, PreDef_Parent, PreDef_Void});
            task.setFrame(true, true, currentFrame);
            context.link({task.block, PreDef_Holds, ExceptionSymbol});
            context.link({task.block, PreDef_Exception, ExceptionSymbol});
            context.setSolitary({task.frame, PreDef_Execute, execute});
            return;
        }
        prevFrame = currentFrame;
    } while(context.getUncertain(currentFrame, PreDef_Parent, currentFrame));
    task.setStatus(PreDef_Exception);
}

PreDefProcedure(Serialize) {
    getSymbolByName(Input)
    getSymbolObjectByName(Output)
    blobIndex.eraseElement(OutputSymbol);
    Serialize serialize(task, OutputSymbol);
    serialize.serializeBlob(InputSymbol);
    serialize.finalizeSymbol();
    task.popCallStack();
}

PreDefProcedure(Deserialize) {
    Deserialize{task};
}

PreDefProcedure(SliceBlob) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Target)
    getUncertainValueByName(Count, InputSymbolObject->blobSize)
    getUncertainValueByName(Destination, 0)
    getUncertainValueByName(Source, 0)
    blobIndex.eraseElement(TargetSymbol);
    if(!TargetSymbolObject->overwriteBlobPartial(*InputSymbolObject, DestinationValue, SourceValue, CountValue))
        throw Exception("Invalid Count, Destination or SrcOffset Value");
    task.popCallStack();
}

PreDefProcedure(AllocateBlob) {
    getSymbolObjectByName(Input)
    getUncertainValueByName(Preserve, 0)
    getSymbolObjectByName(Target)
    checkBlobType(Input, PreDef_Natural)
    blobIndex.eraseElement(TargetSymbol);
    TargetSymbolObject->allocateBlob(InputSymbolObject->accessBlobAt<ArchitectureType>(), PreserveValue);
    task.popCallStack();
}

PreDefProcedure(CloneBlob) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)
    OutputSymbolObject->overwriteBlob(*InputSymbolObject);
    Symbol type = PreDef_Void;
    context.getUncertain(InputSymbol, PreDef_BlobType, type);
    context.setSolitary({OutputSymbol, PreDef_BlobType, type});
    task.popCallStack();
}

PreDefProcedure(GetBlobLength) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)
    context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(InputSymbolObject->blobSize);
    task.popCallStack();
}

template<typename T>
void PreDefProcedure_NumericCastTo(Task& task, Symbol type, SymbolObject* OutputSymbolObject, SymbolObject* InputSymbolObject) {
    switch(type) {
        case PreDef_Natural:
            OutputSymbolObject->overwriteBlob(static_cast<T>(InputSymbolObject->accessBlobAt<uint64_t>()));
            break;
        case PreDef_Integer:
            OutputSymbolObject->overwriteBlob(static_cast<T>(InputSymbolObject->accessBlobAt<int64_t>()));
            break;
        case PreDef_Float:
            OutputSymbolObject->overwriteBlob(static_cast<T>(InputSymbolObject->accessBlobAt<double>()));
            break;
        default:
            throw Exception("Invalid Input SymbolObject");
    }
}

PreDefProcedure(NumericCast) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(To)
    getSymbolObjectByName(Output)

    Symbol type;
    if(!context.getUncertain(InputSymbol, PreDef_BlobType, type))
        throw Exception("Invalid Input SymbolObject");
    context.setSolitary({OutputSymbol, PreDef_BlobType, ToSymbol});
    switch(ToSymbol) {
        case PreDef_Natural:
            PreDefProcedure_NumericCastTo<uint64_t>(task, type, OutputSymbolObject, InputSymbolObject);
            break;
        case PreDef_Integer:
            PreDefProcedure_NumericCastTo<int64_t>(task, type, OutputSymbolObject, InputSymbolObject);
            break;
        case PreDef_Float:
            PreDefProcedure_NumericCastTo<double>(task, type, OutputSymbolObject, InputSymbolObject);
            break;
        default:
            throw Exception("Invalid To Value");
    }
    task.popCallStack();
}

PreDefProcedure(Equal) {
    Symbol type;
    SymbolObject* FirstSymbolObject;
    uint64_t OutputValue = 1;
    bool first = true;

    if(context.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(OutputValue == 0)
            return;
        Symbol _type = PreDef_Void;
        context.getUncertain(result.pos[0], PreDef_BlobType, _type);
        if(first) {
            first = false;
            type = _type;
            FirstSymbolObject = context.getSymbolObject(result.pos[0]);
        } else if(type == _type) {
            SymbolObject* InputSymbolObject = context.getSymbolObject(result.pos[0]);
            if(type == PreDef_Float) {
                if(InputSymbolObject->accessBlobAt<double>() != FirstSymbolObject->accessBlobAt<double>())
                    OutputValue = 0;
            } else if(InputSymbolObject->compareBlob(*FirstSymbolObject) != 0)
                OutputValue = 0;
        } else
            throw Exception("Inputs have different types");
    }) < 2)
        throw Exception("Expected more Input");

    getSymbolObjectByName(Output)
    context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(OutputValue);
    task.popCallStack();
}

struct PreDefProcedure_LessThan {
    static bool n(uint64_t i, uint64_t c) { return i < c; };
    static bool i(int64_t i, int64_t c) { return i < c; };
    static bool f(double i, double c) { return i < c; };
    static bool s(const SymbolObject& i, const SymbolObject& c) { return i.compareBlob(c) < 0; };
};

struct PreDefProcedure_LessEqual {
    static bool n(uint64_t i, uint64_t c) { return i <= c; };
    static bool i(int64_t i, int64_t c) { return i <= c; };
    static bool f(double i, double c) { return i <= c; };
    static bool s(const SymbolObject& i, const SymbolObject& c) { return i.compareBlob(c) <= 0; };
};

template<class op>
PreDefProcedure(CompareLogic) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Comparandum)
    getSymbolObjectByName(Output)

    Symbol type, _type;
    context.getUncertain(InputSymbol, PreDef_BlobType, type);
    context.getUncertain(ComparandumSymbol, PreDef_BlobType, _type);
    if(type != _type)
        throw Exception("Input and Comparandum have different types");

    uint64_t result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(InputSymbolObject->accessBlobAt<uint64_t>(), ComparandumSymbolObject->accessBlobAt<uint64_t>());
            break;
        case PreDef_Integer:
            result = op::i(InputSymbolObject->accessBlobAt<int64_t>(), ComparandumSymbolObject->accessBlobAt<int64_t>());
            break;
        case PreDef_Float:
            result = op::f(InputSymbolObject->accessBlobAt<double>(), ComparandumSymbolObject->accessBlobAt<double>());
            break;
        default:
            result = op::s(*InputSymbolObject, *ComparandumSymbolObject);
            break;
    }
    context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(result);
    task.popCallStack();
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
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Count)
    getSymbolObjectByName(Output)
    checkBlobType(Count, PreDef_Natural)

    auto result = InputSymbolObject->accessBlobAt<uint64_t>();
    auto CountValue = CountSymbolObject->accessBlobAt<uint64_t>();
    switch(context.getGuaranteed(task.block, PreDef_Direction)) {
        case PreDef_Divide:
            op::d(result, CountValue);
            break;
        case PreDef_Multiply:
            op::m(result, CountValue);
            break;
        default:
            throw Exception("Invalid Direction");
    }

    context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    OutputSymbolObject->overwriteBlob(result);
    task.popCallStack();
}

PreDefProcedure(BitwiseComplement) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)

    context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    OutputSymbolObject->overwriteBlob(~InputSymbolObject->accessBlobAt<uint64_t>());
    task.popCallStack();
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

    if(context.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        SymbolObject* InputSymbolObject = context.getSymbolObject(result.pos[0]);
        if(first) {
            first = false;
            OutputValue = InputSymbolObject->accessBlobAt<uint64_t>();
        } else
            op::n(OutputValue, InputSymbolObject->accessBlobAt<uint64_t>());
    }) < 2)
        throw Exception("Expected more Input");

    getSymbolObjectByName(Output)
    context.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    OutputSymbolObject->overwriteBlob(OutputValue);
    task.popCallStack();
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
    Symbol type;
    union {
        uint64_t n;
        int64_t i;
        double f;
    } aux;
    bool first = true;

    if(context.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Symbol _type = context.getGuaranteed(result.pos[0], PreDef_BlobType);
        SymbolObject* InputSymbolObject = context.getSymbolObject(result.pos[0]);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = InputSymbolObject->accessBlobAt<uint64_t>();
                    break;
                case PreDef_Integer:
                    aux.i = InputSymbolObject->accessBlobAt<int64_t>();
                    break;
                case PreDef_Float:
                    aux.f = InputSymbolObject->accessBlobAt<double>();
                    break;
                default:
                    throw Exception("Invalid Input SymbolObject");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, InputSymbolObject->accessBlobAt<uint64_t>());
                    break;
                case PreDef_Integer:
                    op::i(aux.i, InputSymbolObject->accessBlobAt<int64_t>());
                    break;
                case PreDef_Float:
                    op::f(aux.f, InputSymbolObject->accessBlobAt<double>());
                    break;
            }
        } else
            throw Exception("Inputs have different types");
    }) < 2)
        throw Exception("Expected more Input");

    getSymbolObjectByName(Output)
    context.setSolitary({OutputSymbol, PreDef_BlobType, type});
    OutputSymbolObject->overwriteBlob(aux.n);
    task.popCallStack();
}

PreDefProcedure(Subtract) {
    getSymbolObjectByName(Minuend)
    getSymbolObjectByName(Subtrahend)
    getSymbolObjectByName(Output)

    Symbol type = context.getGuaranteed(MinuendSymbol, PreDef_BlobType);
    Symbol _type = context.getGuaranteed(SubtrahendSymbol, PreDef_BlobType);
    if(type != _type)
        throw Exception("Minuend and Subtrahend have different types");

    context.setSolitary({OutputSymbol, PreDef_BlobType, type});
    switch(type) {
        case PreDef_Natural:
            OutputSymbolObject->overwriteBlob(MinuendSymbolObject->accessBlobAt<uint64_t>()-SubtrahendSymbolObject->accessBlobAt<uint64_t>());
            break;
        case PreDef_Integer:
            OutputSymbolObject->overwriteBlob(MinuendSymbolObject->accessBlobAt<int64_t>()-SubtrahendSymbolObject->accessBlobAt<int64_t>());
            break;
        case PreDef_Float:
            OutputSymbolObject->overwriteBlob(MinuendSymbolObject->accessBlobAt<double>()-SubtrahendSymbolObject->accessBlobAt<double>());
            break;
        default:
            throw Exception("Invalid Minuend or Subtrahend SymbolObject");
    }
    task.popCallStack();
}

PreDefProcedure(Divide) {
    getSymbolObjectByName(Dividend)
    getSymbolObjectByName(Divisor)

    Symbol type = context.getGuaranteed(DividendSymbol, PreDef_BlobType);
    Symbol _type = context.getGuaranteed(DivisorSymbol, PreDef_BlobType);
    if(type != _type)
        throw Exception("Dividend and Divisor have different types");

    Symbol RestSymbol, QuotientSymbol;
    SymbolObject *RestSymbolObject, *QuotientSymbolObject;
    bool rest = context.getUncertain(task.block, PreDef_Rest, RestSymbol),
         quotient = context.getUncertain(task.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        context.setSolitary({RestSymbol, PreDef_BlobType, type});
        RestSymbolObject = context.getSymbolObject(RestSymbol);
        if(RestSymbolObject->blobSize != ArchitectureSize)
            throw Exception("Invalid Rest SymbolObject");
    }
    if(quotient) {
        context.setSolitary({QuotientSymbol, PreDef_BlobType, type});
        QuotientSymbolObject = context.getSymbolObject(QuotientSymbol);
        if(QuotientSymbolObject->blobSize != ArchitectureSize)
            throw Exception("Invalid Quotient SymbolObject");
    }

    if(!rest && !quotient)
        throw Exception("Expected Rest or Quotient");

    switch(type) {
        case PreDef_Natural: {
            auto DividendValue =  DividendSymbolObject->accessBlobAt<uint64_t>(),
                 DivisorValue = DivisorSymbolObject->accessBlobAt<uint64_t>();
            if(DivisorValue == 0)
                throw Exception("Division by Zero");
            if(rest)
                RestSymbolObject->overwriteBlob(DividendValue%DivisorValue);
            if(quotient)
                QuotientSymbolObject->overwriteBlob(DividendValue/DivisorValue);
        }   break;
        case PreDef_Integer: {
            auto DividendValue = DividendSymbolObject->accessBlobAt<int64_t>(),
                 DivisorValue = DivisorSymbolObject->accessBlobAt<int64_t>();
            if(DivisorValue == 0)
                throw Exception("Division by Zero");
            if(rest)
                RestSymbolObject->overwriteBlob(DividendValue%DivisorValue);
            if(quotient)
                QuotientSymbolObject->overwriteBlob(DividendValue/DivisorValue);
        }   break;
        case PreDef_Float: {
            auto DividendValue = DividendSymbolObject->accessBlobAt<double>(),
                 DivisorValue = DivisorSymbolObject->accessBlobAt<double>(),
                 QuotientValue = DividendValue/DivisorValue;
            if(DivisorValue == 0.0)
                throw Exception("Division by Zero");
            if(rest) {
                double integerPart = static_cast<int64_t>(QuotientValue);
                RestSymbolObject->overwriteBlob(QuotientValue-integerPart);
                QuotientValue = integerPart;
            }
            if(quotient)
                QuotientSymbolObject->overwriteBlob(QuotientValue);
        }   break;
        default:
            throw Exception("Invalid Dividend or Divisor SymbolObject");
    }
    task.popCallStack();
}

#define PreDefProcedureEntry(Name) \
    case PreDef_##Name: \
        PreDefProcedure_##Name(task); \
        return true;

#define PreDefProcedureGroup(GroupName, Name) \
    case PreDef_##Name: \
        PreDefProcedure_##GroupName<PreDefProcedure_##Name>(task); \
        return true;

bool executePreDefProcedure(Task& task, Symbol procedure) {
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
