CC = clang
CFLAGS = -Werror -Werror -Wextra -g 
SpecialFlags = -DDEBUGFLAG
BIN = 320sh
SRC = $(wildcard *.c)
OFILE = $(wildcard *.o)
# ShellFlags = -d


all: boilerPlate launch

boilerPlate: clean 320sh link


320sh: $(SRC) 
	$(CC) $(CFLAGS) $(SpecialFlags) -c $^
	#gcc -Wall -Werror -o 320sh 320sh.c

clean:
	rm -f *~ *.o 320sh

debug: boilerPlate gdb


link:
	$(CC) -o $(BIN) $(OFILE)


run: 
	./$(BIN) $(ShellFlags)

gdb:
	gdb -tui $(BIN)

launch:
	.	./launcher.sh $(ShellFlags)
