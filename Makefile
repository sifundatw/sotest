CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2

all: sotest test.so other.so

sotest: sotest.c
	$(CC) $(CFLAGS) -o $@ $< -ldl

test.so: libs/test.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

other.so: libs/other.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

check: all
	./sotest test.sc; echo "exit status: $$?"

clean:
	rm -f sotest test.so other.so
