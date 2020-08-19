#include "bonsai.h"

// change to specified path and store working directory
// also set scrolling to the top
void Bonsai::cd(const std::string &path) {
  dir = executeWithOutput("cd " + dir + "; cd " + path + "; pwd").at(0);
  top = 0;
  move(0, 3);
}

// calls ls -[args] command and returns the output
std::vector<std::string> Bonsai::ls(const std::string &arg,
                                    const std::string &lsDir = "") {
  std::vector<std::string> files;
  if (lsDir.empty())
    files = executeWithOutput("cd " + dir + "; ls " + arg);
  else
    files = executeWithOutput("cd " + lsDir + "; ls " + arg);

  files.erase(files.begin()); // get rid of '.' (current dir) file
  return files;
}

// executes a console command and returns the output in a vector
// of each line of output with the last newline taken out as that
// caused a bug when using #def functions
std::vector<std::string> Bonsai::executeWithOutput(const std::string &command) {
  std::array<char, 256> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"),
                                                pclose);

  std::string output;
  std::vector<std::string> outputLines;

  if (!pipe)
    return {"ERROR: POPEN FAILED"};

  while (fgets(buffer.data(), buffer.size(), pipe.get()))
    output += buffer.data();

  size_t firstNL = output.find_first_of('\n');

  if (firstNL == std::string::npos)
    return {output};
  else {
    while (firstNL != std::string::npos) { // separates output by newlines and
                                           // places into vector
      outputLines.push_back(output.substr(0, firstNL));
      output.replace(0, firstNL + 1, "");
      firstNL = output.find_first_of('\n');
    }

    return outputLines; // return the output of the command
  }
}

// quick way to open shell in tmux under pane for longer commands
void Bonsai::openShell() {
  std::string sessionName = executeWithOutput("tmux display-message -p '#S'")
                                .at(0); // get current tmux session name

  system(std::string("tmux split-window -c '" + dir + "' -p 25 -t " + sessionName).c_str());
}

// quick command to execute in shell
// output is displayed on bottom of screen
// and closed after key press
void Bonsai::userCommand() {
  mvaddch(height - 1, 0, '!');

  //  // get command after bang
  std::string command = getInput();
  std::string sessionName = executeWithOutput("tmux display-message -p '#S'")
                                .at(0); // get current tmux session name

  system(std::string("tmux split-window -c '" + dir + "' -l 10 -t " + sessionName + " '" + command + "; read'").c_str());

  // clear the line where command is output
  move(height - 1, 0);
  clrtoeol();
  move(ycur, xcur);
}

// gets the input of user after they type ':' and
// returns the command in a string to be checked by checkInput.
// handles when the user presses the backspace key or cancels
// input with esc.
void Bonsai::processCommand() {
  mvaddch(height - 1, 0, ':');

  std::string command = getInput(); // get user input
  if (command.empty())
    return;

  // clear the line where command is output
  move(height - 1, 0);
  clrtoeol();
  move(ycur, xcur);

  if (command == "q") {
    shouldExit = true;
    return;
  } else if (command.substr(0, 3) ==
             "cd ") { // change directory to specified location
    cd(command.substr(3));
  } else if (command ==
             "pb -A") { // WIP. this puts back all files moved to the trash
    for (const auto &path : trashedFiles) {
      system(std::string("mv ~/.Trash/" +
                         path.substr(path.find_last_of('/') + 1) + " " + path)
                 .c_str());
    }
  } else if (command.at(0) >= '0' ||
             command.at(0) <= '9') { // jump to a specific line
    jumpToLine(command);
  }
}

// try to convert input to an integer
// jump to the specified line if so
void Bonsai::jumpToLine(const std::string &input) {
  int moveTo;

  try {
    std::stoi(input);
    moveTo = std::stoi(input);
  } catch (const std::out_of_range &e) {
    return;
  } catch (const std::invalid_argument e) {
    return;
  }

  if (moveTo >= output.size()) {
    moveTo = ycur;                    // move nowhere
  } else if (moveTo > (height - 3)) { // scroll down to line
    top = moveTo - (height - 3);
    moveTo = height - 3;
  } else if (moveTo < top) { // scroll up
    top = moveTo;
    moveTo = 0;
  } else { // offset by top line being displayed
    moveTo = moveTo - top;
  }

  move(moveTo, xcur);

  getyx(stdscr, ycur, xcur);
}

// searches for the closest file on screen to jump to
void Bonsai::processSearch() {
  mvaddch(height - 1, 0, '/');

  std::string searchFor = getInput();
  if (searchFor.empty())
    return;

  move(height - 1, 0);
  clrtoeol();

  int moveTo = ycur;
  int score = abs(
      searchFor.compare(output.at(ycur + top))); // initial score to compare to

  // check which file is closest to search word
  for (int i = 0; i < output.size(); i++) {
    int comp = abs(searchFor.compare(output.at(i)));
    if (comp < score) { // if word is closer, store the index for later
      moveTo = i;
      score = comp;
    }
  }

  // perform move operations on screen
  if (moveTo < top) {
    top = moveTo;
    moveTo = 0;
  } else if (moveTo > (height - 3)) {
    top = moveTo - (height - 3);
    moveTo = height - 3;
  } else {
    moveTo = moveTo - top;
  }

  // resume normal cursor position on screen
  move(moveTo, xcur);
  getyx(stdscr, ycur, xcur);
}

// returns all possibilities for auto complete from a word
std::vector<std::string> Bonsai::autoComplete(const std::string &input) {
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
// get the input from keyboard and handle backspace, autocomplete, and esc to
// cancel
std::string Bonsai::getInput() {
  std::vector<char> cmd;
  std::vector<std::string> matches;

  wchar_t ch;    // input from user
  char lastChar; // last input from user

  size_t lastMatch;     // for cycling through autocomplete
  size_t inputPrevSize; // for when autocompleted word sizes differ

  attron(A_BOLD);
  while ((ch = getch()) != '\n') {
    int x, y;
    getyx(stdscr, y, x);

    switch (ch) {
    case 27: // ESC - cancel input
      move(height - 1, 0);
      clrtoeol();
      move(ycur, xcur);
      return "";

    case 127: // BACKSPACE
      if (!cmd.empty() && x > 1) {
        mvdelch(y, x - 1);
        cmd.erase(cmd.begin() + (x - 2));
      }
      break;

    case KEY_RIGHT: // ARROW KEYS
      if (x <= cmd.size())
        move(y, x + 1);
      break;

    case KEY_LEFT:
      if (x > 1)
        move(y, x - 1);
      break;

    case 9:                // TAB - autocomplete
      if (lastChar != 9) { // check to see if new word to autocomplete
        lastMatch = 0;

        // find where the last word after the last ' ' or '/' starts
        size_t lastSpaceOrSlash = 0;
        for (size_t i = 0; i < cmd.size(); i++) {
          if (cmd.at(i) == ' ' || cmd.at(i) == '/')
            lastSpaceOrSlash = i + 1;
        }

        inputPrevSize =
            cmd.size(); // keep track of input size before autocompletion

        // get all matches for input and print
        // first one to screen if exists and add to buffer
        matches = autoComplete(std::string(std::begin(cmd) + lastSpaceOrSlash,
                                           std::end(cmd))); // auto complete
        if (matches.size()) {
          for (size_t i = 0; i < matches.at(0).size(); i++) {
            addch(matches.at(0).at(i));
            cmd.push_back(matches.at(0).at(i));
          }
        }
      } else { // already have matches so cycle through the matches
        if (matches.size()) {
          size_t nextMatch = (lastMatch + 1) % matches.size();
          size_t lastMatchSize = matches.at(lastMatch).size();

          int x, y;
          // get rid of the last match
          for (size_t i = 0; i < lastMatchSize; i++) {
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
      break;

    default:
      if (ch == '"') { // so the double quotes dont break strings
        ch = '\"';
      }

      if ((x - 1) == cmd.size())
        addch(ch);
      else {
        insch(ch);
        move(y, x + 1);
      }

      if (!cmd.empty())
        cmd.insert(cmd.begin() + (x - 1), ch);
      else
        cmd.push_back(ch);
    }

    lastChar = ch;
  }
  attroff(A_BOLD);

  return std::string(std::begin(cmd), std::end(cmd));
}

// checks whether the user inputs a command or moves cursor. if the
// user issues a command, the command is then executed.
void Bonsai::checkInput() {
  std::string file;
  char input = getch();
  std::string command;

  if (ycur < output.size())
    file = output.at(ycur + top);
  else
    file = "";

  switch (input) {
  case ':': // command
    processCommand();
    break;

  case '/': // search
    processSearch();
    break;

  case '.': // shortcut for moving to parent dir
    if (lastInput == '.') {
      cd("..");
      input = '\0';
    }
    break;

  case 'd': // move file to trash bin
    if (lastInput == 'd') {
      system(std::string("mv " + dir + "/" + file + " " + trashDir).c_str());
      trashedFiles.push_back(dir + "/" + file);
      input = '\0';
    }
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
      std::string type = executeWithOutput("file " + dir + "/" + file).at(0);

      std::string extension;
      size_t dotExt = file.find_first_of('.');
      if (dotExt != std::string::npos)
        extension = file.substr(dotExt);

      if (type.find("directory") != std::string::npos) {
        cd(file);
      } else if (extension == ".jpg" || extension == ".png" ||
                 extension == ".jpeg") {
        system(std::string("open " + dir + "/" + file + " -a Preview").c_str());
      } else {
        std::string sessionName =
            executeWithOutput("tmux display-message -p '#S'")
                .at(0); // get current tmux session name
        size_t numPanes =
            executeWithOutput("tmux list-panes -t " + sessionName).size();
        if (numPanes > 1)
          system(std::string("cd " + dir + "; tmux send-keys -t " +
                             sessionName + ".1 ':tabnew " + dir + "/" + file +
                             "' Enter")
                     .c_str());
        else
          system(std::string("tmux split-window -p 75 -h -t " + sessionName +
                             " \"cd " + dir + "; nvim " + dir + "/" + file +
                             "\"")
                     .c_str()); // split window in tmux session and open file in
                                // neovim
      }
    }
    break;

  case '!':
    userCommand();
    break;

  case '%':
    openShell();
    break;

  default:
    if (((int)input >= (int)'0' || (int)input <= (int)'9') &&
        ((int)lastInput >= (int)'0' || (int)lastInput <= (int)'9'))
      int donothing = 0; // repeatCmd += std::string(input);
  }

  lastInput = input;
}

// read each line from output buffer and print to screen. if the line
// is where the cursor is located, then underline the text. also prints
// numbers before each line from 0 to n - 1 where n is size of output.
// stops printing when the output reaches two spaces from the bottom of
// the screen in order to print the current directory and any user commands
void Bonsai::printscr() {
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
    } else
      printw(std::string(output.at(y) + "\n").c_str());
  }

  attron(A_STANDOUT | A_BOLD);

  // make sure path does wrap around weirdly
  int width = std::stoi(
      executeWithOutput("tmux list-panes -t bonsai -F \"#{pane_width}\"")
          .at(0));
  if (dir.size() + 3 <= width)
    mvprintw(height - 2, 0, std::string(" > " + dir).c_str());
  else {
    size_t firstSlash = dir.find_first_of('/', 1);
    size_t lastSlash = dir.find_last_of('/');
    std::string shortenedDir =
        dir.substr(0, firstSlash) + "/.." + dir.substr(lastSlash);

    if (shortenedDir.size() + 3 > width)
      shortenedDir = "/.." + dir.substr(lastSlash);

    mvprintw(height - 2, 0, std::string(" > " + shortenedDir).c_str());
  }

  attroff(A_STANDOUT | A_BOLD);
}

// clear the screen and print all that is in the output buffer to screen
// move the cursor to the last line of output if previous y pos was greater
// than the pos of the last line of output.
void Bonsai::newFrame() {
  clear();
  move(0, 0);
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

std::string Bonsai::readFile(const std::string &path) {
  std::string line;
  std::string output;

  std::ifstream file(path.c_str());
  if (file.is_open()) {
    while (getline(file, line))
      output += line + "\n";

    file.close();
  }

  return output;
}

void Bonsai::init() {
  initscr();
  cbreak();
  noecho();
  clear();
  keypad(stdscr, true);

  getmaxyx(stdscr, height, width);
  dir = executeWithOutput("pwd").at(0);
  trashDir = "~/.Trash";

  std::string welcomeASCII = readFile("bonsaiASCII.txt");
  printw(welcomeASCII.c_str());

  getch();
}

void Bonsai::run() {
  while (!shouldExit) {
    newFrame();
    checkInput();

    getyx(stdscr, Bonsai::ycur, Bonsai::xcur);
  }
}

int main() {
  Bonsai::init();
  Bonsai::run();

  clear();
  endwin();
  system("tmux kill-session -t bonsai");
}
