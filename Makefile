CC = clang
CFLAGS = -Weverything -Werror -Wno-padded -Wno-packed -Wno-missing-noreturn \
         -Wno-cast-align -Wno-unused-parameter -g

SOURCES = sdn.c event.c event.h client.c client.h graph.c graph.h openflow.c \
	      openflow.h
OBJECTS = sdn.o event.o client.o graph.o openflow.o
TARGET = sdn

.PHONY: all clean format test

all: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	-rm -rf $(TARGET) $(OBJECTS) *.dSYM

format:
	clang-format -i $(SOURCES)

test_graph: test_graph.o graph.o

test: $(TARGET)
	test/run.sh

testall: $(TARGET)
	test/run.sh --runslow
