CC=gcc
CFLAGS=-c -Wall -O2 -std=c99
LDFLAGS=
PRESOURCES=vm/opcodes.cp
SOURCES=vm/vm.c vm/constest.c vm/util.c vm/queue.c
EXECUTABLE=vm/vm
OBJECTS=$(SOURCES:.c=.o) $(PRESOURCES:.cp=.o) 

all: $(SOURCES) $(PRESOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.c: %.cp
	build/preproc.pl < $< > $@

clean:
	rm -f $(EXECUTABLE) $(OBJECTS) $(PRESOURCES:.cp=.c)
