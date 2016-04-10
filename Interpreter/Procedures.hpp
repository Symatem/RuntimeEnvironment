#include "Deserialize.hpp"

#define Procedure(Name) void Procedure##Name(Thread& thread)

Procedure(Search) {
    Ontology::Triple triple;
    NativeNaturalType modes[3] = {2, 2, 2}, varyingCount = 0;
    Symbol posNames[3] = {Ontology::EntitySymbol, Ontology::AttributeSymbol, Ontology::ValueSymbol};
    Ontology::query(9, {thread.block, Ontology::VaryingSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
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
    getSymbolByName(output, Output)
    Ontology::BlobVector<false, Symbol> outputValue;
    outputValue.symbol = output;
    auto countValue = Ontology::query(modes[0] + modes[1]*3 + modes[2]*9, triple, [&](Ontology::Triple result) {
        for(NativeNaturalType i = 0; i < varyingCount; ++i)
            outputValue.push_back(result.pos[i]);
    });
    Symbol count;
    if(Ontology::getUncertain(thread.block, Ontology::CountSymbol, count)) {
        Ontology::setSolitary({count, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
        Storage::writeBlob(count, countValue);
    }
    thread.popCallStack();
}

struct ProcedureLink {
    static bool e(Thread& thread, Ontology::Triple triple) { return Ontology::link(triple); };
};

struct ProcedureUnlink {
    static bool e(Thread& thread, Ontology::Triple triple) { return Ontology::unlink(triple); };
};

template<typename op>
Procedure(Triple) {
    getSymbolByName(entity, Entity)
    getSymbolByName(attribute, Attribute)
    getSymbolByName(value, Value)
    NativeNaturalType outputValue = op::e(thread, {entity, attribute, value});
    Symbol output;
    if(Ontology::getUncertain(thread.block, Ontology::OutputSymbol, output)) {
        Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
        Storage::writeBlob(output, outputValue);
    }
    thread.popCallStack();
}

Procedure(Create) {
    Symbol input, value;
    bool inputExists = Ontology::getUncertain(thread.block, Ontology::InputSymbol, input);
    if(inputExists)
        value = thread.readBlob<NativeNaturalType>(input);
    Symbol target = thread.getTargetSymbol();
    if(Ontology::query(9, {thread.block, Ontology::OutputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol output = result.pos[0];
        if(!inputExists)
            value = Storage::createSymbol();
        Ontology::setSolitary({target, output, value});
        thread.link({target, Ontology::HoldsSymbol, value});
    }) == 0)
        thread.throwException("Expected Output");
    thread.popCallStack();
}

Procedure(Destroy) {
    Ontology::BlobSet<false, Symbol> symbols;
    symbols.symbol = Storage::createSymbol();
    Ontology::link({thread.block, Ontology::HoldsSymbol, symbols.symbol});
    if(Ontology::query(9, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        symbols.insertElement(result.pos[0]);
    }) == 0)
        thread.throwException("Expected more Inputs");
    symbols.iterate([&](Symbol symbol) {
        Ontology::unlink(symbol);
    });
    thread.popCallStack();
}

Procedure(Push) {
    getSymbolByName(execute, Execute)
    thread.link({thread.frame, Ontology::ExecuteSymbol, execute});
    thread.setBlock(thread.getTargetSymbol());
}

Procedure(Pop) {
    getSymbolByName(count, Count)
    checkBlobType(count, Ontology::NaturalSymbol)
    auto countValue = thread.readBlob<NativeNaturalType>(count);
    if(countValue < 2)
        thread.throwException("Invalid Count Value");
    for(; countValue > 0 && thread.popCallStack(); --countValue);
}

Procedure(Branch) {
    getUncertainValueByName(input, Input, 1)
    getSymbolByName(branch, Branch)
    thread.popCallStack();
    if(inputValue != 0)
        Ontology::setSolitary({thread.frame, Ontology::ExecuteSymbol, branch});
}

Procedure(Exception) {
    Symbol exception = thread.frame;
    if(Ontology::getUncertain(thread.block, Ontology::ExceptionSymbol, exception)) {
        Symbol currentFrame = exception;
        while(Ontology::getUncertain(currentFrame, Ontology::ParentSymbol, currentFrame));
        thread.link({currentFrame, Ontology::ParentSymbol, thread.frame});
    }
    Symbol execute, currentFrame = thread.frame, prevFrame = currentFrame;
    do {
        if(currentFrame != prevFrame && Ontology::getUncertain(currentFrame, Ontology::CatchSymbol, execute)) {
            Ontology::setSolitary({prevFrame, Ontology::ParentSymbol, Ontology::VoidSymbol});
            thread.setFrame(currentFrame, true);
            thread.link({thread.block, Ontology::HoldsSymbol, exception});
            thread.link({thread.block, Ontology::ExceptionSymbol, exception});
            Ontology::setSolitary({thread.frame, Ontology::ExecuteSymbol, execute});
            return;
        }
        prevFrame = currentFrame;
    } while(Ontology::getUncertain(currentFrame, Ontology::ParentSymbol, currentFrame));
    thread.setStatus(Ontology::ExceptionSymbol);
}

Procedure(Serialize) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Serialize serialize(thread, output);
    serialize.serializeBlob(input);
    thread.popCallStack();
}

Procedure(Deserialize) {
    Deserialize{thread};
}

Procedure(CloneBlob) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Storage::cloneBlob(output, input);
    Symbol type = Ontology::VoidSymbol;
    Ontology::getUncertain(input, Ontology::BlobTypeSymbol, type);
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, type});
    thread.popCallStack();
}

Procedure(SliceBlob) {
    getSymbolByName(input, Input)
    getSymbolByName(target, Target)
    getUncertainValueByName(count, Count, Storage::getBlobSize(input))
    getUncertainValueByName(destination, Destination, 0)
    getUncertainValueByName(source, Source, 0)
    if(!Storage::sliceBlob(target, input, destinationValue, sourceValue, countValue))
        thread.throwException("Invalid Count, Destination or SrcOffset Value");
    thread.popCallStack();
}

Procedure(GetBlobSize) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
    Storage::writeBlob(output, Storage::getBlobSize(input));
    thread.popCallStack();
}

struct ProcedureDecreaseBlobSize {
    static bool e(Symbol target, NativeNaturalType at, NativeNaturalType count) {
        return Storage::decreaseBlobSize(target, at, count);
    };
};

struct ProcedureIncreaseBlobSize {
    static bool e(Symbol target, NativeNaturalType at, NativeNaturalType count) {
        return Storage::increaseBlobSize(target, at, count);
    };
};

template<typename op>
Procedure(ChangeBlobSize) {
    getSymbolByName(target, Target)
    getSymbolByName(at, At)
    getSymbolByName(count, Count)
    checkBlobType(at, Ontology::NaturalSymbol)
    checkBlobType(count, Ontology::NaturalSymbol)
    if(!op::e(target, thread.readBlob<NativeNaturalType>(at), thread.readBlob<NativeNaturalType>(count)))
        thread.throwException("Invalid At or Count Value");
    thread.popCallStack();
}

template<typename T>
void ProcedureNumericCastTo(Thread& thread, Symbol type, Symbol output, Symbol input) {
    switch(type) {
        case Ontology::NaturalSymbol:
            Storage::writeBlob(output, static_cast<T>(thread.readBlob<NativeNaturalType>(input)));
            break;
        case Ontology::IntegerSymbol:
            Storage::writeBlob(output, static_cast<T>(thread.readBlob<NativeIntegerType>(input)));
            break;
        case Ontology::FloatSymbol:
            Storage::writeBlob(output, static_cast<T>(thread.readBlob<NativeFloatType>(input)));
            break;
        default:
            thread.throwException("Invalid Input");
    }
}

Procedure(NumericCast) {
    getSymbolByName(input, Input)
    getSymbolByName(to, To)
    getSymbolByName(output, Output)
    Symbol type;
    if(!Ontology::getUncertain(input, Ontology::BlobTypeSymbol, type))
        thread.throwException("Invalid Input");
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, to});
    switch(to) {
        case Ontology::NaturalSymbol:
            ProcedureNumericCastTo<NativeNaturalType>(thread, type, output, input);
            break;
        case Ontology::IntegerSymbol:
            ProcedureNumericCastTo<NativeIntegerType>(thread, type, output, input);
            break;
        case Ontology::FloatSymbol:
            ProcedureNumericCastTo<NativeFloatType>(thread, type, output, input);
            break;
        default:
            thread.throwException("Invalid To Value");
    }
    thread.popCallStack();
}

Procedure(Equal) {
    Symbol type, firstSymbol;
    NativeNaturalType outputValue = 1;
    bool first = true;
    if(Ontology::query(9, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        if(outputValue == 0)
            return;
        Symbol input = result.pos[0], _type = Ontology::VoidSymbol;
        Ontology::getUncertain(input, Ontology::BlobTypeSymbol, _type);
        if(first) {
            first = false;
            type = _type;
            firstSymbol = input;
        } else if(type == _type) {
            if(type == Ontology::FloatSymbol) {
                if(thread.readBlob<NativeFloatType>(input) != thread.readBlob<NativeFloatType>(firstSymbol))
                    outputValue = 0;
            } else if(Storage::compareBlobs(input, firstSymbol) != 0)
                outputValue = 0;
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
    Storage::writeBlob(output, outputValue);
    thread.popCallStack();
}

struct ProcedureLessThan {
    static bool n(NativeNaturalType i, NativeNaturalType c) { return i < c; };
    static bool i(NativeIntegerType i, NativeIntegerType c) { return i < c; };
    static bool f(NativeFloatType i, NativeFloatType c) { return i < c; };
    static bool s(Symbol i, Symbol c) { return Storage::compareBlobs(i, c) < 0; };
};

struct ProcedureLessEqual {
    static bool n(NativeNaturalType i, NativeNaturalType c) { return i <= c; };
    static bool i(NativeIntegerType i, NativeIntegerType c) { return i <= c; };
    static bool f(NativeFloatType i, NativeFloatType c) { return i <= c; };
    static bool s(Symbol i, Symbol c) { return Storage::compareBlobs(i, c) <= 0; };
};

template<typename op>
Procedure(CompareLogic) {
    getSymbolByName(input, Input)
    getSymbolByName(comparandum, Comparandum)
    getSymbolByName(output, Output)
    Symbol type, _type;
    Ontology::getUncertain(input, Ontology::BlobTypeSymbol, type);
    Ontology::getUncertain(comparandum, Ontology::BlobTypeSymbol, _type);
    if(type != _type)
        thread.throwException("Input and Comparandum have different types");
    NativeNaturalType outputValue;
    switch(type) {
        case Ontology::NaturalSymbol:
            outputValue = op::n(thread.readBlob<NativeNaturalType>(input), thread.readBlob<NativeNaturalType>(comparandum));
            break;
        case Ontology::IntegerSymbol:
            outputValue = op::i(thread.readBlob<NativeIntegerType>(input), thread.readBlob<NativeIntegerType>(comparandum));
            break;
        case Ontology::FloatSymbol:
            outputValue = op::f(thread.readBlob<NativeFloatType>(input), thread.readBlob<NativeFloatType>(comparandum));
            break;
        default:
            outputValue = op::s(input, comparandum);
            break;
    }
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
    Storage::writeBlob(output, outputValue);
    thread.popCallStack();
}

struct ProcedureBitShiftEmpty {
    static void d(NativeNaturalType& dst, NativeNaturalType count) { dst >>= count; };
    static void m(NativeNaturalType& dst, NativeNaturalType count) { dst <<= count; };
};

struct ProcedureBitShiftReplicate {
    static void d(NativeNaturalType& dst, NativeNaturalType count) {
        *reinterpret_cast<NativeIntegerType*>(&dst) >>= count;
    };
    static void m(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType lowestBit = dst&BitMask<NativeNaturalType>::one;
        dst <<= count;
        dst |= lowestBit*BitMask<NativeNaturalType>::fillLSBs(count);
    };
};

struct ProcedureBitShiftBarrel {
    static void d(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType aux = dst&BitMask<NativeNaturalType>::fillLSBs(count);
        dst >>= count;
        dst |= aux<<(architectureSize-count);
    };
    static void m(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType aux = dst&BitMask<NativeNaturalType>::fillMSBs(count);
        dst <<= count;
        dst |= aux>>(architectureSize-count);
    };
};

template<typename op>
Procedure(BitShift) {
    getSymbolByName(input, Input)
    getSymbolByName(count, Count)
    getSymbolByName(output, Output)
    checkBlobType(count, Ontology::NaturalSymbol)
    auto outputValue = thread.readBlob<NativeNaturalType>(input);
    auto countValue = thread.readBlob<NativeNaturalType>(count);
    switch(thread.getGuaranteed(thread.block, Ontology::DirectionSymbol)) {
        case Ontology::DivideSymbol:
            op::d(outputValue, countValue);
            break;
        case Ontology::MultiplySymbol:
            op::m(outputValue, countValue);
            break;
        default:
            thread.throwException("Invalid Direction");
    }
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::VoidSymbol});
    Storage::writeBlob(output, outputValue);
    thread.popCallStack();
}

Procedure(BitwiseComplement) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::VoidSymbol});
    Storage::writeBlob(output, ~thread.readBlob<NativeNaturalType>(input));
    thread.popCallStack();
}

struct ProcedureBitwiseAnd {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst &= src; };
};

struct ProcedureBitwiseOr {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst |= src; };
};

struct ProcedureBitwiseXor {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst ^= src; };
};

template<typename op>
Procedure(AssociativeCommutativeBitwise) {
    NativeNaturalType outputValue;
    bool first = true;
    if(Ontology::query(9, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol input = result.pos[0];
        if(first) {
            first = false;
            outputValue = thread.readBlob<NativeNaturalType>(input);
        } else
            op::n(outputValue, thread.readBlob<NativeNaturalType>(input));
    }) < 2)
        thread.throwException("Expected more Input");
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::VoidSymbol});
    Storage::writeBlob(output, outputValue);
    thread.popCallStack();
}

struct ProcedureAdd {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst += src; };
    static void i(NativeIntegerType& dst, NativeIntegerType src) { dst += src; };
    static void f(NativeFloatType& dst, NativeFloatType src) { dst += src; };
};

struct ProcedureMultiply {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst *= src; };
    static void i(NativeIntegerType& dst, NativeIntegerType src) { dst *= src; };
    static void f(NativeFloatType& dst, NativeFloatType src) { dst *= src; };
};

template<typename op>
Procedure(AssociativeCommutativeArithmetic) {
    Symbol type;
    union {
        NativeNaturalType n;
        NativeIntegerType i;
        NativeFloatType f;
    } aux;
    bool first = true;
    if(Ontology::query(9, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol input = result.pos[0],
               _type = thread.getGuaranteed(input, Ontology::BlobTypeSymbol);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case Ontology::NaturalSymbol:
                    aux.n = thread.readBlob<NativeNaturalType>(input);
                    break;
                case Ontology::IntegerSymbol:
                    aux.i = thread.readBlob<NativeIntegerType>(input);
                    break;
                case Ontology::FloatSymbol:
                    aux.f = thread.readBlob<NativeFloatType>(input);
                    break;
                default:
                    thread.throwException("Invalid Input");
            }
        } else if(type == _type) {
            switch(type) {
                case Ontology::NaturalSymbol:
                    op::n(aux.n, thread.readBlob<NativeNaturalType>(input));
                    break;
                case Ontology::IntegerSymbol:
                    op::i(aux.i, thread.readBlob<NativeIntegerType>(input));
                    break;
                case Ontology::FloatSymbol:
                    op::f(aux.f, thread.readBlob<NativeFloatType>(input));
                    break;
            }
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        thread.throwException("Expected more Input");
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, type});
    Storage::writeBlob(output, aux.n);
    thread.popCallStack();
}

Procedure(Subtract) {
    getSymbolByName(minuend, Minuend)
    getSymbolByName(subtrahend, Subtrahend)
    getSymbolByName(output, Output)
    Symbol type = thread.getGuaranteed(minuend, Ontology::BlobTypeSymbol),
           _type = thread.getGuaranteed(subtrahend, Ontology::BlobTypeSymbol);
    if(type != _type)
        thread.throwException("Minuend and Subtrahend have different types");
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, type});
    switch(type) {
        case Ontology::NaturalSymbol:
            Storage::writeBlob(output, thread.readBlob<NativeNaturalType>(minuend)-thread.readBlob<NativeNaturalType>(subtrahend));
            break;
        case Ontology::IntegerSymbol:
            Storage::writeBlob(output, thread.readBlob<NativeIntegerType>(minuend)-thread.readBlob<NativeIntegerType>(subtrahend));
            break;
        case Ontology::FloatSymbol:
            Storage::writeBlob(output, thread.readBlob<NativeFloatType>(minuend)-thread.readBlob<NativeFloatType>(subtrahend));
            break;
        default:
            thread.throwException("Invalid Minuend or Subtrahend");
    }
    thread.popCallStack();
}

Procedure(Divide) {
    getSymbolByName(dividend, Dividend)
    getSymbolByName(divisor, Divisor)
    Symbol type = thread.getGuaranteed(dividend, Ontology::BlobTypeSymbol);
    Symbol _type = thread.getGuaranteed(divisor, Ontology::BlobTypeSymbol);
    if(type != _type)
        thread.throwException("Dividend and Divisor have different types");
    Symbol rest, quotient;
    bool restExists = Ontology::getUncertain(thread.block, Ontology::RestSymbol, rest),
         quotientExists = Ontology::getUncertain(thread.block, Ontology::QuotientSymbol, quotient);
    if(restExists) {
        Ontology::setSolitary({rest, Ontology::BlobTypeSymbol, type});
        if(Storage::getBlobSize(rest) != architectureSize)
            thread.throwException("Invalid Rest");
    }
    if(quotientExists) {
        Ontology::setSolitary({quotient, Ontology::BlobTypeSymbol, type});
        if(Storage::getBlobSize(quotient) != architectureSize)
            thread.throwException("Invalid Quotient");
    }
    if(!restExists && !quotientExists)
        thread.throwException("Expected Rest or Quotient");
    switch(type) {
        case Ontology::NaturalSymbol: {
            auto dividendValue = thread.readBlob<NativeNaturalType>(dividend),
                 divisorValue = thread.readBlob<NativeNaturalType>(divisor);
            if(divisorValue == 0)
                thread.throwException("Division by Zero");
            if(restExists)
                Storage::writeBlob(rest, dividendValue%divisorValue);
            if(quotientExists)
                Storage::writeBlob(quotient, dividendValue/divisorValue);
        }   break;
        case Ontology::IntegerSymbol: {
            auto dividendValue = thread.readBlob<NativeIntegerType>(dividend),
                 divisorValue = thread.readBlob<NativeIntegerType>(divisor);
            if(divisorValue == 0)
                thread.throwException("Division by Zero");
            if(restExists)
                Storage::writeBlob(rest, dividendValue%divisorValue);
            if(quotientExists)
                Storage::writeBlob(quotient, dividendValue/divisorValue);
        }   break;
        case Ontology::FloatSymbol: {
            auto dividendValue = thread.readBlob<NativeFloatType>(dividend),
                 divisorValue = thread.readBlob<NativeFloatType>(divisor),
                 quotientValue = dividendValue/divisorValue;
            if(divisorValue == 0.0)
                thread.throwException("Division by Zero");
            if(restExists) {
                NativeFloatType integerPart = static_cast<NativeIntegerType>(quotientValue);
                Storage::writeBlob(rest, quotientValue-integerPart);
                quotientValue = integerPart;
            }
            if(quotientExists)
                Storage::writeBlob(quotient, quotientValue);
        }   break;
        default:
            thread.throwException("Invalid Dividend or Divisor");
    }
    thread.popCallStack();
}

#define ProcedureEntry(Name) \
    case Ontology::Name##Symbol: \
        Procedure##Name(thread); \
        return true;

#define ProcedureGroup(GroupName, Name) \
    case Ontology::Name##Symbol: \
        Procedure##GroupName<Procedure##Name>(thread); \
        return true;

bool executePreDefinedProcedure(Thread& thread, Symbol procedure) {
    switch(procedure) {
        ProcedureEntry(Search)
        ProcedureGroup(Triple, Link)
        ProcedureGroup(Triple, Unlink)
        ProcedureEntry(Create)
        ProcedureEntry(Destroy)
        ProcedureEntry(Push)
        ProcedureEntry(Pop)
        ProcedureEntry(Branch)
        ProcedureEntry(Exception)
        ProcedureEntry(Serialize)
        ProcedureEntry(Deserialize)
        ProcedureEntry(CloneBlob)
        ProcedureEntry(SliceBlob)
        ProcedureEntry(GetBlobSize)
        ProcedureGroup(ChangeBlobSize, DecreaseBlobSize)
        ProcedureGroup(ChangeBlobSize, IncreaseBlobSize)
        ProcedureEntry(NumericCast)
        ProcedureEntry(Equal)
        ProcedureGroup(CompareLogic, LessThan)
        ProcedureGroup(CompareLogic, LessEqual)
        ProcedureGroup(BitShift, BitShiftEmpty)
        ProcedureGroup(BitShift, BitShiftReplicate)
        ProcedureGroup(BitShift, BitShiftBarrel)
        ProcedureEntry(BitwiseComplement)
        ProcedureGroup(AssociativeCommutativeBitwise, BitwiseAnd)
        ProcedureGroup(AssociativeCommutativeBitwise, BitwiseOr)
        ProcedureGroup(AssociativeCommutativeBitwise, BitwiseXor)
        ProcedureGroup(AssociativeCommutativeArithmetic, Add)
        ProcedureGroup(AssociativeCommutativeArithmetic, Multiply)
        ProcedureEntry(Subtract)
        ProcedureEntry(Divide)
    }
    return false;
}