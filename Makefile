CC = clang
CFLAGS = -Weverything -Werror -Wno-padded -Wno-packed -Wno-missing-noreturn \
         -Wno-cast-align -Wno-unused-parameter -g

SOURCES = sdn.c event.c event.h client.c client.h graph.c graph.h mst.c mst.h \
          openflow.c openflow.h tree_map.c tree_map.h \
		  test/test_graph.c test/test_tree_map.c
OBJECTS = sdn.o event.o client.o graph.o mst.o openflow.o tree_map.o
TARGET = sdn

.PHONY: all clean format test

all: $(TARGET)

$(TARGET): $(OBJECTS)

clean:
	-rm -rf $(TARGET) *.dSYM *.o test/*.o test_graph test_tree_map

format:
	clang-format -i $(SOURCES)

test_graph: test/test_graph.o graph.o
	$(CC) $(CFLAGS) $^ -o $@

test_tree_map: test/test_tree_map.o tree_map.o
	$(CC) $(CFLAGS) $^ -o $@

test: $(TARGET)
	test/run.sh

testall: $(TARGET)
	test/run.sh --runslow
