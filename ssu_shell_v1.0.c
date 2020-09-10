#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define TRUE 1
#define FALSE 0

char COMMAND_PATH[10] = "/usr/bin/";
char pipe_name[5] = "pipe";

char **tokenize(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for(i =0; i < strlen(line); i++){

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

int main(int argc, char* argv[]) {
	char line[MAX_INPUT_SIZE], command[MAX_TOKEN_SIZE];            
	char **tokens, **args;              
	int i, j, cur, exePos;
	FILE *fp_pipe_in, *fp_pipe_out;
	int pid, status;
	int argNum, pipeNum;
	FILE* fp;

	int **pipes = (int**)malloc(MAX_NUM_TOKENS * sizeof(int*));
	int startPid;

	// 배치식 실행 파일 없는 경우 예외 처리
	if(argc == 2) {
		fp = fopen(argv[1],"r");
		if(fp < 0) {
			printf("File doesn't exists.");
			return -1;
		}
	}

	while(1) {			
		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line)); // 버퍼 비워 줌

		if(argc == 2) { // 배치식
			if(fgets(line, sizeof(line), fp) == NULL) { // 명령어파일 read
				break;	
			}
			line[strlen(line) - 1] = '\0';
		} else { // 대화식
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
		}

		if(!strcmp(line, "")) // 엔터 입력 처리
			continue;

		//	printf("Command entered: %s (remove this debug output later)\n", line);
		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);

		//do whatever you want with the commands, here we just print them

		/*	for(i=0; tokens[i] != NULL; i++){
			printf("found token %s (remove this debug output later)\n", tokens[i]);
			total_tokens++;
			}
		 */

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
					fprintf(stderr, "pid[%d]/ppid[%d]\n", getpid(), getppid());

					if(pipeNum > 0){ // 입력 변경
						close(pipes[pipeNum-1][1]);
						dup2(pipes[pipeNum-1][0], 0);
					}

					// 명령어 실행
					if(execv(command, args) < 0){
						fprintf(stderr, "execl error\n");
						exit(1);
					}
				}
				else if(pid > 0){ // 부모 프로세스
					int finPid=0;
					while( wait(&status) > 0){
						fprintf(stderr, "%d\n", finPid-startPid);
					//	close(pipes[finPid][0]);
					//	close(pipes[finPid][1]);
					//	free(pipes[finPid++]);
					}
					printf("i thinks here cnat\n");
					break;
				}
			}
			else if(!strcmp(tokens[cur], "|")){ // 파이프 처리
				//	printf("pipe execute!\n");
				bzero(command, strlen(command));
				strcat(command, COMMAND_PATH);
				strcat(command, tokens[exePos]);

				pipes[pipeNum] = (int*)malloc(2 * sizeof(int));

				if(pipe(pipes[pipeNum]) < 0){
					fprintf(stderr, "error for pipe\n");
					exit(1);
				}

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					//fprintf(stderr, "pid[%d]/ppid[%d]\n", getpid(), getppid());

					// 표준 출력을 pipe로 변경
					dup2(pipes[pipeNum][1], 1);
					startPid = getpid();

					if(pipeNum > 0){ // 입력 변경
						dup2(pipes[pipeNum-1][0], 0);
					}

					// 명령어 실행
					if(execv(command, args) < 0){
						fprintf(stderr, "execl error\n");
						exit(1);
					}
				}
				else if(pid > 0){ // 부모 프로세스
					cur++;
					exePos = cur;
					argNum = 0;
					pipeNum++;

					for(j = 0; args[j] != NULL; j++)
						free(args[j]);
					free(args);

					args = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
				}
			}
			else{
				args[argNum] = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(args[argNum++], tokens[cur++]);
			}

		}

	}

	// Freeing the allocated memory	
	for(i=0;tokens[i]!=NULL;i++){
		free(tokens[i]);
	}
	free(tokens);

	return 0;
}
