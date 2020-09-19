#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "ssu_shell.h"

int pipes;

char **tokenize(char *line) // 명령어 토크나이징
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *)); // 토큰 리스트
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char)); // 토큰
	int i, tokenIndex = 0, tokenNo = 0;

	for(i = 0; i < (int)strlen(line); i++){
		char readChar = line[i];

		if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);

				if(!strcmp(token, "|"))
					pipes++;

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

void execute_command(char* command, char ** args)
{
	char cmd[MAX_TOKEN_SIZE+10];

	bzero(cmd, MAX_TOKEN_SIZE+10);

	if(!strcmp(command, "ttop") || !strcmp(command, "pps")) {
		strcat(cmd, "./");
		strcat(cmd, command);
	}
	else 
		strcpy(cmd, command);

	if(execvp(cmd, args) < 0){
		fprintf(stderr, "SSUShell : Incorrect command\n");
		exit(1);
	}
}

int main(int argc, char* argv[]) 
{
	char line[MAX_INPUT_SIZE], command[MAX_TOKEN_SIZE];
	char **tokens, **args;
	int i, j, cur, exePos;
	int pid, status;
	int argNum, pipeNum;
	int **ssu_pipe;
	FILE *fp;

	// 배치식 실행 파일 없는 경우 예외 처리
	if(argc == 2) {
		if((fp = fopen(argv[1],"r")) == NULL){
			fprintf(stderr, "file doesn't exits\n");
			exit(1);
		}
	}

	while(1) {			
		bzero(line, sizeof(line)); // 버퍼 비워 줌
		pipes = 0;

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
		ssu_pipe = (int**)malloc(pipes * sizeof(int*));

		for(i = 0; i < pipes; i++)
			ssu_pipe[i] = (int*)malloc(2 * sizeof(int));

		exePos=0;
		argNum=0;
		cur = 0;
		pipeNum = 0;

		// 실행
		while(1) 
		{
			//printf("%s\n", tokens[cur]);
			if(tokens[cur] == NULL) { // 명령어 라인 끝 도달 
				bzero(command, strlen(command));
				strcpy(command, tokens[exePos]);

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					if(pipeNum > 0) // 파이프 있는 경우, 입력 변경
						dup2(ssu_pipe[pipeNum-1][0], 0);

					for(i = 0; i < pipeNum; i++) { // 그 외 파이프 close
						close(ssu_pipe[i][1]);
						close(ssu_pipe[i][0]);
					}

					execute_command(command, args); // 명령어 실행
				}
				else if(pid > 0){ // 부모 프로세스
					int cnt = 0;
					while(wait(&status) > 0) { // 모든 자식이 종료될 때까지 wait
						if(cnt < pipes) {
							close(ssu_pipe[cnt][0]);
							close(ssu_pipe[cnt][1]);
							free(ssu_pipe[cnt++]);
						}
					}
					break;
				}
			}
			else if(!strcmp(tokens[cur], "|")) { // 파이프 처리
				bzero(command, strlen(command));
				strcpy(command, tokens[exePos]);
				pipe(ssu_pipe[pipeNum]);

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					if(pipeNum > 0) // pipe 2개 이상인 경우, 입력 변경
						dup2(ssu_pipe[pipeNum - 1][0], 0);

					dup2(ssu_pipe[pipeNum][1], 1);

					for(i = 0; i < pipeNum; i++) { // 그 외 파이프 close
						close(ssu_pipe[i][1]);
						close(ssu_pipe[i][0]);
					}

					execute_command(command, args); // 명령어 실행
				}
				else if(pid > 0){ // 부모 프로세스
					cur++; // 다음 토큰으로 이동
					exePos = cur; // 명령어 실행 파일명 재지정
					argNum = 0; // 인자 개수 초기화
					pipeNum++; // pipe개수 증가

					// 메모리 관리
					for(j = 0; args[j] != NULL; j++){
						args[j] = NULL;
						free(args[j]);
					}
					free(args);
					args = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
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
