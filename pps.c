#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "ssu_shell.h"

int aOption, uOption, xOption;
long long total_cpu_amount;
long long total_mem;
int total_proc;
struct winsize size;

int main(int argc, char *argv[])
{
	Table2 **tablelist;

	if(argc > 2){
		fprintf(stderr, "no more than 2 arguments\n");
		exit(1);
	}

	aOption = FALSE;
	uOption = FALSE;
	xOption = FALSE;
	ioctl(2, TIOCGWINSZ, &size);

	if(argc == 2)
		setOptions(argv[1]);

	setTotalAmount();
	tablelist = fill_Table2();
	sortTableByPID(tablelist, total_proc);
	print_Table2(tablelist);

	return 0;
}

void setOptions(char *args)
{
	int i;

	for(i = 0; args[i] != '\0'; i++) {
		switch(args[i]) {
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

Table2** fill_Table2()
{
	int i, j, num, pNum = 0, cur = 0;
	int tty, selftty, pid, isOkay;
	uid_t uid = getuid();
	char filename[LENGTH_SIZE], tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;
	struct dirent **namelist;
	struct stat statbuf;
	Table2** tablelist;

	num = scandir("/proc", &namelist, NULL, alphasort);
	pid = getpid();

	// 총 프로세스 개수 count
	for(i = 0; i < num; i++) {
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;

		pNum++;

		if(pid == atoi(namelist[i]->d_name)) // 현재 프로세스 터미널 넘버 저장
			selftty = getSelfTerminalNumber(namelist[i]->d_name);
	}

	// tablelist 메모리 할당
	tablelist = (Table2**)malloc(pNum * sizeof(Table2*));
	for(i = 0; i < pNum; i++)
		tablelist[i] = (Table2*)malloc(sizeof(Table2));

	// table data 처리
	for(i = 0; i < num; i++) {
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| !atoi(namelist[i]->d_name))
			continue;

		isOkay = TRUE;
		bzero(filename, LENGTH_SIZE);
		strcat(filename, "/proc/");
		strcat(filename, namelist[i]->d_name);
		stat(filename, &statbuf);
		strcat(filename, "/stat");

		fp = fopen(filename, "r");

		for(j = 0; j < 6; j++){
			fscanf(fp, "%[^ ]", tmp);
			fgetc(fp);
		}

		bzero(buf, BUFSIZE);
		fscanf(fp, "%[^ ]", buf);
		tty = atoi(buf);

		// 옵션, uid, tty로 출력여부 결정
		if(!aOption && uid != statbuf.st_uid) 
			isOkay = FALSE;
		if(!xOption && tty == 0)
			isOkay = FALSE;
		if(!aOption && !xOption && !uOption && selftty != tty)
			isOkay = FALSE;
		if(!aOption && !xOption && uOption && uid != statbuf.st_uid)
			isOkay = FALSE;

		if(isOkay) { // 출력대상인 경우 필드정보 세팅
			tablelist[cur] = getRow(namelist[i]->d_name);
			cur++;
		}

		fclose(fp);
	}

	total_proc = cur; // 총 출력 프로세스 개수 

	// 메모리 해제
	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist);

	return tablelist;
}

void setTotalAmount() // CPU 및 메모리 총량 구하는 함수
{
	int i;
	char tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;

	total_cpu_amount = 0;
	fp = fopen("/proc/stat", "r");

	fscanf(fp, "%[^ ]", tmp);
	fgetc(fp);
	fgetc(fp);

	// cpu의 time 총합
	for(i = 0; i < 8; i++) {
		fscanf(fp, "%[^ ]", buf);
		fgetc(fp);
		total_cpu_amount += atoll(buf);
	}

	fclose(fp);

	// totalMem 추출
	fp = fopen("/proc/meminfo", "r");
	fscanf(fp, "%[^:]", tmp);
	fgetc(fp);
	fscanf(fp, "%[^kB]", buf);
	total_mem = atoll(buf);

	fclose(fp);
}

int getSelfTerminalNumber(char *name) // 명령어 실행 터미널 넘버 구하는 함수
{
	int i;
	char filename[LENGTH_SIZE], tmp[BUFSIZE], buf[BUFSIZE];
	FILE *fp;

	bzero(filename, LENGTH_SIZE);
	strcat(filename, "/proc/");
	strcat(filename, name);
	strcat(filename, "/stat");

	if((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "fopen error for %s\n", filename);
		exit(1);
	}

	for(i = 0; i < 6; i++) { // tty_nr 항목까지 read
		fscanf(fp, "%[^ ]", tmp);
		fgetc(fp);
	}

	fscanf(fp, "%[^ ]", buf);
	fclose(fp);

	return atoi(buf);
}

Table2* getRow(char *pid) // 테이블 목록 한 행 구하는 함수
{
	int i = 0, pos;
	char filename[LENGTH_SIZE], buf[BUFSIZE], tmp[BUFSIZE];
	FILE *fp;
	Table2* row = (Table2*)malloc(sizeof(Table2));
	struct stat statbuf;

	bzero(filename, LENGTH_SIZE);
	bzero(row->STAT, 6);
	row->VSZ = 0;
	row->RSS = 0;

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

		if(i == 1) { // command
			buf[0]='[';
			buf[strlen(buf)-1]=']';
			strcpy(row->command, buf);
		}
		else if(i == 2) // STATE
			row->STAT[0] = buf[0];
		else if(i == 5 && !strcmp(buf, pid)) // session
			row->STAT[3] = 's';
		else if(i == 6)
			strcpy(row->tty, getTerminal(atoi(buf))); // TTY
		else if(i == 7 && atoi(buf) > 0) // tpgid
			row->STAT[5] = '+';
		else if(i == 13) // utime
			row->cpu = atoll(buf);
		else if(i == 14) { // stime
			row->cpu += atoll(buf);
			strcpy(row->time, getTime2(row->cpu));
		}
		else if(i == 18){ // nice
			if(atoi(buf) < 0)
				row->STAT[1] = '<';
			else if(atoi(buf) > 0)
				row->STAT[1] = 'N';
		}
		else if(i == 19 && atoi(buf) > 1) // thread_num
			row->STAT[4] = 'l';
		else if(i == 21) { // starttime
			strcpy(row->start, getStratTime(atoll(buf)));
			row->cpu = getCPUTime(row->cpu, atoll(buf));
		}
	}

	fclose(fp);

	strcat(filename, "us"); // '/proc/pid/status' open
	fp = fopen(filename, "r");

	while(fscanf(fp, "%[^:]", tmp) != EOF){
		fgetc(fp);
		fscanf(fp, "%[^\n]", buf);
		fgetc(fp);

		if(!strcmp(tmp, "VmSize")) // VmSize
			row->VSZ = atoll(buf);
		else if(!strcmp(tmp, "VmRSS")) // VmRSS
			row->RSS = atoll(buf);
		else if(!strcmp(tmp, "VmLck") && atoll(buf) > 0) // VmLck
			row->STAT[2] = 'L';
	}

	fclose(fp);

	// 명령어 필드 추출 위치 결정 (cmdline or command)
	strcpy(tmp, getCommand(pid));  
	if(tmp[0] != '?')
		strcpy(row->command, tmp);

	// STAT 필드 문자열 정렬
	for(i = 1, pos = 1; i < 6; i++)
		if(row->STAT[i] != (char)0)
			row->STAT[pos++] = row->STAT[i];
	row->STAT[pos] = '\0';

	return row;
}

long long getCPUTime(long long cputime, long long starttime) // cpu 점유율 구하는 함수
{
	long long uptime, percent = 0;

	uptime = getUpTime();

	starttime /= sysconf(_SC_CLK_TCK);
	uptime -= starttime;

	if(uptime != 0)
		percent = (cputime * 10) / uptime;

	return percent;
}

char *getStratTime(long long starttime) // 프로세스 시작 시간 구하는 함수
{
	char* stime = (char*)malloc(10 * sizeof(char));
	time_t uptime, now;
	struct tm tm;

	uptime = getUpTime();

	time(&now);
	starttime /= sysconf(_SC_CLK_TCK);
	uptime -= starttime;
	uptime = now - uptime;
	tm = *localtime(&uptime);

	sprintf(stime, "%02d:%02d", tm.tm_hour, tm.tm_min);

	return stime;
}

time_t getUpTime() // uptime 구하는 함수
{
	FILE *fp;
	char buf[LENGTH_SIZE];

	fp = fopen("/proc/uptime", "r");
	fscanf(fp, "%[^.]", buf);
	fclose(fp);

	return atoi(buf);
}

char *getTime2(long long amount) // cputime 구하는 함수
{
	int hour, min, sec;
	char* time = (char*)malloc(10 * sizeof(char));

	amount /= sysconf(_SC_CLK_TCK);
	min = amount / 60;
	hour = min / 60;
	sec = amount % 60;

	if(min > 999) // 999분이 넘는 경우
		return "undefined";
	else if(!aOption && !uOption && !xOption) { // u옵션 없는 경우
		sprintf(time, "%02d:%02d:%02d", hour, min % 60, sec);
		return time;
	}
	else { // 옵션 있는 경우
		sprintf(time, "%3d:%02d", min, sec);
		return time;
	}
}

char * getTerminal(long unsigned int tty_nr) // 터미널 구하는 함수
{
	int i, num;
	char* terminal = (char*)malloc(PATH_SIZE * sizeof(char));
	char filename[LENGTH_SIZE];
	struct stat statbuf;
	struct dirent **namelist;

	bzero(terminal, PATH_SIZE);

	if(tty_nr == 0) // 터미널이 없는 경우
		return "?";

	num = scandir("/dev", &namelist, NULL, alphasort);

	for(i = 0; i < num; i++) { // "/dev/tty*" 탐색
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, "..")
				|| strncmp(namelist[i]->d_name, "tty", 3))
			continue;

		bzero(filename, LENGTH_SIZE);
		strcat(filename, "/dev/");
		strcat(filename, namelist[i]->d_name);
		stat(filename, &statbuf);

		if(tty_nr == statbuf.st_rdev) {  
			strcpy(terminal, namelist[i]->d_name);
			return terminal;
		}
	}

	num = scandir("/dev/pts", &namelist, NULL, alphasort);

	for(i = 0; i < num; i++) { // "/dev/pts/*" 탐색
		if(!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		bzero(filename, LENGTH_SIZE);
		strcat(filename, "/dev/pts/");
		strcat(filename, namelist[i]->d_name);
		stat(filename, &statbuf);

		if(tty_nr == statbuf.st_rdev) {
			strcat(terminal, "pts/");
			strcat(terminal, namelist[i]->d_name);
			return terminal;
		}
	}

	for(i = 0; i < num; i++)
		free(namelist[i]);
	free(namelist[i]);

	return terminal;
}

char* getCommand(char* pid) // 명령어 항목 구하는 함수
{
	int i, length;
	char* command = (char*)malloc(BUFSIZE * sizeof(char));
	char filename[LENGTH_SIZE];
	FILE *fp;

	strcpy(filename, "/proc/");
	strcat(filename, pid);
	strcat(filename, "/cmdline");

	fp = fopen(filename, "r");
	fread(command, BUFSIZE, 1, fp);
	length = ftell(fp);
	fclose(fp);

	if(length == 0) // cmdline 파일이 빈 경우
		return "?";
	else if(aOption || uOption || xOption) { // ^@ 처리
		for(i = 0; i < length - 1; i++)
			if(command[i] == '\0')
				command[i] = ' ';

		return command;
	}

	return command;
}

void sortTableByPID(Table2 **tablelist, int num) // pid 기준 오름차순 정렬 함수
{
	int i, j;
	Table2 *tmp = (Table2*)malloc(sizeof(Table2));

	for(i = 0; i < num; i++)
		for(j = i; j < num; j++)
			if(tablelist[i]->pid > tablelist[j]->pid){
				tmp = tablelist[i];
				tablelist[i] = tablelist[j];
				tablelist[j] = tmp;
			}
}

void print_Table2(Table2 **tablelist) // 목록 출력 함수
{
	int cur = 0;
	char buffer[BUFSIZE], tmp[LENGTH_SIZE], tmp2[BUFSIZE];

	bzero(buffer, BUFSIZE);

	if(uOption) // u옵션 시 출력
	{
		sprintf(buffer, "%-12s%4s %s %s%7s%6s %s      %s %s   %s %s", "USER", "PID", "\%CPU", 
				"%MEM", "VSZ", "RSS", "TTY", "STAT", "START", "TIME", "COMMAND");
		printf("%s\n", buffer);

		while(cur < total_proc)
		{
			bzero(buffer, BUFSIZE);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%-10s", tablelist[cur]->user);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%6d ", tablelist[cur]->pid);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%4.1f ", (double)tablelist[cur]->cpu / 10);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			tablelist[cur]->mem = (double)tablelist[cur]->RSS / total_mem * 1000;
			sprintf(tmp, "%4.1f ", (double)tablelist[cur]->mem / 10);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%6lld ", tablelist[cur]->VSZ);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%5lld ", tablelist[cur]->RSS);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%-8s ", tablelist[cur]->tty);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%-4s ", tablelist[cur]->STAT);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%s ", tablelist[cur]->start);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%6s ", tablelist[cur]->time);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp2, "%s", tablelist[cur]->command);
			strcat(buffer, tmp2);
			buffer[size.ws_col] = '\0';
			printf("%s\n", buffer);
			cur++;
		}
	}
	else // u옵션 없을 시 출력
	{
		if(!aOption && !uOption && !xOption)
			sprintf(buffer, "%7s %-8s %8s %s", "PID", "TTY", "TIME", "CMD");
		else
			sprintf(buffer, "%7s %-8s %s %6s %s", "PID", "TTY", "STAT", "TIME", "COMMAND");
		printf("%s\n", buffer);

		while(cur < total_proc)
		{
			bzero(buffer, BUFSIZE);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%7d ", tablelist[cur]->pid);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%-8s ", tablelist[cur]->tty);
			strcat(buffer, tmp);

			if(aOption || xOption) {
				bzero(tmp, LENGTH_SIZE);
				sprintf(tmp, "%-4s ", tablelist[cur]->STAT);
				strcat(buffer, tmp);
			}

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp, "%6s ", tablelist[cur]->time);
			strcat(buffer, tmp);

			bzero(tmp, LENGTH_SIZE);
			sprintf(tmp2, "%s", tablelist[cur]->command);
			strcat(buffer, tmp2);

			buffer[size.ws_col] = '\0';
			printf("%s\n", buffer);
			cur++;
		}
	}
}
