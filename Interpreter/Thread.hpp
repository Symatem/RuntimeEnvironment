#include "../Ontology/Triple.hpp"

#define checkReturn(expression) \
    if(!(expression)) \
        return false;

#define getSymbolByName(name, Name) \
    Symbol name; \
    checkReturn(thread.getGuaranteed(thread.block, Ontology::Name##Symbol, name));

#define checkBlobType(name, expectedType) \
    if(!Ontology::tripleExists({name, Ontology::BlobTypeSymbol, expectedType})) \
        return thread.throwException("Invalid Blob Type");

#define getUncertainValueByName(name, Name, DefaultValue) \
    Symbol name; \
    NativeNaturalType name##Value = DefaultValue; \
    if(Ontology::getUncertain(thread.block, Ontology::Name##Symbol, name)) { \
        checkBlobType(name, Ontology::NaturalSymbol) \
        name##Value = Storage::readBlobAt<NativeNaturalType>(name); \
    }

struct Thread;
bool executePrimitive(Thread& thread, Symbol primitive, bool& found);

struct Thread {
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
            Ontology::link({task, Ontology::HoldsSymbol, _frame});
            Ontology::setSolitary({task, Ontology::FrameSymbol, _frame});
            if(setBlock)
                assert(getGuaranteed(_frame, Ontology::BlockSymbol, block));
        }
        Ontology::unlink({task, Ontology::HoldsSymbol, frame});
        if(frame != Ontology::VoidSymbol)
            Ontology::scrutinizeExistence(frame);
        frame = _frame;
    }

    void setBlock(Symbol _block) {
        assert(block != _block);
        Ontology::unlink({frame, Ontology::HoldsSymbol, block});
        Ontology::scrutinizeExistence(block);
        block = _block;
        Ontology::link({frame, Ontology::HoldsSymbol, block});
        Ontology::link({frame, Ontology::BlockSymbol, block});
    }

    bool throwException(const char* messageStr, Symbol data = Ontology::VoidSymbol) {
        block = (data != Ontology::VoidSymbol) ? data : Storage::createSymbol();
        Symbol message = Ontology::createFromString(messageStr);
        Ontology::blobIndex.insertElement(message);
        Ontology::link({block, Ontology::MessageSymbol, message});
        pushCallStack();
        Ontology::link({frame, Ontology::ProcedureSymbol, Ontology::ExceptionSymbol}); // TODO: debugging
        return false;
    }

    bool getGuaranteed(Symbol entity, Symbol attribute, Symbol& value) {
        if(Ontology::getUncertain(entity, attribute, value))
            return true;
        Symbol data = Storage::createSymbol();
        Ontology::link({data, Ontology::EntitySymbol, entity});
        Ontology::link({data, Ontology::AttributeSymbol, attribute});
        return throwException("Nonexistent or ambiguous", data);
    }

    void pushCallStack() {
        Symbol childFrame = Storage::createSymbol();
        Ontology::link({childFrame, Ontology::HoldsSymbol, frame});
        Ontology::link({childFrame, Ontology::ParentSymbol, frame});
        Ontology::link({childFrame, Ontology::HoldsSymbol, block});
        Ontology::link({childFrame, Ontology::BlockSymbol, block});
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

    bool getTargetSymbol(Symbol& result) {
        if(!Ontology::getUncertain(block, Ontology::TargetSymbol, result)) {
            Symbol parentFrame;
            checkReturn(getGuaranteed(frame, Ontology::ParentSymbol, parentFrame));
            checkReturn(getGuaranteed(parentFrame, Ontology::BlockSymbol, result));
        }
        return true;
    }

    void clear() {
        if(task == Ontology::VoidSymbol)
            return;
        while(popCallStack());
        Ontology::unlink(task);
        task = status = frame = block = Ontology::VoidSymbol;
    }

    bool tryToStep() {
        bool foundPrimitive;
        Symbol parentBlock = block, parentFrame = frame, execute, procedure,
               next = Ontology::VoidSymbol, catcher, staticParams, dynamicParams;
        if(!Ontology::getUncertain(parentFrame, Ontology::ExecuteSymbol, execute)) {
            popCallStack();
            return true;
        }
        checkReturn(getGuaranteed(execute, Ontology::ProcedureSymbol, procedure));
        block = Storage::createSymbol();
        pushCallStack();
        Ontology::link({frame, Ontology::ProcedureSymbol, procedure}); // TODO: debugging
        if(Ontology::getUncertain(execute, Ontology::StaticSymbol, staticParams))
            Ontology::query(12, {staticParams, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                Ontology::link({block, result.pos[0], result.pos[1]});
            });
        if(Ontology::getUncertain(execute, Ontology::DynamicSymbol, dynamicParams))
            Ontology::query(12, {dynamicParams, Ontology::VoidSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
                switch(result.pos[1]) {
                    case Ontology::TaskSymbol:
                        Ontology::link({block, result.pos[0], task});
                        break;
                    case Ontology::FrameSymbol:
                        Ontology::link({block, result.pos[0], parentFrame});
                        break;
                    case Ontology::BlockSymbol:
                        Ontology::link({block, result.pos[0], parentBlock});
                        break;
                    default:
                        Ontology::query(9, {parentBlock, result.pos[1], Ontology::VoidSymbol}, [&](Ontology::Triple resultB) {
                            Ontology::link({block, result.pos[0], resultB.pos[0]});
                        });
                        break;
                }
            });
        Ontology::getUncertain(execute, Ontology::NextSymbol, next);
        Ontology::setSolitary({parentFrame, Ontology::ExecuteSymbol, next});
        if(Ontology::getUncertain(execute, Ontology::CatchSymbol, catcher))
            Ontology::link({frame, Ontology::CatchSymbol, catcher});
        checkReturn(executePrimitive(*this, procedure, foundPrimitive));
        if(!foundPrimitive) {
            checkReturn(getGuaranteed(procedure, Ontology::ExecuteSymbol, execute));
            Ontology::link({frame, Ontology::ExecuteSymbol, execute});
        }
        return true;
    }

    bool step() {
        if(!running())
            return false;
        if(tryToStep())
            return true;
        bool foundPrimitive;
        assert(task != Ontology::VoidSymbol && frame != Ontology::VoidSymbol);
        executePrimitive(*this, Ontology::ExceptionSymbol, foundPrimitive);
        return false;
    }

    bool uncaughtException() {
        assert(task != Ontology::VoidSymbol);
        return Ontology::tripleExists({task, Ontology::StatusSymbol, Ontology::ExceptionSymbol});
    }

    bool running() {
        assert(task != Ontology::VoidSymbol);
        return Ontology::tripleExists({task, Ontology::StatusSymbol, Ontology::RunSymbol});
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
        Ontology::link({block, Ontology::HoldsSymbol, input});
        if(package == Ontology::VoidSymbol)
            package = block;
        Symbol staticParams = Storage::createSymbol();
        Ontology::link({staticParams, Ontology::PackageSymbol, package});
        Ontology::link({staticParams, Ontology::InputSymbol, input});
        Ontology::link({staticParams, Ontology::TargetSymbol, block});
        Ontology::link({staticParams, Ontology::OutputSymbol, Ontology::OutputSymbol});
        Symbol execute = Storage::createSymbol();
        Ontology::link({execute, Ontology::ProcedureSymbol, Ontology::DeserializeSymbol});
        Ontology::link({execute, Ontology::StaticSymbol, staticParams});
        task = Storage::createSymbol();
        Symbol childFrame = Storage::createSymbol();
        Ontology::link({childFrame, Ontology::HoldsSymbol, staticParams});
        Ontology::link({childFrame, Ontology::HoldsSymbol, execute});
        Ontology::link({childFrame, Ontology::HoldsSymbol, block});
        Ontology::link({childFrame, Ontology::BlockSymbol, block});
        Ontology::link({childFrame, Ontology::ExecuteSymbol, execute});
        setFrame(childFrame, false);
        executeFinite(1);
    }

    bool executeDeserialized() {
        Symbol prev = Ontology::VoidSymbol;
        if(Ontology::query(9, {block, Ontology::OutputSymbol, Ontology::VoidSymbol}, [&](Ontology::Triple result) {
            Symbol next = Storage::createSymbol();
            Ontology::link({next, Ontology::ProcedureSymbol, result.pos[0]});
            if(prev == Ontology::VoidSymbol)
                Ontology::setSolitary({frame, Ontology::ExecuteSymbol, next});
            else
                Ontology::link({prev, Ontology::NextSymbol, next});
            prev = next;
        }) == 0)
            return false;
        executeInfinite();
        return true;
    }
};
