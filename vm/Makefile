CC=gcc
ifeq ($(MODE),dev)
	CFLAGS=-c -Wall -g -std=c99
else
	CFLAGS=-c -Wall -O2 -std=c99
endif
LDFLAGS=
vm_ASSEMBLY=exectest.aa constest.aa
vm_PREHEADERS=opcodes.hp
vm_PRESOURCES=opcodes.cp
vm_SOURCES=vm.c constest.c util.c queue.c cons.c
vm_EXECUTABLE=vm
vm_OBJECTS=$(vm_SOURCES:.c=.o) $(vm_PRESOURCES:.cp=.o) 

ASSEMBLY= $(vm_ASSEMBLY)
PREHEADERS = $(vm_PREHEADERS)
PRESOURCES = $(vm_PRESOURCES)
SOURCES = $(vm_SOURCES)
EXECUTABLES = $(vm_EXECUTABLE)
OBJECTS = $(vm_OBJECTS)

all: $(EXECUTABLES)

dev: $(EXECUTABLES)

$(vm_EXECUTABLE): $(vm_OBJECTS)
	$(CC) $(LDFLAGS) $(vm_OBJECTS) -o $@

%.c: %.cp ../build/preproc.pl
	../build/preproc.pl < $< > $@

%.h: %.hp ../build/preproc.pl
	../build/preproc.pl < $< > $@

%_aa.h: %.aa ../build/assemble.pl
	../build/assemble.pl $< > $@

clean:
	rm -f $(EXECUTABLES) $(OBJECTS) $(PRESOURCES:.cp=.c) $(PREHEADERS:.hp=.h)  $(patsubst %.aa,%_aa.h,$(ASSEMBLY))

depend: .depend

.depend: $(SOURCES) $(PRESOURCES:.cp=.c) $(PREHEADERS:.hp=.h) $(patsubst %.aa,%_aa.h,$(ASSEMBLY))
	rm -f ./.depend
	$(CC) -MM $^ >> ./.depend

include .depend

opcodes.c: opcodes.hp opcodes.dat vm.h
opcodes.h: opcodes.dat
