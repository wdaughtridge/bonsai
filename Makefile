CXX = g++
CXX_FLAGS = -std=c++17 -Bstatic -lncurses -Wall

SRC = bonsai.cpp

UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S), Linux)
		BUILD = build-linux
else ifeq ($(UNAME_S), Darwin)
		BUILD = build-macos
endif

bonsai : $(SRC)
		$(CXX) $(CXX_FLAGS) $(SRC) -o $(BUILD)/bonsai -O3

debug : $(SRC)
		$(CXX) $(CXX_FLAGS) $(SRC) -o $(BUILD)/bonsai -g

format : 
		clang-format -i $(SRC) --style=LLVM

run : 
		tmux new -d -s bonsai
		tmux send-keys -t bonsai C-z './$(BUILD)/bonsai' Enter
		tmux attach-session -t bonsai
