#include "../Ontology/Context.hpp"

struct Task;
bool executePreDefProcedure(Task& task, Symbol procedure);

struct Task {
    Symbol task, status, frame, block;

    Task() :task(PreDef_Void) { }

    void setStatus(Symbol _status) {
        status = _status;
        Context::setSolitary({task, PreDef_Status, status});
    }

    void setFrame(bool unlinkHolds, bool setBlock, Symbol _frame) {
        assert(frame != _frame);
        if(_frame == PreDef_Void)
            block = PreDef_Void;
        else {
            Context::link({task, PreDef_Holds, _frame});
            Context::setSolitary({task, PreDef_Frame, _frame});
            if(setBlock)
                block = Context::getGuaranteed(_frame, PreDef_Block);
        }
        if(unlinkHolds)
            Context::unlink({task, PreDef_Holds, frame});
        if(frame != PreDef_Void)
            Context::scrutinizeHeldBy(frame);
        frame = _frame;
    }

    bool popCallStack() {
        assert(task != PreDef_Void);
        if(frame == PreDef_Void)
            return false;
        Symbol parentFrame = PreDef_Void;
        bool parentExists = Context::getUncertain(frame, PreDef_Parent, parentFrame);
        if(parentFrame == PreDef_Void)
            setStatus(PreDef_Done);
        setFrame(true, true, parentFrame);
        return parentFrame != PreDef_Void;
    }

    Symbol getTargetSymbol() {
        Symbol result;
        if(!Context::getUncertain(block, PreDef_Target, result)) {
            Symbol parentFrame = Context::getGuaranteed(frame, PreDef_Parent);
            result = Context::getGuaranteed(parentFrame, PreDef_Block);
        }
        return result;
    }

    void clear() {
        if(task == PreDef_Void)
            return;
        while(popCallStack());
        Context::destroy(task);
        task = status = frame = block = PreDef_Void;
    }

    bool step() {
        if(!running())
            return false;

        Symbol parentBlock = block, parentFrame = frame, execute,
               procedure, next = PreDef_Void, catcher, staticParams, dynamicParams;
        if(!Context::getUncertain(parentFrame, PreDef_Execute, execute)) {
            popCallStack();
            return true;
        }

        try {
            block = Context::create();
            procedure = Context::getGuaranteed(execute, PreDef_Procedure);

            Symbol frame = Context::create();
            Context::link({frame, PreDef_Holds, parentFrame});
            Context::link({frame, PreDef_Parent, parentFrame});
            Context::link({frame, PreDef_Holds, block});
            Context::link({frame, PreDef_Block, block});
            setFrame(true, false, frame);
            Context::link({frame, PreDef_Procedure, procedure}); // TODO: debugging

            if(Context::getUncertain(execute, PreDef_Static, staticParams))
                Context::query(12, {staticParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    Context::link({block, result.pos[0], result.pos[1]});
                });

            if(Context::getUncertain(execute, PreDef_Dynamic, dynamicParams))
                Context::query(12, {dynamicParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    switch(result.pos[1]) {
                        case PreDef_Task:
                            Context::link({block, result.pos[0], task});
                            break;
                        case PreDef_Frame:
                            Context::link({block, result.pos[0], parentFrame});
                            break;
                        case PreDef_Block:
                            Context::link({block, result.pos[0], parentBlock});
                            break;
                        default:
                            Context::query(9, {parentBlock, result.pos[1], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                                Context::link({block, result.pos[0], resultB.pos[0]});
                            });
                            break;
                    }
                });

            Context::getUncertain(execute, PreDef_Next, next);
            Context::setSolitary({parentFrame, PreDef_Execute, next});

            if(Context::getUncertain(execute, PreDef_Catch, catcher))
                Context::link({frame, PreDef_Catch, catcher});

            if(!executePreDefProcedure(*this, procedure))
                Context::link({frame, PreDef_Execute, Context::getGuaranteed(procedure, PreDef_Execute)});
        } catch(Exception exception) {
            assert(task != PreDef_Void && frame != PreDef_Void);

            Symbol parentFrame = frame, message = Context::createFromData(exception.message);
            Context::blobIndex.insertElement(message);
            exception.links.insert({PreDef_Message, message});
            block = Context::create();
            for(auto pair : exception.links)
                Context::link({block, pair.first, pair.second});

            Symbol frame = Context::create();
            Context::link({frame, PreDef_Holds, parentFrame});
            Context::link({frame, PreDef_Parent, parentFrame});
            Context::link({frame, PreDef_Holds, block});
            Context::link({frame, PreDef_Block, block});
            setFrame(true, false, frame);
            Context::link({frame, PreDef_Procedure, PreDef_Exception}); // TODO: debugging

            executePreDefProcedure(*this, PreDef_Exception);
        }

        return true;
    }

    bool uncaughtException() {
        assert(task != PreDef_Void);
        return Context::tripleExists({task, PreDef_Status, PreDef_Exception});
    }

    bool running() {
        assert(task != PreDef_Void);
        return Context::tripleExists({task, PreDef_Status, PreDef_Run});
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

    void deserializationTask(Symbol input, Symbol package = PreDef_Void) {
        clear();

        block = Context::create();
        Context::link({block, PreDef_Holds, input});
        if(package == PreDef_Void)
            package = block;
        Symbol staticParams = Context::create();
        Context::link({staticParams, PreDef_Package, package});
        Context::link({staticParams, PreDef_Input, input});
        Context::link({staticParams, PreDef_Target, block});
        Context::link({staticParams, PreDef_Output, PreDef_Output});
        Symbol execute = Context::create();
        Context::link({execute, PreDef_Procedure, PreDef_Deserialize});
        Context::link({execute, PreDef_Static, staticParams});

        task = Context::create();
        Symbol frame = Context::create();
        Context::link({frame, PreDef_Holds, staticParams});
        Context::link({frame, PreDef_Holds, execute});
        Context::link({frame, PreDef_Holds, block});
        Context::link({frame, PreDef_Block, block});
        Context::link({frame, PreDef_Execute, execute});
        setFrame(false, false, frame);
        executeFinite(1);
    }

    bool executeDeserialized() {
        Symbol prev = PreDef_Void;
        if(Context::query(9, {block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
            Symbol next = Context::create();
            Context::link({next, PreDef_Procedure, result.pos[0]});
            if(prev == PreDef_Void)
                Context::setSolitary({frame, PreDef_Execute, next});
            else
                Context::link({prev, PreDef_Next, next});
            prev = next;
        }) == 0)
            return false;

        executeInfinite();
        return true;
    }
};
