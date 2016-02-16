#include "Deserialize.hpp"

#define PreDefProcedure(Name) void PreDefProcedure_##Name(Task& task)

PreDefProcedure(Search) {
    Triple triple;
    uint8_t modes[3] = {2, 2, 2};
    Symbol posNames[3] = {PreDef_Entity, PreDef_Attribute, PreDef_Value};
    task.query(9, {task.block, PreDef_Varying, PreDef_Void}, [&](Triple result, ArchitectureType) {
        for(ArchitectureType index = 0; index < 3; ++index)
            if(result.pos[0] == posNames[index]) {
                modes[index] = 1;
                return;
            }
        task.throwException("Invalid Varying");
    });
    for(ArchitectureType index = 0; index < 3; ++index)
        if(task.getUncertain(task.block, posNames[index], triple.pos[index])) {
            if(modes[index] != 2)
                task.throwException("Invalid Input");
                modes[index] = 0;
            }
    getSymbolObjectByName(Output)

    ArchitectureType blobSize = 0, index = 0;
    OutputSymbolObject->allocateBlob(ArchitectureSize);
    auto count = task.query(modes[0] + modes[1]*3 + modes[2]*9, triple,
    [&](Triple result, ArchitectureType size) {
        blobSize = ArchitectureSize*size*(index+1);
        if(blobSize > OutputSymbolObject->blobSize)
            OutputSymbolObject->reallocateBlob(std::max(blobSize, OutputSymbolObject->blobSize*2));
        bitwiseCopy<-1>(OutputSymbolObject->blobData.get(), result.pos, ArchitectureSize*size*index, 0, ArchitectureSize*size);
        ++index;
    });
    OutputSymbolObject->reallocateBlob(blobSize);

    Symbol CountSymbol;
    if(task.getUncertain(task.block, PreDef_Count, CountSymbol)) {
        task.setSolitary({CountSymbol, PreDef_BlobType, PreDef_Natural});
        task.context->getSymbolObject(CountSymbol)->overwriteBlob(count);
    }

    task.popCallStack();
}

struct PreDefProcedure_Link {
    static bool e(Context* context, Triple triple) { return context->link(triple); };
};

struct PreDefProcedure_Unlink {
    static bool e(Context* context, Triple triple) { return context->unlink({triple}); };
};

template<class op>
PreDefProcedure(Triple) {
    getSymbolByName(Entity)
    getSymbolByName(Attribute)
    getSymbolByName(Value)
    ArchitectureType result = op::e(task.context, {EntitySymbol, AttributeSymbol, ValueSymbol});
    Symbol OutputSymbol;
    if(task.getUncertain(task.block, PreDef_Output, OutputSymbol)) {
        task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
        task.context->getSymbolObject(OutputSymbol)->overwriteBlob(result);
    }
    task.popCallStack();
}

PreDefProcedure(Create) {
    Symbol InputSymbol, ValueSymbol;
    bool input = task.getUncertain(task.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = task.accessBlobData<ArchitectureType>(task.context->getSymbolObject(InputSymbol));

    std::set<Symbol> OutputSymbols;
    auto count = task.query(9, {task.block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
        OutputSymbols.insert(result.pos[0]);
    });
    if(count == 0)
        task.throwException("Expected Output");

    Symbol TargetSymbol = task.popCallStackTargetSymbol();
    for(Symbol OutputSymbol : OutputSymbols) {
        if(!input)
            ValueSymbol = task.context->create();
        task.setSolitary({TargetSymbol, OutputSymbol, ValueSymbol});
        task.link({TargetSymbol, PreDef_Holds, ValueSymbol});
    }
}

PreDefProcedure(Destroy) {
    std::set<Symbol> toDestroy;
    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        toDestroy.insert(result.pos[0]);
    }) == 0)
        task.throwException("Expected more Inputs");
    for(Symbol symbol : toDestroy)
        task.destroy(symbol);
    task.popCallStack();
}

PreDefProcedure(GetEnv) {
    getSymbolByName(Input)
    getSymbolByName(Output)
    Symbol TargetSymbol = task.popCallStackTargetSymbol();
    switch(InputSymbol) {
        case PreDef_Task:
            InputSymbol = task.task;
            break;
        case PreDef_Frame:
            InputSymbol = task.frame;
            break;
        case PreDef_Block:
            InputSymbol = task.block;
            break;
        default:
            task.throwException("Invalid Input Symbol");
    }
    task.setSolitary({TargetSymbol, OutputSymbol, InputSymbol});
}

PreDefProcedure(Push) {
    getSymbolByName(Execute)
    task.block = task.popCallStackTargetSymbol();
    Symbol parentFrame = task.frame;
    task.setFrame<true, false>(task.context->create({
        {PreDef_Holds, parentFrame},
        {PreDef_Parent, parentFrame},
        {PreDef_Holds, task.block},
        {PreDef_Block, task.block},
        {PreDef_Execute, ExecuteSymbol}
    }));
}

PreDefProcedure(Pop) {
    getSymbolObjectByName(Count)
    checkBlobType(Count, PreDef_Natural)
    auto CountValue = task.accessBlobData<uint64_t>(CountSymbolObject);
    if(CountValue < 2)
        task.throwException("Invalid Count Value");
    for(; CountValue > 0 && task.popCallStack(); --CountValue);
}

PreDefProcedure(Branch) {
    getUncertainSymbolObjectByName(Input, 1)
    getSymbolByName(Branch)
    task.popCallStack();
    if(InputValue != 0)
        task.setSolitary({task.frame, PreDef_Execute, BranchSymbol});
}

PreDefProcedure(Exception) {
    Symbol ExceptionSymbol;
    if(task.getUncertain(task.block, PreDef_Exception, ExceptionSymbol)) {
        Symbol currentFrame = ExceptionSymbol;
        while(task.getUncertain(currentFrame, PreDef_Parent, currentFrame));
        task.link({currentFrame, PreDef_Parent, task.frame});
    } else
        ExceptionSymbol = task.frame;

    Symbol execute, currentFrame = task.frame, prevFrame = currentFrame;
    do {
        if(currentFrame != prevFrame && task.getUncertain(currentFrame, PreDef_Catch, execute)) {
            task.unlink(prevFrame, PreDef_Parent);
            task.setFrame<true, true>(currentFrame);
            task.link({task.block, PreDef_Holds, ExceptionSymbol});
            task.link({task.block, PreDef_Exception, ExceptionSymbol});
            task.setSolitary({task.frame, PreDef_Execute, execute});
            return;
        }
        prevFrame = currentFrame;
    } while(task.getUncertain(currentFrame, PreDef_Parent, currentFrame));
    task.setStatus(PreDef_Exception);
}

PreDefProcedure(Serialize) {
    getSymbolByName(Input)
    getSymbolObjectByName(Output)
    task.context->unindexBlob(OutputSymbol);
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
    getUncertainSymbolObjectByName(Count, InputSymbolObject->blobSize)
    getUncertainSymbolObjectByName(Destination, 0)
    getUncertainSymbolObjectByName(Source, 0)
    task.context->unindexBlob(TargetSymbol);
    if(!TargetSymbolObject->overwriteBlobPartial(*InputSymbolObject, CountValue, DestinationValue, SourceValue))
        task.throwException("Invalid Count, Destination or SrcOffset Value");
    task.popCallStack();
}

PreDefProcedure(AllocateBlob) {
    getSymbolObjectByName(Input)
    getUncertainSymbolObjectByName(Preserve, 0)
    getSymbolObjectByName(Target)
    checkBlobType(Input, PreDef_Natural)
    task.context->unindexBlob(TargetSymbol);
    TargetSymbolObject->allocateBlob(task.accessBlobData<ArchitectureType>(InputSymbolObject), PreserveValue);
    task.popCallStack();
}

PreDefProcedure(CloneBlob) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)
    OutputSymbolObject->overwriteBlob(*InputSymbolObject);
    Symbol type;
    if(task.getUncertain(InputSymbol, PreDef_BlobType, type))
        task.setSolitary({OutputSymbol, PreDef_BlobType, type});
    else
        task.unlink(OutputSymbol, PreDef_BlobType);
    task.popCallStack();
}

PreDefProcedure(GetBlobLength) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(InputSymbolObject->blobSize);
    task.popCallStack();
}

template<typename T>
void PreDefProcedure_NumericCastTo(Task& task, Symbol type, SymbolObject* OutputSymbolObject, SymbolObject* InputSymbolObject) {
    switch(type) {
        case PreDef_Natural:
            OutputSymbolObject->overwriteBlob(static_cast<T>(task.accessBlobData<uint64_t>(InputSymbolObject)));
            break;
        case PreDef_Integer:
            OutputSymbolObject->overwriteBlob(static_cast<T>(task.accessBlobData<int64_t>(InputSymbolObject)));
            break;
        case PreDef_Float:
            OutputSymbolObject->overwriteBlob(static_cast<T>(task.accessBlobData<double>(InputSymbolObject)));
            break;
        default:
            task.throwException("Invalid Input SymbolObject");
    }
}

PreDefProcedure(NumericCast) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(To)
    getSymbolObjectByName(Output)

    Symbol type;
    if(!task.getUncertain(InputSymbol, PreDef_BlobType, type))
        task.throwException("Invalid Input SymbolObject");
    task.setSolitary({OutputSymbol, PreDef_BlobType, ToSymbol});
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
            task.throwException("Invalid To Value");
    }
    task.popCallStack();
}

PreDefProcedure(Equal) {
    Symbol type;
    SymbolObject* FirstSymbolObject;
    uint64_t OutputValue = 1;
    bool first = true;

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(OutputValue == 0)
            return;
        Symbol _type = PreDef_Void;
        task.getUncertain(result.pos[0], PreDef_BlobType, _type);
        if(first) {
            first = false;
            type = _type;
            FirstSymbolObject = task.context->getSymbolObject(result.pos[0]);
        } else if(type == _type) {
            SymbolObject* InputSymbolObject = task.context->getSymbolObject(result.pos[0]);
            if(type == PreDef_Float) {
                if(task.accessBlobData<double>(InputSymbolObject) != task.accessBlobData<double>(FirstSymbolObject))
                    OutputValue = 0;
            } else if(InputSymbolObject->compareBlob(*FirstSymbolObject) != 0)
                OutputValue = 0;
        } else
            task.throwException("Inputs have different types");
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolObjectByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
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
    task.getUncertain(InputSymbol, PreDef_BlobType, type);
    task.getUncertain(ComparandumSymbol, PreDef_BlobType, _type);
    if(type != _type)
        task.throwException("Input and Comparandum have different types");

    uint64_t result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(task.accessBlobData<uint64_t>(InputSymbolObject), task.accessBlobData<uint64_t>(ComparandumSymbolObject));
            break;
        case PreDef_Integer:
            result = op::i(task.accessBlobData<int64_t>(InputSymbolObject), task.accessBlobData<int64_t>(ComparandumSymbolObject));
            break;
        case PreDef_Float:
            result = op::f(task.accessBlobData<double>(InputSymbolObject), task.accessBlobData<double>(ComparandumSymbolObject));
            break;
        default:
            result = op::s(*InputSymbolObject, *ComparandumSymbolObject);
            break;
    }
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
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

    auto result = task.accessBlobData<uint64_t>(InputSymbolObject);
    auto CountValue = task.accessBlobData<uint64_t>(CountSymbolObject);
    switch(task.getGuaranteed(task.block, PreDef_Direction)) {
        case PreDef_Divide:
            op::d(result, CountValue);
            break;
        case PreDef_Multiply:
            op::m(result, CountValue);
            break;
        default:
            task.throwException("Invalid Direction");
    }

    task.unlink(OutputSymbol, PreDef_BlobType);
    OutputSymbolObject->overwriteBlob(result);
    task.popCallStack();
}

PreDefProcedure(BitwiseComplement) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)

    task.unlink(OutputSymbol, PreDef_BlobType);
    OutputSymbolObject->overwriteBlob(~task.accessBlobData<uint64_t>(InputSymbolObject));
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

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        SymbolObject* InputSymbolObject = task.context->getSymbolObject(result.pos[0]);
        if(first) {
            first = false;
            OutputValue = task.accessBlobData<uint64_t>(InputSymbolObject);
        } else
            op::n(OutputValue, task.accessBlobData<uint64_t>(InputSymbolObject));
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolObjectByName(Output)
    task.unlink(OutputSymbol, PreDef_BlobType);
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

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Symbol _type = task.getGuaranteed(result.pos[0], PreDef_BlobType);
        SymbolObject* InputSymbolObject = task.context->getSymbolObject(result.pos[0]);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = task.accessBlobData<uint64_t>(InputSymbolObject);
                    break;
                case PreDef_Integer:
                    aux.i = task.accessBlobData<int64_t>(InputSymbolObject);
                    break;
                case PreDef_Float:
                    aux.f = task.accessBlobData<double>(InputSymbolObject);
                    break;
                default:
                    task.throwException("Invalid Input SymbolObject");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, task.accessBlobData<uint64_t>(InputSymbolObject));
                    break;
                case PreDef_Integer:
                    op::i(aux.i, task.accessBlobData<int64_t>(InputSymbolObject));
                    break;
                case PreDef_Float:
                    op::f(aux.f, task.accessBlobData<double>(InputSymbolObject));
                    break;
            }
        } else
            task.throwException("Inputs have different types");
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolObjectByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, type});
    OutputSymbolObject->overwriteBlob(aux.n);
    task.popCallStack();
}

PreDefProcedure(Subtract) {
    getSymbolObjectByName(Minuend)
    getSymbolObjectByName(Subtrahend)
    getSymbolObjectByName(Output)

    Symbol type = task.getGuaranteed(MinuendSymbol, PreDef_BlobType);
    Symbol _type = task.getGuaranteed(SubtrahendSymbol, PreDef_BlobType);
    if(type != _type)
        task.throwException("Minuend and Subtrahend have different types");

    task.setSolitary({OutputSymbol, PreDef_BlobType, type});
    switch(type) {
        case PreDef_Natural:
            OutputSymbolObject->overwriteBlob(task.accessBlobData<uint64_t>(MinuendSymbolObject)-task.accessBlobData<uint64_t>(SubtrahendSymbolObject));
            break;
        case PreDef_Integer:
            OutputSymbolObject->overwriteBlob(task.accessBlobData<int64_t>(MinuendSymbolObject)-task.accessBlobData<int64_t>(SubtrahendSymbolObject));
            break;
        case PreDef_Float:
            OutputSymbolObject->overwriteBlob(task.accessBlobData<double>(MinuendSymbolObject)-task.accessBlobData<double>(SubtrahendSymbolObject));
            break;
        default:
            task.throwException("Invalid Minuend or Subtrahend SymbolObject");
    }
    task.popCallStack();
}

PreDefProcedure(Divide) {
    getSymbolObjectByName(Dividend)
    getSymbolObjectByName(Divisor)

    Symbol type = task.getGuaranteed(DividendSymbol, PreDef_BlobType);
    Symbol _type = task.getGuaranteed(DivisorSymbol, PreDef_BlobType);
    if(type != _type)
        task.throwException("Dividend and Divisor have different types");

    Symbol RestSymbol, QuotientSymbol;
    SymbolObject *RestSymbolObject, *QuotientSymbolObject;
    bool rest = task.getUncertain(task.block, PreDef_Rest, RestSymbol),
         quotient = task.getUncertain(task.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        task.setSolitary({RestSymbol, PreDef_BlobType, type});
        RestSymbolObject = task.context->getSymbolObject(RestSymbol);
        if(RestSymbolObject->blobSize != ArchitectureSize)
            task.throwException("Invalid Rest SymbolObject");
    }
    if(quotient) {
        task.setSolitary({QuotientSymbol, PreDef_BlobType, type});
        QuotientSymbolObject = task.context->getSymbolObject(QuotientSymbol);
        if(QuotientSymbolObject->blobSize != ArchitectureSize)
            task.throwException("Invalid Quotient SymbolObject");
    }

    if(!rest && !quotient)
        task.throwException("Expected Rest or Quotient");

    switch(type) {
        case PreDef_Natural: {
            auto DividendValue = task.accessBlobData<uint64_t>(DividendSymbolObject),
                 DivisorValue = task.accessBlobData<uint64_t>(DivisorSymbolObject);
            if(DivisorValue == 0) task.throwException("Division by Zero");
            if(rest) RestSymbolObject->overwriteBlob(DividendValue%DivisorValue);
            if(quotient) QuotientSymbolObject->overwriteBlob(DividendValue/DivisorValue);
        }   break;
        case PreDef_Integer: {
            auto DividendValue = task.accessBlobData<int64_t>(DividendSymbolObject),
                 DivisorValue = task.accessBlobData<int64_t>(DivisorSymbolObject);
            if(DivisorValue == 0) task.throwException("Division by Zero");
            if(rest) RestSymbolObject->overwriteBlob(DividendValue%DivisorValue);
            if(quotient) QuotientSymbolObject->overwriteBlob(DividendValue/DivisorValue);
        }   break;
        case PreDef_Float: {
            auto DividendValue = task.accessBlobData<double>(DividendSymbolObject),
                 DivisorValue = task.accessBlobData<double>(DivisorSymbolObject),
                 QuotientValue = DividendValue/DivisorValue;
            if(DivisorValue == 0.0) task.throwException("Division by Zero");
            if(rest) RestSymbolObject->overwriteBlob(modf(QuotientValue, &QuotientValue));
            if(quotient) QuotientSymbolObject->overwriteBlob(QuotientValue);
        }   break;
        default:
            task.throwException("Invalid Dividend or Divisor SymbolObject");
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
        PreDefProcedureEntry(GetEnv)
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
