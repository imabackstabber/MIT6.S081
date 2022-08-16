#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf = 'b';
  int ping[2];
  int pong[2];
  pipe(ping);
  pipe(pong);
  int state = fork();
  
  if(state < 0){
    fprintf(2,"fork() failed\n");
    close(ping[0]);
    close(ping[1]);
    close(pong[0]);
    close(pong[1]);
    exit(1);
  }else if(state == 0){
    close(ping[1]);
    close(pong[0]);
    if(read(ping[0],&buf,sizeof(char)) != sizeof(char)){
        fprintf(2,"child read fail");   
        close(ping[0]);
        close(pong[1]);
        exit(1);
    }
    close(ping[0]);
    fprintf(1,"%d: received ping\n",getpid());
    if(write(pong[1],&buf,sizeof(char)) != sizeof(char)){
        fprintf(2,"child write fail");   
        close(pong[1]);
        exit(1);
    }
    close(pong[1]);
    exit(0);
  }else{
    close(ping[0]);
    close(pong[1]);
    if(write(ping[1],&buf,sizeof(char)) != sizeof(char)){
        fprintf(2,"parent write fail");   
        close(ping[0]);
        close(ping[1]);
        close(pong[0]);
        close(pong[1]);
        exit(1);
    }
    close(ping[1]); 
    if(read(pong[0],&buf,sizeof(char)) != sizeof(char)){
        fprintf(2,"parent read fail");   
        close(pong[0]);
        exit(1);
    }
    fprintf(1,"%d: received pong\n",getpid());
    close(pong[0]);
    exit(1);
  }
}
