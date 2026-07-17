CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude -pthread

SRC = src/PieceTable.cpp src/GapBuffer.cpp src/TextDocument.cpp \
      src/EditCommand.cpp src/UndoRedoStack.cpp src/DiffEngine.cpp \
      src/VersionHistory.cpp src/AutosaveWorker.cpp src/Terminal.cpp \
      src/SyntaxHighlighter.cpp src/RegexEngine.cpp \
      src/ScreenRenderer.cpp src/InputHandler.cpp src/TextEditor.cpp

OBJ = $(SRC:.cpp=.o)
MAIN_OBJ = src/main.o

.PHONY: all clean debug test_runner test

all: editor

editor: $(OBJ) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o editor

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Debug build with AddressSanitizer + UndefinedBehaviorSanitizer enabled —
# catches use-after-free / buffer overrun bugs (e.g. in PieceTable's
# manual offset arithmetic) that a normal build won't surface.
debug: CXXFLAGS += -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
debug: clean editor

test_runner: $(OBJ) tests/tests.cpp
	$(CXX) $(CXXFLAGS) $^ -o tests/run_tests

test: test_runner
	./tests/run_tests

clean:
	rm -f src/*.o editor tests/run_tests
