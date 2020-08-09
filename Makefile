CXX = g++
CXX_FLAGS = -std=c++17 -Bstatic -lncurses 

SRC = bonsai.cpp

UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S), Linux)
		BUILD = build-linux
endif

ifeq ($(UNAME_S), Darwin)
		BUILD = build-macos
endif

bonsai : $(SRC)
		$(CXX) $(CXX_FLAGS) $(SRC) -o $(BUILD)/bonsai 

run : 
		./$(BUILD)/bonsai
