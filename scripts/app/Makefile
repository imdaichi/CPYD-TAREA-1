CXX = g++
CXXFLAGS = -g3 -O2 -Wall -Wpedantic -Wextra -fopenmp -std=c++17 -I../../third_party
LDFLAGS = -fopenmp

PAQUETES = libcurl

ifneq ($(shell pkg-config --exists $(PAQUETES) && echo ok), ok)
$(error Faltan paquetes de desarrollo. Instale: $(PAQUETES))
endif

CXXFLAGS += $(shell pkg-config --cflags $(PAQUETES))
LDLIBS += $(shell pkg-config --libs $(PAQUETES))

FUENTES = main.cpp cliente_http.cpp configuracion_entorno.cpp utem.cpp tasa_cambio.cpp venta.cpp procesador_ventas.cpp

OBJETOS = $(addprefix build/, $(FUENTES:.cpp=.o))
DEPENDENCIAS = $(OBJETOS:.o=.d)

.PHONY: all clean run

all: dist/programa

dist/programa: $(OBJETOS) | dist
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJETOS) $(LDLIBS)

build/%.o: %.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	@mkdir -p $@

dist:
	@mkdir -p $@

clean:
	@echo "Limpiando artefactos de compilacion..."
	rm -rf build dist *.csv

run: all
	./dist/programa

-include $(DEPENDENCIAS)

.DEFAULT_GOAL := all

