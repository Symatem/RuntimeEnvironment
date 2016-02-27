#include "../Ontology/Containers.hpp"

struct Task;
bool executePreDefProcedure(Task& task, Symbol procedure);

struct Task {
    Symbol task, status, frame, block;

    Task() :task(PreDef_Void) { }

    void setStatus(Symbol _status) {
        status = _status;
        context.setSolitary({task, PreDef_Status, status});
    }

    void setFrame(bool unlinkHolds, bool setBlock, Symbol _frame) {
        assert(frame != _frame);
        if(_frame == PreDef_Void) {
            block = PreDef_Void;
        } else {
            context.link({task, PreDef_Holds, _frame});
            context.setSolitary({task, PreDef_Frame, _frame});
            if(setBlock)
                block = context.getGuaranteed(_frame, PreDef_Block);
        }
        if(unlinkHolds)
            context.unlink({task, PreDef_Holds, frame});
        if(frame != PreDef_Void)
            context.scrutinizeHeldBy(frame);
        frame = _frame;
    }

    bool popCallStack() {
        assert(task != PreDef_Void);
        if(frame == PreDef_Void)
            return false;
        Symbol parentFrame = PreDef_Void;
        bool parentExists = context.getUncertain(frame, PreDef_Parent, parentFrame);
        if(parentFrame == PreDef_Void)
            setStatus(PreDef_Done);
        setFrame(true, true, parentFrame);
        return parentFrame != PreDef_Void;
    }

    Symbol getTargetSymbol() {
        Symbol result;
        if(!context.getUncertain(block, PreDef_Target, result)) {
            Symbol parentFrame = context.getGuaranteed(frame, PreDef_Parent);
            result = context.getGuaranteed(parentFrame, PreDef_Block);
        }
        return result;
    }

    void clear() {
        if(task == PreDef_Void)
            return;
        while(popCallStack());
        context.destroy(task);
        task = status = frame = block = PreDef_Void;
    }

    bool step() {
        if(!running())
            return false;

        Symbol parentBlock = block, parentFrame = frame, execute,
               procedure, next = PreDef_Void, catcher, staticParams, dynamicParams;
        if(!context.getUncertain(parentFrame, PreDef_Execute, execute)) {
            popCallStack();
            return true;
        }

        try {
            block = context.create();
            procedure = context.getGuaranteed(execute, PreDef_Procedure);
            setFrame(true, false, context.create({
                {PreDef_Holds, parentFrame},
                {PreDef_Parent, parentFrame},
                {PreDef_Holds, block},
                {PreDef_Block, block},
                {PreDef_Procedure, procedure} // TODO: debugging
            }));

            if(context.getUncertain(execute, PreDef_Static, staticParams))
                context.query(12, {staticParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    context.link({block, result.pos[0], result.pos[1]});
                });

            if(context.getUncertain(execute, PreDef_Dynamic, dynamicParams))
                context.query(12, {dynamicParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    switch(result.pos[1]) {
                        case PreDef_Task:
                            context.link({block, result.pos[0], task});
                            break;
                        case PreDef_Frame:
                            context.link({block, result.pos[0], parentFrame});
                            break;
                        case PreDef_Block:
                            context.link({block, result.pos[0], parentBlock});
                            break;
                        default:
                            context.query(9, {parentBlock, result.pos[1], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                                context.link({block, result.pos[0], resultB.pos[0]});
                            });
                            break;
                    }
                });

            context.getUncertain(execute, PreDef_Next, next);
            context.setSolitary({parentFrame, PreDef_Execute, next});

            if(context.getUncertain(execute, PreDef_Catch, catcher))
                context.link({frame, PreDef_Catch, catcher});

            if(!executePreDefProcedure(*this, procedure))
                context.link({frame, PreDef_Execute, context.getGuaranteed(procedure, PreDef_Execute)});
        } catch(Exception exception) {
            assert(task != PreDef_Void && frame != PreDef_Void);

            Symbol parentFrame = frame, message = context.createFromData(exception.message);
            blobIndex.insertElement(message);
            exception.links.insert({PreDef_Message, message});
            block = context.create(exception.links);
            setFrame(true, false, context.create({
                {PreDef_Holds, parentFrame},
                {PreDef_Parent, parentFrame},
                {PreDef_Holds, block},
                {PreDef_Block, block},
                {PreDef_Procedure, PreDef_Exception} // TODO: debugging
            }));

            executePreDefProcedure(*this, PreDef_Exception);
        }

        return true;
    }

    bool uncaughtException() {
        assert(task != PreDef_Void);
        return context.tripleExists({task, PreDef_Status, PreDef_Exception});
    }

    bool running() {
        assert(task != PreDef_Void);
        return context.tripleExists({task, PreDef_Status, PreDef_Run});
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

        block = context.create({
            {PreDef_Holds, input}
        });
        if(package == PreDef_Void)
            package = block;
        Symbol staticParams = context.create({
            {PreDef_Package, package},
            {PreDef_Input, input},
            {PreDef_Target, block},
            {PreDef_Output, PreDef_Output}
        }), execute = context.create({
            {PreDef_Procedure, PreDef_Deserialize},
            {PreDef_Static, staticParams}
        });
        task = context.create();
        setFrame(false, false, context.create({
            {PreDef_Holds, staticParams},
            {PreDef_Holds, execute},
            {PreDef_Holds, block},
            {PreDef_Block, block},
            {PreDef_Execute, execute}
        }));
        executeFinite(1);
    }

    bool executeDeserialized() {
        Symbol prev = PreDef_Void;
        if(context.query(9, {block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
            Symbol next = context.create({
                {PreDef_Procedure, result.pos[0]}
            });
            if(prev == PreDef_Void)
                context.setSolitary({frame, PreDef_Execute, next});
            else
                context.link({prev, PreDef_Next, next});
            prev = next;
        }) == 0)
            return false;

        executeInfinite();
        return true;
    }
};
