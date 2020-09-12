#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ncurses.h>

#define BUFSIZE 1024

// 상태창 구조체
typedef struct top_status {
	char *uptime_status; // uptime 정보 [현재 시간, 부팅 걸린 시간, 유저수, load avg(1,5,15분 간격)]
	int process_state[5]; // 프로세스 상태 정보 [total, running, sleeping, stopped, zombie]
	long long cpu_share[8]; // user, system, nice, idle, IO-wait, hardware interrupts, software interrupts, stolen
	double physical_memory[4]; // total, free, used, buff/cache
	double virtual_memory[4]; // total, free, used, avail Mem
}Status;

// 프로세스 테이블 구조체
typedef struct process_table {
	int pid; // pid
	char *user; // username
	long priority; // priority
	long nice; // nice
	long long VIRT; // virtual memory
	long long RES; // real memory
	long long SHR; // shared memory
	double cpu_share; // cpu percent
	double mem_share; // memory percent
	char *time; // process time
	char *command; // command name
}Table;

void init_Status(Status *status);
void fill_Status(Status *status);
char *get_UptimeStatus();
void get_ProcessStatus(Status *status);
void get_CPUStatus(Status *status);
void print_Status(Status *status);

long long old_cpulist[8];

int main()
{
	Status *status = (Status*)malloc(sizeof(Status));

	initscr();
	int count = 0;

	init_Status(status);

	while(1)
	{
		fill_Status(status);

		printw("%d\n", count++);	

		refresh();
		sleep(3);
		getch();
		clear();
		endwin();
	}
}

void init_Status(Status *status) {
	int i;

//	bzero(status->uptime_status, strlen(status->uptime_status));
//	free(status->uptime_status);
//	for(i = 0; i < 5; i++)
//		status->process_state[i] = 0;
	for(i = 0; i < 8; i++)
		old_cpulist[i] = 0;
	for(i = 0; i < 4; i++){
		status->physical_memory[i] = 0;
		status->virtual_memory[i] = 0;
	}
}

void fill_Status(Status *status) 
{
	char buffer[BUFSIZE];

	strcpy(buffer, get_UptimeStatus());
	status->uptime_status = (char*)malloc(strlen(buffer)*sizeof(char));
	strcpy(status->uptime_status, buffer);
	status->uptime_status[strlen(status->uptime_status)-1] = '\0';

	get_ProcessStatus(status);

	get_CPUStatus(status);

	print_Status(status);

	free(status->uptime_status);
}

char *get_UptimeStatus() 
{
	int pid, status, length;
	int uptime_pipe[2];
	char* buffer, temp_buffer[BUFSIZE];

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
			wait(&status);
		}

		length = read(0, temp_buffer, 1024);
		dup2(100, 0);
		temp_buffer[strlen(temp_buffer)-1] = '\0';
		buffer = (char*)malloc((strlen("top -") + length) * sizeof(char));
		strcat(buffer, "top -");
		strcat(buffer, temp_buffer);

		close(uptime_pipe[0]);
		close(uptime_pipe[1]);

		return buffer;
}

void get_ProcessStatus(Status *status)
{
	int i, num;
	char filename[BUFSIZE], buf[BUFSIZE];
	FILE *fp;
	struct stat statbuf;
	struct dirent **namelist;

	for(i = 0; i < 5; i++)
		status->process_state[i] = 0;

	num = scandir("/proc", &namelist, NULL, alphasort);
	strcat(filename, "/proc/");

	for(i = 0; i < num; i++){ // '/proc' 디렉토리 순회 탐색
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;

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

}
