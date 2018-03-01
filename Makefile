CC = clang
CFLAGS = -Weverything -Werror -Wno-padded -Wno-packed -Wno-missing-noreturn \
         -Wno-cast-align -Wno-unused-parameter -g

SOURCES = sdn.c event.c event.h client.c client.h graph.c graph.h mst.c mst.h \
          openflow.c openflow.h tree_set.c tree_set.h
OBJECTS = sdn.o event.o client.o graph.o mst.o openflow.o tree_set.o
TARGET = sdn

.PHONY: all clean format test

all: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	-rm -rf $(TARGET) $(OBJECTS) *.dSYM

format:
	clang-format -i $(SOURCES)

test_graph: test_graph.o graph.o
test_tree_set: test_tree_set.o tree_set.o

test: $(TARGET)
	test/run.sh

testall: $(TARGET)
	test/run.sh --runslow
