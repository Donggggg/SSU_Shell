#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ssu_shell.h"

char* getUser(int uid)
{
	char *name, tmp[LENGTH_SIZE], value[LENGTH_SIZE];
	FILE *fp;

	name = (char*)malloc(LENGTH_SIZE * sizeof(char));
	bzero(tmp, LENGTH_SIZE);
	bzero(value, LENGTH_SIZE);
	if((fp = fopen("/etc/passwd", "r")) == NULL){
		fprintf(stderr, "fopen error for /etc/passwd");
		exit(1);
	}

	while(1)
	{
		fscanf(fp, "%[^:]", value);
		fgetc(fp);
		fscanf(fp, "%[^:]", tmp);
		fgetc(fp);
		fscanf(fp, "%[^:]", tmp);

		if(atoi(tmp) == uid){
			if(strlen(value) > 7){
				value[7] = '+';
				value[8] = '\0';
			}
			name = value;
			fclose(fp);
			return name;
		}
		else{
			fscanf(fp, "%[^\n]", tmp);
			fgetc(fp);
		}
	}
}
