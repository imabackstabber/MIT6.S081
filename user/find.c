#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


void
find(char *path,char* fname)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type != T_DIR){
    printf("find: need to find in T_DIR\n");
    exit(1);
  }
  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
    printf("find: path too long\n");
    exit(1);
  }
  strcpy(buf, path);
  p = buf+strlen(buf);
  *p++ = '/';
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if(stat(buf, &st) < 0){
      printf("ls: cannot stat %s\n", buf);
      continue;
    }
    if(st.type == T_DIR && strcmp(p,"..") != 0 && strcmp(p,".") != 0){
	find(buf,fname);
    } else if(st.type != T_DIR && strcmp(p,fname) == 0){
	printf("%s\n",buf);
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{

  if(argc != 3){
    fprintf(1,"usage: find <DIR> <FNAME>\n");
    exit(0);
  }
  find(argv[1],argv[2]);
  exit(0);
}
