ssu_shell : ssu_shell.o util.o ttop.o pps.o
		gcc ssu_shell.o -o ssu_shell
		gcc ttop.o util.o -o ttop -lcurses
		gcc pps.o util.o -o pps

ssu_shell.o : ssu_shell.c ssu_shell.h
		gcc -c -W -Wall -Wextra ssu_shell.c

util.o : util.c ssu_shell.h
		gcc -c -W -Wall -Wextra util.c

ttop.o : ttop.c ssu_shell.h
		gcc -c -W -Wall -Wextra ttop.c -lcurses

pps.o : pps.c ssu_shell.h
		gcc -c -W -Wall -Wextra pps.c

clean :
		rm *.o
		rm ssu_shell
		rm ttop
		rm pps
