#include "Deserialize.hpp"

#define PreDefProcedure(Name) void PreDefProcedure_##Name(Thread& thread)

PreDefProcedure(Search) {
    Triple triple;
    uint8_t modes[3] = {2, 2, 2};
    Symbol posNames[3] = {PreDef_Entity, PreDef_Attribute, PreDef_Value};
    thread.query(9, {thread.block, PreDef_Varying, PreDef_Void}, [&](Triple result, ArchitectureType) {
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
    getSymbolObjectByName(Output)

    ArchitectureType blobSize = 0, index = 0;
    OutputSymbolObject->allocateBlob(ArchitectureSize);
    auto count = thread.query(modes[0] + modes[1]*3 + modes[2]*9, triple,
    [&](Triple result, ArchitectureType size) {
        blobSize = ArchitectureSize*size*(index+1);
        if(blobSize > OutputSymbolObject->blobSize)
            OutputSymbolObject->reallocateBlob(std::max(blobSize, OutputSymbolObject->blobSize*2));
        bitwiseCopy<-1>(OutputSymbolObject->blobData.get(), result.pos, ArchitectureSize*size*index, 0, ArchitectureSize*size);
        ++index;
    });
    OutputSymbolObject->reallocateBlob(blobSize);

    Symbol CountSymbol;
    if(thread.getUncertain(thread.block, PreDef_Count, CountSymbol)) {
        thread.setSolitary({CountSymbol, PreDef_BlobType, PreDef_Natural});
        Ontology::getSymbolObject(CountSymbol)->overwriteBlob(count);
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
    Symbol OutputSymbol;
    if(thread.getUncertain(thread.block, PreDef_Output, OutputSymbol)) {
        thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
        Ontology::getSymbolObject(OutputSymbol)->overwriteBlob(result);
    }
    thread.popCallStack();
}

PreDefProcedure(Create) {
    Symbol InputSymbol, ValueSymbol;
    bool input = thread.getUncertain(thread.block, PreDef_Input, InputSymbol);
    if(input)
        ValueSymbol = thread.accessBlobAs<ArchitectureType>(Ontology::getSymbolObject(InputSymbol));

    Symbol TargetSymbol = thread.getTargetSymbol();
    if(thread.query(9, {thread.block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Symbol OutputSymbol = result.pos[0];
        if(!input)
            ValueSymbol = Ontology::create();
        thread.setSolitary({TargetSymbol, OutputSymbol, ValueSymbol});
        thread.link({TargetSymbol, PreDef_Holds, ValueSymbol});
    }) == 0)
        thread.throwException("Expected Output");
    thread.popCallStack();
}

PreDefProcedure(Destroy) {
    Set<Symbol, true> symbols;
    if(thread.query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        symbols.insertElement(result.pos[0]);
    }) == 0)
        thread.throwException("Expected more Inputs");
    symbols.iterate([&](Symbol symbol) {
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
    getSymbolObjectByName(Count)
    checkBlobType(Count, PreDef_Natural)
    auto CountValue = thread.accessBlobAs<uint64_t>(CountSymbolObject);
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
    Symbol ExceptionSymbol = thread.frame;
    if(thread.getUncertain(thread.block, PreDef_Exception, ExceptionSymbol)) {
        Symbol currentFrame = ExceptionSymbol;
        while(thread.getUncertain(currentFrame, PreDef_Parent, currentFrame));
        thread.link({currentFrame, PreDef_Parent, thread.frame});
    }

    Symbol execute, currentFrame = thread.frame, prevFrame = currentFrame;
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
    getSymbolObjectByName(Output)
    Ontology::blobIndex.eraseElement(OutputSymbol);
    Serialize serialize(thread, OutputSymbol);
    serialize.serializeBlob(InputSymbol);
    serialize.finalizeSymbol();
    thread.popCallStack();
}

PreDefProcedure(Deserialize) {
    Deserialize{thread};
}

PreDefProcedure(SliceBlob) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Target)
    getUncertainValueByName(Count, InputSymbolObject->blobSize)
    getUncertainValueByName(Destination, 0)
    getUncertainValueByName(Source, 0)
    Ontology::blobIndex.eraseElement(TargetSymbol);
    if(!TargetSymbolObject->overwriteBlobPartial(*InputSymbolObject, DestinationValue, SourceValue, CountValue))
        thread.throwException("Invalid Count, Destination or SrcOffset Value");
    thread.popCallStack();
}

PreDefProcedure(AllocateBlob) {
    getSymbolObjectByName(Input)
    getUncertainValueByName(Preserve, 0)
    getSymbolObjectByName(Target)
    checkBlobType(Input, PreDef_Natural)
    Ontology::blobIndex.eraseElement(TargetSymbol);
    TargetSymbolObject->allocateBlob(thread.accessBlobAs<ArchitectureType>(InputSymbolObject), PreserveValue);
    thread.popCallStack();
}

PreDefProcedure(CloneBlob) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)
    OutputSymbolObject->overwriteBlob(*InputSymbolObject);
    Symbol type = PreDef_Void;
    thread.getUncertain(InputSymbol, PreDef_BlobType, type);
    thread.setSolitary({OutputSymbol, PreDef_BlobType, type});
    thread.popCallStack();
}

PreDefProcedure(GetBlobLength) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(InputSymbolObject->blobSize);
    thread.popCallStack();
}

template<typename T>
void PreDefProcedure_NumericCastTo(Thread& thread, Symbol type, SymbolObject* OutputSymbolObject, SymbolObject* InputSymbolObject) {
    switch(type) {
        case PreDef_Natural:
            OutputSymbolObject->overwriteBlob(static_cast<T>(thread.accessBlobAs<uint64_t>(InputSymbolObject)));
            break;
        case PreDef_Integer:
            OutputSymbolObject->overwriteBlob(static_cast<T>(thread.accessBlobAs<int64_t>(InputSymbolObject)));
            break;
        case PreDef_Float:
            OutputSymbolObject->overwriteBlob(static_cast<T>(thread.accessBlobAs<double>(InputSymbolObject)));
            break;
        default:
            thread.throwException("Invalid Input SymbolObject");
    }
}

PreDefProcedure(NumericCast) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(To)
    getSymbolObjectByName(Output)

    Symbol type;
    if(!thread.getUncertain(InputSymbol, PreDef_BlobType, type))
        thread.throwException("Invalid Input SymbolObject");
    thread.setSolitary({OutputSymbol, PreDef_BlobType, ToSymbol});
    switch(ToSymbol) {
        case PreDef_Natural:
            PreDefProcedure_NumericCastTo<uint64_t>(thread, type, OutputSymbolObject, InputSymbolObject);
            break;
        case PreDef_Integer:
            PreDefProcedure_NumericCastTo<int64_t>(thread, type, OutputSymbolObject, InputSymbolObject);
            break;
        case PreDef_Float:
            PreDefProcedure_NumericCastTo<double>(thread, type, OutputSymbolObject, InputSymbolObject);
            break;
        default:
            thread.throwException("Invalid To Value");
    }
    thread.popCallStack();
}

PreDefProcedure(Equal) {
    Symbol type;
    SymbolObject* FirstSymbolObject;
    uint64_t OutputValue = 1;
    bool first = true;

    if(thread.query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        if(OutputValue == 0)
            return;
        Symbol _type = PreDef_Void;
        thread.getUncertain(result.pos[0], PreDef_BlobType, _type);
        if(first) {
            first = false;
            type = _type;
            FirstSymbolObject = Ontology::getSymbolObject(result.pos[0]);
        } else if(type == _type) {
            SymbolObject* InputSymbolObject = Ontology::getSymbolObject(result.pos[0]);
            if(type == PreDef_Float) {
                if(thread.accessBlobAs<double>(InputSymbolObject) != thread.accessBlobAs<double>(FirstSymbolObject))
                    OutputValue = 0;
            } else if(InputSymbolObject->compareBlob(*FirstSymbolObject) != 0)
                OutputValue = 0;
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");

    getSymbolObjectByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(OutputValue);
    thread.popCallStack();
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
    thread.getUncertain(InputSymbol, PreDef_BlobType, type);
    thread.getUncertain(ComparandumSymbol, PreDef_BlobType, _type);
    if(type != _type)
        thread.throwException("Input and Comparandum have different types");

    uint64_t result;
    switch(type) {
        case PreDef_Natural:
            result = op::n(thread.accessBlobAs<uint64_t>(InputSymbolObject), thread.accessBlobAs<uint64_t>(ComparandumSymbolObject));
            break;
        case PreDef_Integer:
            result = op::i(thread.accessBlobAs<int64_t>(InputSymbolObject), thread.accessBlobAs<int64_t>(ComparandumSymbolObject));
            break;
        case PreDef_Float:
            result = op::f(thread.accessBlobAs<double>(InputSymbolObject), thread.accessBlobAs<double>(ComparandumSymbolObject));
            break;
        default:
            result = op::s(*InputSymbolObject, *ComparandumSymbolObject);
            break;
    }
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Natural});
    OutputSymbolObject->overwriteBlob(result);
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
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Count)
    getSymbolObjectByName(Output)
    checkBlobType(Count, PreDef_Natural)

    auto result = thread.accessBlobAs<uint64_t>(InputSymbolObject);
    auto CountValue = thread.accessBlobAs<uint64_t>(CountSymbolObject);
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
    OutputSymbolObject->overwriteBlob(result);
    thread.popCallStack();
}

PreDefProcedure(BitwiseComplement) {
    getSymbolObjectByName(Input)
    getSymbolObjectByName(Output)

    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    OutputSymbolObject->overwriteBlob(~thread.accessBlobAs<uint64_t>(InputSymbolObject));
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

    if(thread.query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        SymbolObject* InputSymbolObject = Ontology::getSymbolObject(result.pos[0]);
        if(first) {
            first = false;
            OutputValue = thread.accessBlobAs<uint64_t>(InputSymbolObject);
        } else
            op::n(OutputValue, thread.accessBlobAs<uint64_t>(InputSymbolObject));
    }) < 2)
        thread.throwException("Expected more Input");

    getSymbolObjectByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, PreDef_Void});
    OutputSymbolObject->overwriteBlob(OutputValue);
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
    Symbol type;
    union {
        uint64_t n;
        int64_t i;
        double f;
    } aux;
    bool first = true;

    if(thread.query(9, {thread.block, PreDef_Input, PreDef_Void}, [&](Triple result, ArchitectureType) {
        Symbol _type = thread.getGuaranteed(result.pos[0], PreDef_BlobType);
        SymbolObject* InputSymbolObject = Ontology::getSymbolObject(result.pos[0]);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case PreDef_Natural:
                    aux.n = thread.accessBlobAs<uint64_t>(InputSymbolObject);
                    break;
                case PreDef_Integer:
                    aux.i = thread.accessBlobAs<int64_t>(InputSymbolObject);
                    break;
                case PreDef_Float:
                    aux.f = thread.accessBlobAs<double>(InputSymbolObject);
                    break;
                default:
                    thread.throwException("Invalid Input SymbolObject");
            }
        } else if(type == _type) {
            switch(type) {
                case PreDef_Natural:
                    op::n(aux.n, thread.accessBlobAs<uint64_t>(InputSymbolObject));
                    break;
                case PreDef_Integer:
                    op::i(aux.i, thread.accessBlobAs<int64_t>(InputSymbolObject));
                    break;
                case PreDef_Float:
                    op::f(aux.f, thread.accessBlobAs<double>(InputSymbolObject));
                    break;
            }
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");

    getSymbolObjectByName(Output)
    thread.setSolitary({OutputSymbol, PreDef_BlobType, type});
    OutputSymbolObject->overwriteBlob(aux.n);
    thread.popCallStack();
}

PreDefProcedure(Subtract) {
    getSymbolObjectByName(Minuend)
    getSymbolObjectByName(Subtrahend)
    getSymbolObjectByName(Output)

    Symbol type = thread.getGuaranteed(MinuendSymbol, PreDef_BlobType);
    Symbol _type = thread.getGuaranteed(SubtrahendSymbol, PreDef_BlobType);
    if(type != _type)
        thread.throwException("Minuend and Subtrahend have different types");

    thread.setSolitary({OutputSymbol, PreDef_BlobType, type});
    switch(type) {
        case PreDef_Natural:
            OutputSymbolObject->overwriteBlob(thread.accessBlobAs<uint64_t>(MinuendSymbolObject)-thread.accessBlobAs<uint64_t>(SubtrahendSymbolObject));
            break;
        case PreDef_Integer:
            OutputSymbolObject->overwriteBlob(thread.accessBlobAs<int64_t>(MinuendSymbolObject)-thread.accessBlobAs<int64_t>(SubtrahendSymbolObject));
            break;
        case PreDef_Float:
            OutputSymbolObject->overwriteBlob(thread.accessBlobAs<double>(MinuendSymbolObject)-thread.accessBlobAs<double>(SubtrahendSymbolObject));
            break;
        default:
            thread.throwException("Invalid Minuend or Subtrahend SymbolObject");
    }
    thread.popCallStack();
}

PreDefProcedure(Divide) {
    getSymbolObjectByName(Dividend)
    getSymbolObjectByName(Divisor)

    Symbol type = thread.getGuaranteed(DividendSymbol, PreDef_BlobType);
    Symbol _type = thread.getGuaranteed(DivisorSymbol, PreDef_BlobType);
    if(type != _type)
        thread.throwException("Dividend and Divisor have different types");

    Symbol RestSymbol, QuotientSymbol;
    SymbolObject *RestSymbolObject, *QuotientSymbolObject;
    bool rest = thread.getUncertain(thread.block, PreDef_Rest, RestSymbol),
         quotient = thread.getUncertain(thread.block, PreDef_Quotient, QuotientSymbol);
    if(rest) {
        thread.setSolitary({RestSymbol, PreDef_BlobType, type});
        RestSymbolObject = Ontology::getSymbolObject(RestSymbol);
        if(RestSymbolObject->blobSize != ArchitectureSize)
            thread.throwException("Invalid Rest SymbolObject");
    }
    if(quotient) {
        thread.setSolitary({QuotientSymbol, PreDef_BlobType, type});
        QuotientSymbolObject = Ontology::getSymbolObject(QuotientSymbol);
        if(QuotientSymbolObject->blobSize != ArchitectureSize)
            thread.throwException("Invalid Quotient SymbolObject");
    }

    if(!rest && !quotient)
        thread.throwException("Expected Rest or Quotient");

    switch(type) {
        case PreDef_Natural: {
            auto DividendValue =  thread.accessBlobAs<uint64_t>(DividendSymbolObject),
                 DivisorValue = thread.accessBlobAs<uint64_t>(DivisorSymbolObject);
            if(DivisorValue == 0)
                thread.throwException("Division by Zero");
            if(rest)
                RestSymbolObject->overwriteBlob(DividendValue%DivisorValue);
            if(quotient)
                QuotientSymbolObject->overwriteBlob(DividendValue/DivisorValue);
        }   break;
        case PreDef_Integer: {
            auto DividendValue = thread.accessBlobAs<int64_t>(DividendSymbolObject),
                 DivisorValue = thread.accessBlobAs<int64_t>(DivisorSymbolObject);
            if(DivisorValue == 0)
                thread.throwException("Division by Zero");
            if(rest)
                RestSymbolObject->overwriteBlob(DividendValue%DivisorValue);
            if(quotient)
                QuotientSymbolObject->overwriteBlob(DividendValue/DivisorValue);
        }   break;
        case PreDef_Float: {
            auto DividendValue = thread.accessBlobAs<double>(DividendSymbolObject),
                 DivisorValue = thread.accessBlobAs<double>(DivisorSymbolObject),
                 QuotientValue = DividendValue/DivisorValue;
            if(DivisorValue == 0.0)
                thread.throwException("Division by Zero");
            if(rest) {
                double integerPart = static_cast<int64_t>(QuotientValue);
                RestSymbolObject->overwriteBlob(QuotientValue-integerPart);
                QuotientValue = integerPart;
            }
            if(quotient)
                QuotientSymbolObject->overwriteBlob(QuotientValue);
        }   break;
        default:
            thread.throwException("Invalid Dividend or Divisor SymbolObject");
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
