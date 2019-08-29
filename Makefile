CC=gcc
CFLAGS=-g

ODIR=bin
LDIR =../lib

LIBS=-lasound

DEPS = src/rawtowav.c
OBJ = $(ODIR)/raw2wav.o
$(ODIR)/%.o:$(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/raw2wav: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*
