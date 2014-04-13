all: rx rxo

rx: rx.c
	gcc -Wall -Wextra -lrtlsdr -o rx rx.c

rxo: rxo.c
	gcc -Wall -Wextra -lrtlsdr -lm -o rxo rxo.c
