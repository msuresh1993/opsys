#ifndef _STDLIB_C
#define _STDLIB_C
#include<syscall.h>
//#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#define ALIGNMENT 16
#define DEFAULT_MODE 00744
//typedef uint64_t size_t;
//typedef int32_t pid_t;
extern __thread int errno;

struct blockHeader {
	uint64_t size;
	struct blockHeader *next;

};
//does not have an errno to set accouring to gnu standard: http://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
void exit(int status) {
	syscall_1(SYS_exit, status);
}
//check if we have to do PIPE ERRORS: EAGAIN if O_NONBLOCK is set,2)if interupted by a signal. http://linux.die.net/man/3/read
//EBADMSG ?? LOT OF ERRNOS did not check
ssize_t read(int fd, void *buf, size_t count) {
	size_t size = syscall_4_write(SYS_read, fd, buf, count);//we could change this to use the generic syscall_4 but why break.
	if(size == 0xFFFFFFFFFFFFFFF7){
		errno = EBADF;
		return -1;
	}
	else if((signed long)size <0){
		errno = READERR;
		return -1;
	}
	//if(size == 0x)
	return size;
}
//write same issues as read
size_t write(int fd, const void *buf, size_t count) {
	/*
	 __asm__ __volatile__ (
	 "movq $1, %%rax\n\t"
	 "movq $1, %%rdi\n\t"
	 "movq %0, %%rsi\n\t"
	 "movq $1, %%rdx\n\t"
	 "syscall"
	 :
	 :"r"(hello)
	 :"rax", "rdi", "rsi", "rdx", "memory"
	 );*/
	size_t size = syscall_4_write(SYS_write, fd, buf, count);
	if(size == 0xFFFFFFFFFFFFFFF7){
		errno = EBADF;
		return -1;
		}
	else if((signed long)size <0){
		errno = WRITEERR;
		return -1;
	}
	return size;
}
//no errors for strlen
size_t strlen(const char *str) {
	size_t current = 0;
	size_t i = 0;
	while (str[i] != '\0') {
		i++;
		current++;
	}
	return current;
}

int printInteger(int n, int check) {
	if (n == 0){
		if(check == 0){
			char *zeropointer;
			char zero = 48;
			zeropointer = &zero;
			write(1, zeropointer, 1);
		}
		return 0;
	}
	int temp = n;
	int rem;
	char *apointer, a;
	rem = temp % 10;
	a = 48 + rem;
	apointer = &a;
	int prevcount = printInteger(temp / 10, 1);
	write(1, apointer, 1);
	return prevcount + 1;

}
//do a check for errors after complete function
int printf(const char *format, ...) {
	va_list val;
	int printed = 0;

	va_start(val, format);

	while (*format) {
		if (*format == '%') {
			++format;
			if (*format == '%') {
				write(1, format, 1);
				++printed;
				++format;
			} else if (*format == 'd') {
				int tempd = va_arg(val, int);
				int count = printInteger(tempd, 0);
				printed = printed + count;
				++format;
			} else if (*format == 's') {
				char *temps = va_arg(val, char *);
				int length = strlen(temps);
				write(1, temps, length);
				printed = printed + length;
				++format;
			}
			/*
			 else if(*format == 'p'){
			 void *tempp = va_arg(val,void *);

			 }*/
		} else {
			write(1, format, 1);
			++printed;
			++format;
		}
	}
	va_end(val);
	return printed;
}

int is_digit(char c) {
	int ascii = c - '0';
	return (ascii >= 0 && ascii <= 9);
}

int atoi(char buffer[], char* s, char * e) {
	char *temp = (e - 1);
	if (*s == *e)
		return 0;
	int sum = 0, pow = 1;
	while ((temp + 1) != s) {
		char c = *temp;
		sum += ((c - '0') * pow);
		pow *= 10;
		temp--;
	}
	return sum;
}
//same as printf
int scanf(const char *format, ...) {
	int scanned = 0;
	int limit = 1000;
// static since scanf unread input across calls
	static char buffer[1000];
	static char *buffer_ptr = buffer;

	read(0, buffer, limit);

	va_list val;
	va_start(val, format);
	while (*format) {
		if (*format == '%') {
			format++;
			switch (*format) {
			case 'd':
				format++;
				scanned++;
				char* temp = buffer_ptr;
				while (buffer_ptr && is_digit(*buffer_ptr)) {
					buffer_ptr++;
				}
				int *int_ptr = va_arg(val, int *);
				*int_ptr = atoi(buffer, temp, buffer_ptr);
				break;
			case 's':
				format++;
				scanned++;
				break;
			case 'x':
				format++;
				scanned++;
				break;
			case 'c':
				format++;
				scanned++;
				break;
			default:
				break;
			}
		} else {
			return 0;
		}
	}
	va_end(val);
	return scanned;
}

uint64_t get_brk(uint64_t size) {
	return syscall_1_p(SYS_brk, size);

}
//brk done returns -1 on failure and sets ENOMEM
int brk(void *end_data_segment){
	uint64_t memoryStart = get_brk(0);
	uint64_t returnBrk = get_brk((uint64_t) (end_data_segment));//todo: does get_brk return a value?? adding here: in our shell see if we free all mallocs
		//heap overflow check.
	if(returnBrk == memoryStart){
		errno =ENOMEM;
		return -1;
	}
	return 0;

}
//test function
void printAllocmemory(struct blockHeader *head) {
	struct blockHeader *current = head;
	while (current != NULL) {
		printf("%d %d %d|||||", current, current->size, current->next);
		current = current->next;
	}
	printf("\n");
}
void *findBest(struct blockHeader *head, uint64_t size) {
	if (head == NULL)
		return NULL;
	struct blockHeader *current = head;
	uint64_t snug = ~0x0;
	void *ptr = NULL;
	while (current != NULL) {
		if ((((current->size) & 0x1) == 0)
				&& ((current->size) & (0xFFFFFFFFFFFFFFFE)) >= size) {
			if (snug > (current->size) - size) {
				snug = (current->size) - size;
				ptr = current;
			}
		}
		current = current->next;
	}
	return ptr;
}
//malloc set errno for EMEM
void *malloc(uint64_t size) {
	static struct blockHeader *head = NULL;
	static struct blockHeader *tail = NULL;
	uint64_t memSize = (size + sizeof(struct blockHeader) + (ALIGNMENT - 1))
			& ~(ALIGNMENT - 1); //Source:cracking the coding interview: page 247
//best fit algorithm to use the empty blocks in the middle of the heap
	void *loc = findBest(head, memSize);
	if (loc != NULL) {
		struct blockHeader *metaData = (struct blockHeader *) loc;
		metaData->size = memSize;
		metaData->size=(metaData->size)|1;
		void *returnAddress = (void *)((uint64_t)loc+sizeof(struct blockHeader));
		printAllocmemory(head);
		return returnAddress;
	}
//end of best fit

	uint64_t memoryStart = get_brk(0);
	printf("MEMORY START:%d\n",memoryStart);
	//printf("memsize:%d  %d\n",memSize,memoryStart);
	uint64_t newBrk = (uint64_t) (memoryStart + memSize);//todo: does get_brk return a value?? adding here: in our shell see if we free all mallocs
	//heap overflow check.
	int retBrk = brk((void *)newBrk);
	if(retBrk== -1){
		if(errno == ENOMEM){
			//errno == ENOMEM;
			printf("\nmem. Alloc failed. Out of Memory");
			return NULL;
		}

	}
	//printf("RRRRETURNBRK:%d\n",returnBrk);
	//printf("%d\n",get_brk(0));
	struct blockHeader *metaData = (struct blockHeader*) memoryStart;
	metaData->size = memSize;
	metaData->next = NULL;
	metaData->size = (metaData->size) | 1; //flag for whether it is a valid address.
	if (head == NULL) {
		head = metaData;
		tail = metaData;
	} else {
		tail->next = metaData;
		tail = metaData;
	}
	void *returnAddress = (void *) (memoryStart + sizeof(struct blockHeader));
	printAllocmemory(head);
	return returnAddress;

}
//does not have error value, linux defines illegal pointer access as undefined.
void free(void *ptr) {

	struct blockHeader *current = (struct blockHeader *) ((uint64_t) (ptr)
			- sizeof(struct blockHeader));
	current->size = (current->size) & 0xFFFFFFFFFFFFFFFE;
}
char *strcpy(char *dst, char *src) {
	size_t len = 0;
	while(src[len] != '\0'){
		dst[len] = src[len];
		len++;
	}
	dst[len] = '\0';
	return dst;
}


size_t strncmp(char *string1, char *string2, int n) {
	size_t len = 0;
	while(len<n && string1[len] != '\0' && string2[len] != '\0' ){
		if(string1[len] != string2[len])
			break;
		len++;
	}
	if((len == n) || (string1[len] == string2[len]))
		return 0;
	else if(string1[len] >string2[len])
		return (size_t)string1[len];
	else
		return (size_t)string2[len];
}
//source:mode values taken from linux man pages. http://man7.org/linux/man-pages/man2/open.2.html
enum{
	S_IRWXU = 00700,
	S_IRUSR = 00400,
	S_IWUSR = 00200,
	S_IXUSR = 00100,
	S_IRWXP = 00070,
	S_IRGRP = 00040,
	S_IWGRP = 00020,
	S_IRWXG = 00001,
	S_IRWXO = 00007,
	S_IROTH = 00004,
	S_IWOTH = 00002,
	S_IXOTH = 00001
};
//
size_t open(const char *filename, int permission) {
	unsigned short mode;
	if((permission & 0x40)!=0)
		 mode = DEFAULT_MODE; //todo:verify the mode. default rwx
	uint64_t result =  syscall_3(SYS_open, (uint64_t)filename, (uint64_t)permission,(uint64_t)mode);
	if((signed long)result == -EACCES){//checked
		errno =EACCES;//premission denied to access the file
		return -1;
	}
	else if((signed long)result == -ENOENT){//checked
		errno = ENOENT;//file or directory does not exist
		printf("file or directory not present");
		return -1;
	}
	else if((signed long)result == -EEXIST){//checked
		errno = EEXIST;//file already exists returns when using O_CREATE
		printf("file exists");
		return -1;
	}
	else if((signed long)result == -EDQUOT){
		errno = EDQUOT;//quota of open files exceeded by user.
		return -1;
		}
	else if((signed long)result == -EFAULT){
		errno = EFAULT;//bad address (Accessing illegal memory).
		return -1;
	}
	else if((signed long)result == -EFBIG){
		errno = EFBIG;//file we are reading is too large
		return -1;
	}
	else if((signed long)result == -EINTR){
		errno = EINTR;//if a signal handler interrupts an open
		return -1;
	}
	else if((signed long)result == -EISDIR){
			errno = EISDIR;//happens if the program wanted to read or write a directory.
			return -1;
			}
	else if((signed long)result == -ELOOP){
		errno = ELOOP;//too many sybolic links to follow.
		return -1;
	}
	else if((signed long)result == -EMFILE){//checked
		errno = EMFILE;//too many files open by process
		return -1;
	}
	else if((signed long)result == -ENAMETOOLONG){
			errno = ENAMETOOLONG;//too many files open by process
			return -1;
		}
	else if((signed long)result == -ENFILE){
		errno = ENFILE;//the device address does not exist
		return -1;
	}
	else if((signed long)result == -ENODEV){
			errno = ENODEV;//too many files open by process
			return -1;
		}
	else if((signed long)result == -ENOENT){
			errno = ENOENT;//too many files open by process
			return -1;
		}
	else if((signed long)result == -ENOMEM){
		errno = ENOMEM;//insufficient kernel are available
		return -1;
	}
	else if((signed long)result == -ENOSPC){
		errno = ENOSPC;//no way to create pathname as no space for file
		return -1;
	}
	else if((signed long)result == -ENOTDIR){//checked
		errno = ENOTDIR;//pathname not a dir, but O_DIR specified
		return -1;
	}
	else if((signed long)result == -ENXIO){
		errno = ENXIO;// no file is open for reading
		return -1;
	}
	else if((signed long)result == -EOVERFLOW){
		errno = EOVERFLOW;//file is too large to open
		return -1;
	}
	else if((signed long)result == -EPERM){
		errno = EPERM;
		return -1;
	}
	else if((signed long)result == -EROFS){
		errno = EROFS;//pathname refers to a file read-only, write access requested
		return -1;
	}
	else if((signed long)result == -ETXTBSY){
		errno = ETXTBSY;//an executable is being executed and write access req
		return -1;
	}
	else if((signed long)result == -EWOULDBLOCK){
		errno = EWOULDBLOCK;//too many files open by process
		return -1;
	}
	else if((signed long)result <0 ){
		errno = OPENERROR;
		printf("\nopen error");
		return -1;
	}
	return result;
}

pid_t fork(void) {
	pid_t result;
	result = syscall_0(SYS_fork);
	if((pid_t)result == -EAGAIN){
		errno = EAGAIN;
		return -1;
	}
	else if((pid_t)result == -EAGAIN){
		errno = EAGAIN;
		return -1;
	}
	else if((pid_t)result == -ENOMEM){
		errno = ENOMEM;
		return -1;
	}
	return result;
}
int execve(const char *filename, char * const argv[], char * const envp[]) {
	int64_t result;
	result = syscall_3(SYS_execve, (uint64_t) filename, (uint64_t) argv,
			(uint64_t) envp);
	if(result == -E2BIG){
		errno = E2BIG;
		return -1;
	}
	else if(result == -EACCES){
		errno = EACCES;
		return -1;
	}
	else if(result == -EFAULT){
		errno = EFAULT;
		return -1;
	}
	else if(result == -EIO){
		errno = EIO;
		return -1;
	}
	else if(result == -ELOOP){
		errno = ELOOP;
		return -1;
	}
	else if(result == -EMFILE){
		errno = EMFILE;
		return -1;
	}
	else if(result == -ENAMETOOLONG){
		errno = ENAMETOOLONG;
		return -1;
	}
	else if(result == -ENFILE){
		errno = ENFILE;
		return -1;
	}
	else if(result == -ENOENT){
		errno = ENOENT;
		return -1;
	}
	else if(result == -ENOMEM){
		errno = ENOMEM;
		return -1;
	}
	else if(result == -ENOEXEC){
		errno = ENOEXEC;
		return -1;
	}
	else if(result == -ENOTDIR){
		errno = ENOTDIR;
		return -1;
	}
	else if(result == -EPERM){
		errno = EPERM;
		return -1;
	}
	else if(result == -ETXTBSY){
		errno = ETXTBSY;
		return -1;
	}
	errno =EXECVEERROR;
	return -1;
}

char *strchr(const char *s, int c) {
	char *current =(char *)s;
	while(*current != '\0' && *current!= c){
		current++;
	}
	if(*current == c)
		return current;
	return NULL;
}

char *strcat(char *dest, const char *src) {
	size_t length = strlen(dest);
	size_t len = 0;
	while(src[len] != '\0'){
		dest[length + len] = src[len];
		len++;
	}
	dest[length+len] = '\0';
	return dest;
}

pid_t waitpid(pid_t pid, int *stat_loc, int options) {
//struct ruseage info;
	int result;
	result =  syscall_4_wait(SYS_wait4, pid, stat_loc, options);
	if(result <0){
		if(result == -ECHILD){
			errno = ECHILD;
			return -1;
		}
		if(result == -EINTR){
			errno = EINTR;
			return -1;
		}
		if(result == -EINVAL){
			errno = EINVAL;
			return -1;
		}
	}
	return result;
}

pid_t getppid(void) {
	return syscall_0(SYS_getppid);
}

pid_t getpid(void) {
	return syscall_0(SYS_getpid);
}

int chdir(const char* path) {
	int64_t result;
	result =  syscall_1(SYS_chdir, (uint64_t) path);
	if(result <0){
		if(result == -EACCES){
			errno = EACCES;
			return -1;
		}
		else if(result == -EFAULT){
			errno = EFAULT;
			return -1;
		}
		else if(result == -EIO){
			errno = EIO;
			return -1;
		}
		else if(result == -ELOOP){
			errno = ELOOP;
			return -1;
		}
		else if(result == -ENAMETOOLONG){
			errno = ENAMETOOLONG;
			return -1;
		}
		else if(result == -ENOENT){
			errno = ENOENT;
			return -1;
		}
		else if(result == -ENOMEM){
			errno = ENOMEM;
			return -1;
		}
		else if(result == -ENOTDIR){
			errno = ENOTDIR;
			return -1;
		}
	}
	return 0;
}

int close(int handle) {
	int64_t result;
	result = syscall_1(SYS_close, (uint64_t) handle);
	if(result <0){
		if(result == -EBADF){
			errno = EBADF;
			return -1;
		}
		else if(result == -EINTR){
			errno = EINTR;
			return -1;
		}
		else if(result == -EIO){
			errno = EIO;
			return -1;
		}
	}
	return 0;
}

int dup2(int oldfd, int newfd) {
	return 0;
}

int pipe(int pipefd[2]) {
	return 0;
}
#define NAME_MAX 255
#define MAX_DIR 200
struct dirent {
	long d_ino;
	uint64_t d_off;
	unsigned short d_reclen;
	char d_name[NAME_MAX + 1];
};
struct dir{
	struct dirent *current;
	int fd;
};
typedef struct dir DIR;
void *opendir(const char *name){

	struct dir *returnVal = (struct dir *)malloc(sizeof(struct dir));
	int fd = open(name,0|0x10000);
	if((signed long)fd < 0){
		returnVal = NULL;//errno has been set by open
		return returnVal;
	}
	struct dirent *direntries = (struct dirent *)malloc(MAX_DIR*sizeof(struct dirent));
	syscall_3((uint64_t)SYS_getdents,(uint64_t)fd,(uint64_t)direntries, MAX_DIR*sizeof(struct dirent));
	//struct dirent *temp = (struct dirent *)(direntries);
	//printf("2STR1:%s \n",temp->d_name);
	//printf("2STR1:%d \n",temp->d_off);
	//printf("2STR1:%d \n",temp->d_reclen);

	//struct dirent *temp1 = (struct dirent *)((uint64_t)temp + (uint64_t)temp->d_reclen); //check if you want to use offset
		//printf("2STR1:%s\n",temp1->d_name);

	returnVal->current = direntries;
	returnVal->fd = fd;
	return returnVal;
}
struct dirent *readdir(void *dir){
	DIR *dentry = (DIR *)dir;
	if(dentry == NULL){
		errno = EBADF;
		return NULL;
	}
	struct dirent *returnDirent = dentry->current;
	if(dentry->current->d_reclen == 0)
		return NULL;
	dentry->current = (struct dirent *)((uint64_t)dentry->current+ (uint64_t)dentry->current->d_reclen);
	return returnDirent;
}
int closedir(void *dir){

	DIR *dentry = (DIR *)dir;
	if(dentry == NULL){
		errno = EBADF;
		return -1;
	}
	free(dentry->current);
	int check =  close(dentry->fd);
	if(check <0){
		errno = EBADF;
		return -1;
	}
	return 0;
}
#endif