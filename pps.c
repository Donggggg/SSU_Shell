#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "ssu_shell.h"

void setOptions(char *argv);
void execute2();
void fill_Table2(Table2 **tablelist);
void setTotalAmount();
int getSelfTerminalNumber(char *name);
Table2* getRow(char *pid);
char * getTerminal(long unsigned int tty_nr);

int aOption, uOption, xOption;
long long total_cpu_amount;
long long total_mem;

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

	execute2();

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

void execute2()
{
	Table2 **tablelist;

	setTotalAmount();
	fill_Table2(tablelist);

}

void fill_Table2(Table2 **tablelist)
{
	int i, j, num, pid, pNum = 0, selftty, cur = 0;
	char filename[LENGTH_SIZE], tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;
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

		if(selftty == atoi(buf)){
			printf("%s\n", namelist[i]->d_name);
			tablelist[cur] = getRow(namelist[i]->d_name);
		}

		fclose(fp);
	}

	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist);
}

void setTotalAmount()
{
	int i;
	char tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;

	total_cpu_amount = 0;
	fp = fopen("/proc/stat", "r");

	fscanf(fp, "%[^ ]", tmp);
	fgetc(fp);
	fgetc(fp);

	for(i = 0; i < 8; i++){
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		total_cpu_amount += atoll(buf);
	}

	fclose(fp);
	
	fp = fopen("/proc/meminfo", "r");
	fscanf(fp, "%[^:]", tmp);
	fgetc(fp);
	fscanf(fp, "%[^kB]", buf);
	total_mem = atoll(buf);

	fclose(fp);
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

Table2* getRow(char *pid)
{
	int i = 0;
	char filename[LENGTH_SIZE], buf[BUFSIZE], tmp[BUFSIZE];
	FILE *fp;
	Table2* row = (Table2*)malloc(sizeof(Table2));
	struct stat statbuf;

	bzero(filename, LENGTH_SIZE);
	strcat(filename, "/proc/");
	strcat(filename, pid);

	stat(filename, &statbuf);

	strcpy(row->user, getUser(statbuf.st_uid)); // user
	row->pid = atoi(pid); // pid

	strcat(filename, "/stat");

	fp = fopen(filename, "r");

	for(i = 0; i < 22; i++){
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		
		if(i == 6)
			strcpy(row->tty, getTerminal(atoi(buf)));
		else if(i == 13)
			row->cpu = atoll(buf);
		else if(i == 14)
			row->cpu += atoll(buf);
	}

	printf("%s\n", row->tty);
	fclose(fp);

	strcat(filename, "us");
	fp = fopen(filename, "r");

	while(fscanf(fp, "%[^:]", tmp) != EOF){
		fgetc(fp);
		fscanf(fp, "%[^\n]", buf);
		fgetc(fp);
		//printf("%s\n", tmp);

		if(!strcmp(tmp, "VMSize"))
			row->VSZ = atoll(buf);
		else if(!strcmp(tmp, "VMRSS"))
			row->RSS = atoll(buf);
	}

	fclose(fp);

	return row;
}

char * getTerminal(long unsigned int tty_nr)
{
	int i, num;
	char* terminal = (char*)malloc(PATH_SIZE * sizeof(char));
	char filename[LENGTH_SIZE];
	struct stat statbuf;
	struct dirent **namelist;

	bzero(terminal, PATH_SIZE);
	if(tty_nr == 0)
		return "?";

	num = scandir("/dev", &namelist, NULL, alphasort);

	for(i = 0; i < num; i++){
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| strncmp(namelist[i]->d_name, "tty", 3))
			continue;

		bzero(filename, LENGTH_SIZE);
		strcat(filename, "/dev/");
		strcat(filename, namelist[i]->d_name);
		stat(filename, &statbuf);

		if(tty_nr == statbuf.st_rdev){
			strcpy(terminal, namelist[i]->d_name);
			return terminal;
		}
	}

	num = scandir("/dev/pts", &namelist, NULL, alphasort);

	for(i = 0; i < num; i++){
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		bzero(filename, LENGTH_SIZE);
		strcat(filename, "/dev/pts/");
		strcat(filename, namelist[i]->d_name);
		stat(filename, &statbuf);

//		printf("%s\n", filename);
//		printf(">%ld\n", tty_nr);
//		printf("%ld\n", statbuf.st_rdev);
		if(tty_nr == statbuf.st_rdev){
			strcat(terminal, "pts/");
			strcat(terminal, namelist[i]->d_name);
			printf("%s\n", terminal);
			return terminal;
		}
	}

	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist[i]);
	
	return terminal;

}
