all: rx

rx: rx.c
	gcc -Wall -Wextra -lrtlsdr -o rx rx.c
