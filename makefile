all:		unix_terminal.c
		gcc -Wall -pedantic unix_terminal.c -o unix_terminal
clean:		
		rm unix_terminal
