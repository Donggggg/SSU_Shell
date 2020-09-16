ssu_shell : ssu_shell.o ttop.o pps.o
		gcc ssu_shell.o -o ssu_shell
		gcc ttop.o -o ttop -lcurses
		gcc pps.o -o pps

ssu_shell.o : ssu_shell.c ssu_shell.h
		gcc -c -W -Wall -Wextra ssu_shell.c

ttop.o : ttop.c ssu_shell.h
		gcc -c -W -Wall -Wextra ttop.c -lcurses

pps.o : pps.c ssu_shell.h
		gcc -c -W -Wall -Wextra pps.c

clean :
		rm *.o
		rm ssu_shell
		rm ttop
		rm pps
