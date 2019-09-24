#define STRCMP(a, b) c_mem_cmp((a), sizeof((a)), (b))
#define IOQSIZ (sizeof(ctype_arr) + sizeof(ctype_ioq))

/* archive routines */
int archive(char *, char **);
int archivefd(int, char **);
int unarchive(char *);
int unarchivefd(int);

/* utils routines */
ctype_ioq *ioqfd_new(ctype_fd, ctype_iofn);
ctype_ioq *ioq_new(char *, uint, uint);
ctype_arr *getln(ctype_ioq *);
void assign(char **, char *);
char *concat(char *, char *);
char *sdup(char *, uint);
char **avmake2(char *, char *);
char **avmake3(char *, char *, char *);
int makepath(char *);
int destroypath(char *, usize);
