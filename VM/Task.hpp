#include "Context.hpp"

struct Task {
    Context* context;
    Symbol task, status, frame, block;

    template <typename T>
    T& get(Extend* extend) {
        if(extend->size != sizeof(T)*8)
            throwException("Invalid Extend Size");
        return *reinterpret_cast<T*>(extend->data.get());
    }

    ArchitectureType query(ArchitectureType mode, Triple triple, std::function<void(Triple, ArchitectureType)> callback = nullptr) {
        struct QueryMethod {
            uint8_t index, pos, size;
            ArchitectureType(Context::*function)(ArchitectureType, Triple&, std::function<void()>);
        };

        const QueryMethod lookup[] = {
            {EAV, 0, 0, &Context::searchGGG},
            {AVE, 2, 1, &Context::searchGGV},
            {AVE, 0, 0, nullptr},
            {VEA, 2, 1, &Context::searchGGV},
            {VEA, 1, 2, &Context::searchGVV},
            {VAE, 1, 1, &Context::searchGVI},
            {VEA, 0, 0, nullptr},
            {VEA, 1, 1, &Context::searchGVI},
            {VEA, 0, 0, nullptr},
            {EAV, 2, 1, &Context::searchGGV},
            {AVE, 1, 2, &Context::searchGVV},
            {AVE, 1, 1, &Context::searchGVI},
            {EAV, 1, 2, &Context::searchGVV},
            {EAV, 0, 3, &Context::searchVVV},
            {AVE, 0, 2, &Context::searchVVI},
            {EVA, 1, 1, &Context::searchGVI},
            {VEA, 0, 2, &Context::searchVVI},
            {VEA, 0, 1, &Context::searchVII},
            {EAV, 0, 0, nullptr},
            {AEV, 1, 1, &Context::searchGVI},
            {AVE, 0, 0, nullptr},
            {EAV, 1, 1, &Context::searchGVI},
            {EAV, 0, 2, &Context::searchVVI},
            {AVE, 0, 1, &Context::searchVII},
            {EAV, 0, 0, nullptr},
            {EAV, 0, 1, &Context::searchVII},
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

        if(context->indexMode == Context::MonoIndex) {
            if(method.index != EAV) {
                method.index = EAV;
                method.function = &Context::searchVVV;
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
        } else if(context->indexMode == Context::TriIndex && method.index >= 3) {
            method.index -= 3;
            method.pos = 2;
            method.function = &Context::searchGIV;
        }

        triple = triple.reordered(method.index);
        if(!callback) handleNext = nullptr;
        return (context->*method.function)(method.index, triple, handleNext);
    }

    void link(Triple triple) {
        if(!context->link(triple))
            throwException("Already linked");
    }

    template<bool skip = false>
    void unlink(std::set<Triple> triples, std::set<Symbol> skipSymbols = {}) {
        if(!context->unlink<skip>(triples, skipSymbols))
            throwException("Already unlinked");
    }

    void unlink(Symbol alpha, Symbol beta) {
        std::set<Triple> triples;
        query(9, {alpha, beta, PreDef_Void}, [&](Triple result, ArchitectureType) {
            triples.insert({alpha, beta, result.pos[0]});
        });
        unlink(triples);
    }

    void unlink(Triple triple) {
        unlink(std::set<Triple>{triple});
    }

    template<bool search = true>
    Symbol symbolFor(uint64_t value) {
        return context->symbolFor<PreDef_Natural, search>(value);
    }

    template<bool search = true>
    Symbol symbolFor(int64_t value) {
        return context->symbolFor<PreDef_Integer, search>(value);
    }

    template<bool search = true>
    Symbol symbolFor(double value) {
        return context->symbolFor<PreDef_Float, search>(value);
    }

    template<bool search = true>
    Symbol symbolFor(const char* value) {
        return context->symbolFor<PreDef_Text, search>(value);
    }

    template<bool search = true>
    Symbol symbolFor(const std::string& value) {
        return context->symbolFor<PreDef_Text, search>(value.c_str());
    }

    template<bool search = true>
    Symbol symbolFor(std::istream& stream) {
        Extend extend;
        extend.overwrite(stream);
        return context->symbolFor<PreDef_Text, search>(std::move(extend));
    }

    template<bool setBlock>
    void setFrame(Symbol _frame) {
        frame = _frame;
        link({task, PreDef_Holds, frame});
        setSolitary({task, PreDef_Frame, frame});
        if(setBlock)
            block = getGuaranteed(frame, PreDef_Block);
    }

    void throwException(const char* message, std::set<std::pair<Symbol, Symbol>> links = {}) {
        assert(task != PreDef_Void && frame != PreDef_Void);
        links.insert(std::make_pair(PreDef_Message, symbolFor<false>(message)));

        Symbol parentFrame = frame;
        block = context->create(links);
        setFrame<false>(context->create({
            {PreDef_Holds, block},
            {PreDef_Block, block}
        }));

        if(parentFrame != PreDef_Void) {
            link({frame, PreDef_Holds, parentFrame});
            link({frame, PreDef_Parent, parentFrame});
            unlink({task, PreDef_Holds, parentFrame});
        }
        if(context->debug)
            link({frame, PreDef_Module, PreDef_Exception});

        setSolitary({task, PreDef_Frame, frame});
        throw Exception{};
    }

    void destroy(Symbol alpha) {
        std::set<Triple> triples;
        auto topIter = context->topIndex.find(alpha);
        if(topIter == context->topIndex.end())
            throwException("Already destroyed");
        if(topIter->second->extend.size > 0) {
            Symbol type = PreDef_Void;
            getUncertain(alpha, PreDef_Extend, type);
            context->extendIndexOfType(type)->erase(&topIter->second->extend);
        }
        for(ArchitectureType i = EAV; i <= VEA; ++i)
            for(auto& beta : topIter->second->subIndices[i])
                for(auto gamma : beta.second)
                    triples.insert(Triple(alpha, beta.first, gamma).normalized(i));
        unlink<true>(triples, {alpha});
        context->topIndex.erase(topIter);
    }

    void scrutinizeExistence(Symbol symbol) {
        // TODO Prototype and Destructor
        std::queue<Symbol> symbols;
        symbols.push(symbol);
        while(!symbols.empty()) {
            symbol = symbols.front();
            symbols.pop();
            if(context->topIndex.find(symbol) != context->topIndex.end() &&
               query(1, {PreDef_Void, PreDef_Holds, symbol}) == 0) {
                query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result, ArchitectureType) {
                    symbols.push(result.pos[0]);
                });
                destroy(symbol);
            }
        }
    }

    void setSolitary(Triple triple) {
        bool toLink = true;
        std::set<Triple> triples;
        query(9, triple, [&](Triple result, ArchitectureType) {
            if(triple.pos[2] == result.pos[0])
                toLink = false;
            else
                triples.insert({triple.pos[0], triple.pos[1], result.pos[0]});
        });
        if(toLink)
            link(triple);
        unlink(triples);
    }

    bool getUncertain(Symbol alpha, Symbol beta, Symbol& gamma) {
        if(query(9, {alpha, beta, PreDef_Void}, [&](Triple result, ArchitectureType) {
            gamma = result.pos[0];
        }) != 1) return false;
        return true;
    }

    Symbol getGuaranteed(Symbol entity, Symbol attribute) {
        Symbol value;
        if(!getUncertain(entity, attribute, value))
            throwException("Nonexistent or Ambiguous", {
                {PreDef_Entity, entity},
                {PreDef_Attribute, attribute}
            });
        return value;
    }

    bool popCallStack() {
        assert(task != PreDef_Void);
        if(frame == PreDef_Void) return false;
        assert(context->topIndex.find(frame) != context->topIndex.end());

        Symbol parentFrame;
        bool parentExists = getUncertain(frame, PreDef_Parent, parentFrame);
        destroy(frame);
        scrutinizeExistence(block);
        if(parentExists) {
            setFrame<true>(parentFrame);
            return true;
        }

        unlink(task, PreDef_Frame);
        setSolitary({task, PreDef_Status, PreDef_Done});
        frame = block = PreDef_Void;
        status = PreDef_Done;
        return false;
    }

    Symbol popCallStackTargetSymbol() {
        Symbol TargetSymbol;
        bool target = getUncertain(block, PreDef_Target, TargetSymbol);
        assert(popCallStack());
        if(target)
            return TargetSymbol;
        return block;
    }

    void clear() {
        if(task == PreDef_Void) return;
        while(popCallStack());
        destroy(task);
        task = status = frame = block = PreDef_Void;
    }

    void remapExtend(Symbol entity, Symbol toType) {
        // TODO use this method when setSolitary of PreDef_Extend is called
        Extend* extend = context->getExtend(entity);
        if(extend->size == 0) return;
        Symbol fromType = PreDef_Void;
        getUncertain(entity, PreDef_Extend, fromType);
        context->extendIndexOfType(fromType)->erase(extend);
        context->extendIndexOfType(toType)->insert(std::make_pair(extend, entity));
    }

    void serializeExtend(std::ostream& stream, Symbol entity) {
        auto topIter = context->topIndex.find(entity);
        assert(topIter != context->topIndex.end());
        if(topIter->second->extend.size) {
            Symbol type = PreDef_Void;
            getUncertain(entity, PreDef_Extend, type);
            switch(type) {
                case PreDef_Text: {
                    std::string str((const char*)topIter->second->extend.data.get(), (topIter->second->extend.size+7)/8);
                    if(std::find_if(str.begin(), str.end(), ::isspace) == str.end())
                        stream << str;
                    else
                        stream << '"' << str << '"';
                } break;
                case PreDef_Natural:
                    stream << get<uint64_t>(&topIter->second->extend);
                break;
                case PreDef_Integer:
                    stream << get<int64_t>(&topIter->second->extend);
                break;
                case PreDef_Float:
                    stream << get<double>(&topIter->second->extend);
                break;
                default:
                    stream << "raw:" << std::setfill('0') << std::hex << std::uppercase;
                    auto ptr = reinterpret_cast<uint8_t*>(topIter->second->extend.data.get());
                    for(size_t i = 0; i < (topIter->second->extend.size+7)/8; ++i)
                        stream << std::setw(2) << static_cast<uint16_t>(ptr[i]);
                    stream << std::dec;
                break;
            }
        } else
            stream << "#" << topIter->first;
    }

    void serializeEntity(std::ostream& stream, Symbol entity, std::function<Symbol(Symbol)> followCallback = nullptr) {
        Symbol followAttribute;
        while(true) {
            auto topIter = context->topIndex.find(entity);
            assert(topIter != context->topIndex.end());

            if(followCallback)
                followAttribute = followCallback(entity);

            stream << "(\n    ";
            serializeExtend(stream, entity);
            stream << ";\n";

            std::set<Symbol>* lastAttribute = nullptr;
            for(auto& j : topIter->second->subIndices[EAV]) {
                if(followCallback && j.first == followAttribute) {
                    lastAttribute = &j.second;
                    continue;
                }
                stream << "    ";
                serializeExtend(stream, j.first);
                for(auto& k : j.second) {
                    stream << " ";
                    serializeExtend(stream, k);
                }
                stream << ";" << std::endl;
            }
            if(!followCallback || !lastAttribute) {
                stream << ")";
                return;
            }

            stream << "    ";
            serializeExtend(stream, followAttribute);
            for(auto iter = lastAttribute->begin(); iter != --lastAttribute->end(); ++iter) {
                stream << " ";
                serializeExtend(stream, *iter);
            }
            entity = *lastAttribute->rbegin();
            stream << std::endl;
            stream << ") ";
        }
    }

    void writeToStream(std::ostream& stream) {
        auto preDefCount = sizeof(PreDefStrings)/sizeof(void*);
        for(auto& entity : context->topIndex) {
            auto& subIndex = entity.second->subIndices[EAV];
            if((entity.first < preDefCount) &&
               ((entity.first == PreDef_Foundation && subIndex.size() == 2 &&
                   subIndex.find(PreDef_Extend) != subIndex.end() &&
                   subIndex.find(PreDef_Holds) != subIndex.end()) ||
                   (subIndex.size() == 1 && subIndex.begin()->first == PreDef_Extend)))
                continue;
            if(subIndex.empty()) {
                stream << "(";
                serializeExtend(stream, entity.first);
                stream << ";)" << std::endl;
            } else
                for(auto& j : subIndex) {
                    auto attribute = context->topIndex.find(j.first);
                    for(auto& k : j.second) {
                        auto value = context->topIndex.find(k);
                        stream << "(";
                        serializeExtend(stream, entity.first);
                        stream << "; ";
                        serializeExtend(stream, j.first);
                        stream << " ";
                        serializeExtend(stream, k);
                        stream << ")" << std::endl;
                    }
                }
        }
    }

    bool step();

    bool uncaughtException() {
        return query(0, {task, PreDef_Status, PreDef_Exception}) == 1;
    }

    bool running() {
        return query(0, {task, PreDef_Status, PreDef_Run}) == 1;
    }

    void executeFinite(ArchitectureType n) {
        if(task == PreDef_Void) return;
        for(ArchitectureType i = 0; i < n && step(); ++i);
    }

    void executeInfinite() {
        if(task == PreDef_Void) return;
        while(step());
    }

    void executeUntilNextPop() {
        if(task == PreDef_Void) return;
        while(running() && query(9, {frame, PreDef_Execute, PreDef_Void}) == 1 && step());
    }

    void executeUntilFrameIsReached(Symbol frameToReach) {
        // TODO: What about exceptions
        if(task == PreDef_Void) return;
        while(step())
            if(getGuaranteed(task, PreDef_Frame) == frameToReach)
                break;
    }

    void executeFinishFrame() {
        if(task == PreDef_Void) return;
        Symbol frameToReach;
        if(getUncertain(frame, PreDef_Parent, frameToReach))
            executeUntilFrameIsReached(frameToReach);
        else
            executeInfinite();
    }

    void executeStepOver() {
        executeUntilFrameIsReached(getGuaranteed(task, PreDef_Frame));
    }

    void evaluateExtend(Symbol input, bool doExecute, Symbol package = PreDef_Void) {
        clear();

        block = context->create({
            {PreDef_Holds, input}
        });
        if(package == PreDef_Void)
            package = block;
        Symbol deserializeInst = context->create({
            {PreDef_Module, PreDef_Deserialize},
            {PreDef_Package, package},
            {PreDef_Input, input},
            {PreDef_Target, block},
            {PreDef_Output, PreDef_Target}
        });
        task = context->create({{PreDef_Status, PreDef_Run}});
        setFrame<false>(context->create({
            {PreDef_Holds, block},
            {PreDef_Block, block},
            {PreDef_Execute, deserializeInst}
        }));
        status = PreDef_Run;
        link({block, PreDef_Holds, deserializeInst});

        ArchitectureType execCount = 2;
        if(doExecute) {
            Symbol executeInst = context->create({
                {PreDef_Module, PreDef_Branch},
                {PreDef_Branch, PreDef_Target}
            });
            link({deserializeInst, PreDef_Next, executeInst});
            link({block, PreDef_Holds, executeInst});
            ++execCount;
        }
        executeFinite(execCount);
    }
};
