#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define PATH_SIZE 10
#define PIPE_NAME_SIZE 5
#define TRUE 1
#define FALSE 0
#define BUFSIZE 1024
#define KB_TO_MiB 1024
#define LENGTH_SIZE 64

// 상태창 구조체
typedef struct top_status {
	char uptime_status[BUFSIZE]; // uptime 정보 [현재 시간, 부팅 걸린 시간, 유저수, load avg(1,5,15분 간격)]
	int process_state[5]; // 프로세스 상태 정보 [total, running, sleeping, stopped, zombie]
	long long cpu_share[8]; // user, system, nice, idle, IO-wait, hardware interrupts, software interrupts, stolen
	double physical_memory[4]; // total, free, used, buff/cache
	double virtual_memory[4]; // total, free, used, avail Mem
}Status;

// 프로세스 테이블 구조체
typedef struct process_table {
	int pid; // pid
	char user[16]; // username
	long priority; // priority
	long nice; // nice
	long long VIRT; // virtual memory
	long long RES; // real memory
	long long SHR; // shared memory
	long long cpu_share; // cpu percent
	//double mem_share; // memory percent
	char state; // state
	char time[16]; // process time
	char command[30]; // command name

	double cpu_percent;
	long long cpu_amount;
}Table;

void fill_Status(Status *status);
void get_UptimeStatus(Status *status);
void get_ProcessStatus(Status *status);
void get_CPUStatus(Status *status);
void get_MemoryStatus(Status *status);
void print_Status(Status *status);
Table* fill_Table(Status *status);
long long findOldAmount(int pid);
char* getUser(int uid);
char* getTime(long long ttime);
void print_Table(Table *table_list, Status *status);
