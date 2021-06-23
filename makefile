all:		hw1.c
		gcc -Wall -pedantic hw1.c -o cs345sh
clean:		
		rm cs345sh *.txt
