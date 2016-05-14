#include "../Ontology/Triple.hpp"
#include <setjmp.h> // TODO: Replace setjmp/longjmp

struct Thread;
bool executePrimitive(Thread& thread, Symbol procedure);

struct Thread {
    bool valueCountIs(Symbol entity, Symbol attribute, NativeNaturalType size) {
        return Ontology::query(9, {entity, attribute, Ontology::VoidSymbol}) == size;
    }

    bool tripleExists(Ontology::Triple triple) {
        return Ontology::query(0, triple) == 1;
    }

    void link(Ontology::Triple triple) {
        if(!Ontology::link(triple)) {
            Symbol data = Storage::createSymbol();
            link({data, Ontology::EntitySymbol, triple.pos[0]});
            link({data, Ontology::AttributeSymbol, triple.pos[1]});
            link({data, Ontology::ValueSymbol, triple.pos[2]});
            throwException("Already linked", data);
        }
    }

    void unlink(Ontology::Triple triple) {
        if(!Ontology::unlink(triple)) {
            Symbol data = Storage::createSymbol();
            link({data, Ontology::EntitySymbol, triple.pos[0]});
            link({data, Ontology::AttributeSymbol, triple.pos[1]});
            link({data, Ontology::ValueSymbol, triple.pos[2]});
            throwException("Already unlinked", data);
        }
    }

    template <typename T>
    T readBlob(Symbol symbol) {
        if(Storage::getBlobSize(symbol) != sizeOfInBits<T>::value)
            throwException("Invalid Blob Size");
        return Storage::readBlob<T>(symbol);
    }

    Symbol getGuaranteed(Symbol entity, Symbol attribute) {
        Symbol value;
        if(!Ontology::getUncertain(entity, attribute, value)) {
            Symbol data = Storage::createSymbol();
            link({data, Ontology::EntitySymbol, entity});
            link({data, Ontology::AttributeSymbol, attribute});
            throwException("Nonexistent or ambiguous", data);
        }
        return value;
    }

    jmp_buf exceptionEnv; // TODO: Replace setjmp/longjmp
    Symbol task, status, frame, block;

    Thread() :task(Ontology::VoidSymbol) {}

    void setStatus(Symbol _status) {
        status = _status;
        Ontology::setSolitary({task, Ontology::StatusSymbol, status});
    }

    void setFrame(Symbol _frame, bool setBlock) {
        assert(frame != _frame);
        if(_frame == Ontology::VoidSymbol)
            block = Ontology::VoidSymbol;
        else {
            link({task, Ontology::HoldsSymbol, _frame});
            Ontology::setSolitary({task, Ontology::FrameSymbol, _frame});
            if(setBlock)
                block = getGuaranteed(_frame, Ontology::BlockSymbol);
        }
        Ontology::unlink({task, Ontology::HoldsSymbol, frame});
        if(frame != Ontology::VoidSymbol)
            Ontology::scrutinizeExistence(frame);
        frame = _frame;
    }

    void setBlock(Symbol _block) {
        assert(block != _block);
        unlink({frame, Ontology::HoldsSymbol, block});
        Ontology::scrutinizeExistence(block);
        block = _block;
        link({frame, Ontology::HoldsSymbol, block});
        link({frame, Ontology::BlockSymbol, block});
    }

    void throwException(const char* messageStr, Symbol data = Ontology::VoidSymbol) {
        block = (data != Ontology::VoidSymbol) ? data : Storage::createSymbol();
        Symbol message = Ontology::createFromString(messageStr);
        Ontology::blobIndex.insertElement(message);
        link({block, Ontology::MessageSymbol, message});
        pushCallStack();
        link({frame, Ontology::ProcedureSymbol, Ontology::ExceptionSymbol}); // TODO: debugging
        longjmp(exceptionEnv, 1); // TODO: Replace setjmp/longjmp
    }

    void pushCallStack() {
        Symbol childFrame = Storage::createSymbol();
        link({childFrame, Ontology::HoldsSymbol, frame});
        link({childFrame, Ontology::ParentSymbol, frame});
        link({childFrame, Ontology::HoldsSymbol, block});
        link({childFrame, Ontology::BlockSymbol, block});
        setFrame(childFrame, false);
    }

    bool popCallStack() {
        assert(task != Ontology::VoidSymbol);
        if(frame == Ontology::VoidSymbol)
            return false;
        Symbol parentFrame = Ontology::VoidSymbol;
        bool parentExists = Ontology::getUncertain(frame, Ontology::ParentSymbol, parentFrame);
        if(!parentExists)
            setStatus(Ontology::DoneSymbol);
        setFrame(parentFrame, true);
        return parentExists;
    }

    Symbol getTargetSymbol() {
        Symbol result;
        if(!Ontology::getUncertain(block, Ontology::TargetSymbol, result)) {
            Symbol parentFrame = getGuaranteed(frame, Ontology::ParentSymbol);
            result = getGuaranteed(parentFrame, Ontology::BlockSymbol);
        }
        return result;
    }

    void clear() {
        if(task == Ontology::VoidSymbol)
            return;
        while(popCallStack());
        Ontology::unlink(task);
        task = status = frame = block = Ontology::VoidSymbol;
    }

    bool step() {
        if(!running())
            return false;
        Symbol parentBlock = block, parentFrame = frame, execute,
               procedure, next = Ontology::VoidSymbol, catcher, staticParams, dynamicParams;
        if(!Ontology::getUncertain(parentFrame, Ontology::ExecuteSymbol, execute)) {
            popCallStack();
            return true;
        }
        if(setjmp(exceptionEnv) == 0) { // TODO: Replace setjmp/longjmp
            procedure = getGuaranteed(execute, Ontology::ProcedureSymbol);
            block = Storage::createSymbol();
            pushCallStack();
            link({frame, Ontology::ProcedureSymbol, procedure}); // TODO: debugging
            if(Ontology::getUncertain(execute, Ontology::StaticSymbol, staticParams))
                Ontology::query(12, {staticParams, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                    link({block, result.pos[0], result.pos[1]});
                });
            if(Ontology::getUncertain(execute, Ontology::DynamicSymbol, dynamicParams))
                Ontology::query(12, {dynamicParams, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                    switch(result.pos[1]) {
                        case Ontology::TaskSymbol:
                            link({block, result.pos[0], task});
                            break;
                        case Ontology::FrameSymbol:
                            link({block, result.pos[0], parentFrame});
                            break;
                        case Ontology::BlockSymbol:
                            link({block, result.pos[0], parentBlock});
                            break;
                        default:
                            Ontology::query(9, {parentBlock, result.pos[1], Ontology::VoidSymbol}, [&](Ontology::Triple resultB) {
                                link({block, result.pos[0], resultB.pos[0]});
                            });
                            break;
                    }
                });
            Ontology::getUncertain(execute, Ontology::NextSymbol, next);
            Ontology::setSolitary({parentFrame, Ontology::ExecuteSymbol, next});
            if(Ontology::getUncertain(execute, Ontology::CatchSymbol, catcher))
                link({frame, Ontology::CatchSymbol, catcher});
            if(!executePrimitive(*this, procedure))
                link({frame, Ontology::ExecuteSymbol, getGuaranteed(procedure, Ontology::ExecuteSymbol)});
        } else {
            assert(task != Ontology::VoidSymbol && frame != Ontology::VoidSymbol);
            executePrimitive(*this, Ontology::ExceptionSymbol);
        }
        return true;
    }

    bool uncaughtException() {
        assert(task != Ontology::VoidSymbol);
        return tripleExists({task, Ontology::StatusSymbol, Ontology::ExceptionSymbol});
    }

    bool running() {
        assert(task != Ontology::VoidSymbol);
        return tripleExists({task, Ontology::StatusSymbol, Ontology::RunSymbol});
    }

    void executeFinite(NativeNaturalType n) {
        if(task == Ontology::VoidSymbol)
            return;
        setStatus(Ontology::RunSymbol);
        for(NativeNaturalType i = 0; i < n && step(); ++i);
    }

    void executeInfinite() {
        if(task == Ontology::VoidSymbol)
            return;
        setStatus(Ontology::RunSymbol);
        while(step());
    }

    void deserializationTask(Symbol input, Symbol package = Ontology::VoidSymbol) {
        clear();
        block = Storage::createSymbol();
        link({block, Ontology::HoldsSymbol, input});
        if(package == Ontology::VoidSymbol)
            package = block;
        Symbol staticParams = Storage::createSymbol();
        link({staticParams, Ontology::PackageSymbol, package});
        link({staticParams, Ontology::InputSymbol, input});
        link({staticParams, Ontology::TargetSymbol, block});
        link({staticParams, Ontology::OutputSymbol, Ontology::OutputSymbol});
        Symbol execute = Storage::createSymbol();
        link({execute, Ontology::ProcedureSymbol, Ontology::DeserializeSymbol});
        link({execute, Ontology::StaticSymbol, staticParams});
        task = Storage::createSymbol();
        Symbol childFrame = Storage::createSymbol();
        link({childFrame, Ontology::HoldsSymbol, staticParams});
        link({childFrame, Ontology::HoldsSymbol, execute});
        link({childFrame, Ontology::HoldsSymbol, block});
        link({childFrame, Ontology::BlockSymbol, block});
        link({childFrame, Ontology::ExecuteSymbol, execute});
        setFrame(childFrame, false);
        executeFinite(1);
    }

    bool executeDeserialized() {
        Symbol prev = Ontology::VoidSymbol;
        if(Ontology::query(9, {block, Ontology::OutputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
            Symbol next = Storage::createSymbol();
            link({next, Ontology::ProcedureSymbol, result.pos[0]});
            if(prev == Ontology::VoidSymbol)
                Ontology::setSolitary({frame, Ontology::ExecuteSymbol, next});
            else
                link({prev, Ontology::NextSymbol, next});
            prev = next;
        }) == 0)
            return false;
        executeInfinite();
        return true;
    }
};
