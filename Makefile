# Compilar desde la raíz del repositorio:   make            (o: scripts/gcc14.sh make)
# Ejecutar:  ./programa --input data/ventas.csv --output resumen_ventas_usd.csv --threads 8

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -fopenmp -Ithird_party
LDLIBS   ?= -lcurl

ifneq ($(shell pkg-config --exists libcurl 2>/dev/null && echo ok), ok)
$(warning No se encontró libcurl con pkg-config. En Fedora: sudo dnf install libcurl-devel, o compile con scripts/gcc14.sh make)
endif

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
DEP := $(OBJ:.o=.d)
BIN := programa

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

# -MMD -MP: recompila un .cpp cuando cambia alguno de los .h que incluye
build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build $(BIN)

-include $(DEP)

.PHONY: all clean
