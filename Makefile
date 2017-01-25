CC=gcc
CFLAGS=-I. -O2 -Wall
LIBS=-lhidapi-hidraw
OBJ = rfidtest.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

rfidtest: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f *.o *~ rfidtest
