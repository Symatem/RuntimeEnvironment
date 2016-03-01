#include "../Ontology/Ontology.hpp"

struct Thread;
bool executePreDefProcedure(Thread& thread, Symbol procedure);

struct Thread {
    ArchitectureType searchGGG(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = Ontology::topIndex.find(triple.pos[0]);
        assert(topIter != Ontology::topIndex.end());
        auto& subIndex = topIter->second->subIndices[0];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        auto gammaIter = betaIter->second.find(triple.pos[2]);
        if(gammaIter == betaIter->second.end())
            return 0;
        if(callback)
            callback();
        return 1;
    }

    ArchitectureType searchGGV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = Ontology::topIndex.find(triple.pos[0]);
        assert(topIter != Ontology::topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        auto betaIter = subIndex.find(triple.pos[1]);
        if(betaIter == subIndex.end())
            return 0;
        if(callback)
            for(auto gamma : betaIter->second) {
                triple.pos[2] = gamma;
                callback();
            }
        return betaIter->second.size();
    }

    ArchitectureType searchGVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = Ontology::topIndex.find(triple.pos[0]);
        assert(topIter != Ontology::topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        ArchitectureType count = 0;
        for(auto& beta : subIndex) {
            count += beta.second.size();
            if(callback) {
                triple.pos[1] = beta.first;
                for(auto gamma : beta.second) {
                    triple.pos[2] = gamma;
                    callback();
                }
            }
        }
        return count;
    }

    ArchitectureType searchGIV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = Ontology::topIndex.find(triple.pos[0]);
        assert(topIter != Ontology::topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        Set<Symbol, true> result;
        for(auto& beta : subIndex)
            for(auto& gamma : beta.second)
                result.insertElement(gamma);
        if(callback)
            result.iterate([&](Symbol gamma) {
                triple.pos[2] = gamma;
                callback();
            });
        return result.size();
    }

    ArchitectureType searchGVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        auto topIter = Ontology::topIndex.find(triple.pos[0]);
        assert(topIter != Ontology::topIndex.end());
        auto& subIndex = topIter->second->subIndices[index];
        if(callback)
            for(auto& beta : subIndex) {
                triple.pos[1] = beta.first;
                callback();
            }
        return subIndex.size();
    }

    ArchitectureType searchVII(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        if(callback)
            for(auto& alpha : Ontology::topIndex) {
                triple.pos[0] = alpha.first;
                callback();
            }
        return Ontology::topIndex.size();
    }

    ArchitectureType searchVVI(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        ArchitectureType count = 0;
        for(auto& alpha : Ontology::topIndex) {
            auto& subIndex = alpha.second->subIndices[index];
            count += subIndex.size();
            if(callback) {
                triple.pos[0] = alpha.first;
                for(auto& beta : subIndex) {
                    triple.pos[1] = beta.first;
                    callback();
                }
            }
        }
        return count;
    }

    ArchitectureType searchVVV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
        ArchitectureType count = 0;
        for(auto& alpha : Ontology::topIndex) {
            triple.pos[0] = alpha.first;
            for(auto& beta : alpha.second->subIndices[0]) {
                count += beta.second.size();
                if(callback) {
                    triple.pos[1] = beta.first;
                    for(auto gamma : beta.second) {
                        triple.pos[2] = gamma;
                        callback();
                    }
                }
            }
        }
        return count;
    }

    ArchitectureType query(ArchitectureType mode, Triple triple, std::function<void(Triple, ArchitectureType)> callback = nullptr) {
        struct QueryMethod {
            uint8_t index, pos, size;
            ArchitectureType(Thread::*function)(ArchitectureType, Triple&, std::function<void()>);
        };

        const QueryMethod lookup[] = {
            {EAV, 0, 0, &Thread::searchGGG},
            {AVE, 2, 1, &Thread::searchGGV},
            {AVE, 0, 0, nullptr},
            {VEA, 2, 1, &Thread::searchGGV},
            {VEA, 1, 2, &Thread::searchGVV},
            {VAE, 1, 1, &Thread::searchGVI},
            {VEA, 0, 0, nullptr},
            {VEA, 1, 1, &Thread::searchGVI},
            {VEA, 0, 0, nullptr},
            {EAV, 2, 1, &Thread::searchGGV},
            {AVE, 1, 2, &Thread::searchGVV},
            {AVE, 1, 1, &Thread::searchGVI},
            {EAV, 1, 2, &Thread::searchGVV},
            {EAV, 0, 3, &Thread::searchVVV},
            {AVE, 0, 2, &Thread::searchVVI},
            {EVA, 1, 1, &Thread::searchGVI},
            {VEA, 0, 2, &Thread::searchVVI},
            {VEA, 0, 1, &Thread::searchVII},
            {EAV, 0, 0, nullptr},
            {AEV, 1, 1, &Thread::searchGVI},
            {AVE, 0, 0, nullptr},
            {EAV, 1, 1, &Thread::searchGVI},
            {EAV, 0, 2, &Thread::searchVVI},
            {AVE, 0, 1, &Thread::searchVII},
            {EAV, 0, 0, nullptr},
            {EAV, 0, 1, &Thread::searchVII},
            {EAV, 0, 0, nullptr}
        };

        if(mode >= sizeof(lookup)/sizeof(QueryMethod))
            throwException("Invalid Mode Value");
        QueryMethod method = lookup[mode];
        if(method.function == nullptr)
            throwException("Invalid Mode Value");

        std::function<void()> handleNext = [&]() {
            Triple result;
            for(ArchitectureType i = 0; i < method.size; ++i)
                result.pos[i] = triple.pos[method.pos+i];
            callback(result, method.size);
        };

        if(Ontology::indexMode == Ontology::MonoIndex) {
            if(method.index != EAV) {
                method.index = EAV;
                method.function = &Thread::searchVVV;
                handleNext = [&]() {
                    ArchitectureType index = 0;
                    if(mode%3 == 1) ++index;
                    if(mode%9 >= 3 && mode%9 < 6) {
                        triple.pos[index] = triple.pos[1];
                        ++index;
                    }
                    if(mode >= 9 && mode < 18)
                        triple.pos[index] = triple.pos[2];
                    callback(triple, method.size);
                };
            }
        } else if(Ontology::indexMode == Ontology::TriIndex && method.index >= 3) {
            method.index -= 3;
            method.pos = 2;
            method.function = &Thread::searchGIV;
        }

        triple = triple.reordered(method.index);
        if(!callback) handleNext = nullptr;
        return (this->*method.function)(method.index, triple, handleNext);
    }

    bool valueCountIs(Symbol entity, Symbol attribute, ArchitectureType size) {
        return query(9, {entity, attribute, PreDef_Void}) == size;
    }

    bool tripleExists(Triple triple) {
        return query(0, triple) == 1;
    }

    bool link(Triple triple) {
        if(!Ontology::link(triple)) {
            Symbol data = Ontology::create();
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
            Symbol data = Ontology::create();
            link({data, PreDef_Entity, triple.entity});
            link({data, PreDef_Attribute, triple.attribute});
            link({data, PreDef_Value, triple.value});
            throwException("Already unlinked", data);
            return false;
        }
        return true;
    }

    template <typename T>
    T& accessBlobAs(Symbol symbol) {
        if(Ontology::accessBlobSize(symbol) != sizeof(T)*8)
            throwException("Invalid Blob Size");
        return *reinterpret_cast<T*>(Ontology::accessBlobData(symbol));
    }

    void scrutinizeExistence(Symbol symbol) {
        Set<Symbol, true> symbols;
        symbols.insertElement(symbol);
        while(!symbols.empty()) {
            symbol = symbols.pop_back();
            if(Ontology::topIndex.find(symbol) == Ontology::topIndex.end() ||
               query(1, {PreDef_Void, PreDef_Holds, symbol}) > 0)
                continue;
            query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result, ArchitectureType) {
                symbols.insertElement(result.pos[0]);
            });
            Ontology::destroy(symbol);
        }
    }

    void setSolitary(Triple triple, bool linkVoid = false) {
        bool toLink = (linkVoid || triple.value != PreDef_Void);
        Set<Symbol, true> symbols;
        query(9, triple, [&](Triple result, ArchitectureType) {
            if((triple.pos[2] == result.pos[0]) && (linkVoid || result.pos[0] != PreDef_Void))
                toLink = false;
            else
                symbols.insertElement(result.pos[0]);
        });
        if(toLink)
            link(triple);
        symbols.iterate([&](Symbol symbol) {
            Ontology::unlinkInternal({triple.pos[0], triple.pos[1], symbol});
        });
        if(!linkVoid)
            symbols.insertElement(triple.pos[0]);
        symbols.insertElement(triple.pos[1]);
        symbols.iterate([&](Symbol symbol) {
            Ontology::checkSymbolLinkCount(symbol);
        });
    }

    bool getUncertain(Symbol alpha, Symbol beta, Symbol& gamma) {
        return (query(9, {alpha, beta, PreDef_Void}, [&](Triple result, ArchitectureType) {
            gamma = result.pos[0];
        }) == 1);
    }

    Symbol getGuaranteed(Symbol entity, Symbol attribute) {
        Symbol value;
        if(!getUncertain(entity, attribute, value)) {
            Symbol data = Ontology::create();
            link({data, PreDef_Entity, entity});
            link({data, PreDef_Attribute, attribute});
            throwException("Nonexistent or ambiguous", data);
        }
        return value;
    }

    // TODO: Cleanup created symbols in case of an exception
    jmp_buf exceptionEnv;
    Symbol task, status, frame, block;

    Thread() :task(PreDef_Void) { }

    void setStatus(Symbol _status) {
        status = _status;
        setSolitary({task, PreDef_Status, status});
    }

    void setFrame(Symbol _frame, bool setBlock) {
        assert(frame != _frame);
        if(_frame == PreDef_Void)
            block = PreDef_Void;
        else {
            link({task, PreDef_Holds, _frame});
            setSolitary({task, PreDef_Frame, _frame});
            if(setBlock)
                block = getGuaranteed(_frame, PreDef_Block);
        }
        Ontology::unlink({task, PreDef_Holds, frame});
        if(frame != PreDef_Void)
            scrutinizeExistence(frame);
        frame = _frame;
    }

    void throwException(const char* messageStr, Symbol data = PreDef_Void) {
        block = (data != PreDef_Void) ? data : Ontology::create();
        Symbol message = Ontology::createFromData(messageStr);
        Ontology::blobIndex.insertElement(message);
        link({block, PreDef_Message, message});
        pushCallStack();
        link({frame, PreDef_Procedure, PreDef_Exception}); // TODO: debugging
        longjmp(exceptionEnv, 1);
    }

    void pushCallStack() {
        Symbol childFrame = Ontology::create();
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
        Symbol parentFrame = PreDef_Void;
        bool parentExists = getUncertain(frame, PreDef_Parent, parentFrame);
        if(parentFrame == PreDef_Void)
            setStatus(PreDef_Done);
        setFrame(parentFrame, true);
        return parentFrame != PreDef_Void;
    }

    Symbol getTargetSymbol() {
        Symbol result;
        if(!getUncertain(block, PreDef_Target, result)) {
            Symbol parentFrame = getGuaranteed(frame, PreDef_Parent);
            result = getGuaranteed(parentFrame, PreDef_Block);
        }
        return result;
    }

    void clear() {
        if(task == PreDef_Void)
            return;
        while(popCallStack());
        Ontology::destroy(task);
        task = status = frame = block = PreDef_Void;
    }

    bool step() {
        if(!running())
            return false;

        Symbol parentBlock = block, parentFrame = frame, execute,
               procedure, next = PreDef_Void, catcher, staticParams, dynamicParams;
        if(!getUncertain(parentFrame, PreDef_Execute, execute)) {
            popCallStack();
            return true;
        }

        if(setjmp(exceptionEnv) == 0) {
            procedure = getGuaranteed(execute, PreDef_Procedure);
            block = Ontology::create();
            pushCallStack();
            link({frame, PreDef_Procedure, procedure}); // TODO: debugging

            if(getUncertain(execute, PreDef_Static, staticParams))
                query(12, {staticParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    link({block, result.pos[0], result.pos[1]});
                });

            if(getUncertain(execute, PreDef_Dynamic, dynamicParams))
                query(12, {dynamicParams, PreDef_Void, PreDef_Void}, [&](Triple result, ArchitectureType) {
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
                            query(9, {parentBlock, result.pos[1], PreDef_Void}, [&](Triple resultB, ArchitectureType) {
                                link({block, result.pos[0], resultB.pos[0]});
                            });
                            break;
                    }
                });

            getUncertain(execute, PreDef_Next, next);
            setSolitary({parentFrame, PreDef_Execute, next});

            if(getUncertain(execute, PreDef_Catch, catcher))
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

    void deserializationTask(Symbol input, Symbol package = PreDef_Void) {
        clear();

        block = Ontology::create();
        link({block, PreDef_Holds, input});
        if(package == PreDef_Void)
            package = block;
        Symbol staticParams = Ontology::create();
        link({staticParams, PreDef_Package, package});
        link({staticParams, PreDef_Input, input});
        link({staticParams, PreDef_Target, block});
        link({staticParams, PreDef_Output, PreDef_Output});
        Symbol execute = Ontology::create();
        link({execute, PreDef_Procedure, PreDef_Deserialize});
        link({execute, PreDef_Static, staticParams});

        task = Ontology::create();
        Symbol childFrame = Ontology::create();
        link({childFrame, PreDef_Holds, staticParams});
        link({childFrame, PreDef_Holds, execute});
        link({childFrame, PreDef_Holds, block});
        link({childFrame, PreDef_Block, block});
        link({childFrame, PreDef_Execute, execute});
        setFrame(childFrame, false);
        executeFinite(1);
    }

    bool executeDeserialized() {
        Symbol prev = PreDef_Void;
        if(query(9, {block, PreDef_Output, PreDef_Void}, [&](Triple result, ArchitectureType) {
            Symbol next = Ontology::create();
            link({next, PreDef_Procedure, result.pos[0]});
            if(prev == PreDef_Void)
                setSolitary({frame, PreDef_Execute, next});
            else
                link({prev, PreDef_Next, next});
            prev = next;
        }) == 0)
            return false;

        executeInfinite();
        return true;
    }
};
