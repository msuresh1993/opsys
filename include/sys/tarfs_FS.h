#ifndef __TARFS_FS_H

#define __TARFS_FS_H
#include<sys/defs.h>
#define MAX_FILES_SYSTEM 50
#define NAME_MAX 255
#define MAX_DIR 200

typedef struct{
	//stuff that an elf file needs
	struct posix_header_ustar *posix_header;
	char *current_pointer;
	uint64_t flags;
	uint64_t size;
	int used; // not needed
	//syncronization stuff
	int busy;//if the file is being used.
	int current_process;//if the file descriptor is shared then only 1 process can use it. busy and current process maintain the syncronization
	int ready;//variable to tell if buffer is ready
}file_desc_t;
file_desc_t *stdin_fd;
file_desc_t *stdout_fd;//also for error
struct global_fd_node{
	file_desc_t *fd;
	int count;
};
#define MAX_FILES_OS 200
typedef struct global_fd_node global_fd_node_t;
global_fd_node_t global_fd_array[MAX_FILES_OS];
void *input_buffer;
char *current_stdin_pointer;
struct dirent {
	long d_ino;
	uint64_t d_off;
	unsigned short d_reclen;
	char d_name[NAME_MAX + 1];
};
enum {
	O_RDONLY = 0,
	O_WRONLY = 1,
	O_RDWR = 2,
	O_CREAT = 0x40,
	O_EXCL = 0x80,
	O_DIRECTORY = 0x10000,

};
struct pipe_struct{
	file_desc_t *write_end;
	file_desc_t *read_end;
	uint64_t size;
	struct pipe_struct *next;
};
typedef struct pipe_struct pipe_struct_t;
uint64_t *find_file_tarfs(char *);
uint64_t open_tarfs(char *file_name, int flags);
uint64_t read_tarfs(int fd, void *buffer, uint64_t size, uint64_t stack_top);
void dents_tarfs(int fd, struct dirent *dirent_array, uint64_t size);
void init_tarfs();
int dup_tarfs(int fd);
int close_tarfs(int fd);
int dup2_tarfs(int fd_old, int fd_new);
int pipe_tarfs(int pipe[2]);
uint64_t write_syscall(int fd, void *buffer, uint64_t size, uint64_t stack_top);
#endif
