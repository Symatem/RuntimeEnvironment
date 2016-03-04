#include "../Ontology/Triple.hpp"

struct Thread;
bool executePreDefProcedure(Thread& thread, Identifier procedure);

struct Thread {
    bool valueCountIs(Identifier entity, Identifier attribute, ArchitectureType size) {
        return Ontology::query(9, {entity, attribute, PreDef_Void}) == size;
    }

    bool tripleExists(Triple triple) {
        return Ontology::query(0, triple) == 1;
    }

    bool link(Triple triple) {
        if(!Ontology::link(triple)) {
            Identifier data = Storage::createIdentifier();
            link({data, PreDef_Entity, triple.entity});
            link({data, PreDef_Attribute, triple.attribute});
            link({data, PreDef_Value, triple.value});
            throwException("Already linked", data);
            return false;
        }
        return true;
    }

    bool unlink(Triple triple) {
        if(!Ontology::unlink(triple)) {
            Identifier data = Storage::createIdentifier();
            link({data, PreDef_Entity, triple.entity});
            link({data, PreDef_Attribute, triple.attribute});
            link({data, PreDef_Value, triple.value});
            throwException("Already unlinked", data);
            return false;
        }
        return true;
    }

    template <typename T>
    T accessBlobAs(Identifier symbol) {
        if(Storage::getBlobSize(symbol) != sizeof(T)*8)
            throwException("Invalid Blob Size");
        return *reinterpret_cast<T*>(Storage::accessBlobData(symbol));
    }

    void scrutinizeExistence(Identifier symbol) {
        Set<true, Identifier> symbols;
        symbols.insertElement(symbol);
        while(!symbols.empty()) {
            symbol = symbols.pop_back();
            // Ontology::topIndex.find(symbol) == Ontology::topIndex.end() || // TODO: Prevent not found error
            if(Ontology::query(1, {PreDef_Void, PreDef_Holds, symbol}) > 0)
                continue;
            Ontology::query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result, ArchitectureType) {
                symbols.insertElement(result.pos[0]);
            });
            Ontology::unlink(symbol);
        }
    }

    Identifier getGuaranteed(Identifier entity, Identifier attribute) {
        Identifier value;
        if(!Ontology::getUncertain(entity, attribute, value)) {
            Identifier data = Storage::createIdentifier();
            link({data, PreDef_Entity, entity});
            link({data, PreDef_Attribute, attribute});
            throwException("Nonexistent or ambiguous", data);
        }
        return value;
    }

    // TODO: Cleanup created symbols in case of an exception
    jmp_buf exceptionEnv;
    Identifier task, status, frame, block;

    Thread() :task(PreDef_Void) { }

    void setStatus(Identifier _status) {
        status = _status;
        Ontology::setSolitary({task, PreDef_Status, status});
    }

    void setFrame(Identifier _frame, bool setBlock) {
        assert(frame != _frame);
        if(_frame == PreDef_Void)
            block = PreDef_Void;
        else {
            link({task, PreDef_Holds, _frame});
            Ontology::setSolitary({task, PreDef_Frame, _frame});
            if(setBlock)
                block = getGuaranteed(_frame, PreDef_Block);
        }
        Ontology::unlink({task, PreDef_Holds, frame});
        if(frame != PreDef_Void)
            scrutinizeExistence(frame);
        frame = _frame;
    }

    void throwException(const char* messageStr, Identifier data = PreDef_Void) {
        block = (data != PreDef_Void) ? data : Storage::createIdentifier();
        Identifier message = Ontology::createFromData(messageStr);
        Ontology::blobIndex.insertElement(message);
        link({block, PreDef_Message, message});
        pushCallStack();
        link({frame, PreDef_Procedure, PreDef_Exception}); // TODO: debugging
        longjmp(exceptionEnv, 1);
    }

    void pushCallStack() {
        Identifier childFrame = Storage::createIdentifier();
        link({childFrame, PreDef_Holds, frame});
        link({childFrame, PreDef_Parent, frame});
        link({childFrame, PreDef_Holds, block});
        link({childFrame, PreDef_Block, block});
        setFrame(childFrame, false);
    }

    bool popCallStack() {
        assert(task != PreDef_Void);
        if(frame == PreDef_Void)
            return false;
        Identifier parentFrame = PreDef_Void;
        bool parentExists = Ontology::getUncertain(frame, PreDef_Parent, parentFrame);
        if(parentFrame == PreDef_Void)
            setStatus(PreDef_Done);
        setFrame(parentFrame, true);
        return parentFrame != PreDef_Void;
    }

    Identifier getTargetSymbol() {
        Identifier result;
        if(!Ontology::getUncertain(block, PreDef_Target, result)) {
            Identifier parentFrame = getGuaranteed(frame, PreDef_Parent);
            result = getGuaranteed(parentFrame, PreDef_Block);
        }
        return result;
    }

    void clear() {
        if(task == PreDef_Void)
            return;
        while(popCallStack());
        Ontology::unlink(task);
        task = status = frame = block = PreDef_Void;
    }

    bool step() {
        if(!running())
            return false;

        Identifier parentBlock = block, parentFrame = frame, execute,
               procedure, next = PreDef_Void, catcher, staticParams, dynamicParams;
        if(!Ontology::getUncertain(parentFrame, PreDef_Execute, execute)) {
            popCallStack();
            return true;
        }

        if(setjmp(exceptionEnv) == 0) {
            procedure = getGuaranteed(execute, PreDef_Procedure);
            block = Storage::createIdentifier();
            pushCallStack();
            link({frame, PreDef_Procedure, procedure}); // TODO: debugging

            if(Ontology::getUncertain(execute, PreDef_Static, staticParams))
                Ontology::query(12, {staticParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    link({block, result.pos[0], result.pos[1]});
                });

            if(Ontology::getUncertain(execute, PreDef_Dynamic, dynamicParams))
                Ontology::query(12, {dynamicParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    switch(result.pos[1]) {
                        case PreDef_Task:
                            link({block, result.pos[0], task});
                            break;
                        case PreDef_Frame:
                            link({block, result.pos[0], parentFrame});
                            break;
                        case PreDef_Block:
                            link({block, result.pos[0], parentBlock});
                            break;
                        default:
                            Ontology::query(9, {parentBlock, result.pos[1], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                                link({block, result.pos[0], resultB.pos[0]});
                            });
                            break;
                    }
                });

            Ontology::getUncertain(execute, PreDef_Next, next);
            Ontology::setSolitary({parentFrame, PreDef_Execute, next});

            if(Ontology::getUncertain(execute, PreDef_Catch, catcher))
                link({frame, PreDef_Catch, catcher});

            if(!executePreDefProcedure(*this, procedure))
                link({frame, PreDef_Execute, getGuaranteed(procedure, PreDef_Execute)});
        } else {
            assert(task != PreDef_Void && frame != PreDef_Void);
            executePreDefProcedure(*this, PreDef_Exception);
        }

        return true;
    }

    bool uncaughtException() {
        assert(task != PreDef_Void);
        return tripleExists({task, PreDef_Status, PreDef_Exception});
    }

    bool running() {
        assert(task != PreDef_Void);
        return tripleExists({task, PreDef_Status, PreDef_Run});
    }

    void executeFinite(ArchitectureType n) {
        if(task == PreDef_Void)
            return;
        setStatus(PreDef_Run);
        for(ArchitectureType i = 0; i < n && step(); ++i);
    }

    void executeInfinite() {
        if(task == PreDef_Void)
            return;
        setStatus(PreDef_Run);
        while(step());
    }

    void deserializationTask(Identifier input, Identifier package = PreDef_Void) {
        clear();

        block = Storage::createIdentifier();
        link({block, PreDef_Holds, input});
        if(package == PreDef_Void)
            package = block;
        Identifier staticParams = Storage::createIdentifier();
        link({staticParams, PreDef_Package, package});
        link({staticParams, PreDef_Input, input});
        link({staticParams, PreDef_Target, block});
        link({staticParams, PreDef_Output, PreDef_Output});
        Identifier execute = Storage::createIdentifier();
        link({execute, PreDef_Procedure, PreDef_Deserialize});
        link({execute, PreDef_Static, staticParams});

        task = Storage::createIdentifier();
        Identifier childFrame = Storage::createIdentifier();
        link({childFrame, PreDef_Holds, staticParams});
        link({childFrame, PreDef_Holds, execute});
        link({childFrame, PreDef_Holds, block});
        link({childFrame, PreDef_Block, block});
        link({childFrame, PreDef_Execute, execute});
        setFrame(childFrame, false);
        executeFinite(1);
    }

    bool executeDeserialized() {
        Identifier prev = PreDef_Void;
        if(Ontology::query(9, {block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
            Identifier next = Storage::createIdentifier();
            link({next, PreDef_Procedure, result.pos[0]});
            if(prev == PreDef_Void)
                Ontology::setSolitary({frame, PreDef_Execute, next});
            else
                link({prev, PreDef_Next, next});
            prev = next;
        }) == 0)
            return false;

        executeInfinite();
        return true;
    }
};
