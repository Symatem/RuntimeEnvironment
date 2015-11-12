#include "../VM/PreDefModules.hpp"
#include <sys/ioctl.h>
#include <termios.h>

const std::string CSI = "\33["; // "\e["
const ArchitectureType historyLimit = 8;
struct termios termiosOld, termiosNew;
struct winsize screenSize;
enum {
    Mode_Browse,
    Mode_Input
} mode = Mode_Browse;
std::stringstream stream;
std::string interfaceBuffer;
std::vector<ArchitectureType> history;
ArchitectureType historyTop, historySub, linesForExtend;
Context context;
struct Task task = {&context, PreDef_Void};
decltype(context.topIndex)::iterator topIter;
Extend* extend;

void pollKeyboard(std::function<uint64_t(bool, uint64_t, const char*)> callback) {
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    select(STDIN_FILENO+1, &readset, NULL, NULL, NULL);
    if(FD_ISSET(STDIN_FILENO, &readset)) {
        char buffer[16];
        uint64_t pos = 0, readBytes = read(STDIN_FILENO, buffer, sizeof(buffer));
        while(pos < readBytes) {
            bool special = (readBytes-pos >= 2 && CSI.compare(0, 2, buffer+pos, 2) == 0);
            if(special) pos += 2;
            pos += callback(special, readBytes-pos, buffer+pos);
        }
    }
}

void setCursorPosition(uint64_t x, uint64_t y) {
    std::cout << CSI << (y+1) << ';' << (x+1) << 'f';
}

void clearScreen() {
    // std::cout << CSI << "2J";
    for(uint64_t y = 0; y < screenSize.ws_row; ++y) {
        setCursorPosition(0, y);
        std::cout << CSI << "2K";
    }
    setCursorPosition(0, 0);
}

void setTextStyle(uint64_t attr, uint64_t foreground, uint64_t background) {
    std::cout << CSI << attr << ';' << (foreground+30) << ';' << (background+40) << 'm';
}

void setCursorHidden(bool hidden) {
    std::cout << CSI << ((hidden) ? "?25l" : "?25h");
    std::cout.flush();
}

void terminate() {
    clearScreen();
    setCursorHidden(false);
    tcsetattr(0, TCSANOW, &termiosOld);
    exit(0);
}

void init() {
    tcgetattr(STDIN_FILENO, &termiosOld);
    tcgetattr(STDIN_FILENO, &termiosNew);
    termiosNew.c_lflag &= ~ICANON;
    termiosNew.c_lflag &= ~ECHO;
    termiosNew.c_cc[VMIN] = 1;
    termiosNew.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &termiosNew);
    ioctl(STDIN_FILENO, TIOCGWINSZ, &screenSize);
    signal(SIGINT, (void(*)(int))terminate);
    setCursorHidden(true);
}

void replaceAllInString(std::string& str, const std::string& oldStr, const std::string& newStr) {
    auto pos = str.find(oldStr, 0);
    while(pos != std::string::npos) {
         str.replace(pos, oldStr.length(), newStr);
        pos = str.find(oldStr, pos+newStr.length());
    }
}

ArchitectureType printStreamLimited(ArchitectureType mode = 0,
                                    ArchitectureType leadingSpace = 0,
                                    ArchitectureType maxLines = 1,
                                    ArchitectureType lineOffset = 0) {
    std::string str = stream.str();
    stream.str(std::string());

    replaceAllInString(str, "\t", "    ");
    ArchitectureType prevPos = 0, pos = 0, col = 0, row = 0, maxWidth = screenSize.ws_col-leadingSpace;
    while(pos < str.size()) {
        bool newLine = (str[pos] == '\n'), end = (pos+1 == str.size());
        if(newLine || end || col >= maxWidth) {
            if(++row > lineOffset) {
                if(mode != 2) {
                    std::cout << std::endl;
                    for(ArchitectureType i = 0; i < leadingSpace; ++i)
                        std::cout << ' ';
                }
                if(mode == 1) std::cout << CSI << "7m";
                if(mode != 2) std::cout << str.substr(prevPos, pos-prevPos+end);
                if(mode == 1) std::cout << CSI << "m";
                if(maxLines > 0 && row == lineOffset+maxLines)
                    break;
            }
            prevPos = pos+1;
            col = -1;
        }
        while(++pos < str.size() && (str[pos] & 0xC0) == 0x80);
        ++col;
    }
    return row;
}
