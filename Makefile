CC=gcc
CFLAGS=-c -Wall -O2 -std=c99
LDFLAGS=
PREHEADERS=vm/opcodes.hp
PRESOURCES=vm/opcodes.cp
SOURCES=vm/vm.c vm/exectest.c vm/util.c vm/queue.c
EXECUTABLE=vm/vm
OBJECTS=$(SOURCES:.c=.o) $(PRESOURCES:.cp=.o) 

all: $(SOURCES) $(PRESOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.c: %.cp
	build/preproc.pl < $< > $@

%.h: %.hp
	build/preproc.pl < $< > $@

clean:
	rm -f $(EXECUTABLE) $(OBJECTS) $(PRESOURCES:.cp=.c) $(PREHEADERS:.hp=.h)

depend: .depend

.depend: $(SOURCES) $(PRESOURCES:.cp=.c) $(PREHEADERS:.hp=.h)
	rm -f ./.depend
	$(CC) -MM $^ >> ./.depend

include .depend

vm/opcodes.cp: vm/opcodes.hp vm/opcodes.dat
vm/opcodes.hp: vm/opcodes.dat
