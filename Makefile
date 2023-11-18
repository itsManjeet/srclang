CFLAGS						?= -ggdb -O2

SRCLANG_SOURCE_FILES		:= $(wildcard src/*.c)
SRCLANG_OBJECT_FILES		:= $(SRCLANG_SOURCE_FILES:.c=.o)

SRCLANG						?= srclang

all: $(SRCLANG)

$(SRCLANG): $(SRCLANG_OBJECT_FILES)
	$(CC) -static $(SRCLANG_OBJECT_FILES) -o $@

clean:
	rm -f $(SRCLANG) $(SRCLANG_OBJECT_FILES)