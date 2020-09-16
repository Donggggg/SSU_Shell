#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "ssu_shell.h"


void setOptions(char *argv);
void execute();
void fill_Table2(Table2 **tablelist);
int getSelfTerminalNumber(char *name);

int aOption, uOption, xOption;

int main(int argc, char *argv[])
{
	if(argc > 2){
		fprintf(stderr, "no more than 2 arguments\n");
		exit(1);
	}

	aOption = FALSE;
	uOption = FALSE;
	xOption = FALSE;

	if(argc == 2)
		setOptions(argv[1]);

	execute();

	return 0;
}

void setOptions(char *args)
{
	int i;

	for(i = 0; args[i] != '\0'; i++)
	{
		switch(args[i]) 
		{
			case 'a' :
				aOption = TRUE;
				break;
			case 'u' :
				uOption = TRUE;
				break;
			case 'x' :
				xOption = TRUE;
				break;
			default :
				fprintf(stderr, "only a, u, x options can be used.\n");
				exit(1);
				break;
		}
	}
}

void execute()
{
	Table2 **tablelist;

	fill_Table2(tablelist);

}

void fill_Table2(Table2 **tablelist)
{
	int i, j, num, pid, pNum = 0, selftty;
	char filename[LENGTH_SIZE], tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;
	struct stat statbuf;
	struct dirent **namelist;

	num = scandir("/proc", &namelist, NULL, alphasort);
	pid = getpid();

	for(i = 0; i < num; i++) {
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;
		pNum++;

		if(pid == atoi(namelist[i]->d_name))
			selftty = getSelfTerminalNumber(namelist[i]->d_name);
	}

	tablelist = (Table2**)malloc(pNum * sizeof(Table2*));
	for(i = 0; i < pNum; i++)
		tablelist[i] = (Table2*)malloc(sizeof(Table2));

	for(i = 0; i < num; i++) {
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;

		bzero(filename, LENGTH_SIZE);
		strcat(filename, "/proc/");
		strcat(filename, namelist[i]->d_name);
		strcat(filename, "/stat");

		fp = fopen(filename, "r");

		for(j = 0; j < 6; j++){
			fscanf(fp, "%[^ ]", tmp);
			fgetc(fp);
		}

		bzero(buf, BUFSIZE);
		fscanf(fp, "%[^ ]", buf);

		if(selftty == atoi(buf))
			printf("%s\n", namelist[i]->d_name);
		fclose(fp);
	}

	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist);
}

int getSelfTerminalNumber(char *name)
{
	int i;
	char filename[LENGTH_SIZE], tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;

	bzero(filename, LENGTH_SIZE);
	strcat(filename, "/proc/");
	strcat(filename, name);
	strcat(filename, "/stat");

	if((fp = fopen(filename, "r")) == NULL){
		fprintf(stderr, "fopen error for %s\n", filename);
		exit(1);
	}

	for(i = 0; i < 6; i++){
		fscanf(fp, "%[^ ]", tmp);
		fgetc(fp);
	}
	fscanf(fp, "%[^ ]", buf);

	return atoi(buf);
}
