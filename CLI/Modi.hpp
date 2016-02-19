#include "CLI.hpp"

#define clippedForEachInContainer(iter, container, item) \
    auto iter##Begin = container.begin(), iter##End = container.begin(); \
    size = container.size(); \
    pos = std::distance(iter##Begin, container.find(item)); \
    if(size > maxSize) size = maxSize; \
    linesLeft -= size; \
    pos = (pos >= (size-1)/2) ? pos-(size-1)/2 : 0; \
    if(pos > container.size()-size) pos = container.size()-size; \
    std::advance(iter##Begin, pos); \
    std::advance(iter##End, pos+size); \
    for(auto iter = iter##Begin; iter != iter##End; ++iter)

#define historyTop0() \
    historyTop = (history.size()-1)/4*4; \
    historySub = history.size()-1-historyTop; \
    topIter = context.topIndex.find(history[historyTop]);

#define historyTop2() \
    auto& subIndex = symbolObject->subIndices[history[historyTop+1]-1]; \
    auto subIter = subIndex.find(history[historyTop+2]); \
    if(subIter == subIndex.end()) { \
        history.pop_back(); \
        return 1; \
    }

#define historyTop3() \
    historyTop2() \
    auto& set = subIter->second; \
    auto iter = set.find(history[historyTop+3]); \
    if(iter == set.end()) { \
        history.pop_back(); \
        return 1; \
    }

void render() {
    ioctl(STDIN_FILENO, TIOCGWINSZ, &screenSize);
    clearScreen();
    if(screenSize.ws_row <= context.indexMode+4) {
        std::cout << std::endl << "Console is too small" << std::endl;
        pollKeyboard([&](bool special, uint64_t size, const char* buffer) {
            exit(0);
            return 1;
        });
    }

    if(history.size() == 0)
        history.push_back(task.task);
    ArchitectureType linesLeft = screenSize.ws_row-context.indexMode-5,
                     pos, size, maxSize;
    linesForBlob = linesLeft;
    historyTop0()
    if(topIter == context.topIndex.end()) {
        history.clear();
        history.push_back(task.task);
        return;
    }
    symbolObject = topIter->second.get();

    std::cout << "Stats: ";
    std::cout << context.topIndex.size();
    std::cout << " / " << context.nextSymbol;
    std::cout << " / " << task.context.query(13, {PreDef_Void, PreDef_Void, PreDef_Void});

    stream << "History: ";
    if(history.size() > 4)
        for(ArchitectureType i = historyTop-4; true; i -= 4) {
            serializeBlob(task, stream, history[i]);
            if(i > 0)
                stream << " / ";
            else
                break;
        }
    printStreamLimited();

    stream << history[historyTop] << ": ";
    serializeBlob(task, stream, history[historyTop]);
    printStreamLimited(historySub == 0);

    stream << "Blob " << symbolObject->blobSize;
    printStreamLimited(historySub == 1 && history[historyTop+1] == 0, 2);
    if(historySub == 2 && history[historyTop+1] == 0) {
        serializeBlob(task, stream, history[historyTop]);
        linesLeft -= printStreamLimited(1, 4, linesLeft, history[historyTop+2]);
    }

    const char* indexName[] = { "EAV", "AVE", "VEA", "EVA", "AEV", "VAE" };
    for(ArchitectureType i = 0; i < context.indexMode; ++i) {
        auto& subIndex = symbolObject->subIndices[i];
        stream << indexName[i] << " " << subIndex.size();
        printStreamLimited(historySub == 1 && i+1 == history[historyTop+1], 2);
        if(historySub < 2 || i+1 != history[historyTop+1]) continue;
        maxSize = linesLeft;
        if(historySub == 3) {
            size = subIndex.find(history[historyTop+2])->second.size();
            maxSize = (size <= maxSize-3) ? maxSize-size : 3;
        }
        clippedForEachInContainer(j, subIndex, history[historyTop+2]) {
            serializeBlob(task, stream, j->first);
            stream << " " << j->second.size();
            printStreamLimited(historySub == 2 && j->first == history[historyTop+2], 4);
            if(historySub < 3 || j->first != history[historyTop+2]) continue;
            maxSize = linesLeft;
            auto& set = subIndex.find(j->first)->second;
            clippedForEachInContainer(k, set, history[historyTop+3]) {
                serializeBlob(task, stream, *k);
                printStreamLimited(historySub == 3 && *k == history[historyTop+3], 6);
            }
        }
    }

    setCursorPosition(0, screenSize.ws_row-2);
    switch(mode) {
        case Mode_Browse:
            stream << interfaceBuffer;
            break;
        case Mode_Input:
            stream << "> " << interfaceBuffer;
            break;
    }
    printStreamLimited();

    std::cout.flush();
}

uint64_t ModeBrowse(bool special, uint64_t size, const char* buffer) {
    enum Command {
        Up, Down, Right, Left
    } com;

    if(special) {
        switch(*buffer) {
            case 'A':
                com = Up;
                break;
            case 'B':
                com = Down;
                break;
            case 'C':
                com = Right;
                break;
            case 'D':
                com = Left;
                break;
            default:
                return 1;
        }
    } else {
        switch(*buffer) {
            case 'a':
                task.clear();
                return 1;
            case 'n':
                task.executeFinite(1);
                if(!task.uncaughtException())
                    return 1;
                break;
            case 'c':
                task.executeInfinite();
                if(!task.uncaughtException())
                    return 1;
                break;
            case 'h':
                com = Left;
                break;
            case 'j':
                com = Down;
                break;
            case 'k':
                com = Up;
                break;
            case 'l':
                com = Right;
                break;
            case ' ':
                mode = Mode_Input;
                interfaceBuffer.clear();
                setCursorHidden(false);
                return 1;
            case 27:
                clearScreen();
                terminate();
                return 1;
            case 9:
                switch(historySub) {
                    case 0:
                    case 1:
                        return 1;
                    case 2: {
                        if(history[historyTop+1] == 0) return 1;
                        historyTop2()
                        auto& set = subIter->second;
                        if(set.size() > 0)
                            history.push_back(history[historyTop+2]);
                        history.push_back(history[historyTop+2]);
                        if(history.size() > historyLimit*4)
                            history.erase(0, 4);
                    }   return 1;
                    case 3:
                        com = Right;
                        break;
                }
                break;
            default:
                return 1;
        }
    }

    switch(com) {
        case Up:
            switch(historySub) {
                case 0:
                    if(topIter != context.topIndex.begin())
                        history[historyTop] = (--topIter)->first;
                    break;
                case 1:
                    if(history.back() > 0) --history.back();
                    break;
                case 2: {
                    if(history[historyTop+1] == 0) {
                        if(history.back() > 0) --history.back();
                    } else {
                        historyTop2()
                        if(subIter != subIndex.begin())
                            history[historyTop+2] = (--subIter)->first;
                    }
                }   break;
                case 3: {
                    historyTop3()
                    if(iter != set.begin())
                        history[historyTop+3] = *(--iter);
                }   break;
            }
            break;
        case Down:
            switch(historySub) {
                case 0:
                    ++topIter;
                    if(topIter != context.topIndex.end())
                        history[historyTop] = topIter->first;
                        break;
                case 1:
                    if(history.back() < context.indexMode)
                        ++history.back();
                        break;
                case 2: {
                    if(history[historyTop+1] == 0) {
                        serializeBlob(task, stream, history[historyTop]);
                        auto lines = printStreamLimited(2, 0, 0, 0);
                        if(lines > linesForBlob && history.back() < lines-linesForBlob)
                            ++history.back();
                    } else {
                        historyTop2()
                        if(++subIter != subIndex.end())
                            history[historyTop+2] = subIter->first;
                    }
                }   break;
                case 3: {
                    historyTop3()
                    if(++iter != set.end())
                        history[historyTop+3] = *iter;
                }   break;
            }
            break;
        case Right:
            switch(historySub) {
                case 0:
                    history.push_back(1);
                    break;
                case 1: {
                    if(history[historyTop+1] == 0) {
                        if(symbolObject->blobSize > 0)
                            history.push_back(0);
                    } else {
                        auto& subIndex = symbolObject->subIndices[history[historyTop+1]-1];
                        if(subIndex.size() > 0)
                            history.push_back(subIndex.begin()->first);
                    }
                }   break;
                case 2: {
                    if(history[historyTop+1] == 0) break;
                    historyTop2()
                    auto& set = subIter->second;
                    if(set.size() > 0)
                        history.push_back(*set.begin());
                }   break;
                case 3:
                    history.push_back(history[historyTop+3]);
                    if(history.size() > historyLimit*4)
                        history.erase(0, 4);
                break;
            }
            break;
        case Left:
            if(history.size() > 1) {
                history.pop_back();
                historyTop0()
                switch(historySub) {
                    case 2: {
                        historyTop2()
                    }   break;
                    case 3: {
                        historyTop3()
                    }   break;
                }
            }
            break;
    }

    return 1;
}

uint64_t ModeInput(bool special, uint64_t size, const char* buffer) {
    if(!special) {
        switch(*buffer) {
            case 27:
                interfaceBuffer.clear();
                setCursorHidden(true);
                mode = Mode_Browse;
                break;
            case 10: {
                setCursorHidden(true);
                mode = Mode_Browse;
                history.clear();
                history.push_back(task.task);
                if(interfaceBuffer.empty()) break;
                task.deserializationTask(task.context.createFromData(interfaceBuffer.c_str()));
                history[0] = task.task;
                if(task.uncaughtException()) {
                    interfaceBuffer = "Exception occurred while deserializing input";
                    break;
                }
                interfaceBuffer.clear();
                Symbol OutputSymbol, ExecuteSymbol;
                if(task.context.getUncertain(task.block, PreDef_Output, OutputSymbol) &&
                   !task.context.getUncertain(OutputSymbol, PreDef_Execute, ExecuteSymbol)) {
                    history[0] = OutputSymbol;
                    break;
                }
                if(task.executeDeserialized() && task.uncaughtException())
                    interfaceBuffer = "Exception occurred while executing input";
            }   break;
            case 127:
                if(interfaceBuffer.size())
                    interfaceBuffer.erase(interfaceBuffer.size()-1);
                break;
            default:
                interfaceBuffer += *buffer;
                break;
        }
    }
    return 1;
}
