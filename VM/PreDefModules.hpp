#include "Deserialize.hpp"

#define PreDefModule(Name) void PreDefModule_##Name(Task& task)

/*void IterateArray(Task& exec, Symbol symbol, std::function<void(ArchitectureType, Symbol)> callback) {
    task.query(21, {symbol, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(task.query(0, {result.pos[0], PreDef_Extend, PreDef_Natural}) == 1) {
            ArchitectureType index = task.get<ArchitectureType>(task.context->getExtend(result.pos[0]));
            task.query(9, {symbol, result.pos[0], PreDef_Void}, [&](Triple result2, ArchitectureType) {
                callback(index, result2.pos[0]);
            });
        }
    });
}

void FillSymbolArray(Task& exec, Symbol* storrage, ArchitectureType maxCount = 3) {
    ArchitectureType counter = maxCount;
    IterateArray(exec, task.block, [&](ArchitectureType index, Symbol value) {
        if(index >= maxCount)
            task.throwException("Invalid Input");
        storrage[index] = value;
        --counter;
    });
    if(counter != 0)
        task.throwException("Too few Inputs");
}*/

PreDefModule(Search) {
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
    getSymbolAndExtendByName(Output)

    ArchitectureType extendSize = 0, index = 0;
    OutputExtend->reallocate(ArchitectureSize);
    auto count = task.query(modes[0] + modes[1]*3 + modes[2]*9, triple,
    [&](Triple result, ArchitectureType size) {
        extendSize = ArchitectureSize*size*(index+1);
        if(extendSize > OutputExtend->size)
            OutputExtend->reallocate(OutputExtend->size*2);
        bitWiseCopyForward(OutputExtend->data.get(), result.pos, ArchitectureSize*size, ArchitectureSize*size*index, 0);
        ++index;
    });
    OutputExtend->reallocate(extendSize);

    Symbol CountSymbol;
    if(task.getUncertain(task.block, PreDef_Count, CountSymbol)) {
        task.setSolitary({CountSymbol, PreDef_Extend, PreDef_Natural});
        task.context->getExtend(CountSymbol)->overwrite(count);
    }

    task.popCallStack();
}

struct PreDefModule_Link {
    static bool e(Context* context, Triple triple) { return context->link(triple); };
};

struct PreDefModule_Unlink {
    static bool e(Context* context, Triple triple) { return context->unlink({triple}); };
};

template<class op>
PreDefModule(Triple) {
    getSymbolByName(Entity)
    getSymbolByName(Attribute)
    getSymbolByName(Value)
    ArchitectureType result = op::e(task.context, {EntitySymbol, AttributeSymbol, ValueSymbol});
    Symbol OutputSymbol;
    if(task.getUncertain(task.block, PreDef_Output, OutputSymbol)) {
        task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
        task.context->getExtend(OutputSymbol)->overwrite(result);
    }
    task.popCallStack();
}

PreDefModule(Create) {
    Symbol InputSymbol, ValueSymbol;
    bool input = task.getUncertain(task.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = task.get<ArchitectureType>(task.context->getExtend(InputSymbol));

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

PreDefModule(Destroy) {
    std::set<Symbol> toDestroy;
    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        toDestroy.insert(result.pos[0]);
    }) == 0)
        task.throwException("Expected more Inputs");
    for(Symbol symbol : toDestroy)
        task.destroy(symbol);
    task.popCallStack();
}

PreDefModule(GetEnv) {
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

PreDefModule(Push) {
    getSymbolByName(Execute)
    task.block = task.popCallStackTargetSymbol();
    Symbol parentFrame = task.frame;
    task.frame = task.context->create({
        {PreDef_Holds, parentFrame},
        {PreDef_Parent, parentFrame},
        {PreDef_Holds, task.block},
        {PreDef_Block, task.block},
        {PreDef_Execute, ExecuteSymbol}
    });
    task.setSolitary({task.task, PreDef_Frame, task.frame});
}

PreDefModule(Pop) {
    getSymbolAndExtendByName(Count)
    checkExtendType(Count, PreDef_Natural)
    auto CountValue = task.get<uint64_t>(CountExtend);
    if(CountValue < 2)
        task.throwException("Invalid Count Value");
    for(; CountValue > 0 && task.popCallStack(); --CountValue);
}

PreDefModule(Branch) {
    getUncertainSymbolAndExtendByName(Input, 1)
    getSymbolByName(Branch)
    task.popCallStack();
    if(InputValue != 0)
        task.setSolitary({task.frame, PreDef_Execute, BranchSymbol});
}

PreDefModule(Exception) {
    Symbol ExceptionSymbol;
    if(task.getUncertain(task.block, PreDef_Input, ExceptionSymbol)) {
        Symbol currentFrame = ExceptionSymbol;
        while(task.getUncertain(currentFrame, PreDef_Parent, currentFrame));
        task.link({currentFrame, PreDef_Parent, task.frame});
    } else
        ExceptionSymbol = task.frame;

    Symbol execute, prevFrame = task.frame;
    while(true) {
        if(task.frame != prevFrame && task.getUncertain(task.frame, PreDef_Catch, execute)) {
            task.unlink(prevFrame, PreDef_Parent);
            task.setSolitary({task.frame, PreDef_Execute, execute});
            task.block = task.getGuaranteed(task.frame, PreDef_Block);
            task.link({task.block, PreDef_Holds, ExceptionSymbol});
            task.link({task.block, PreDef_Exception, ExceptionSymbol});
            task.setSolitary({task.task, PreDef_Execute, task.frame});
            return;
        }
        prevFrame = task.frame;
        if(!task.getUncertain(task.frame, PreDef_Parent, task.frame))
            break;
    }

    task.frame = task.block = PreDef_Void;
    task.status = PreDef_Exception;
    task.unlink(task.task, PreDef_Frame);
    task.setSolitary({task.task, PreDef_Exception, ExceptionSymbol});
    task.setSolitary({task.task, PreDef_Status, PreDef_Exception});
}

PreDefModule(Serialize) {
    getSymbolByName(Input)
    getSymbolAndExtendByName(Output)

    std::stringstream stream;
    task.serializeExtend(stream, InputSymbol);
    OutputExtend->overwrite(stream);
    task.popCallStack();
}

PreDefModule(Deserialize) {
    Deserialize{task};
}

PreDefModule(CloneExtend) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Output)
    Symbol type;
    if(task.getUncertain(InputSymbol, PreDef_Extend, type))
        task.setSolitary({OutputSymbol, PreDef_Extend, type});
    else
        task.unlink(OutputSymbol, PreDef_Extend);
    OutputExtend->overwrite(*InputExtend);
    task.popCallStack();
}

PreDefModule(SliceExtend) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Target)
    getUncertainSymbolAndExtendByName(Count, InputExtend->size)
    getUncertainSymbolAndExtendByName(Destination, 0)
    getUncertainSymbolAndExtendByName(Source, 0)
    if(!TargetExtend->replacePartial(*InputExtend, CountValue, DestinationValue, SourceValue))
        task.throwException("Invalid Count, Destination or SrcOffset Value");
    task.popCallStack();
}

PreDefModule(AllocateExtend) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Target)
    checkExtendType(Input, PreDef_Natural)
    TargetExtend->allocate(task.get<ArchitectureType>(InputExtend));
    task.popCallStack();
}

PreDefModule(ReallocateExtend) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Target)
    checkExtendType(Input, PreDef_Natural)
    TargetExtend->reallocate(task.get<ArchitectureType>(InputExtend));
    task.popCallStack();
}

PreDefModule(EraseFromExtend) {
    getSymbolAndExtendByName(Target)
    getUncertainSymbolAndExtendByName(Begin, 0)

    ArchitectureType EndValue;
    Symbol EndSymbol, CountSymbol;
    bool end = task.getUncertain(task.block, PreDef_End, EndSymbol),
         count = task.getUncertain(task.block, PreDef_Count, CountSymbol);
    if(end) {
        if(count)
            task.throwException("Count and End given");
        checkExtendType(End, PreDef_Natural)
        EndValue = task.get<ArchitectureType>(task.context->getExtend(EndSymbol));
    } else if(count) {
        checkExtendType(Count, PreDef_Natural)
        EndValue = BeginValue+task.get<ArchitectureType>(task.context->getExtend(CountSymbol));
    } else
        EndValue = TargetExtend->size;

    if(!TargetExtend->erase(BeginValue, EndValue))
        task.throwException("Invalid Begin or End Value");
    task.popCallStack();
}

PreDefModule(InsertIntoExtend) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Target)
    getUncertainSymbolAndExtendByName(Begin, TargetExtend->size)
    if(!TargetExtend->insert(*InputExtend, BeginValue))
        task.throwException("Invalid Begin Value");
    task.popCallStack();
}

PreDefModule(GetExtendLength) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Output)
    task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
    OutputExtend->overwrite(InputExtend->size);
    task.popCallStack();
}

template<typename T>
void PreDefModule_NumericCastTo(Task& task, Symbol type, Extend* OutputExtend, Extend* InputExtend) {
    switch(type) {
        case PreDef_Natural:
            OutputExtend->overwrite(static_cast<T>(task.get<uint64_t>(InputExtend)));
        break;
        case PreDef_Integer:
            OutputExtend->overwrite(static_cast<T>(task.get<int64_t>(InputExtend)));
        break;
        case PreDef_Float:
            OutputExtend->overwrite(static_cast<T>(task.get<double>(InputExtend)));
        break;
        default:
            task.throwException("Invalid Input Extend");
    }
}

PreDefModule(NumericCast) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(To)
    getSymbolAndExtendByName(Output)

    Symbol type;
    if(!task.getUncertain(InputSymbol, PreDef_Extend, type))
        task.throwException("Invalid Input Extend");
    task.setSolitary({OutputSymbol, PreDef_Extend, ToSymbol});
    switch(ToSymbol) {
        case PreDef_Natural:
            PreDefModule_NumericCastTo<uint64_t>(task, type, OutputExtend, InputExtend);
        break;
        case PreDef_Integer:
            PreDefModule_NumericCastTo<int64_t>(task, type, OutputExtend, InputExtend);
        break;
        case PreDef_Float:
            PreDefModule_NumericCastTo<double>(task, type, OutputExtend, InputExtend);
        break;
        default:
            task.throwException("Invalid To Value");
    }
    task.popCallStack();
}

PreDefModule(Equal) {
    Symbol type;
    Extend* FirstExtend;
    uint64_t OutputValue = 1;
    bool first = true;

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(OutputValue == 0) return;
        Symbol _type = PreDef_Void;
        task.getUncertain(result.pos[0], PreDef_Extend, _type);
        if(first) {
            first = false;
            type = _type;
            FirstExtend = task.context->getExtend(result.pos[0]);
        } else if(type == _type) {
            Extend* InputExtend = task.context->getExtend(result.pos[0]);
            if(type == PreDef_Float) {
                if(task.get<double>(InputExtend) != task.get<double>(FirstExtend))
                    OutputValue = 0;
            } else if(InputExtend->compare(*FirstExtend) != 0)
                OutputValue = 0;
        } else
            task.throwException("Inputs have different types");
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolAndExtendByName(Output)
    task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
    OutputExtend->overwrite(OutputValue);
    task.popCallStack();
}

struct PreDefModule_LessThan {
    static bool n(uint64_t i, uint64_t c) { return i < c; };
    static bool i(int64_t i, int64_t c) { return i < c; };
    static bool f(double i, double c) { return i < c; };
    static bool s(const Extend& i, const Extend& c) { return i.compare(c) < 0; };
};

struct PreDefModule_LessEqual {
    static bool n(uint64_t i, uint64_t c) { return i <= c; };
    static bool i(int64_t i, int64_t c) { return i <= c; };
    static bool f(double i, double c) { return i <= c; };
    static bool s(const Extend& i, const Extend& c) { return i.compare(c) <= 0; };
};

template<class op>
PreDefModule(CompareLogic) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Comparandum)
    getSymbolAndExtendByName(Output)

    Symbol type, _type;
    task.getUncertain(InputSymbol, PreDef_Extend, type);
    task.getUncertain(ComparandumSymbol, PreDef_Extend, _type);
    if(type != _type)
        task.throwException("Input and Comparandum have different types");

    uint64_t result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(task.get<uint64_t>(InputExtend), task.get<uint64_t>(ComparandumExtend));
        break;
        case PreDef_Integer:
            result = op::i(task.get<int64_t>(InputExtend), task.get<int64_t>(ComparandumExtend));
        break;
        case PreDef_Float:
            result = op::f(task.get<double>(InputExtend), task.get<double>(ComparandumExtend));
        break;
        default:
            result = op::s(*InputExtend, *ComparandumExtend);
        break;
    }
    task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
    OutputExtend->overwrite(result);
    task.popCallStack();
}

PreDefModule(Complement) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Output)

    task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
    OutputExtend->overwrite(~task.get<uint64_t>(InputExtend));
    task.popCallStack();
}

struct PreDefModule_ClearShift {
    static void n(ArchitectureType& dst, uint64_t count) { dst >>= count; };
    static void p(ArchitectureType& dst, uint64_t count) { dst <<= count; };
};

struct PreDefModule_CloneShift {
    static void n(ArchitectureType& dst, uint64_t count) {
        *reinterpret_cast<int64_t*>(&dst) >>= count;
    };
    static void p(ArchitectureType& dst, uint64_t count) {
        ArchitectureType lowestBit = dst&BitMask<uint64_t>::one;
        dst <<= count;
        dst |= lowestBit*BitMask<uint64_t>::fillLSBs(count);
    };
};

struct PreDefModule_BarrelShift {
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
PreDefModule(Shift) {
    getSymbolAndExtendByName(Input)
    getSymbolAndExtendByName(Count)
    getSymbolAndExtendByName(Output)
    checkExtendType(Count, PreDef_Integer)

    auto result = task.get<uint64_t>(InputExtend);
    auto CountValue = task.get<int64_t>(CountExtend);
    if(CountValue < 0)
        op::n(result, -CountValue);
    else
        op::p(result, CountValue);

    task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
    OutputExtend->overwrite(result);
    task.popCallStack();
}

struct PreDefModule_And {
    static void n(uint64_t& dst, uint64_t src) { dst &= src; };
};

struct PreDefModule_Or {
    static void n(uint64_t& dst, uint64_t src) { dst |= src; };
};

struct PreDefModule_Xor {
    static void n(uint64_t& dst, uint64_t src) { dst ^= src; };
};

template<class op>
PreDefModule(AssociativeCommutativeBitWise) {
    uint64_t OutputValue;
    bool first = true;

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Extend* InputExtend = task.context->getExtend(result.pos[0]);
        if(first) {
            first = false;
            OutputValue = task.get<uint64_t>(InputExtend);
        } else
            op::n(OutputValue, task.get<uint64_t>(InputExtend));
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolAndExtendByName(Output)
    task.setSolitary({OutputSymbol, PreDef_Extend, PreDef_Natural});
    OutputExtend->overwrite(OutputValue);
    task.popCallStack();
}

struct PreDefModule_Add {
    static void n(uint64_t& dst, uint64_t src) { dst += src; };
    static void i(int64_t& dst, int64_t src) { dst += src; };
    static void f(double& dst, double src) { dst += src; };
};

struct PreDefModule_Multiply {
    static void n(uint64_t& dst, uint64_t src) { dst *= src; };
    static void i(int64_t& dst, int64_t src) { dst *= src; };
    static void f(double& dst, double src) { dst *= src; };
};

template<class op>
PreDefModule(AssociativeCommutativeArithmetic) {
    Symbol type;
    union {
        uint64_t n;
        int64_t i;
        double f;
    } aux;
    bool first = true;

    if(task.query(9, {task.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Symbol _type = task.getGuaranteed(result.pos[0], PreDef_Extend);
        Extend* InputExtend = task.context->getExtend(result.pos[0]);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = task.get<uint64_t>(InputExtend);
                break;
                case PreDef_Integer:
                    aux.i = task.get<int64_t>(InputExtend);
                break;
                case PreDef_Float:
                    aux.f = task.get<double>(InputExtend);
                break;
                default:
                    task.throwException("Invalid Input Extend");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, task.get<uint64_t>(InputExtend));
                break;
                case PreDef_Integer:
                    op::i(aux.i, task.get<int64_t>(InputExtend));
                break;
                case PreDef_Float:
                    op::f(aux.f, task.get<double>(InputExtend));
                break;
            }
        } else
            task.throwException("Inputs have different types");
    }) < 2)
        task.throwException("Expected more Input");

    getSymbolAndExtendByName(Output)
    task.setSolitary({OutputSymbol, PreDef_Extend, type});
    OutputExtend->overwrite(aux.n);
    task.popCallStack();
}

PreDefModule(Subtract) {
    getSymbolAndExtendByName(Minuend)
    getSymbolAndExtendByName(Subtrahend)
    getSymbolAndExtendByName(Output)

    Symbol type = task.getGuaranteed(MinuendSymbol, PreDef_Extend);
    Symbol _type = task.getGuaranteed(SubtrahendSymbol, PreDef_Extend);
    if(type != _type)
        task.throwException("Minuend and Subtrahend have different types");

    task.setSolitary({OutputSymbol, PreDef_Extend, type});
    switch(type) {
        case PreDef_Natural:
            OutputExtend->overwrite(task.get<uint64_t>(MinuendExtend)-task.get<uint64_t>(SubtrahendExtend));
        break;
        case PreDef_Integer:
            OutputExtend->overwrite(task.get<int64_t>(MinuendExtend)-task.get<int64_t>(SubtrahendExtend));
        break;
        case PreDef_Float:
            OutputExtend->overwrite(task.get<double>(MinuendExtend)-task.get<double>(SubtrahendExtend));
        break;
        default:
            task.throwException("Invalid Minuend or Subtrahend Extend");
    }
    task.popCallStack();
}

PreDefModule(Divide) {
    getSymbolAndExtendByName(Dividend)
    getSymbolAndExtendByName(Divisor)

    Symbol type = task.getGuaranteed(DividendSymbol, PreDef_Extend);
    Symbol _type = task.getGuaranteed(DivisorSymbol, PreDef_Extend);
    if(type != _type)
        task.throwException("Dividend and Divisor have different types");

    Symbol RestSymbol, QuotientSymbol;
    Extend *RestExtend, *QuotientExtend;
    bool rest = task.getUncertain(task.block, PreDef_Rest, RestSymbol),
         quotient = task.getUncertain(task.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        task.setSolitary({RestSymbol, PreDef_Extend, type});
        RestExtend = task.context->getExtend(RestSymbol);
        if(RestExtend->size != ArchitectureSize)
            task.throwException("Invalid Rest Extend");
    }
    if(quotient) {
        task.setSolitary({QuotientSymbol, PreDef_Extend, type});
        QuotientExtend = task.context->getExtend(QuotientSymbol);
        if(QuotientExtend->size != ArchitectureSize)
            task.throwException("Invalid Quotient Extend");
    }

    if(!rest && !quotient)
        task.throwException("Expected Rest or Quotient");

    switch(type) {
        case PreDef_Natural: {
            auto DividendValue = task.get<uint64_t>(DividendExtend),
                 DivisorValue = task.get<uint64_t>(DivisorExtend);
            if(rest) RestExtend->overwrite(DividendValue%DivisorValue);
            if(quotient) QuotientExtend->overwrite(DividendValue/DivisorValue);
        } break;
        case PreDef_Integer: {
            auto DividendValue = task.get<int64_t>(DividendExtend),
                 DivisorValue = task.get<int64_t>(DivisorExtend);
            if(rest) RestExtend->overwrite(DividendValue%DivisorValue);
            if(quotient) QuotientExtend->overwrite(DividendValue/DivisorValue);
        } break;
        case PreDef_Float: {
            auto DividendValue = task.get<double>(DividendExtend),
                 DivisorValue = task.get<double>(DivisorExtend),
                 QuotientValue = DividendValue/DivisorValue;
            if(rest) RestExtend->overwrite(modf(QuotientValue, &QuotientValue));
            if(quotient) QuotientExtend->overwrite(QuotientValue);
        } break;
        default:
            task.throwException("Invalid Dividend or Divisor Extend");
    }
    task.popCallStack();
}

std::map<Symbol, void(*)(Task&)> PreDefModules = {
    {PreDef_Search, PreDefModule_Search},
    {PreDef_Link, PreDefModule_Triple<PreDefModule_Link>},
    {PreDef_Unlink, PreDefModule_Triple<PreDefModule_Unlink>},
    {PreDef_Create, PreDefModule_Create},
    {PreDef_Destroy, PreDefModule_Destroy},
    {PreDef_GetEnv, PreDefModule_GetEnv},
    {PreDef_Push, PreDefModule_Push},
    {PreDef_Pop, PreDefModule_Pop},
    {PreDef_Branch, PreDefModule_Branch},
    {PreDef_Exception, PreDefModule_Exception},
    {PreDef_Serialize, PreDefModule_Serialize},
    {PreDef_Deserialize, PreDefModule_Deserialize},
    {PreDef_CloneExtend, PreDefModule_CloneExtend},
    {PreDef_SliceExtend, PreDefModule_SliceExtend},
    {PreDef_AllocateExtend, PreDefModule_AllocateExtend},
    {PreDef_ReallocateExtend, PreDefModule_ReallocateExtend},
    {PreDef_EraseFromExtend, PreDefModule_EraseFromExtend},
    {PreDef_InsertIntoExtend, PreDefModule_InsertIntoExtend},
    {PreDef_GetExtendLength, PreDefModule_GetExtendLength},
    {PreDef_NumericCast, PreDefModule_NumericCast},
    {PreDef_Equal, PreDefModule_Equal},
    {PreDef_LessThan, PreDefModule_CompareLogic<PreDefModule_LessThan>},
    {PreDef_LessEqual, PreDefModule_CompareLogic<PreDefModule_LessEqual>},
    {PreDef_Complement, PreDefModule_Complement},
    {PreDef_ClearShift, PreDefModule_Shift<PreDefModule_ClearShift>},
    {PreDef_CloneShift, PreDefModule_Shift<PreDefModule_CloneShift>},
    {PreDef_BarrelShift, PreDefModule_Shift<PreDefModule_BarrelShift>},
    {PreDef_And, PreDefModule_AssociativeCommutativeBitWise<PreDefModule_And>},
    {PreDef_Or, PreDefModule_AssociativeCommutativeBitWise<PreDefModule_Or>},
    {PreDef_Xor, PreDefModule_AssociativeCommutativeBitWise<PreDefModule_Xor>},
    {PreDef_Add, PreDefModule_AssociativeCommutativeArithmetic<PreDefModule_Add>},
    {PreDef_Multiply, PreDefModule_AssociativeCommutativeArithmetic<PreDefModule_Multiply>},
    {PreDef_Subtract, PreDefModule_Subtract},
    {PreDef_Divide, PreDefModule_Divide}
};

bool Task::step() {
    if(!running())
        return false;

    Symbol parentBlock = block, parentFrame = frame,
           module = PreDef_Void, next = PreDef_Void,
           execute;
    if(!getUncertain(parentFrame, PreDef_Execute, execute)) {
        popCallStack();
        return true;
    }

    block = context->create();
    frame = context->create({
        {PreDef_Holds, parentFrame},
        {PreDef_Parent, parentFrame},
        {PreDef_Holds, block},
        {PreDef_Block, block},
    });

    try {
        setSolitary({task, PreDef_Frame, frame});

        query(12, {execute, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
            Symbol value;
            if(!getUncertain(parentBlock, result.pos[1], value))
                value = result.pos[1];
            switch(result.pos[0]) {
                case PreDef_Extend:
                case PreDef_Holds:
                return;
                case PreDef_Module:
                    module = value;
                break;
                case PreDef_Next:
                    next = value;
                break;
                case PreDef_Catch:
                    link({frame, result.pos[0], value});
                break;
                default:
                    link({block, result.pos[0], value});
                break;
            }
        });
        if(context->debug)
            link({frame, PreDef_Module, module});

        if(module == PreDef_Void)
            throwException("Expected Module");

        if(next == PreDef_Void)
            unlink(parentFrame, PreDef_Execute);
        else
            setSolitary({parentFrame, PreDef_Execute, next});

        auto iter = PreDefModules.find(module);
        if(iter == PreDefModules.end()) {
            execute = getGuaranteed(module, PreDef_Execute);
            link({frame, PreDef_Execute, execute});
        } else
            iter->second(*this);
    } catch(Exception) {
        PreDefModule_Exception(*this);
    }

    return true;
}
