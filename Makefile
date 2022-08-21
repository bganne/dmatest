ATARI:=$(HOME)/src/atari
OBJDUMP:=$(CROSS_PREFIX)-objdump
LIBCMINI:=$(ATARI)/libcmini
CROSS:=$(ATARI)/cross/m68k-atari-mint
PATH:=$(PATH):$(CROSS)/bin
LD_LIBRARY_PATH:=$(CROSS)/lib64
CROSS_PREFIX:=m68k-atari-mint
CC:=$(CROSS_PREFIX)-gcc
export PATH LD_LIBRARY_PATH

CFLAGS:=-O2 -s -Wextra -Werror

BIN:=dmatest

all: release.zip

$(BIN).tos: $(BIN).c
	$(CC) -I$(LIBCMINI)/include -nostdlib $(LIBCMINI)/build/crt0.o $^ -o $@ $(CFLAGS) -L$(LIBCMINI)/build -lcmini -lgcc

run: $(BIN).tos
	hatari $<

$(BIN).st: AUTO/$(BIN).prg
	dd if=/dev/zero of=$@ bs=1k count=720
	mformat -a -f 720 -i $@ ::
	MTOOLS_NO_VFAT=1 mcopy -i $@ -spmv AUTO ::

AUTO/$(BIN).prg: $(BIN).tos AUTO
	cp -vf $< $@

AUTO:
	mkdir $@

release.zip: $(BIN).st $(BIN).tos
	zip -9 $@ $^

clean:
	$(RM) -r $(BIN).tos $(BIN).st AUTO release.zip
