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
    getSymbolAndBlobByName(Output)

    ArchitectureType blobSize = 0, index = 0;
    OutputBlob->allocate(ArchitectureSize);
    auto count = task.query(modes[0] + modes[1]*3 + modes[2]*9, triple,
    [&](Triple result, ArchitectureType size) {
        blobSize = ArchitectureSize*size*(index+1);
        if(blobSize > OutputBlob->size)
            OutputBlob->reallocate(OutputBlob->size*2);
        bitWiseCopyForward(OutputBlob->data.get(), result.pos, ArchitectureSize*size, ArchitectureSize*size*index, 0);
        ++index;
    });
    OutputBlob->reallocate(blobSize);

    Symbol CountSymbol;
    if(task.getUncertain(task.block, PreDef_Count, CountSymbol)) {
        task.setSolitary({CountSymbol, PreDef_BlobType, PreDef_Natural});
        task.context->getBlob(CountSymbol)->overwrite(count);
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
        task.context->getBlob(OutputSymbol)->overwrite(result);
    }
    task.popCallStack();
}

PreDefProcedure(Create) {
    Symbol InputSymbol, ValueSymbol;
    bool input = task.getUncertain(task.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = task.get<ArchitectureType>(task.context->getBlob(InputSymbol));

    std::set<Symbol> OutputSymbols;
    auto count = task.query(9, {task.block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
        OutputSymbols.insert(result.pos[0]);
    });
    if(count == 0)
        task.throwException("Expected Output");

    Symbol TargetSymbol = task.popCallStackTargetSymbol();
    for(Symbol OutputSymbol : OutputSymbols) {
        if(!input) ValueSymbol = task.context->create();
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
    getSymbolAndBlobByName(Count)
    checkBlobType(Count, PreDef_Natural)
    auto CountValue = task.get<uint64_t>(CountBlob);
    if(CountValue < 2)
        task.throwException("Invalid Count Value");
    for(; CountValue > 0 && task.popCallStack(); --CountValue);
}

PreDefProcedure(Branch) {
    getUncertainSymbolAndBlobByName(Input, 1)
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
    while(true) {
        if(currentFrame != prevFrame && task.getUncertain(currentFrame, PreDef_Catch, execute)) {
            task.unlink(prevFrame, PreDef_Parent);
            task.setFrame<true, true>(currentFrame);
            task.link({task.block, PreDef_Holds, ExceptionSymbol});
            task.link({task.block, PreDef_Exception, ExceptionSymbol});
            task.setSolitary({task.frame, PreDef_Execute, execute});
            return;
        }
        prevFrame = currentFrame;
        if(!task.getUncertain(currentFrame, PreDef_Parent, currentFrame))
            break;
    }

    task.setFrame<true, true>(ExceptionSymbol);
    task.setStatus(PreDef_Exception);
}

PreDefProcedure(Serialize) {
    getSymbolByName(Input)
    getSymbolAndBlobByName(Output)

    std::stringstream stream;
    task.serializeBlob(stream, InputSymbol);
    OutputBlob->overwrite(stream);
    task.popCallStack();
}

PreDefProcedure(Deserialize) {
    Deserialize{task};
}

PreDefProcedure(SliceBlob) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(Target)
    getUncertainSymbolAndBlobByName(Count, InputBlob->size)
    getUncertainSymbolAndBlobByName(Destination, 0)
    getUncertainSymbolAndBlobByName(Source, 0)
    if(!TargetBlob->replacePartial(*InputBlob, CountValue, DestinationValue, SourceValue))
        task.throwException("Invalid Count, Destination or SrcOffset Value");
    task.updateBlobIndexFor(TargetSymbol, TargetBlob);
    task.popCallStack();
}

PreDefProcedure(AllocateBlob) {
    getSymbolAndBlobByName(Input)
    getUncertainSymbolAndBlobByName(Preserve, 0)
    getSymbolAndBlobByName(Target)
    checkBlobType(Input, PreDef_Natural)

    TargetBlob->allocate(task.get<ArchitectureType>(InputBlob), PreserveValue);
    task.updateBlobIndexFor(TargetSymbol, TargetBlob);
    task.popCallStack();
}

PreDefProcedure(CloneBlob) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(Output)
    OutputBlob->overwrite(*InputBlob);
    Symbol type;
    if(task.getUncertain(InputSymbol, PreDef_BlobType, type))
        task.setSolitary({OutputSymbol, PreDef_BlobType, type});
    else
        task.unlink(OutputSymbol, PreDef_BlobType);
    task.popCallStack();
}

PreDefProcedure(GetBlobLength) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputBlob->overwrite(InputBlob->size);
    task.popCallStack();
}

template<typename T>
void PreDefProcedure_NumericCastTo(Task& task, Symbol type, Blob* OutputBlob, Blob* InputBlob) {
    switch(type) {
        case PreDef_Natural:
            OutputBlob->overwrite(static_cast<T>(task.get<uint64_t>(InputBlob)));
        break;
        case PreDef_Integer:
            OutputBlob->overwrite(static_cast<T>(task.get<int64_t>(InputBlob)));
        break;
        case PreDef_Float:
            OutputBlob->overwrite(static_cast<T>(task.get<double>(InputBlob)));
        break;
        default:
            task.throwException("Invalid Input Blob");
    }
}

PreDefProcedure(NumericCast) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(To)
    getSymbolAndBlobByName(Output)

    Symbol type;
    if(!task.getUncertain(InputSymbol, PreDef_BlobType, type))
        task.throwException("Invalid Input Blob");
    task.setSolitary({OutputSymbol, PreDef_BlobType, ToSymbol});
    switch(ToSymbol) {
        case PreDef_Natural:
            PreDefProcedure_NumericCastTo<uint64_t>(task, type, OutputBlob, InputBlob);
        break;
        case PreDef_Integer:
            PreDefProcedure_NumericCastTo<int64_t>(task, type, OutputBlob, InputBlob);
        break;
        case PreDef_Float:
            PreDefProcedure_NumericCastTo<double>(task, type, OutputBlob, InputBlob);
        break;
        default:
            task.throwException("Invalid To Value");
    }
    task.popCallStack();
}

PreDefProcedure(Equal) {
    Symbol type;
    Blob* FirstBlob;
    uint64_t OutputValue = 1;
    bool first = true;

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(OutputValue == 0) return;
        Symbol _type = PreDef_Void;
        task.getUncertain(result.pos[0], PreDef_BlobType, _type);
        if(first) {
            first = false;
            type = _type;
            FirstBlob = task.context->getBlob(result.pos[0]);
        } else if(type == _type) {
            Blob* InputBlob = task.context->getBlob(result.pos[0]);
            if(type == PreDef_Float) {
                if(task.get<double>(InputBlob) != task.get<double>(FirstBlob))
                    OutputValue = 0;
            } else if(InputBlob->compare(*FirstBlob) != 0)
                OutputValue = 0;
        } else
            task.throwException("Inputs have different types");
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolAndBlobByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputBlob->overwrite(OutputValue);
    task.popCallStack();
}

struct PreDefProcedure_LessThan {
    static bool n(uint64_t i, uint64_t c) { return i < c; };
    static bool i(int64_t i, int64_t c) { return i < c; };
    static bool f(double i, double c) { return i < c; };
    static bool s(const Blob& i, const Blob& c) { return i.compare(c) < 0; };
};

struct PreDefProcedure_LessEqual {
    static bool n(uint64_t i, uint64_t c) { return i <= c; };
    static bool i(int64_t i, int64_t c) { return i <= c; };
    static bool f(double i, double c) { return i <= c; };
    static bool s(const Blob& i, const Blob& c) { return i.compare(c) <= 0; };
};

template<class op>
PreDefProcedure(CompareLogic) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(Comparandum)
    getSymbolAndBlobByName(Output)

    Symbol type, _type;
    task.getUncertain(InputSymbol, PreDef_BlobType, type);
    task.getUncertain(ComparandumSymbol, PreDef_BlobType, _type);
    if(type != _type)
        task.throwException("Input and Comparandum have different types");

    uint64_t result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(task.get<uint64_t>(InputBlob), task.get<uint64_t>(ComparandumBlob));
        break;
        case PreDef_Integer:
            result = op::i(task.get<int64_t>(InputBlob), task.get<int64_t>(ComparandumBlob));
        break;
        case PreDef_Float:
            result = op::f(task.get<double>(InputBlob), task.get<double>(ComparandumBlob));
        break;
        default:
            result = op::s(*InputBlob, *ComparandumBlob);
        break;
    }
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputBlob->overwrite(result);
    task.popCallStack();
}

PreDefProcedure(Complement) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(Output)

    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputBlob->overwrite(~task.get<uint64_t>(InputBlob));
    task.popCallStack();
}

struct PreDefProcedure_ClearShift {
    static void n(ArchitectureType& dst, uint64_t count) { dst >>= count; };
    static void p(ArchitectureType& dst, uint64_t count) { dst <<= count; };
};

struct PreDefProcedure_CloneShift {
    static void n(ArchitectureType& dst, uint64_t count) {
        *reinterpret_cast<int64_t*>(&dst) >>= count;
    };
    static void p(ArchitectureType& dst, uint64_t count) {
        ArchitectureType lowestBit = dst&BitMask<uint64_t>::one;
        dst <<= count;
        dst |= lowestBit*BitMask<uint64_t>::fillLSBs(count);
    };
};

struct PreDefProcedure_BarrelShift {
    static void n(ArchitectureType& dst, uint64_t count) {
        ArchitectureType aux = dst&BitMask<uint64_t>::fillLSBs(count);
        dst >>= count;
        dst |= aux<<(ArchitectureSize-count);
    };
    static void p(ArchitectureType& dst, uint64_t count) {
        ArchitectureType aux = dst&BitMask<uint64_t>::fillMSBs(count);
        dst <<= count;
        dst |= aux>>(ArchitectureSize-count);
    };
};

template<class op>
PreDefProcedure(Shift) {
    getSymbolAndBlobByName(Input)
    getSymbolAndBlobByName(Count)
    getSymbolAndBlobByName(Output)
    checkBlobType(Count, PreDef_Integer)

    auto result = task.get<uint64_t>(InputBlob);
    auto CountValue = task.get<int64_t>(CountBlob);
    if(CountValue < 0)
        op::n(result, -CountValue);
    else
        op::p(result, CountValue);

    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputBlob->overwrite(result);
    task.popCallStack();
}

struct PreDefProcedure_And {
    static void n(uint64_t& dst, uint64_t src) { dst &= src; };
};

struct PreDefProcedure_Or {
    static void n(uint64_t& dst, uint64_t src) { dst |= src; };
};

struct PreDefProcedure_Xor {
    static void n(uint64_t& dst, uint64_t src) { dst ^= src; };
};

template<class op>
PreDefProcedure(AssociativeCommutativeBitWise) {
    uint64_t OutputValue;
    bool first = true;

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Blob* InputBlob = task.context->getBlob(result.pos[0]);
        if(first) {
            first = false;
            OutputValue = task.get<uint64_t>(InputBlob);
        } else
            op::n(OutputValue, task.get<uint64_t>(InputBlob));
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolAndBlobByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputBlob->overwrite(OutputValue);
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
        Blob* InputBlob = task.context->getBlob(result.pos[0]);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = task.get<uint64_t>(InputBlob);
                break;
                case PreDef_Integer:
                    aux.i = task.get<int64_t>(InputBlob);
                break;
                case PreDef_Float:
                    aux.f = task.get<double>(InputBlob);
                break;
                default:
                    task.throwException("Invalid Input Blob");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, task.get<uint64_t>(InputBlob));
                break;
                case PreDef_Integer:
                    op::i(aux.i, task.get<int64_t>(InputBlob));
                break;
                case PreDef_Float:
                    op::f(aux.f, task.get<double>(InputBlob));
                break;
            }
        } else
            task.throwException("Inputs have different types");
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolAndBlobByName(Output)
    task.setSolitary({OutputSymbol, PreDef_BlobType, type});
    OutputBlob->overwrite(aux.n);
    task.popCallStack();
}

PreDefProcedure(Subtract) {
    getSymbolAndBlobByName(Minuend)
    getSymbolAndBlobByName(Subtrahend)
    getSymbolAndBlobByName(Output)

    Symbol type = task.getGuaranteed(MinuendSymbol, PreDef_BlobType);
    Symbol _type = task.getGuaranteed(SubtrahendSymbol, PreDef_BlobType);
    if(type != _type)
        task.throwException("Minuend and Subtrahend have different types");

    task.setSolitary({OutputSymbol, PreDef_BlobType, type});
    switch(type) {
        case PreDef_Natural:
            OutputBlob->overwrite(task.get<uint64_t>(MinuendBlob)-task.get<uint64_t>(SubtrahendBlob));
        break;
        case PreDef_Integer:
            OutputBlob->overwrite(task.get<int64_t>(MinuendBlob)-task.get<int64_t>(SubtrahendBlob));
        break;
        case PreDef_Float:
            OutputBlob->overwrite(task.get<double>(MinuendBlob)-task.get<double>(SubtrahendBlob));
        break;
        default:
            task.throwException("Invalid Minuend or Subtrahend Blob");
    }
    task.popCallStack();
}

PreDefProcedure(Divide) {
    getSymbolAndBlobByName(Dividend)
    getSymbolAndBlobByName(Divisor)

    Symbol type = task.getGuaranteed(DividendSymbol, PreDef_BlobType);
    Symbol _type = task.getGuaranteed(DivisorSymbol, PreDef_BlobType);
    if(type != _type)
        task.throwException("Dividend and Divisor have different types");

    Symbol RestSymbol, QuotientSymbol;
    Blob *RestBlob, *QuotientBlob;
    bool rest = task.getUncertain(task.block, PreDef_Rest, RestSymbol),
         quotient = task.getUncertain(task.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        task.setSolitary({RestSymbol, PreDef_BlobType, type});
        RestBlob = task.context->getBlob(RestSymbol);
        if(RestBlob->size != ArchitectureSize)
            task.throwException("Invalid Rest Blob");
    }
    if(quotient) {
        task.setSolitary({QuotientSymbol, PreDef_BlobType, type});
        QuotientBlob = task.context->getBlob(QuotientSymbol);
        if(QuotientBlob->size != ArchitectureSize)
            task.throwException("Invalid Quotient Blob");
    }

    if(!rest && !quotient)
        task.throwException("Expected Rest or Quotient");

    switch(type) {
        case PreDef_Natural: {
            auto DividendValue = task.get<uint64_t>(DividendBlob),
                 DivisorValue = task.get<uint64_t>(DivisorBlob);
            if(rest) RestBlob->overwrite(DividendValue%DivisorValue);
            if(quotient) QuotientBlob->overwrite(DividendValue/DivisorValue);
        } break;
        case PreDef_Integer: {
            auto DividendValue = task.get<int64_t>(DividendBlob),
                 DivisorValue = task.get<int64_t>(DivisorBlob);
            if(rest) RestBlob->overwrite(DividendValue%DivisorValue);
            if(quotient) QuotientBlob->overwrite(DividendValue/DivisorValue);
        } break;
        case PreDef_Float: {
            auto DividendValue = task.get<double>(DividendBlob),
                 DivisorValue = task.get<double>(DivisorBlob),
                 QuotientValue = DividendValue/DivisorValue;
            if(rest) RestBlob->overwrite(modf(QuotientValue, &QuotientValue));
            if(quotient) QuotientBlob->overwrite(QuotientValue);
        } break;
        default:
            task.throwException("Invalid Dividend or Divisor Blob");
    }
    task.popCallStack();
}

std::map<Symbol, void(*)(Task&)> PreDefProcedures = {
    {PreDef_Search, PreDefProcedure_Search},
    {PreDef_Link, PreDefProcedure_Triple<PreDefProcedure_Link>},
    {PreDef_Unlink, PreDefProcedure_Triple<PreDefProcedure_Unlink>},
    {PreDef_Create, PreDefProcedure_Create},
    {PreDef_Destroy, PreDefProcedure_Destroy},
    {PreDef_GetEnv, PreDefProcedure_GetEnv},
    {PreDef_Push, PreDefProcedure_Push},
    {PreDef_Pop, PreDefProcedure_Pop},
    {PreDef_Branch, PreDefProcedure_Branch},
    {PreDef_Exception, PreDefProcedure_Exception},
    {PreDef_Serialize, PreDefProcedure_Serialize},
    {PreDef_Deserialize, PreDefProcedure_Deserialize},
    {PreDef_SliceBlob, PreDefProcedure_SliceBlob},
    {PreDef_AllocateBlob, PreDefProcedure_AllocateBlob},
    {PreDef_CloneBlob, PreDefProcedure_CloneBlob},
    {PreDef_GetBlobLength, PreDefProcedure_GetBlobLength},
    {PreDef_NumericCast, PreDefProcedure_NumericCast},
    {PreDef_Equal, PreDefProcedure_Equal},
    {PreDef_LessThan, PreDefProcedure_CompareLogic<PreDefProcedure_LessThan>},
    {PreDef_LessEqual, PreDefProcedure_CompareLogic<PreDefProcedure_LessEqual>},
    {PreDef_Complement, PreDefProcedure_Complement},
    {PreDef_ClearShift, PreDefProcedure_Shift<PreDefProcedure_ClearShift>},
    {PreDef_CloneShift, PreDefProcedure_Shift<PreDefProcedure_CloneShift>},
    {PreDef_BarrelShift, PreDefProcedure_Shift<PreDefProcedure_BarrelShift>},
    {PreDef_And, PreDefProcedure_AssociativeCommutativeBitWise<PreDefProcedure_And>},
    {PreDef_Or, PreDefProcedure_AssociativeCommutativeBitWise<PreDefProcedure_Or>},
    {PreDef_Xor, PreDefProcedure_AssociativeCommutativeBitWise<PreDefProcedure_Xor>},
    {PreDef_Add, PreDefProcedure_AssociativeCommutativeArithmetic<PreDefProcedure_Add>},
    {PreDef_Multiply, PreDefProcedure_AssociativeCommutativeArithmetic<PreDefProcedure_Multiply>},
    {PreDef_Subtract, PreDefProcedure_Subtract},
    {PreDef_Divide, PreDefProcedure_Divide}
};

bool Task::step() {
    if(!running())
        return false;

    Symbol parentBlock = block, parentFrame = frame,
           procedure = PreDef_Void, next = PreDef_Void,
           execute;
    if(!getUncertain(parentFrame, PreDef_Execute, execute)) {
        popCallStack();
        return true;
    }

    try {
        block = context->create();
        setFrame<true, false>(context->create({
            {PreDef_Holds, parentFrame},
            {PreDef_Parent, parentFrame},
            {PreDef_Holds, block},
            {PreDef_Block, block},
        }));

        query(12, {execute, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
            // TODO: Restructure, throw exeption if multiple procedure or next occur
            switch(result.pos[0]) {
                case PreDef_BlobType:
                case PreDef_Holds:
                return;
                case PreDef_Procedure:
                    procedure = getUncertainWithFallback(parentBlock, result.pos[1]);
                break;
                case PreDef_Next:
                    next = getUncertainWithFallback(parentBlock, result.pos[1]);
                break;
                case PreDef_Catch:
                    link({frame, result.pos[0], getUncertainWithFallback(parentBlock, result.pos[1])});
                break;
                default:
                    if(query(0, {execute, PreDef_Global, result.pos[1]}) == 0 &&
                       query(9, {parentBlock, result.pos[1], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                        link({block, result.pos[0], resultB.pos[0]});
                    }) == 0)
                        link({block, result.pos[0], result.pos[1]});
                break;
            }
        });
        if(context->debug)
            link({frame, PreDef_Procedure, procedure});

        if(procedure == PreDef_Void)
            throwException("Expected Procedure");

        if(next == PreDef_Void)
            unlink(parentFrame, PreDef_Execute);
        else
            setSolitary({parentFrame, PreDef_Execute, next});

        auto iter = PreDefProcedures.find(procedure);
        if(iter == PreDefProcedures.end()) {
            execute = getGuaranteed(procedure, PreDef_Execute);
            link({frame, PreDef_Execute, execute});
        } else
            iter->second(*this);
    } catch(Exception) {
        PreDefProcedure_Exception(*this);
    }

    return true;
}
