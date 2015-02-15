#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "shell_functions.h"
#define ARG_LIMIT 1000

void do_execute(char** tokens, char** envpp) {
	char pathPrefix[250];
	char *current, *next;
	// this is what Input has to change.execve(newargs[0], newargs, NULL);
	execve(tokens[0], tokens, envpp);
	char* path = getEnv("PATH=", envpp);
	current = path;
	//printf("%s",path);
	while (1) {
		next = strchr(current, ':');
		if (next != NULL) {
			strcpybtwptrs(current, next, pathPrefix);
			strcat(pathPrefix, "/");
			strcat(pathPrefix, tokens[0]);
			execve(pathPrefix, tokens, envpp);
			current = next + 1;
		} else {
			printf("command not found\n");
			exit(0);
		}
	}
}

void handleChildExec(char **tokens, char** envpp) {
	int status;
	pid_t child = fork();
	if (child >= 0) {
		if (child == 0) {
			do_execute(tokens, envpp);
		} else {
			waitpid(-1, &status, 0);
		}
	} else {
		printf("internal error unsuccessful fork");
		exit(0);
	}
}

void close_fds(int fds[]) {
	// close both file descriptors
	close(fds[0]);
	close(fds[1]);
}
void add_terminating_char(int fd_to) {
	if (fd_to == 1) {
		char buffer = '\0';
		write(fd_to, &buffer, 1);
	}
}

void handleChildPipeExec(char **tokens, char** envpp, int fds[], int fd_to) {
	int fd_from = fds[fd_to];
	pid_t child = fork();
	if (child >= 0) {
		if (child == 0) {
			dup2(fd_from, fd_to);
//			close_fds(fds);
			close(fds[1 ^ fd_to]);
			do_execute(tokens, envpp);
			add_terminating_char(fd_to);
			exit(0);
		} else {
// todo: check why no wait has t be done in this case
//			int status;
//			waitpid(-1, &status, 0);
		}
	} else {
		printf("internal error unsuccessful fork");
		exit(0);
	}
}

int is_safe_to_dup(int fds_tobe_dupd[]) {
	return (fds_tobe_dupd[0] != -1 && fds_tobe_dupd[1] != -1);
}

void safe_dup2(int fds_tobe_dupd[]) {
	if (is_safe_to_dup(fds_tobe_dupd)) {
		dup2(fds_tobe_dupd[0], fds_tobe_dupd[1]);
	}
}

void handleChildPipeExec2(char **tokens, char** envpp, int pipe_fd_to_close[],
		int n, int read_fds_tobe_dupd[], int write_fds_tobe_dupd[]) {
	pid_t child = fork();
	if (child >= 0) {
		if (child == 0) {
			safe_dup2(read_fds_tobe_dupd);
			safe_dup2(write_fds_tobe_dupd);
			for (int i = 0; i < n; i++)
				if (pipe_fd_to_close[i] != -1)
					close(pipe_fd_to_close[i]);
			do_execute(tokens, envpp);
			if (is_safe_to_dup(write_fds_tobe_dupd))
//				add_terminating_char(write_fds_tobe_dupd[1]);
				exit(0);
		} else {
// todo: check why no wait has t be done in this case
//			int status;
//			waitpid(-1, &status, 0);
		}
	} else {
		printf("internal error unsuccessful fork");
		exit(0);
	}
}

void handleGetenv(char **tokens, char *envpp[]) {
	char *cmd = tokens[1];
	if (cmd == NULL) {
		printEnviron(envpp);
		return;
	}
	if (tokens[2] != NULL) {
		printf("too many arguments to getenv\n");
		return;
	}
	char* getenvOutput = getEnv(cmd, envpp);
	if (getenvOutput[0] != '=') {
		printf("\n");
		return;
	}
	if (getenvOutput != NULL)
		printf("%s\n", getenvOutput + 1);
	else
		printf("\n");
}
/*
 * setenv: TESTED
 * more checks done:
 * if first only setenv is the command prints all the envvars
 * if one arg then the env variable is set to blank
 * if 2 deals with normally
 * solved the seg fault problem
 * Similar for getenv
 * NOT KNOWN: why does top= come in envpp, just run setenv <no arguments> and see!!
 */
char ** handleSetenv(char **tokens, char *envpp[]) {
	int envStrlen = 0, cmdlen = 0;
	char *envStr = NULL, *cmd = NULL;
	if (tokens[1] != NULL) {
		cmd = tokens[1];
		cmdlen = strlen(cmd);
	} else {
		printEnviron(envpp);
		return envpp;
	}

	if (tokens[2] != NULL) {
		envStr = tokens[2];
		envStrlen = strlen(envStr);
	}
	int size = cmdlen + envStrlen + 2;
	char* newStr = (char*) malloc(size * sizeof(char));
	strcpy(newStr, cmd);
	strcat(newStr, "=");
	if (envStr != NULL)
		strcat(newStr, envStr);
	envpp = setEnv(newStr, envpp);
	return envpp;
}

char ** take_action(char** tokens, char *envpp[]) {
	print_tokens(tokens);
	char *cmd = tokens[0];
	if (tokens[0] == NULL)
		return envpp;
	if (strcmp("setenv", cmd) == 0) {
		envpp = handleSetenv(tokens, envpp);
	} else if (strcmp("getenv", cmd) == 0) {
		handleGetenv(tokens, envpp);
	} else if (strcmp("cd", cmd) == 0) {
		chdir(tokens[1]); //todo: return status
	} else {
		handleChildExec(tokens, envpp);
	}
	return envpp;
}
/*
 * tested file handling when no file as such
 * when I pass multiple files, only first script is run
 * not checking comments in between coz its not needed.
 * NOT TESTED: proper scripts with pipe etc,
 * NOT TESTED: the individual functions it calls, check
 * function def for details of those tests.
 */
char** cmd_line_arg_case(char input[ARG_LIMIT], char* argv[], char* envpp[]) {
	int flag = 1;
//todo: check for size greater than ARG_LIMIT
	int fileHandle = open(argv[1], O_RDONLY);
	if (fileHandle < 0) {
		printf("Opening Script: Failed, No such Script exists\n");
		return envpp;
	}
	do {
		flag = read_line(input, fileHandle);
		char** tokens = advance_tokenize(input, ' ', '"');
		if (tokens[0] == NULL || strncmp(tokens[0], "#", 1) == 0)
			continue;
		envpp = take_action(tokens, envpp);
	} while (flag == 1);
	close(fileHandle);

	return envpp;
}
/*
 * FIXED BUG: setenv without arguments setenv with 1 argument.
 *
 */
void remove_trail_nl(char *input) {
	size_t len = 0;
	while (input[len] != '\n') {
		len++;
	}
	input[len] = '\0';
	printf("%s", input);
}
char** interactive_case(char input[ARG_LIMIT], char* envpp[]) {
	char ps1[] = "PS1=prompt>>";
	envpp = setEnv(ps1, envpp);
	while (1) {
		printf("%s", getEnv("PS1=", envpp));
		//printEnviron(envpp);
		read(0, input, 1000);
		remove_trail_nl(input);
		//scanf(" %1000[^\n]", input);
		if (strncmp(input, "exit", 4) == 0) {
			break;
		}
		char** tokens = advance_tokenize(input, ' ', '"');
		envpp = take_action(tokens, envpp);
		free_char_array(tokens);
		//free(tokens);
	}
	return envpp;
}

char ** process_main(int argc, char* argv[], char* envpp[]) {
	char input[ARG_LIMIT];
	if (argc >= 2) { //note it will run only the first script if it has more than one script as parameter. like normal shell
		envpp = cmd_line_arg_case(input, argv, envpp);
	} else {
		envpp = interactive_case(input, envpp);
	}
	return envpp;
}

//void handle_pipe(char **tokens, char * envpp[]) {
//	// partition **tokens based on '|' occurance 
//	char *p = *tokens;
//	int pipes = 0;
//	int MAX_PIPES = 10;
//	int max_tokens = 0;
//	
//	while(p != NULL) {
//		max_tokens++;
//		if(*p == '|' && *(p+1) != '\0') {
//			pipes++;
//			if(pipes > MAX_PIPES) {
//				printf("too many pipes passed. Only %d allowed", MAX_PIPES);
//				exit(1);
//			}
//		}
//		p++;
//	}
//
//	p = *tokens;
//	char *q = p;
//	char *subset_tokens[max_tokens];
//	int *all_filedes[pipes*2]; // all filedes arrays as part of singe huge array
//	for(int i = 0; p != NULL; i++, p++) {
//		subset_tokens[i++] = p;
//		if(*p == '|' && *(p+1) != '\0') { // pipe enountered
//			subset_tokens[i++] = NULL; // subset of tokens to be passed to execve
//			i = 0; // reset index for reuse
//			int *filedes = all_filedes + (i*2);
//		}
//	}
//}
//
void pipetest2(char *envpp[]) {
	char* ps = "/bin/ls";
	char *tokens1[] = { ps, NULL };
	char* less = "/usr/bin/wc";
	char *tokens2[] = { less, NULL };

	int filedes[2];
	int status = pipe(filedes);
//	int filedes2[2];
//	int status2 = pipe(filedes2);
//	printf("%d %d", status, status2);
	if (status == 0) {
		// write channel of filedes pointed to stdout of 1st child.
		int arr1[2] = { filedes[1], 1 };
		int arr1dash[2] = { -1, -1 };
		handleChildPipeExec2(tokens1, envpp, filedes, 2, arr1dash, arr1);
		// read channel of filedes pointed to stdin of 2nd child.
		int arr2[2] = { filedes[0], 0 };
		int arr2dash[2] = { -1, -1 };
		handleChildPipeExec2(tokens2, envpp, filedes, 2, arr2, arr2dash);

//		char *tokens3[] = { less, NULL };
//		int arr3[2] = { filedes[0], 0 };
//		int arr3dash[2] = { -1, -1 };
//		handleChildPipeExec2(tokens3, envpp, -1, arr3, arr3dash);

		int child_status;
		waitpid(-1, &child_status, 0);
//		printf("status was %d", child_status);
	} else {
		printf("error while piping");
	}
	close_fds(filedes);
}

void pipetest(char *envpp[]) {
	char* ps = "/bin/ls";
	char *tokens1[] = { ps, NULL };
	char* wc = "/usr/bin/wc";
	char *tokens2[] = { wc, NULL };
	char* wc1 = "/usr/bin/wc";
	char *tokens3[] = { wc1, NULL };

	int pipe1[2];
	int status = pipe(pipe1);

	int pipe2[2];
	pipe(pipe2);
	int newpipe[4] = { pipe1[0], pipe1[1], pipe2[0], pipe2[1] };

//	printf("%d %d", status, status2);
	if (status == 0) {
		// write channel of filedes pointed to stdout of 1st child.
		int reads1[2] = { -1, -1 };
		int writes1[2] = { pipe1[1], 1 };
		handleChildPipeExec2(tokens1, envpp, newpipe, 4, reads1, writes1);
		// read channel of filedes pointed to stdin of 2nd child.
		int reads2[2] = { pipe1[0], 0 };
		int writes2[2] = { pipe2[1], 1 };
		handleChildPipeExec2(tokens2, envpp, newpipe, 4, reads2, writes2);

		int reads3[2] = { pipe2[0], 0 };
		int writes3[2] = { -1, -1 };
		handleChildPipeExec2(tokens3, envpp, newpipe, 4, reads3, writes3);

		int child_status;
		waitpid(-1, &child_status, 0);

//		printf("status was %d", child_status);
	} else {
		printf("error while piping");
	}
	close_fds(pipe1);
	close_fds(pipe2);
}

//Support changing current directory ( cd ) -
//Execute binaries interactively
//Execute scripts
//Execute pipelines of binaries ( /bin/ls | /bin/grep test )
//Set and use PATH and PS1 variables

int main(int argc, char* argv[], char* envpp[]) {
//	char input[ARG_LIMIT];
//	envpp = process_main(argc, argv, envpp); //made input inside the process_main
//	return 0;				//and checking if it is script with 1 script only

	pipetest(envpp);
	return 0;
}

