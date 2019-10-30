#define STRCMP(a, b) c_mem_cmp((a), sizeof((a)) - 1, (b))

/* archive routines */
int archive(char *, char **);
int archivefd(int, char **);
int unarchive(char *);
int unarchivefd(int);

/* utils routines */
char **avmake3(char *, char *, char *);
char *concat(char *, char *);
char *sdup(char *, uint);
ctype_arr *getln(ctype_ioq *);
ctype_ioq *new_ioqfd(ctype_fd, ctype_iofn);
int checksum_fletcher32(char *, char *, u64int);
void checksum_whirlpool(ctype_fd fd, char *);
int destroypath(char *, usize);
int makepath(char *);
size eioqget(ctype_ioq *, char *, usize);
vlong estrtovl(char *, int, vlong, vlong);
void assign(char **, char *);
void efchdir(ctype_fd);
void eioqgetall(ctype_ioq *, char *, usize);
void dofetch(ctype_fd, char *);
void uncompress(ctype_fd, ctype_fd);

/* main routines */
int ar_main(int, char **);
int cksum_main(int, char **);
int venus_main(int, char **);

/* global variables */
extern int fd_dot;
extern char *fetch;
extern char *inflate;
