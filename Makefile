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

install :
		

format : 
		clang-format -i $(SRC) *.h --style=LLVM

run : 
		./bonsai
