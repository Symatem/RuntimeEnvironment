#include "Deserialize.hpp"

#define Primitive(Name) bool Primitive##Name(Thread& thread)

Primitive(Search) {
    Ontology::Triple triple;
    NativeNaturalType mask[3] = {Ontology::QueryMode::Ignore, Ontology::QueryMode::Ignore, Ontology::QueryMode::Ignore}, varyingCount = 0;
    Symbol posNames[3] = {Ontology::EntitySymbol, Ontology::AttributeSymbol, Ontology::ValueSymbol};
    Ontology::query(Ontology::MMV, {thread.block, Ontology::VaryingSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        for(NativeNaturalType index = 0; index < 3; ++index)
            if(result.pos[0] == posNames[index]) {
                mask[index] = Ontology::QueryMode::Varying;
                ++varyingCount;
                return;
            }
        thread.throwException("Invalid Varying");
    });
    if(thread.uncaughtException())
        return false;
    for(NativeNaturalType index = 0; index < 3; ++index)
        if(Ontology::getUncertain(thread.block, posNames[index], triple.pos[index])) {
            if(mask[index] != Ontology::QueryMode::Ignore)
                return thread.throwException("Invalid Input");
            mask[index] = 0;
        }
    getSymbolByName(output, Output)
    Ontology::BlobVector<false, Symbol> outputValue;
    outputValue.symbol = output;
    auto countValue = Ontology::query(static_cast<Ontology::QueryMask>(mask[0]+mask[1]*3+mask[2]*9), triple, [&](Ontology::Triple result) {
        for(NativeNaturalType i = 0; i < varyingCount; ++i)
            outputValue.push_back(result.pos[i]);
    });
    Symbol count;
    if(Ontology::getUncertain(thread.block, Ontology::CountSymbol, count)) {
        Ontology::setSolitary({count, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
        Storage::Blob(count).write(countValue);
    }
    return thread.popCallStack();
}

struct PrimitiveLink {
    static bool e(Thread& thread, Ontology::Triple triple) { return Ontology::link(triple); };
};

struct PrimitiveUnlink {
    static bool e(Thread& thread, Ontology::Triple triple) { return Ontology::unlink(triple); };
};

template<typename op>
Primitive(Triple) {
    getSymbolByName(entity, Entity)
    getSymbolByName(attribute, Attribute)
    getSymbolByName(value, Value)
    NativeNaturalType outputValue = op::e(thread, {entity, attribute, value});
    Symbol output;
    if(Ontology::getUncertain(thread.block, Ontology::OutputSymbol, output)) {
        Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
        Storage::Blob(output).write(outputValue);
    }
    return thread.popCallStack();
}

Primitive(Create) {
    Symbol input, value;
    bool inputExists = Ontology::getUncertain(thread.block, Ontology::InputSymbol, input);
    if(inputExists)
        value = Storage::Blob(input).readAt<NativeNaturalType>();
    Symbol target;
    checkReturn(thread.getTargetSymbol(target));
    if(Ontology::query(Ontology::MMV, {thread.block, Ontology::OutputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol output = result.pos[0];
        if(!inputExists)
            value = Storage::createSymbol();
        Ontology::setSolitary({target, output, value});
        Ontology::link({target, Ontology::HoldsSymbol, value});
    }) == 0)
        return thread.throwException("Expected Output");
    return thread.popCallStack();
}

Primitive(Destroy) {
    Ontology::BlobSet<false, Symbol> symbols;
    symbols.symbol = Storage::createSymbol();
    Ontology::link({thread.block, Ontology::HoldsSymbol, symbols.symbol});
    if(Ontology::query(Ontology::MMV, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        symbols.insertElement(result.pos[0]);
    }) == 0)
        return thread.throwException("Expected more Inputs");
    symbols.iterate([&](Symbol symbol) {
        Ontology::unlink(symbol);
    });
    return thread.popCallStack();
}

Primitive(Push) {
    Symbol target;
    getSymbolByName(execute, Execute)
    Ontology::link({thread.frame, Ontology::ExecuteSymbol, execute});
    checkReturn(thread.getTargetSymbol(target));
    thread.setBlock(target);
    return true;
}

Primitive(Pop) {
    getSymbolByName(count, Count)
    checkBlobType(count, Ontology::NaturalSymbol)
    auto countValue = Storage::Blob(count).readAt<NativeNaturalType>();
    if(countValue < 2)
        return thread.throwException("Invalid Count Value");
    for(; countValue > 0 && thread.popCallStack(); --countValue);
    return true;
}

Primitive(Branch) {
    getUncertainValueByName(input, Input, 1)
    getSymbolByName(branch, Branch)
    thread.popCallStack();
    if(inputValue != 0)
        Ontology::setSolitary({thread.frame, Ontology::ExecuteSymbol, branch});
    return true;
}

Primitive(Exception) {
    Symbol exception = thread.frame;
    if(Ontology::getUncertain(thread.block, Ontology::ExceptionSymbol, exception)) {
        Symbol currentFrame = exception;
        while(Ontology::getUncertain(currentFrame, Ontology::ParentSymbol, currentFrame));
        Ontology::link({currentFrame, Ontology::ParentSymbol, thread.frame});
    }
    Symbol execute, currentFrame = thread.frame, prevFrame = currentFrame;
    do {
        if(currentFrame != prevFrame && Ontology::getUncertain(currentFrame, Ontology::CatchSymbol, execute)) {
            Ontology::setSolitary({prevFrame, Ontology::ParentSymbol, Ontology::VoidSymbol});
            thread.setFrame(currentFrame, true);
            Ontology::link({thread.block, Ontology::HoldsSymbol, exception});
            Ontology::link({thread.block, Ontology::ExceptionSymbol, exception});
            Ontology::setSolitary({thread.frame, Ontology::ExecuteSymbol, execute});
            return true;
        }
        prevFrame = currentFrame;
    } while(Ontology::getUncertain(currentFrame, Ontology::ParentSymbol, currentFrame));
    thread.setStatus(Ontology::ExceptionSymbol);
    return false;
}

Primitive(Serialize) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Serializer serializer(thread, output);
    serializer.serializeBlob(input);
    return thread.popCallStack();
}

Primitive(Deserialize) {
    Deserializer deserializer(thread);
    checkReturn(deserializer.deserialize());
    return true;
}

Primitive(CloneBlob) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Storage::Blob(output).deepCopy(Storage::Blob(input));
    Symbol type = Ontology::VoidSymbol;
    Ontology::getUncertain(input, Ontology::BlobTypeSymbol, type);
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, type});
    return thread.popCallStack();
}

Primitive(SliceBlob) {
    getSymbolByName(input, Input)
    getSymbolByName(target, Target)
    getUncertainValueByName(count, Count, Storage::Blob(input).getSize())
    getUncertainValueByName(destination, Destination, 0)
    getUncertainValueByName(source, Source, 0)
    if(!Storage::Blob(target).slice(Storage::Blob(input), destinationValue, sourceValue, countValue))
        return thread.throwException("Invalid Count, Destination or SrcOffset Value");
    return thread.popCallStack();
}

Primitive(GetBlobSize) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
    Storage::Blob(output).write(Storage::Blob(input).getSize());
    return thread.popCallStack();
}

struct PrimitiveDecreaseBlobSize {
    static bool e(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
        return Storage::Blob(symbol).decreaseSize(offset, length);
    };
};

struct PrimitiveIncreaseBlobSize {
    static bool e(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
        return Storage::Blob(symbol).increaseSize(offset, length);
    };
};

template<typename op>
Primitive(ChangeBlobSize) {
    getSymbolByName(target, Target)
    getSymbolByName(at, At)
    getSymbolByName(count, Count)
    checkBlobType(at, Ontology::NaturalSymbol)
    checkBlobType(count, Ontology::NaturalSymbol)
    if(!op::e(target, Storage::Blob(at).readAt<NativeNaturalType>(), Storage::Blob(count).readAt<NativeNaturalType>()))
        return thread.throwException("Invalid At or Count Value");
    return thread.popCallStack();
}

template<typename T>
bool PrimitiveNumericCastTo(Thread& thread, Symbol type, Symbol output, Symbol input) {
    switch(type) {
        case Ontology::NaturalSymbol:
            Storage::Blob(output).write<T>(Storage::Blob(input).readAt<NativeNaturalType>());
            return true;
        case Ontology::IntegerSymbol:
            Storage::Blob(output).write<T>(Storage::Blob(input).readAt<NativeIntegerType>());
            return true;
        case Ontology::FloatSymbol:
            Storage::Blob(output).write<T>(Storage::Blob(input).readAt<NativeFloatType>());
            return true;
        default:
            return thread.throwException("Invalid Input");
    }
}

Primitive(NumericCast) {
    getSymbolByName(input, Input)
    getSymbolByName(to, To)
    getSymbolByName(output, Output)
    Symbol type;
    if(!Ontology::getUncertain(input, Ontology::BlobTypeSymbol, type))
        return thread.throwException("Invalid Input");
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, to});
    switch(to) {
        case Ontology::NaturalSymbol:
            checkReturn(PrimitiveNumericCastTo<NativeNaturalType>(thread, type, output, input));
            break;
        case Ontology::IntegerSymbol:
            checkReturn(PrimitiveNumericCastTo<NativeIntegerType>(thread, type, output, input));
            break;
        case Ontology::FloatSymbol:
            checkReturn(PrimitiveNumericCastTo<NativeFloatType>(thread, type, output, input));
            break;
        default:
            return thread.throwException("Invalid To Value");
    }
    return thread.popCallStack();
}

Primitive(Equal) {
    Symbol type, firstSymbol;
    NativeNaturalType outputValue = 1;
    bool first = true;
    if(Ontology::query(Ontology::MMV, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
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
                if(Storage::Blob(input).readAt<NativeNaturalType>() != Storage::Blob(firstSymbol).readAt<NativeNaturalType>())
                    outputValue = 0;
            } else if(Storage::Blob(input).compare(Storage::Blob(firstSymbol)) != 0)
                outputValue = 0;
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        return thread.throwException("Expected more Input");
    if(thread.uncaughtException())
        return false;
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
    Storage::Blob(output).write(outputValue);
    return thread.popCallStack();
}

struct PrimitiveLessThan {
    static bool n(NativeNaturalType i, NativeNaturalType c) { return i < c; };
    static bool i(NativeIntegerType i, NativeIntegerType c) { return i < c; };
    static bool f(NativeFloatType i, NativeFloatType c) { return i < c; };
    static bool s(Symbol i, Symbol c) { return Storage::Blob(i).compare(Storage::Blob(c)) < 0; };
};

struct PrimitiveLessEqual {
    static bool n(NativeNaturalType i, NativeNaturalType c) { return i <= c; };
    static bool i(NativeIntegerType i, NativeIntegerType c) { return i <= c; };
    static bool f(NativeFloatType i, NativeFloatType c) { return i <= c; };
    static bool s(Symbol i, Symbol c) { return Storage::Blob(i).compare(Storage::Blob(c)) <= 0; };
};

template<typename op>
Primitive(CompareLogic) {
    getSymbolByName(input, Input)
    getSymbolByName(comparandum, Comparandum)
    getSymbolByName(output, Output)
    Symbol type, _type;
    Ontology::getUncertain(input, Ontology::BlobTypeSymbol, type);
    Ontology::getUncertain(comparandum, Ontology::BlobTypeSymbol, _type);
    if(type != _type)
        return thread.throwException("Input and Comparandum have different types");
    NativeNaturalType outputValue;
    switch(type) {
        case Ontology::NaturalSymbol:
            outputValue = op::n(Storage::Blob(input).readAt<NativeNaturalType>(), Storage::Blob(comparandum).readAt<NativeNaturalType>());
            break;
        case Ontology::IntegerSymbol:
            outputValue = op::i(Storage::Blob(input).readAt<NativeIntegerType>(), Storage::Blob(comparandum).readAt<NativeIntegerType>());
            break;
        case Ontology::FloatSymbol:
            outputValue = op::f(Storage::Blob(input).readAt<NativeFloatType>(), Storage::Blob(comparandum).readAt<NativeFloatType>());
            break;
        default:
            outputValue = op::s(input, comparandum);
            break;
    }
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::NaturalSymbol});
    Storage::Blob(output).write(outputValue);
    return thread.popCallStack();
}

struct PrimitiveBitShiftEmpty {
    static void d(NativeNaturalType& dst, NativeNaturalType count) { dst >>= count; };
    static void m(NativeNaturalType& dst, NativeNaturalType count) { dst <<= count; };
};

struct PrimitiveBitShiftReplicate {
    static void d(NativeNaturalType& dst, NativeNaturalType count) {
        *reinterpret_cast<NativeIntegerType*>(&dst) >>= count;
    };
    static void m(NativeNaturalType& dst, NativeNaturalType count) {
        NativeNaturalType lowestBit = dst&BitMask<NativeNaturalType>::one;
        dst <<= count;
        dst |= lowestBit*BitMask<NativeNaturalType>::fillLSBs(count);
    };
};

struct PrimitiveBitShiftBarrel {
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
Primitive(BitShift) {
    getSymbolByName(input, Input)
    getSymbolByName(count, Count)
    getSymbolByName(output, Output)
    checkBlobType(count, Ontology::NaturalSymbol)
    auto outputValue = Storage::Blob(input).readAt<NativeNaturalType>();
    auto countValue = Storage::Blob(count).readAt<NativeNaturalType>();
    Symbol direction;
    checkReturn(thread.getGuaranteed(thread.block, Ontology::DirectionSymbol, direction));
    switch(direction) {
        case Ontology::DivideSymbol:
            op::d(outputValue, countValue);
            break;
        case Ontology::MultiplySymbol:
            op::m(outputValue, countValue);
            break;
        default:
            return thread.throwException("Invalid Direction");
    }
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::VoidSymbol});
    Storage::Blob(output).write(outputValue);
    return thread.popCallStack();
}

Primitive(BitwiseComplement) {
    getSymbolByName(input, Input)
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::VoidSymbol});
    Storage::Blob(output).write(~Storage::Blob(input).readAt<NativeNaturalType>());
    return thread.popCallStack();
}

struct PrimitiveBitwiseAnd {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst &= src; };
};

struct PrimitiveBitwiseOr {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst |= src; };
};

struct PrimitiveBitwiseXor {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst ^= src; };
};

template<typename op>
Primitive(AssociativeCommutativeBitwise) {
    NativeNaturalType outputValue;
    bool first = true;
    if(Ontology::query(Ontology::MMV, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol input = result.pos[0];
        if(first) {
            first = false;
            outputValue = Storage::Blob(input).readAt<NativeNaturalType>();
        } else
            op::n(outputValue, Storage::Blob(input).readAt<NativeNaturalType>());
    }) < 2)
        return thread.throwException("Expected more Input");
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, Ontology::VoidSymbol});
    Storage::Blob(output).write(outputValue);
    return thread.popCallStack();
}

struct PrimitiveAdd {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst += src; };
    static void i(NativeIntegerType& dst, NativeIntegerType src) { dst += src; };
    static void f(NativeFloatType& dst, NativeFloatType src) { dst += src; };
};

struct PrimitiveMultiply {
    static void n(NativeNaturalType& dst, NativeNaturalType src) { dst *= src; };
    static void i(NativeIntegerType& dst, NativeIntegerType src) { dst *= src; };
    static void f(NativeFloatType& dst, NativeFloatType src) { dst *= src; };
};

template<typename op>
Primitive(AssociativeCommutativeArithmetic) {
    Symbol type;
    union {
        NativeNaturalType n;
        NativeIntegerType i;
        NativeFloatType f;
    } aux;
    bool first = true;
    if(Ontology::query(Ontology::MMV, {thread.block, Ontology::InputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
        Symbol input = result.pos[0], _type;
        thread.getGuaranteed(input, Ontology::BlobTypeSymbol, _type);
        if(first) {
            first = false;
            type = _type;
            switch(type) {
                case Ontology::NaturalSymbol:
                    aux.n = Storage::Blob(input).readAt<NativeNaturalType>();
                    break;
                case Ontology::IntegerSymbol:
                    aux.i = Storage::Blob(input).readAt<NativeIntegerType>();
                    break;
                case Ontology::FloatSymbol:
                    aux.f = Storage::Blob(input).readAt<NativeFloatType>();
                    break;
                default:
                    thread.throwException("Invalid Input");
            }
        } else if(type == _type) {
            switch(type) {
                case Ontology::NaturalSymbol:
                    op::n(aux.n, Storage::Blob(input).readAt<NativeNaturalType>());
                    break;
                case Ontology::IntegerSymbol:
                    op::i(aux.i, Storage::Blob(input).readAt<NativeIntegerType>());
                    break;
                case Ontology::FloatSymbol:
                    op::f(aux.f, Storage::Blob(input).readAt<NativeFloatType>());
                    break;
            }
        } else
            thread.throwException("Inputs have different types");
    }) < 2)
        return thread.throwException("Expected more Input");
    if(thread.uncaughtException())
        return false;
    getSymbolByName(output, Output)
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, type});
    Storage::Blob(output).write(aux.n);
    return thread.popCallStack();
}

Primitive(Subtract) {
    getSymbolByName(minuend, Minuend)
    getSymbolByName(subtrahend, Subtrahend)
    getSymbolByName(output, Output)
    Symbol type, _type;
    checkReturn(thread.getGuaranteed(minuend, Ontology::BlobTypeSymbol, type));
    checkReturn(thread.getGuaranteed(subtrahend, Ontology::BlobTypeSymbol, _type));
    if(type != _type)
        return thread.throwException("Minuend and Subtrahend have different types");
    Ontology::setSolitary({output, Ontology::BlobTypeSymbol, type});
    switch(type) {
        case Ontology::NaturalSymbol:
            Storage::Blob(output).write(Storage::Blob(minuend).readAt<NativeNaturalType>()-Storage::Blob(subtrahend).readAt<NativeNaturalType>());
            break;
        case Ontology::IntegerSymbol:
            Storage::Blob(output).write(Storage::Blob(minuend).readAt<NativeIntegerType>()-Storage::Blob(subtrahend).readAt<NativeIntegerType>());
            break;
        case Ontology::FloatSymbol:
            Storage::Blob(output).write(Storage::Blob(minuend).readAt<NativeFloatType>()-Storage::Blob(subtrahend).readAt<NativeFloatType>());
            break;
        default:
            return thread.throwException("Invalid Minuend or Subtrahend");
    }
    return thread.popCallStack();
}

Primitive(Divide) {
    getSymbolByName(dividend, Dividend)
    getSymbolByName(divisor, Divisor)
    Symbol type, _type;
    checkReturn(thread.getGuaranteed(dividend, Ontology::BlobTypeSymbol, type));
    checkReturn(thread.getGuaranteed(divisor, Ontology::BlobTypeSymbol, _type));
    if(type != _type)
        return thread.throwException("Dividend and Divisor have different types");
    Symbol rest, quotient;
    bool restExists = Ontology::getUncertain(thread.block, Ontology::RestSymbol, rest),
         quotientExists = Ontology::getUncertain(thread.block, Ontology::QuotientSymbol, quotient);
    if(restExists)
        Ontology::setSolitary({rest, Ontology::BlobTypeSymbol, type});
    if(quotientExists)
        Ontology::setSolitary({quotient, Ontology::BlobTypeSymbol, type});
    if(!restExists && !quotientExists)
        return thread.throwException("Expected Rest or Quotient");
    switch(type) {
        case Ontology::NaturalSymbol: {
            auto dividendValue = Storage::Blob(dividend).readAt<NativeNaturalType>(),
                 divisorValue = Storage::Blob(divisor).readAt<NativeNaturalType>();
            if(divisorValue == 0)
                return thread.throwException("Division by Zero");
            if(restExists)
                Storage::Blob(rest).write(dividendValue%divisorValue);
            if(quotientExists)
                Storage::Blob(quotient).write(dividendValue/divisorValue);
        }   break;
        case Ontology::IntegerSymbol: {
            auto dividendValue = Storage::Blob(dividend).readAt<NativeIntegerType>(),
                 divisorValue = Storage::Blob(divisor).readAt<NativeIntegerType>();
            if(divisorValue == 0)
                return thread.throwException("Division by Zero");
            if(restExists)
                Storage::Blob(rest).write(dividendValue%divisorValue);
            if(quotientExists)
                Storage::Blob(quotient).write(dividendValue/divisorValue);
        }   break;
        case Ontology::FloatSymbol: {
            auto dividendValue = Storage::Blob(dividend).readAt<NativeFloatType>(),
                 divisorValue = Storage::Blob(divisor).readAt<NativeFloatType>(),
                 quotientValue = dividendValue/divisorValue;
            if(divisorValue == 0.0)
                return thread.throwException("Division by Zero");
            if(restExists) {
                NativeFloatType integerPart = static_cast<NativeIntegerType>(quotientValue);
                Storage::Blob(rest).write(quotientValue-integerPart);
                quotientValue = integerPart;
            }
            if(quotientExists)
                Storage::Blob(quotient).write(quotientValue);
        }   break;
        default:
            return thread.throwException("Invalid Dividend or Divisor");
    }
    return thread.popCallStack();
}

#define PrimitiveEntry(Name) \
    case Ontology::Name##Symbol: \
        found = true; \
        return Primitive##Name(thread);

#define PrimitiveGroup(GroupName, Name) \
    case Ontology::Name##Symbol: \
        found = true; \
        return Primitive##GroupName<Primitive##Name>(thread);

bool executePrimitive(Thread& thread, Symbol primitive, bool& found) {
    switch(primitive) {
        PrimitiveEntry(Search)
        PrimitiveGroup(Triple, Link)
        PrimitiveGroup(Triple, Unlink)
        PrimitiveEntry(Create)
        PrimitiveEntry(Destroy)
        PrimitiveEntry(Push)
        PrimitiveEntry(Pop)
        PrimitiveEntry(Branch)
        PrimitiveEntry(Exception)
        PrimitiveEntry(Serialize)
        PrimitiveEntry(Deserialize)
        PrimitiveEntry(CloneBlob)
        PrimitiveEntry(SliceBlob)
        PrimitiveEntry(GetBlobSize)
        PrimitiveGroup(ChangeBlobSize, DecreaseBlobSize)
        PrimitiveGroup(ChangeBlobSize, IncreaseBlobSize)
        PrimitiveEntry(NumericCast)
        PrimitiveEntry(Equal)
        PrimitiveGroup(CompareLogic, LessThan)
        PrimitiveGroup(CompareLogic, LessEqual)
        PrimitiveGroup(BitShift, BitShiftEmpty)
        PrimitiveGroup(BitShift, BitShiftReplicate)
        PrimitiveGroup(BitShift, BitShiftBarrel)
        PrimitiveEntry(BitwiseComplement)
        PrimitiveGroup(AssociativeCommutativeBitwise, BitwiseAnd)
        PrimitiveGroup(AssociativeCommutativeBitwise, BitwiseOr)
        PrimitiveGroup(AssociativeCommutativeBitwise, BitwiseXor)
        PrimitiveGroup(AssociativeCommutativeArithmetic, Add)
        PrimitiveGroup(AssociativeCommutativeArithmetic, Multiply)
        PrimitiveEntry(Subtract)
        PrimitiveEntry(Divide)
    }
    found = false;
    return true;
}
