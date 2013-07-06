
all: test profile

CC = clang
CFLAGS = -g -O3

OBJS = sha256-ref.o sha256-sse.o

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

test: $(OBJS) test.o 
	$(CC) -o $@ $(OBJS) test.o

profile: $(OBJS) profile.o
	$(CC) -o $@ $(OBJS) profile.o

clean:
	-rm -f $(OBJS) test