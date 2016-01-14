BINARIES = outerr sleepycat cattysleep pipetest

CFLAGS = -Wall


all: $(BINARIES)


outerr: outerr.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

sleepycat: sleepycat.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

cattysleep: cattysleep.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

pipetest: pipetest.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(BINARIES) *~


.PHONY: all clean

