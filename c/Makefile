CC=gcc
CFLAGS=-I.
DEPS = 
OBJ = smbus.o INA260.o example.o
EXTRA_LIBS=-lm

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

example: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(EXTRA_LIBS)

.PHONY: clean

clean:
	rm -f example $(OBJ) 
