LIBUSB_PREFIX=$(HOME)/local

CXXFLAGS = -g -DHAVE_LIBURJTAG -I$(LIBUSB_PREFIX)/include --std=c++11 -Wall
LDFLAGS=-L$(LIBUSB_PREFIX)/lib -lurjtag -lusb-1.0 -lreadline

OBJS=lemon.o menu.o links.o devices.o soc.o dsu.o outputs.o parse.o \
 loader.o sparc.o osdep.o breakpoint.o spim.o

SPARC_CC=sparc-elf-gcc
SPARC_OBJCOPY=sparc-elf-objcopy

all: lemon

lemon: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

spim_prg.h: spim_prg.bin
	./bin2c.py $< > $@

spim_prg.bin: spim_prg.elf
	$(SPARC_OBJCOPY) -O binary $< $@

spim_prg.elf: spim_prg.o
	$(SPARC_CC) -o $@ $< -nostdlib -Wl,-Ttext=0x40000000

spim_prg.o: spim_prg.c
	$(SPARC_CC) -c -o $@ $< -O -Wall -fno-toplevel-reorder

clean:
	$(RM) -f *.o lemon *~ spim_prg.elf spim_prg.bin

# FIXME: update this (automatically)
lemon.o: lemon.h soc.h devices.h menu.h links.h dsu.h outputs.h parse.h \
 loader.h sparc.h osdep.h breakpoint.h spim.h
soc.o: soc.h lemon.h links.h
dsu.o: dsu.h devices.h outputs.h
outputs.o: outputs.h
breakpoint.o: breakpoint.h dsu.h
parse.o: parse.h
menu.o: menu.h
links.o: links.h
spim.o: soc.h spim.h spim_prg.h

