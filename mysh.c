#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>

void sigchld(int sig) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

char* quotes(char* str) {
	if(str[0] == '"' && str[strlen(str) - 1] == '"') {
		str[strlen(str) - 1] = '\0';
		return str + 1;
	}
	return str;
}

int tokenize(char* buf, char* delims, char* tokens[], int maxTokens, char* pipe1[], char* pipe2[]) {
	int count = 0;
	char *token = strtok(buf, delims);
	int type = 0;

	while(token != NULL && count < maxTokens) {
		token = quotes(token);
		tokens[count] = token;

		if(strcmp(tokens[count], "|") == 0) {
			type = 1;
			int i, j = 0;
			for(i = 0; i < count; i++) {
				pipe1[i] = tokens[i];
			}
			pipe1[count] = (char *) 0;
			while(i++ < maxTokens && tokens[i] != NULL) {
			       pipe2[j++] = tokens[i];
		 	}
			pipe2[j] = (char *) 0;
		}
		else if(strcmp(tokens[count], ">") == 0) {
			type = 2;
		}
		else if(strcmp(tokens[count], ">>") == 0) {
			type = 3;
		}
		if(strcmp(tokens[count], "&") == 0) {
			type = 4;
			tokens[count] = NULL;
		}
		count++;
		token = strtok(NULL, delims);
	}
	tokens[count] = NULL;
	return type;
}

bool cmd_help() {
        printf("/**********simple shell**********/\n");
        printf("<Internal command>\n");
        printf("quit : quit the shell\n");
        printf("exit : quit the shell\n");
        printf("help : Show the help of the commands\n");
        printf("<External command>\n");
        printf("ls : Display a list of files and folders in the directory\n");
        printf("cat : Print the contents of a file or combine files\n");
        printf("rm : Delete a file or directory\n");
        printf("There are a variety of other external commands.\n");
        printf("/********************************/\n");

        return true;
}

void redirection(int type, char *tokens[]) {
	int fd;
	int i;
	for(i = 0; tokens[i] != NULL; i++) {
		if(strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) {
			break;
		}
	}
	
	if(tokens[i] == NULL) return;
	tokens[i] = NULL;
	
	pid_t pid = fork();
	if(pid == 0) {
		if(type == 2) {
			fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
		}
		else if(type == 3) {
			fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
		}

		if(fd < 0) {
			printf("Can't open %s file\n", tokens[i + 1]);
			exit(1);
		}

		dup2(fd, STDOUT_FILENO);
		close(fd);
		execvp(tokens[0], tokens);
		printf("execvp failed\n");
		exit(1);
	}
	else {
		wait(NULL);
	}
}

void pipepipe(char* pipe1[], char* pipe2[]) {
	int fd[2];
	pid_t pid1, pid2;
	if(pipe(fd) == -1) {
		printf("pipe failed\n");
		return;
	}

	pid1 = fork();
	if(pid1 == 0) {
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]);
		execvp(pipe1[0], pipe1);
		printf("execvp failed\n");
		exit(1);
	}
	
	pid2 = fork();
	if(pid2 == 0) {
		dup2(fd[0], STDIN_FILENO);
		close(fd[1]);
		close(fd[0]);
		execvp(pipe2[0], pipe2);
		printf("execvp failed\n");
		exit(1);
	}

	close(fd[0]);
	close(fd[1]);
	waitpid(pid1, NULL, 0);
	waitpid(pid2, NULL, 0);
}

bool run(char* line) {
	char* tokens[100];
	char* pipe1[100];
	char* pipe2[100];
	int type = tokenize(line, " \n", tokens, 100, pipe1, pipe2);
	if(tokens[0] == NULL) {
		return true;
	}

	if(strcmp(tokens[0], "exit") == 0 || strcmp(tokens[0], "quit") == 0) {
		return false;
	}

	if(strcmp(tokens[0], "help") == 0) {
		cmd_help();
		return true;
	}

	if(type == 0) {
		pid_t pid = fork();
		
		if(pid == 0) {
			execvp(tokens[0], tokens);
			printf("execvp failed\n");
			return true;
		}
		
		else {
			wait(NULL);
		}
	}

	else if(type == 1) {
		pipepipe(pipe1, pipe2);
		return true;
	}

	else if(type == 2 || type == 3) {
		redirection(type, tokens);
		return true;
	}

	else if(type == 4) {
		pid_t pid = fork();

		if(pid == 0) {
			execvp(tokens[0], tokens);
			printf("execvp failed\n");
			exit(1);
		}
		else {
			printf("[1] %d\n", getpid());
		}
	}

	return true;

}

int main() {
	signal(SIGCHLD, sigchld);

	char line[1024];
	char cwd[PATH_MAX];
	while(1) {
		getcwd(cwd, sizeof(cwd));
		printf("%s $ ", cwd);
		fgets(line, sizeof(line) - 1, stdin);
		if(run(line) == false) break;
	}

	return 0;
}
