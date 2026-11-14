CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -fopenmp -Ithird_party
LDLIBS   ?= -lcurl

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := programa

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build $(BIN)

.PHONY: all clean
