TARGETS = cisparse

CFLAGS = -Wall -g3 -O1

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

.PHONY: all clean
