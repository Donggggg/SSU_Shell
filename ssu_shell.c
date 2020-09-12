#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define PATH_SIZE 10
#define PIPE_NAME_SIZE 5
#define TRUE 1
#define FALSE 0

const char COMMAND_PATH[PATH_SIZE] = "/usr/bin/";
char pipe_name[PIPE_NAME_SIZE] = "pipe";

char **tokenize(char *line) // 명령어 토크나이징하는 함수
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *)); // 토큰 리스트
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char)); // 토큰
	int i, tokenIndex = 0, tokenNo = 0;

	for(i = 0; i < strlen(line); i++){
		char readChar = line[i];

		if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} else {
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL ;
	return tokens;
}

int main(int argc, char* argv[]) 
{
	char line[MAX_INPUT_SIZE], command[MAX_TOKEN_SIZE];
	char **tokens, **args;
	int i, j, cur, exePos;
	int pid, status;
	int argNum, pipeNum;
	FILE *fp, *fp_pipe_in, *fp_pipe_out;

	// 배치식 실행 파일 없는 경우 예외 처리
	if(argc == 2) {
		if((fp = fopen(argv[1],"r")) == NULL){
			fprintf(stderr, "file doesn't exits\n");
			exit(1);
		}
	}

	while(1) {			
		bzero(line, sizeof(line)); // 버퍼 비워 줌

		if(argc == 2) { // 배치식
			if(fscanf(fp, "%[^\n]", line) < 0)  // 명령어파일 read
				break;	
		
			fgetc(fp);
			line[strlen(line) - 1] = '\0';
		}
		else { // 대화식
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();

			if(!strcmp(line, "")) // 엔터 입력 처리
				continue;
		}

		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);

		// 명령어 실행 전처리
		args = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
		exePos=0;
		argNum=0;
		cur = 0;
		pipeNum = 0;

		while(1) 
		{
			//printf("CUR > %s\n", tokens[cur]);
			if(tokens[cur] == NULL){
				//		printf("NULL execute!\n");
				bzero(command, strlen(command));
				strcat(command, COMMAND_PATH);
				strcat(command, tokens[exePos]);

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					//fprintf(stderr, "pid[%d]/ppid[%d]\n", getpid(), getppid());

					if(pipeNum > 0){ // 입력 변경
						pipe_name[PIPE_NAME_SIZE - 1] = pipeNum - 1 + 48;	
						//	printf("fin's filename : %s\n", pipe_name);

						while((fp_pipe_in = fopen(pipe_name, "r")) == NULL);

						dup2(fileno(fp_pipe_in), 0);
					}

					// 명령어 실행
					if(execv(command, args) < 0){
						fprintf(stderr, "SSUShell : Incorrect command\n");
						exit(1);
					}
				}
				else if(pid > 0){ // 부모 프로세스
					while(wait(&status) > 0); // 모든 자식이 종료될 때까지 wait

					for(i = 0; i < pipeNum; i++){ // 파이프 파일 삭제
						pipe_name[4] = i+48;
						unlink(pipe_name);
					}
					break;
				}
			}
			else if(!strcmp(tokens[cur], "|")) { // 파이프 처리
				//	printf("pipe execute!\n");
				bzero(command, strlen(command));
				strcat(command, COMMAND_PATH);
				strcat(command, tokens[exePos]);

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					//fprintf(stderr, "pid[%d]/ppid[%d]\n", getpid(), getppid());

					// 표준 출력을 pipe로 변경
					pipe_name[4] = pipeNum+48;	
					//	printf("out's filename : %s\n", pipe_name);

					if((fp_pipe_out = fopen(pipe_name, "w+")) == NULL){
						fprintf(stderr, "fopen error for pipe file[out]\n");
						exit(1);
					}

					dup2(fileno(fp_pipe_out), 1);

					if(pipeNum > 0){ // 입력 변경
						pipe_name[PIPE_NAME_SIZE - 1] = pipeNum - 1 + 48;	

						while((fp_pipe_in = fopen(pipe_name, "r")) == NULL);

						dup2(fileno(fp_pipe_in), 0);
					}

					// 명령어 실행
					if(execv(command, args) < 0){
						fprintf(stderr, "SSUShell : Incorrect command\n");
						exit(1);
					}
				}
				else if(pid > 0){ // 부모 프로세스
					cur++; // 다음 토큰으로 이동
					exePos = cur; // 명령어 실행 파일명 재지정
					argNum = 0; // 인자 개수 초기화
					pipeNum++; // pipe개수 증가

					// 메모리 관리
					for(j = 0; args[j] != NULL; j++)
						free(args[j]);
					free(args);
					args = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));

					usleep(100000); // 순서 조정
				}
			}
			else { // 명령인 경우 리스트에 추가
				args[argNum] = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(args[argNum++], tokens[cur++]);
			}
		}
	}

	for(i = 0; tokens[i] != NULL; i++)
		free(tokens[i]);
	free(tokens);

	return 0;
}
