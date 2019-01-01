CFLAGS ?= --std=c99 -Wall -O0 -g
CROSS_AS ?= ./ulna-as
bin_PROGRAMS = hw4 ulna-as
ulna_DATA = multiplier.img
C_SOURCES = hw4.c ulna-as.c
OBJS = hw4.o ulna-as.o

all: $(bin_PROGRAMS) $(ulna_DATA)

hw4: hw4.o
	$(CC) -o $@ $^ $(LDFLAGS)

ulna-as: ulna-as.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.img: %.S $(CROSS_AS)
	$(CROSS_AS) $(ASFLAGS) $< $@

indent:
	for f in $(C_SOURCES); do indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs -cp1 -il0 $$f; done

clean:
	-rm -f $(bin_PROGRAMS) $(ulna_DATA) $(OBJS)

.PHONY: indent clean
