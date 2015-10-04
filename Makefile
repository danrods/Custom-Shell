all: 320sh

320sh: 320sh.c
	gcc -Wall -Werror -o 320sh 320sh.c

clean:
	rm -f *~ *.o 320sh
