#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ncurses.h>
#include "ssu_shell.h"

#define KB_TO_MiB 1024
#define LENGTH_SIZE 64
void get_MemoryStatus(Status *status);

Table* fill_Table(Status *status);
long long findOldAmount(int pid);
void print_Table(Table *table_list, Status *status);

long long old_cpulist[8];
long long **old_cpu_amount;
int old_num;
long long total_memory;
long long total_cpu_amount;

int main()
{
	Status *status = (Status*)malloc(sizeof(Status));
	Table *table_list;

	initscr();
	int count = 0;

	init_Status(status);

	cbreak();

	while(1)
	{
		//sleep(3);
		fill_Status(status);
		print_Status(status);
		printf("%d\n", status->process_state[0]);

		table_list = fill_Table(status);
		print_Table(table_list, status);
		printw("reset count : %d\n", count++);	

		//free(table_list);
		refresh();
		sleep(3);
		//getch();
		clear();
	}
	endwin();
}

void init_Status(Status *status) {
	int i;

	for(i = 0; i < 8; i++)
		old_cpulist[i] = 0;
	for(i = 0; i < 4; i++){
		status->physical_memory[i] = 0;
		status->virtual_memory[i] = 0;
	}
}

void fill_Status(Status *status) 
{
	get_UptimeStatus(status);

	get_ProcessStatus(status);

	get_CPUStatus(status);

	get_MemoryStatus(status);

}

void get_UptimeStatus(Status *status) 
{
	int pid, wstatus;
	int uptime_pipe[2];
	char buffer[BUFSIZE], temp_buffer[BUFSIZE - 10];

	bzero(buffer, BUFSIZE);
	bzero(status->uptime_status, BUFSIZE);
	pipe(uptime_pipe);

	if((pid = fork()) < 0){
		fprintf(stderr, "fork error\n");
		exit(1);
	}
	else if(pid == 0){
		dup2(uptime_pipe[1], 1);
		execl("/usr/bin/uptime", "uptime", (char*)0);
	}
	else if(pid > 0) {
		dup2(0, 100);
		dup2(uptime_pipe[0],0);
		wait(&wstatus);
	}

	read(0, temp_buffer, 1024);
	dup2(100, 0);
	temp_buffer[strlen(temp_buffer)-1] = '\0';
	strcat(buffer, "top -");
	strcat(buffer, temp_buffer);
	strcpy(status->uptime_status, buffer);
	status->uptime_status[strlen(status->uptime_status)-1] = '\0';

	close(uptime_pipe[0]);
	close(uptime_pipe[1]);

}

void get_ProcessStatus(Status *status)
{
	int i, num, pNum = 0;
	char filename[BUFSIZE], buf[BUFSIZE];
	FILE *fp;
	struct dirent **namelist;

	for(i = 0; i < 5; i++)
		status->process_state[i] = 0;

	num = scandir("/proc", &namelist, NULL, alphasort);
	//	strcat(filename, "/proc/");

	for(i = 0; i < num; i++){ // '/proc' 디렉토리 순회 탐색
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;

		pNum++;
		bzero(filename, strlen(filename));
		strcat(filename, "/proc/");
		strcat(filename, namelist[i]->d_name);
		strcat(filename, "/stat");

		if((fp = fopen(filename, "r")) == NULL){
			fprintf(stderr, "open error for %s\n", filename);
			exit(1);
		}

		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		bzero(buf, strlen(buf));
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		bzero(buf, strlen(buf));
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);

		status->process_state[0]++;

		if(!strcmp(buf, "R"))
			status->process_state[1]++;
		else if(!strcmp(buf, "S") || !strcmp(buf, "I"))
			status->process_state[2]++;
		else if(!strcmp(buf, "T") || !strcmp(buf, "t"))
			status->process_state[3]++;
		else if(!strcmp(buf, "Z"))
			status->process_state[4]++;

		fclose(fp);

	}

	if(old_cpu_amount == NULL) {
		old_cpu_amount = (long long**)malloc(pNum *sizeof(long long*));
		old_num = pNum;
		for(i = 0; i < pNum; i++){
			old_cpu_amount[i] = (long long*)malloc(2 * sizeof(long long));
			old_cpu_amount[i][0] = 0;
			old_cpu_amount[i][1] = 0;
		}
	}

	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist);
}

void get_CPUStatus(Status *status)
{
	int i;
	FILE *fp;
	long long cpu_list[8], total = 0;
	char tmp[BUFSIZE];

	if((fp = fopen("/proc/stat", "r")) == NULL){
		fprintf(stderr, "fopen error for /proc/stat\n");
		exit(1);
	}

	while(fgetc(fp) != ' ');
	fgetc(fp);

	for(i = 0; i < 8; i++){
		fscanf(fp, "%[^ ]", tmp);
		fgetc(fp);
		cpu_list[i] = atoll(tmp);
		total += cpu_list[i];
	}

	status->cpu_share[0] = cpu_list[0] - old_cpulist[0];
	status->cpu_share[1] = cpu_list[2] - old_cpulist[2];
	status->cpu_share[2] = cpu_list[1] - old_cpulist[1];
	for(i = 3; i < 8; i++)
		status->cpu_share[i] = cpu_list[i] - old_cpulist[i];

	for(i = 0; i < 8; i++)
		old_cpulist[i] = cpu_list[i];
}

void get_MemoryStatus(Status *status)
{
	int i;
	long long size, buff_cache = 0;
	char buffer[BUFSIZE], tmp[LENGTH_SIZE];
	FILE *fp;

	if((fp = fopen("/proc/meminfo", "r")) == NULL){
		fprintf(stderr, "fopen error for /proc/meminfo\n");
		exit(1);
	}

	bzero(buffer, BUFSIZE);

	for(i = 0 ; i < 22; i++){
		fscanf(fp, "%[^:]", tmp);
		fgetc(fp);
		fscanf(fp, "%[^kB]", buffer);
		fgetc(fp);
		fscanf(fp, "%[^\n]", tmp);
		fgetc(fp);
		size = atoll(buffer);

		switch(i) {
			case 0 : // MemTotal
				status->physical_memory[0] = (double)size / KB_TO_MiB; 
				total_memory = size;
				break;
			case 1 : // MemFree
				status->physical_memory[1] = (double)size / KB_TO_MiB; 
				break;
			case 2 : // MemAvailable
				status->virtual_memory[3] = (double)size / KB_TO_MiB; 
				break;
			case 3 : // Buffers
				buff_cache += size;
				break;
			case 4 : // Cached
				buff_cache += size;
				break;
			case 14 : // SwapTotal
				status->virtual_memory[0] = (double)size / KB_TO_MiB; 
				break;
			case 15 : // SwapFree
				status->virtual_memory[1] = (double)size / KB_TO_MiB; 
				status->virtual_memory[2] = status->virtual_memory[0] - status->virtual_memory[1];
				break;
			case 21 : // Reclaim
				buff_cache += size;
				status->physical_memory[3] = (double)buff_cache / KB_TO_MiB; 
				status->physical_memory[2] = status->physical_memory[0] 
					- status->physical_memory[1] - status->physical_memory[3];
				break;
		}
	}

	fclose(fp);
}

void print_Status(Status *status)
{
	int i;
	long long total_cpu  = 0;
	char buffer[BUFSIZE], tmp[BUFSIZE-10];
	bzero(buffer, strlen(buffer));

	printw("%s\n", status->uptime_status);

	strcat(buffer, "Tasks: ");
	sprintf(tmp, "%d total,  %d running,  %d sleeping,  %d stopped,  %d zombie", 
			status->process_state[0], status->process_state[1], status->process_state[2], 
			status->process_state[3], status->process_state[4]);
	strcat(buffer, tmp);

	printw("%s\n", buffer);
	bzero(buffer, strlen(buffer));
	bzero(tmp, strlen(tmp));

	for(i = 0; i < 8; i++)
		total_cpu += status->cpu_share[i];
	strcat(buffer, "\%Cpu(s): ");
	sprintf(tmp, "%.1f us,  %.1f sy,  %.1f ni,  %.1f id,  %.1f wa,  %.1f hi, %.1f si,  %.1f st"
			, (double)status->cpu_share[0] / total_cpu * 100
			, (double)status->cpu_share[1] / total_cpu * 100
			, (double)status->cpu_share[2] / total_cpu * 100
			, (double)status->cpu_share[3] / total_cpu * 100
			, (double)status->cpu_share[4] / total_cpu * 100
			, (double)status->cpu_share[5] / total_cpu * 100
			, (double)status->cpu_share[6] / total_cpu * 100
			, (double)status->cpu_share[7] / total_cpu * 100);
	strcat(buffer, tmp);

	printw("%s\n", buffer);
	bzero(buffer, strlen(buffer));
	bzero(tmp, strlen(tmp));

	strcat(buffer, "MiB Mem :");
	sprintf(tmp, "    %5.1f total,    %5.1f free,    %5.1f used,    %5.1f buff/cache"
			, status->physical_memory[0], status->physical_memory[1]
			, status->physical_memory[2], status->physical_memory[3]);
	strcat(buffer, tmp);

	printw("%s\n", buffer);
	bzero(buffer, strlen(buffer));
	bzero(tmp, strlen(tmp));

	strcat(buffer, "MiB Swap :");
	sprintf(tmp, "    %5.1f total,    %5.1f free,    %5.1f used,    %5.1f avail Mem"
			, status->virtual_memory[0], status->virtual_memory[1]
			, status->virtual_memory[2], status->virtual_memory[3]);
	strcat(buffer, tmp);

	printw("%s\n\n", buffer);
}

Table* fill_Table(Status *status) // 이거 할당 타이밍 생각해보자.
{
	int i, j, num, pNum = 0, cur=0;
	char filename[LENGTH_SIZE], buf[BUFSIZE], tmp[BUFSIZE];
	struct dirent **namelist;
	Table *tablelist;
	FILE *fp, *sfp;

	bzero(filename , LENGTH_SIZE);
	num = scandir("/proc", &namelist, NULL, alphasort);
	pNum = status->process_state[0];
	total_cpu_amount = 0;

	tablelist = (Table*)malloc(pNum * sizeof(Table));

	for(i = 0; i < num; i++){
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;

		bzero(filename, LENGTH_SIZE);
		strcpy(filename, "/proc/");
		strcat(filename, namelist[i]->d_name);
		strcat(filename, "/stat");

		if((fp = fopen(filename, "r")) == NULL){
			fprintf(stderr, "fopen error for %s\n", filename);
			continue;
		}
		strcat(filename, "us");
		if((sfp = fopen(filename, "r")) == NULL){
			fprintf(stderr, "fopen error for %s\n", filename);
			continue;
		}


		for(j = 0 ; j < 20; j++){
			fscanf(fp, "%[^ ]", buf);
			fgetc(fp);

			switch(j)
			{
				case 0 : // pid
					tablelist[cur].pid = atoi(buf);
					strcpy(tablelist[cur].user, "donggyu");
					break;
				case 1 : // command
					strcpy(tablelist[cur].command, buf);
					break;
				case 2 : // state
					tablelist[cur].state = buf[0];
					break;
				case 14 : // utime
					tablelist[cur].cpu_amount = atoll(buf);
					break;
				case 15 : // stime
					tablelist[cur].cpu_amount += atoll(buf);
					tablelist[cur].cpu_share = 
						tablelist[cur].cpu_amount - findOldAmount(tablelist[cur].pid);
					total_cpu_amount += tablelist[cur].cpu_share;
					break;
				case 17 : // priority
					tablelist[cur].priority = atoi(buf);
					break;
				case 18 : // nice
					tablelist[cur].nice = atoi(buf);
					break;
			}
		}

		tablelist[cur].VIRT = 0;
		tablelist[cur].RES = 0;
		tablelist[cur].SHR = 0;

		while(1) {
			bzero(tmp, BUFSIZE);
			bzero(buf, BUFSIZE);

			if(fscanf(sfp, "%[^:]", tmp) == EOF)
				break;

			fgetc(sfp);

			if(!strcmp(tmp, "VmSize")){
				fscanf(sfp, "%[^kB]", buf);
				fgetc(sfp);
				fscanf(sfp, "%[^\n]", tmp);
				fgetc(sfp);
				tablelist[cur].VIRT = atoll(buf);
			}
			else if(!strcmp(tmp, "VmRSS")){
				fscanf(sfp, "%[^kB]", buf);
				fgetc(sfp);
				fscanf(sfp, "%[^\n]", tmp);
				fgetc(sfp);
				tablelist[cur].RES = atoll(buf);
			}
			else if(!strcmp(tmp, "RssFile")){
				fscanf(sfp, "%[^kB]", buf);
				fgetc(sfp);
				fscanf(sfp, "%[^\n]", tmp);
				fgetc(sfp);
				tablelist[cur].SHR = atoll(buf);
			}
			else{
				fscanf(sfp, "%[^\n]", tmp);
				fgetc(sfp);
			}
		}

		cur++;
		fclose(fp);
		fclose(sfp);

	}

	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist);

	free(old_cpu_amount);
	old_cpu_amount = (long long**)malloc(pNum * sizeof(long long*));
	old_num = pNum;

	for(i = 0; i < pNum; i++){
		tablelist[i].cpu_percent = (double)tablelist[i].cpu_share / total_cpu_amount * 100;
		int ceil = tablelist[i].cpu_percent * 100;
		tablelist[i].cpu_percent = (double)ceil / 100;
		if(tablelist[i].cpu_percent !=0)
			printf("%lld\n", tablelist[i].cpu_share);
		old_cpu_amount[i] = (long long*)malloc(2 * sizeof(long long));
		old_cpu_amount[i][0] = tablelist[i].pid;
		old_cpu_amount[i][1] = tablelist[i].cpu_amount;
	}

	return tablelist;

}

long long findOldAmount(int pid)
{
	int i;

	for(i = 0; i < old_num; i++)
		if(pid == (int)old_cpu_amount[i][0])
			return old_cpu_amount[i][1];

	return 0;
}

void sortByPid(Table *tablelist, int num)
{
	int i, j;
	Table tmp;

	for(i = 0; i < num; i++){
		for(j = i; j < num; j++){
			if(tablelist[i].pid > tablelist[j].pid){
				tmp = tablelist[i];
				tablelist[i] = tablelist[j];
				tablelist[j] = tmp;
			}
		}
	}
}

void sortByCPUShare(Table *tablelist, int num)
{
	int i, j;
	Table tmp;

	for(i = 0; i < num; i++){
		for(j = i; j < num; j++){
			if(tablelist[i].cpu_share < tablelist[j].cpu_share){
				tmp = tablelist[i];
				tablelist[i] = tablelist[j];
				tablelist[j] = tmp;
			}
		}
	}
}

void print_Table(Table *table_list, Status *status)
{
	int i, num;
	char buffer[BUFSIZE], tmp[LENGTH_SIZE];
	int atts = A_STANDOUT;
	num = status->process_state[0];

	attrset(atts);
	bzero(buffer, BUFSIZE);
	strcat(buffer, "    PID USER      PR  NI    VIRT    RES    SHR S  ");
	strcat(buffer, "\%CPU  \%MEM      TIME+ COMMAND           " );

	attron(atts);
	printw("%s\n", buffer);
	attroff(atts);
	bzero(buffer, BUFSIZE);

	sortByPid(table_list, num);
	sortByCPUShare(table_list, num);

	for(i = 0; i < num ; i++){
		bzero(buffer, BUFSIZE);
		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%7d ", table_list[i].pid);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%-9s",table_list[i].user);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		if(table_list[i].priority == -100)
			sprintf(tmp, "%3s ", "rt");
		else
			sprintf(tmp, "%3ld ",table_list[i].priority);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%3ld",table_list[i].nice);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%8lld",table_list[i].VIRT);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%7lld",table_list[i].RES);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%7lld ",table_list[i].SHR);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%c  ",table_list[i].state);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%4.1f", table_list[i].cpu_percent);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, " %lld", table_list[i].cpu_share);
		strcat(buffer, tmp);
		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, " %lld", total_cpu_amount);
		strcat(buffer, tmp);


		printw("%s\n", buffer);
	}


	free(table_list);

}
