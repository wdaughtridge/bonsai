#include <ncurses.h>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <string>
#include <fstream>

namespace Bonsai {
    // current directory
    std::string dir;

    // trash directory
    std::string trashDir;

    // list of files that were moved to trash
    std::vector<std::string> trashedFiles;

    // output buffer for printing screen 
    std::vector<std::string> output;

    // width and height of the terminal window
    size_t width, height;

    // starting cursor position
    size_t xcur = 3, ycur = 0;

    // current top line that is displayed on screen to handle scrolling
    size_t top = 0;

    // exits the while loop that runs the application
    bool shouldExit = false;

    // last input to check for doubly repeated characters
    char lastInput = '\0';

    // todo: repeat commands 
    std::string repeatCmd;

    void init();
    void run();
    void printscr();
    void checkInput();
    void newFrame();
    void processCommand();
    void processSearch();
    void cd(const std::string& path);
    void userCommand();
    void jumpToLine(const std::string& input);
    std::string getInput();
    std::string readFile(const std::string& path);
    std::vector<std::string> executeWithOutput(const std::string& command);
    std::vector<std::string> ls(const std::string& arg, const std::string& lsDir);
    std::vector<std::string> autoComplete(const std::string& input);
}
