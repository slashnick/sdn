CXX = clang++
CXXFLAGS = -std=c++11 -Weverything -Werror -Wno-packed -Wno-padded \
           -Wno-c++98-compat -Wno-c99-extensions -Wno-missing-noreturn \
		   -Wno-cast-align -Wno-old-style-cast -Wno-exit-time-destructors \
		   -Wno-global-constructors -Wno-unused-function -Wno-unused-parameter \
		   -Wno-unused-macros
LD = clang++

SOURCES = beacon.cpp beacon.h client.cpp client.h event.cpp event.h graph.cpp \
          graph.h openflow.cpp openflow.h sdn.cpp test/test_graph.cpp
OBJECTS = beacon.o client.o event.o graph.o openflow.o sdn.o
TARGET = sdn

.PHONY: all clean format test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $^ -o $@

clean:
	-rm -rf $(TARGET) *.dSYM *.o test/*.o test_graph

format:
	clang-format -i $(SOURCES)

test_graph: test/test_graph.o graph.o
	$(LD) $^ -o $@

test: $(TARGET)
	test/run.sh

testall: $(TARGET)
	test/run.sh --runslow
