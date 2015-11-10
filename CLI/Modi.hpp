#include "CLI.hpp"

enum {
    Mode_Exception,
    Mode_Menu,
    Mode_Browse,
    Mode_Execute
} mode = Mode_Menu;

void ModeException() {
    printTitle("Exception");
    std::cout << std::endl << interfaceBuffer;
    std::cout.flush();

    pollKeyboard([&](bool special, uint64_t size, const char* buffer) {
        interfaceBuffer.clear();
        mode = Mode_Menu;
        return 1;
    });
}

void ModeMenu() {
    printTitle("Menu");
    std::cout << std::endl << "ESC: Quit";
    std::cout << std::endl << "B: Browse";
    std::cout << std::endl << "E: Execute";

    std::cout << std::endl << "A: Abort";
    std::cout << std::endl << "N: Step Next";
    std::cout << std::endl << "C: Continue Infinite";
    std::cout << std::endl << "F: Finish Frame";
    std::cout << std::endl << "O: Step Over";

    std::cout << std::endl << std::endl << "Image Stats: ";
    std::cout << std::endl << "Symbols: " << context.topIndex.size() << " / " << context.nextSymbol;
    std::cout << std::endl << "Triples: " << task.query(13, {PreDef_Void, PreDef_Void, PreDef_Void});
    std::cout.flush();

    pollKeyboard([&](bool special, uint64_t size, const char* buffer) {
        if(!special) {
            switch(*buffer) {
                case 'b':
                    mode = Mode_Browse;
                    cursor.clear();
                    cursor.push_back(task.task);
                return 1;
                case 'e':
                    mode = Mode_Execute;
                    setCursorHidden(false);
                    cursor.clear();
                    cursor.push_back(0);
                return 1;
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
                case 'f':
                    task.executeFinishFrame();
                    if(!task.uncaughtException())
                        return 1;
                break;
                case 'o':
                    task.executeStepOver();
                    if(!task.uncaughtException())
                        return 1;
                break;
                case 27:
                    terminate();
                return 1;
                default:
                return 1;
            }
        } else
            return 1;
        interfaceBuffer = "Execution stopped";
        mode = Mode_Exception;
        return 1;
    });
}

#define clippedForEachInContainer(iter, container, item) \
    auto iter##Begin = container.begin(), iter##End = container.begin(); \
    size = container.size(); \
    pos = std::distance(iter##Begin, container.find(item)); \
    if(size > maxSize) size = maxSize; \
    linesLeft -= size; \
    pos = (pos < size/2) ? 0 : pos-size/2; \
    if(pos > container.size()-size) pos = container.size()-size; \
    std::advance(iter##Begin, pos); \
    pos += size; \
    if(pos > container.size()) pos = container.size(); \
    std::advance(iter##End, pos); \
    for(auto iter = iter##Begin; iter != iter##End; ++iter)

#define cursorTop0() \
    cursorTop = (cursor.size()-1)/4*4; \
    cursorSub = cursor.size()-1-cursorTop; \
    topIter = context.topIndex.find(cursor[cursorTop]);

#define cursorTop2() \
    auto& subIndex = topIter->second->subIndices[cursor[cursorTop+1]-1]; \
    auto subIter = subIndex.find(cursor[cursorTop+2]); \
    if(subIter == subIndex.end()) { \
        cursor.pop_back(); \
        return 1; \
    }

#define cursorTop3() \
    cursorTop2() \
    auto& set = subIter->second; \
    auto iter = set.find(cursor[cursorTop+3]); \
    if(iter == set.end()) { \
        cursor.pop_back(); \
        return 1; \
    }

void ModeBrowse() {
    printTitle("Browse");
    ArchitectureType historyLimit = 8,
                     linesLeft = screenSize.ws_row-context.indexMode-4,
                     linesForExtend = linesLeft,
                     pos, size, maxSize, cursorTop, cursorSub;
    decltype(context.topIndex)::iterator topIter;
    cursorTop0()
    if(topIter == context.topIndex.end()) {
        mode = Mode_Menu;
        return;
    }
    Extend* extend = &topIter->second->extend;

    if(screenSize.ws_row <= context.indexMode+4) {
        std::cout << std::endl << "Console is too small" << std::endl;
    } else {
        stream << "History: ";
        if(cursor.size() > 4)
            for(ArchitectureType i = cursorTop-4; true; i -= 4) {
                stream << cursor[i] << " ";
                task.serializeExtend(stream, cursor[i]);
                if(i > 0)
                    stream << " / ";
                else
                    break;
            }
        printStreamLimited();

        stream << cursor[cursorTop] << " ";
        task.serializeExtend(stream, cursor[cursorTop]);
        printStreamLimited(cursorSub == 0);

        stream << "Extend " << extend->size;
        printStreamLimited(cursorSub == 1 && cursor[cursorTop+1] == 0, 2);
        if(cursorSub == 2 && cursor[cursorTop+1] == 0) {
            task.serializeExtend(stream, cursor[cursorTop]);
            linesLeft -= printStreamLimited(1, 4, linesLeft, cursor[cursorTop+2]);
        }

        const char* indexName[] = { "EAV", "AVE", "VEA", "EVA", "AEV", "VAE" };
        for(ArchitectureType i = 0; i < context.indexMode; ++i) {
            auto& subIndex = topIter->second->subIndices[i];
            stream << indexName[i] << " " << subIndex.size();
            printStreamLimited(cursorSub == 1 && i+1 == cursor[cursorTop+1], 2);
            if(cursorSub < 2 || i+1 != cursor[cursorTop+1]) continue;
            maxSize = linesLeft;
            if(cursorSub == 3) {
                size = subIndex.find(cursor[cursorTop+2])->second.size();
                if(maxSize > size) maxSize -= size;
            }
            clippedForEachInContainer(j, subIndex, cursor[cursorTop+2]) {
                stream << j->first << " ";
                task.serializeExtend(stream, j->first);
                stream << " " << j->second.size();
                printStreamLimited(cursorSub == 2 && j->first == cursor[cursorTop+2], 4);
                if(cursorSub < 3 || j->first != cursor[cursorTop+2]) continue;
                maxSize = linesLeft;
                auto& set = subIndex.find(j->first)->second;
                clippedForEachInContainer(k, set, cursor[cursorTop+3]) {
                    stream << *k << " ";
                    task.serializeExtend(stream, *k);
                    printStreamLimited(cursorSub == 3 && *k == cursor[cursorTop+3], 6);
                }
            }
        }
        std::cout.flush();
    }

    pollKeyboard([&](bool special, uint64_t size, const char* buffer) {
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
                case 127:
                    if(cursorSub == 0)
                        com = Left;
                    else {
                        if(cursorTop == 0)
                            mode = Mode_Menu;
                        else
                            cursor.erase(cursor.begin()+cursorTop, cursor.end());
                        return 1;
                    }
                break;
                case 10:
                    switch(cursorSub) {
                        case 0:
                            /* TODO: Set focus
                            task.clear();
                            task.task = cursor[cursorTop];
                            mode = Mode_Menu;*/
                        case 1:
                        case 3:
                        return 1;
                        case 2: {
                            if(cursor[cursorTop+1] == 0) return 1;
                            cursorTop2()
                            auto& set = subIter->second;
                            if(set.size() > 0)
                                cursor.push_back(cursor[cursorTop+2]);
                            if(cursor.size() > historyLimit*4)
                                cursor.erase(cursor.begin(), cursor.begin()+4);
                            cursor.push_back(cursor[cursorTop+2]);
                        } return 1;
                    }
                break;
                case 27:
                    mode = Mode_Menu;
                return 1;
                default:
                return 1;
            }
        }

        switch(com) {
            case Up:
                switch(cursorSub) {
                    case 0:
                        if(topIter != context.topIndex.begin())
                            cursor[cursorTop] = (--topIter)->first;
                    break;
                    case 1:
                        if(*cursor.rbegin() > 0) *cursor.rbegin() -= 1;
                    break;
                    case 2: {
                        if(cursor[cursorTop+1] == 0) {
                            if(*cursor.rbegin() > 0) *cursor.rbegin() -= 1;
                        } else {
                            cursorTop2()
                            if(subIter != subIndex.begin())
                                cursor[cursorTop+2] = (--subIter)->first;
                        }
                    } break;
                    case 3: {
                        cursorTop3()
                        if(iter != set.begin())
                            cursor[cursorTop+3] = *(--iter);
                    } break;
                }
            break;
            case Down:
                switch(cursorSub) {
                    case 0:
                        ++topIter;
                        if(topIter != context.topIndex.end())
                            cursor[cursorTop] = topIter->first;
                    break;
                    case 1:
                        if(*cursor.rbegin() < context.indexMode)
                            *cursor.rbegin() += 1;
                    break;
                    case 2: {
                        if(cursor[cursorTop+1] == 0) {
                            task.serializeExtend(stream, cursor[cursorTop]);
                            auto lines = printStreamLimited(2, 0, 0, 0);
                            if(lines > linesForExtend && *cursor.rbegin() < lines-linesForExtend)
                                *cursor.rbegin() += 1;
                        } else {
                            cursorTop2()
                            if(++subIter != subIndex.end())
                                cursor[cursorTop+2] = subIter->first;
                        }
                    } break;
                    case 3: {
                        cursorTop3()
                        if(++iter != set.end())
                            cursor[cursorTop+3] = *iter;
                    } break;
                }
            break;
            case Right:
                switch(cursorSub) {
                    case 0:
                        cursor.push_back(1);
                    break;
                    case 1: {
                        if(cursor[cursorTop+1] == 0) {
                            if(extend->size > 0)
                                cursor.push_back(0);
                        } else {
                            auto& subIndex = topIter->second->subIndices[cursor[cursorTop+1]-1];
                            if(subIndex.size() > 0)
                                cursor.push_back(subIndex.begin()->first);
                        }
                    } break;
                    case 2: {
                        if(cursor[cursorTop+1] == 0) break;
                        cursorTop2()
                        auto& set = subIter->second;
                        if(set.size() > 0)
                            cursor.push_back(*set.begin());
                    } break;
                    case 3:
                        if(cursor.size() > historyLimit*4)
                            cursor.erase(cursor.begin(), cursor.begin()+4);
                        cursor.push_back(cursor[cursorTop+3]);
                    break;
                }
            break;
            case Left:
                if(cursor.size() <= 1)
                    mode = Mode_Menu;
                else{
                    cursor.pop_back();
                    cursorTop0()
                    switch(cursorSub) {
                        case 2: {
                            cursorTop2()
                        } break;
                        case 3: {
                            cursorTop3()
                        } break;
                    }
                }
            break;
        }

        return 1;
    });
}

void ModeEvaluate() {
    printTitle("Evaluate");
    std::cout << std::endl << "> " << interfaceBuffer;
    std::cout.flush();

    pollKeyboard([&](bool special, uint64_t size, const char* buffer) {
        if(!special) {
            switch(*buffer) {
                case 27:
                    setCursorHidden(true);
                    mode = Mode_Menu;
                break;
                case 10:
                    setCursorHidden(true);
                    task.evaluateExtend(task.Task::symbolFor<false>(interfaceBuffer), true);
                    if(task.uncaughtException()) {
                        interfaceBuffer = "Could not evaluate input";
                        mode = Mode_Exception;
                    } else {
                        interfaceBuffer.clear();
                        mode = Mode_Menu;
                    }
                break;
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
    });
}
