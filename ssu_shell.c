#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

char COMMAND_PATH[10] = "/usr/bin/";

/* Splits the string by space and returns the array of tokens
 *
 */
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

char **makeTokensCommand(char **tokens) // 토큰들을 명령어 리스트화 해주는 함수
{
	char **command_list = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
	char *word = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, commandNo = 0;

	bzero(word, strlen(word));

	for(i = 0; tokens[i] != NULL; i++){
		printf("now tok :: %s\n", tokens[i]);
		if(!strcmp(tokens[i], "|")){
			command_list[commandNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
			strcpy(command_list[commandNo], word);
			printf("noew :: %s\n", command_list[commandNo]);
			command_list[commandNo][strlen(command_list[commandNo++])-1] = '\0';
			bzero(word, strlen(word));
		}
		else {
			strcat(word, tokens[i]);
			strcat(word, " ");
		}
	}

	command_list[commandNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
	strcpy(command_list[commandNo], word);
	printf("noew :: %s\n", command_list[commandNo]);
	command_list[commandNo][strlen(command_list[commandNo++])-1] = '\0';

	free(word);
	command_list[commandNo] = NULL;

	return command_list;
}

int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens, **args;              
	int i, cur;
	int total_tokens;
	int pid, status;
	FILE* fp;

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
		total_tokens = 0;

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

		for(i=0; tokens[i] != NULL; i++){
			printf("found token %s (remove this debug output later)\n", tokens[i]);
			total_tokens++;
		}

		/*	command_list = makeTokensCommand(tokens);

			for(i = 0; command_list[i] != NULL; i++){
			printf("%s\n", command_list[i]);
			}
		 */
		char command[MAX_TOKEN_SIZE];
		args = (char**)malloc(MAX_NUM_TOKENS * sizeof(char*));
		int exePos=0, argNum=0;
		cur =0;

		while(1) 
		{
			printf("CUR > %s\n", tokens[cur]);
			if(tokens[cur] == NULL){
				bzero(command, strlen(command));
				strcat(command, COMMAND_PATH);
				strcat(command, tokens[exePos]);

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					fprintf(stderr, "pid[%d]/ppid[%d]\n", getpid(), getppid());
					fprintf(stderr, "command > %s\n", command);

					// 명령어 실행
					if(execv(command, args) < 0){
						fprintf(stderr, "execl error\n");
						exit(1);
					}
				}
				else if(pid > 0){ // 부모 프로세스
					wait(&status);
					break;
				}
			}
			else if(!strcmp(tokens[cur], "|")){ // 파이프가 있으면
				bzero(command, strlen(command));
				strcat(command, COMMAND_PATH);
				strcat(command, tokens[exePos]);

				if((pid = fork()) < 0){
					fprintf(stderr, "fork() error on command\n");
					exit(1);
				}
				else if(pid == 0){ // 자식 프로세스
					fprintf(stderr, "pid[%d]/ppid[%d]\n", getpid(), getppid());
					fprintf(stderr, "command > %s\n", command);

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
