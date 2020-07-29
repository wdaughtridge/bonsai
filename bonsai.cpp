#include <ncurses.h>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <string>
#include <fstream>

#define ls(arg) execute("cd " + dir + "; ls " + std::string(arg))
#define SHOW_WELCOME 1

namespace Bonsai {

    // current directory
    std::string dir;

    // output buffer for printing screen 
    std::vector<std::string> output;

    // width and height of the terminal window
    int width, height;

    // starting cursor position
    int xcur = 3, ycur = 0;

    // current top line that is displayed on screen to handle scrolling
    int top = 0;

    // exits the while loop that runs the application
    bool shouldExit = false;

    void init();
    void run();
    void printscr();
    void checkInput();
    void newFrame();
    void processCommand();
    void processSearch();
    void cd(const std::string& file);
    std::string getInput();
    std::vector<std::string> execute(const std::string& command);

    void cd(const std::string& file) {
        dir = execute("cd " + dir + "; cd " + file + "; pwd").at(0); 
        top = 0;
    }

    // executes a console command and returns the output in a vector
    // of each line of output with the last newline taken out as that
    // caused a bug when using #def functions
    std::vector<std::string> execute(const std::string& command) {
        std::array<char, 128> buffer;
        std::string output;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        std::vector<std::string> outputLines;

        if (!pipe) {
            outputLines.push_back("ERROR: popen() failed");
            return outputLines;
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()))
            output += buffer.data();

        size_t firstNL = output.find_first_of('\n'); 
        
        if (firstNL == std::string::npos)
            outputLines.push_back(output);
        else {
            while (firstNL != std::string::npos) {
                outputLines.push_back(output.substr(0, firstNL));
                output.replace(0, firstNL+1, "");
                firstNL = output.find_first_of('\n');
            }
        }

        return outputLines;
    }

    // gets the input of user after they type ':' and
    // returns the command in a string to be checked by checkInput.
    // handles when the user presses the backspace key or cancels
    // input with esc.
    void processCommand() {
        mvaddch(height - 1, 0, ':');

        // make buffer of chars to a string for command
        std::string command = getInput();
        if (!command.compare(""))
            return;

        // clear the line where command is output
        move(height - 1, 0);
        clrtoeol();
        move(ycur, xcur);
        
        if (command == "q") {
            shouldExit = true;
        }
        else if (command == "..") { // change directory back one
            cd("..");
        }
        else if (command.substr(0,2) == "cd") { // change directory to specified location
            cd(command.substr(2));
        }
        else if ((int)command.at(0) >= (int)'0' || (int)command.at(0) <= (int)'9') { // jump to a specific line
            int moveTo;

            try {
                std::stoi(command);
                moveTo = std::stoi(command);
            }
            catch (const std::out_of_range& e) { return; }
            catch (const std::invalid_argument e) { return; } 

            if (moveTo >= output.size()) {
                moveTo = ycur; // move nowhere
            }
            else if (moveTo > (height - 3)) { // scroll down to line
                top = moveTo - (height - 3);
                moveTo = height - 3;
            }
            else if (moveTo < top) { // scroll up
                top = moveTo;
                moveTo = 0;
            }
            else { // offset by top line being displayed
                moveTo = moveTo - top;
            }

            move(moveTo, xcur);

            getyx(stdscr, ycur, xcur);
        }
    }

    void processSearch() {
        mvaddch(height - 1, 0, '/');

        std::string searchFor = getInput(); 
        if (!searchFor.compare(""))
            return;    

        move(height - 1, 0);
        clrtoeol();

        int moveTo = ycur;
        int score = abs(searchFor.compare(output.at(ycur + top)));

        // check which file is closest to search word
        for (int i = 0; i < output.size(); i++) {
            int comp = abs(searchFor.compare(output.at(i)));
            if (comp < score) {
                moveTo = i;
                score = comp;
            }
        }

        if (moveTo < top) {
            top = moveTo;
            moveTo = 0;
        }
        else if (moveTo > (height - 3)) {
            top = moveTo - (height - 3);
            moveTo = height - 3;
        }
        else {
            moveTo = moveTo - top;
        }

        move(moveTo, xcur);
        getyx(stdscr, ycur, xcur);
    }

    std::vector<std::string> autoComplete(const std::string& input) {
        size_t inputSize = input.size();
        auto filesInDir = ls("-a");

        std::vector<std::string> matches;
        for (size_t i = 0; i < filesInDir.size(); i++) {
            int score = input.compare(filesInDir.at(i).substr(0, inputSize));
            if (!score)
                matches.push_back(filesInDir.at(i).substr(input.size()));
        }

        return matches;
    }

    // a bit of a monster
    // get the input from keyboard and handle backspace, autocomplete, and esc to cancel
    std::string getInput() {
        std::vector<char> cmd;
        std::vector<std::string> matches;

        char ch = getch();
        char lastChar;

        size_t lastMatch;
        size_t inputPrevSize;

        attron(A_BOLD);
        while (ch != '\n') {
            if ((int)ch == 27) { // ESC - cancel input
                move(height - 1, 0);
                clrtoeol();
                move(ycur, xcur);
                return ""; 
            }
            else if ((int)ch == 127) { // BACKSPACE - delete last char
                int x, y;
                getyx(stdscr, y, x);

                if (!cmd.empty()) {
                    mvdelch(y, x - 1);
                    cmd.pop_back();
                }
            }
            else if ((int)ch == 9) { // TAB - autocomplete
                if ((int)lastChar != 9) { // check to see if new word to autocomplete
                    lastMatch = 0;

                    // find where the last word after the last ' ' or '/' starts
                    size_t lastSpaceOrSlash = 0;
                    for (size_t i = 0; i < cmd.size(); i++) {
                        if (cmd.at(i) == ' ' || cmd.at(i) == '/')
                            lastSpaceOrSlash = i + 1;
                    }

                    inputPrevSize = cmd.size(); // keep track of input size before autocompletion
                    
                    // get all matches for input and print 
                    // first one to screen if exists and add to buffer
                    matches = autoComplete(std::string(std::begin(cmd) + lastSpaceOrSlash, std::end(cmd))); // auto complete
                    if (matches.size()) {
                        for (size_t i = 0; i < matches.at(0).size(); i++) {
                            addch(matches.at(0).at(i));
                            cmd.push_back(matches.at(0).at(i));
                        }
                    }
                }
                else { // already have matches so cycle through the matches
                    if (matches.size()) {
                        size_t nextMatch = (lastMatch + 1) % matches.size();
                        size_t lastMatchSize = matches.at(lastMatch).size();

                        // get rid of the last match
                        for (size_t i = 0; i < lastMatchSize; i++) {
                            int x, y;
                            getyx(stdscr, y, x);

                            cmd.pop_back();
                            mvdelch(y, x - 1);
                        }

                        lastMatch = nextMatch;
                        // put next match onto screen and in buffer 
                        for (size_t i = 0; i < matches.at(nextMatch).size(); i++) {
                            addch(matches.at(nextMatch).at(i));
                            cmd.push_back(matches.at(nextMatch).at(i));
                        }
                    }
                }
            }
            else { // simply add a char that is typed
                addch(ch);
                cmd.push_back(ch);
            }

            lastChar = ch;
            ch = getch();
        }
        attroff(A_BOLD);

        return std::string(std::begin(cmd), std::end(cmd));
    }

    // checks whether the user inputs a command or moves cursor. if the
    // user issues a command, the command is then executed.
    void checkInput() {
        std::string file;
        char input = getch();
        std::string command;

        if (ycur < output.size()) 
            file = output.at(ycur + top);
        else
            file = "";

        switch(input) {
            case ':': // command
                processCommand();
                break;

            case '/': // search
                processSearch();
                break;

            case '.': // shortcut for moving to parent dir
                if (getch() == '.')
                    cd("..");
                break;

            case 'd': // delete file but keep
                if (getch() == 'd')
                    cd("..");
                break;

            case 'k':
                if (ycur == 0 && top > 0)
                    top--;
                else if (ycur != 0)
                    move(ycur - 1, xcur);
                break;

            case 'j':
                if (ycur == height - 3 && ycur + top < output.size() - 1)
                    top++;
                else if (ycur != height - 3 && ycur != output.size() - 1)
                    move(ycur + 1, xcur);
                break;

            case '\n':
                if (file != "") {
                    std::string type = execute("file " + file).at(0);
                    if (type.find("directory") != std::string::npos)
                        cd(file);
                    else
                        system(std::string("vim " + file).c_str());
                }
                break;
        }

    }

    // read each line from output buffer and print to screen. if the line
    // is where the cursor is located, then underline the text. also prints
    // numbers before each line from 0 to n - 1 where n is size of output.
    // stops printing when the output reaches two spaces from the bottom of
    // the screen in order to print the current directory and any user commands
    void printscr() {
        int y = top;
        for (y = top; y < output.size(); y++) {
            if (y == height + top - 2)
                break;

            attron(A_BOLD);
            if (y < 10)
                printw(std::string("  " + std::to_string(y) + " ").c_str());
            else if (y < 100)
                printw(std::string(" " + std::to_string(y) + " ").c_str());
            else if (y < 1000)
                printw(std::string(std::to_string(y) + " ").c_str());
            attroff(A_BOLD);

            if (ycur == y - top) {
                attron(A_UNDERLINE | A_BOLD);
                printw(std::string(output.at(y) + "\n").c_str());
                attroff(A_UNDERLINE | A_BOLD);
            }
            else
                printw(std::string(output.at(y) + "\n").c_str());
        }
        
        attron(A_STANDOUT);
        mvprintw(height - 2, 0, std::string(" > " + dir).c_str());
        attroff(A_STANDOUT);
    }

    // clear the screen and print all that is in the output buffer to screen
    // move the cursor to the last line of output if previous y pos was greater
    // than the pos of the last line of output. 
    void newFrame() {
        clear();
        move(0,0);
        output = ls("-a");

        if (ycur >= output.size())
            ycur = output.size() - 1;

        int tempHeight, tempWidth;
        getmaxyx(stdscr, tempHeight, tempWidth);
        if (tempHeight != height || tempWidth != width) {
            top = 0;
            height = tempHeight;
            width = tempWidth;
        }
        
        printscr();

        move(ycur, xcur);
    }

    void init() {
        initscr();
        cbreak();
        noecho();
        clear();

        getmaxyx(stdscr, height, width);
        dir = execute("pwd").at(0);

#if SHOW_WELCOME == 1
        std::string line;
        std::ifstream file("bonsaiASCII.txt");
        if (file.is_open()) {
            while(getline(file, line))
                printw(std::string(line + "\n").c_str());

            file.close();
        }

        getch();
#endif
    }

    void run() {
        while (!Bonsai::shouldExit) {
            Bonsai::newFrame();

            Bonsai::checkInput();

            getyx(stdscr, Bonsai::ycur, Bonsai::xcur);
        }
    }

}

int main() {
    Bonsai::init();

    Bonsai::run();

    clear();
    endwin();
    system("clear");
}
