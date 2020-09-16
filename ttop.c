#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <ncurses.h>
#include "ssu_shell.h"

long long old_cpulist[8]; // status용
long long **old_cpu_amount; // table 용
int old_num, table_pos, isAlarmed;
long long total_memory;
long long total_cpu_amount;
struct winsize size;

int main()
{
	int i, key = 0;
	table_pos = 0;
	isAlarmed = TRUE;

	initscr();
	cbreak();

	for(i = 0; i < 8; i++)
		old_cpulist[i] = 0;

	signal(SIGALRM, alarm_handler); // 시그널 등록

	while(1)
	{
		ioctl(2, TIOCGWINSZ, &size); // 터미널크기 측정
		alarm(3);
		execute(); // 명령어 실행
		refresh();
		isAlarmed = FALSE;
		key = getch();

		if(key == 113) // 'q' 입력
			break;
		else if(key == 65 && !(table_pos == 0)) // KEY_DOWN
			table_pos--;
		else if(key == 66 && !(table_pos == old_num)) // KEY_UP
			table_pos++;

		clear();
	}

	endwin();
}

void alarm_handler(int signo)
{
	if(signo == SIGALRM){
		clear();
		alarm(3);
		execute();
		refresh();
	}
}

void execute()
{
	Status *status = (Status*)malloc(sizeof(Status));
	Table *table_list;

	fill_Status(status);
	print_Status(status);
	table_list = fill_Table(status);
	print_Table(table_list, status);
	printw("											\n"); // 커서 지우기
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
	int i, userNum = 0;
	char buffer[BUFSIZE], tmp[BUFSIZE - 10];
	FILE *fp;
	time_t now;
	struct tm tm;

	bzero(buffer, BUFSIZE);
	bzero(status->uptime_status, BUFSIZE);

	strcat(buffer, "top - ");

	// 현재 시간 
	time(&now);
	tm = *localtime(&now);
	sprintf(tmp, "%02d:%02d:%02d ", tm.tm_hour, tm.tm_min, tm.tm_sec);
	strcat(buffer, tmp);

	// 부팅 후 흐른 시간
	strcat(buffer, "up ");
	strcat(buffer, getUptime());

	// 총 유저 수
	if((fp = fopen("/etc/passwd", "r")) == NULL){
		fprintf(stderr, "fopen error for /proc/loadavg");
		exit(1);
	}

	while(fscanf(fp, "%[^\n]", tmp) != 0)
		if(strstr(tmp, "/bin/bash") != NULL)
			userNum++;

	bzero(tmp, strlen(tmp));
	sprintf(tmp, "%d user,  ", userNum);
	strcat(buffer, tmp);
	fclose(fp);

	// load average
	if((fp = fopen("/proc/loadavg", "r")) == NULL){
		fprintf(stderr, "fopen error for /proc/loadavg");
		exit(1);
	}

	strcat(buffer, "load average: ");
	for(i = 0; i < 3; i++){
		fscanf(fp, "%[^ ]", tmp);
		fgetc(fp);
		strcat(buffer, tmp);
		strcat(buffer, ", ");
	}

	buffer[strlen(buffer)-2] ='\0';
	strcpy(status->uptime_status, buffer);

	fclose(fp);

}

char* getUptime()
{
	int hour, min;
	long long amount;
	char* uptime = (char*)malloc(LENGTH_SIZE * sizeof(char));
	char buffer[LENGTH_SIZE];
	FILE *fp;

	if((fp = fopen("/proc/uptime", "r")) == NULL){
		fprintf(stderr, "fopen error for /proc/uptime\n");
		exit(1);
	}

	fscanf(fp, "%[^.]", buffer);
	amount = atoll(buffer);
	min = (amount / 60) % 60;
	hour = amount / (60 * 60);

	// 경우에 따른 출력형식 조정
	if(hour == 0)
		sprintf(uptime, "%02d minutes  ", min);
	else if(hour > 23)
		sprintf(uptime, "%d day, %02d:%02d,  ", hour / 24, hour % 24, min);
	else
		sprintf(uptime, "%02d:%02d,  ", hour, min);

	fclose(fp);

	return uptime;
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
			continue;
		}

		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		bzero(buf, strlen(buf));
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		bzero(buf, strlen(buf));
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);

		status->process_state[0]++; // total 증가

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

	fclose(fp);
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

	status->uptime_status[size.ws_col - 1] ='\0';
	printw("%s\n", status->uptime_status);

	strcat(buffer, "Tasks: ");
	sprintf(tmp, "%d total,  %d running,  %d sleeping,  %d stopped,  %d zombie", 
			status->process_state[0], status->process_state[1], status->process_state[2], 
			status->process_state[3], status->process_state[4]);
	strcat(buffer, tmp);

	buffer[size.ws_col - 1] ='\0';
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

	buffer[size.ws_col - 1] ='\0';
	printw("%s\n", buffer);
	bzero(buffer, strlen(buffer));
	bzero(tmp, strlen(tmp));

	strcat(buffer, "MiB Mem :");
	sprintf(tmp, "    %5.1f total,    %5.1f free,    %5.1f used,    %5.1f buff/cache"
			, status->physical_memory[0], status->physical_memory[1]
			, status->physical_memory[2], status->physical_memory[3]);
	strcat(buffer, tmp);

	buffer[size.ws_col - 1] ='\0';
	printw("%s\n", buffer);
	bzero(buffer, strlen(buffer));
	bzero(tmp, strlen(tmp));

	strcat(buffer, "MiB Swap :");
	sprintf(tmp, "    %5.1f total,    %5.1f free,    %5.1f used,    %5.1f avail Mem"
			, status->virtual_memory[0], status->virtual_memory[1]
			, status->virtual_memory[2], status->virtual_memory[3]);
	strcat(buffer, tmp);

	buffer[size.ws_col - 1] ='\0';
	printw("%s\n\n", buffer);
}

Table* fill_Table(Status *status) // 이거 할당 타이밍 생각해보자.
{
	int i, j, num, pNum = 0, cur=0;
	char filename[LENGTH_SIZE], buf[BUFSIZE], tmp[BUFSIZE];
	struct stat statbuf;
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
		stat(filename, &statbuf);
		strcpy(tablelist[cur].user, getUser(statbuf.st_uid));
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


		for(j = 0 ; j < 23; j++){
			fscanf(fp, "%[^ ]", buf);
			fgetc(fp);

			switch(j)
			{
				case 0 : // pid
					tablelist[cur].pid = atoi(buf);
					break;
				case 1 : // command
					strcpy(tmp, buf+1);
					tmp[strlen(tmp)-1] = '\0';
					strcpy(tablelist[cur].command, tmp);
					break;
				case 2 : // state
					tablelist[cur].state = buf[0];
					break;
				case 13 : // utime
					tablelist[cur].cpu_amount = atoll(buf);
					break;
				case 14 : // stime
					tablelist[cur].cpu_amount += atoll(buf);
					tablelist[cur].cpu_share = 
						tablelist[cur].cpu_amount - findOldAmount(tablelist[cur].pid);
					total_cpu_amount += tablelist[cur].cpu_share;

					strcpy(tablelist[cur].time,	getTime(tablelist[cur].cpu_amount));
					break;
				case 17 : // priority
					tablelist[cur].priority = atoi(buf);
					break;
				case 18 : // nice
					tablelist[cur].nice = atoi(buf);
					break;
				case 22 : // starttime
					//	strcpy(tablelist[cur].time, getTime(starttime));
					//tablelist[cur].nice = atoi(buf);
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
		old_cpu_amount[i] = (long long*)malloc(2 * sizeof(long long));
		old_cpu_amount[i][0] = tablelist[i].pid;
		old_cpu_amount[i][1] = tablelist[i].cpu_amount;
	}

	return tablelist;

}

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

char* getTime(long long ttime)
{
	int hour, sec, min;
	char *name, tmp[LENGTH_SIZE];

	name = (char*)malloc(LENGTH_SIZE * sizeof(char));

	min = ttime / sysconf(_SC_CLK_TCK);
	sec = ttime % sysconf(_SC_CLK_TCK);
	hour = min / 60;
	min %= 60;

	if(1000 <= hour)
		sprintf(tmp, "%d:%02d", hour, min);
	else
		sprintf(tmp, "%d:%02d.%02d", hour, min, sec);
	strcpy(name, tmp);

	return name;
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
	strcat(buffer, "\%CPU  \%MEM      TIME+ COMMAND          	");

	attron(atts);
	buffer[size.ws_col-1] ='\0';
	printw("%s\n", buffer);
	attroff(atts);
	atts = A_BOLD;

	sortByPid(table_list, num);
	sortByCPUShare(table_list, num);

	for(i = table_pos; i < size.ws_row+table_pos-7; i++){
		bzero(buffer, BUFSIZE);
		bzero(tmp, LENGTH_SIZE);

		if(table_list[i].state == 'R')
			attron(atts);

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
		sprintf(tmp, "%4.1f  ", table_list[i].cpu_percent);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%4.1f", (double)table_list[i].RES / total_memory * 100);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%11s ", table_list[i].time);
		strcat(buffer, tmp);

		bzero(tmp, LENGTH_SIZE);
		sprintf(tmp, "%s", table_list[i].command);
		strcat(buffer, tmp);

		buffer[size.ws_col-1]='\0';
		printw("%s\n", buffer);
		attroff(atts);
	}


	free(table_list);

}
