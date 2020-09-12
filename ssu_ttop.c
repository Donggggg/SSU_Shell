#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ncurses.h>

#define BUFSIZE 1024

// 상태창 구조체
typedef struct top_status {
	char *uptime_status; // uptime 정보 [현재 시간, 부팅 걸린 시간, 유저수, load avg(1,5,15분 간격)]
	int process_state[5]; // 프로세스 상태 정보 [total, running, sleeping, stopped, zombie]
	double cpu_share[8]; // user, system, nice, idle, IO-wait, hardware interrupts, software interrupts, stolen
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

int main()
{
	Status *status = (Status*)malloc(sizeof(Status));

	initscr();
	int count = 0;

	while(1)
	{
		init_Status(status);
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
	free(status->uptime_status);
	for(i = 0; i < 5; i++)
		status->process_state[i] = 0;
	for(i = 0; i < 8; i++)
		status->cpu_share[i] = 0;
	for(i = 0; i < 4; i++){
		status->physical_memory[i] = 0;
		status->virtual_memory[i] = 0;
	}
}

void fill_Status(Status *status) {
	char buffer[BUFSIZE];

	strcpy(buffer, get_UptimeStatus());
	status->uptime_status = (char*)malloc(strlen(buffer)*sizeof(char));
	strcpy(status->uptime_status, buffer);
	status->uptime_status[strlen(status->uptime_status)-1] = '\0';
	printw("%s\n", status->uptime_status);

}

char *get_UptimeStatus() {
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

		return buffer;
}
