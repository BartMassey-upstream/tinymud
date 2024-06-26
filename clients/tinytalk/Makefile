CC = cc
CFLAGS = -DTERMCAP
LIBRARIES = -ltermcap

OBJECTS = fred.o config.o util.o stuff.o keyboard.o wrap.o log.o macro.o \
	  command.o special.o gag.o

all: tinytalk

tinytalk: $(OBJECTS)
	$(CC) $(CFLAGS) -o tinytalk $(OBJECTS) $(LIBRARIES)

clean:
	rm -f $(OBJECTS)

$(OBJECTS): tl.h
