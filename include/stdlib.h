#ifndef _STDLIB_H
#define _STDLIB_H
#include<errno.h>
#include <sys/defs.h>

//extern __thread int errno;

void exit(int status);

// memory
//typedef uint64_t size_t;
void *malloc(size_t size);
void free(void *ptr);
int brk(void *end_data_segment);
uint64_t get_brk(uint64_t size);
// processes
//typedef uint32_t pid_t;
struct timespec {
	time_t tv_sec;
	long tv_nsec;

};
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
int execve(const char *filename, char * const argv[], char * const envp[]);
pid_t waitpid(pid_t pid, int *status, int options);
unsigned int sleep(unsigned int seconds);
unsigned int alarm(unsigned int seconds);
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
// paths
char *getcwd(char *buf, size_t size);
int chdir(const char *path);

// files

enum {
	O_RDONLY = 0,
	O_WRONLY = 1,
	O_RDWR = 2,
	O_CREAT = 0x40,
	O_EXCL = 0x80,
	O_DIRECTORY = 0x10000,

};
int open(const char *pathname, int flags);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
enum {
	SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2
};
off_t lseek(int fildes, off_t offset, int whence);
int close(int fd);
int pipe(int filedes[2]);
int dup(int oldfd);
int dup2(int oldfd, int newfd);

// directories
#define NAME_MAX 255
struct dirent {
	long d_ino;
	uint64_t d_off;
	unsigned short d_reclen;
	char d_name[NAME_MAX + 1];
};
struct dir {
	struct dirent *start;
	struct dirent *current;
	int fd;
};
typedef struct dir DIR;
void *opendir(const char *name);
struct dirent *readdir(void *dir);
int closedir(void *dir);
//string functions
size_t strlen(char *str);
void errorHandler(int errorCode);

int process_state();
int kill(pid_t pid);
#endif
