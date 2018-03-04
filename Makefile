CC = clang
CXX = clang++
CFLAGS = -Weverything -Werror -Wno-padded -Wno-packed -Wno-missing-noreturn \
         -Wno-cast-align -Wno-unused-parameter -g
CXXFLAGS = -Wall -Werror
LD = clang++

SOURCES = sdn.cpp event.c event.h client.c client.h graph.c graph.h map.cpp \
          map.h mst.c mst.h openflow.c openflow.h test/test_graph.c
OBJECTS = sdn.o event.o client.o graph.o map.o mst.o openflow.o
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
	$(CC) $(CFLAGS) $^ -o $@

test: $(TARGET)
	test/run.sh

testall: $(TARGET)
	test/run.sh --runslow
