#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int get_left_first(int* left,int* first){
    if(read(left[0],first,sizeof(int)) != 0){
	fprintf(1,"prime %d\n",*first);
	return 0;
    }
    return -1;
}

void transmit_data(int *left,int *p,int first){
    int num;
    close(p[0]); // it won't use read port of right child
    while(read(left[0],&num,sizeof(int)) != 0){
	if(num % first){
	  write(p[1],&num,sizeof(int));
	}
    }
    close(left[0]);
    close(p[1]);
}

void prime(int* left){
  close(left[1]); // it don't use write port of left child
  int first;  
  if(get_left_first(left,&first) == 0){
     int p[2];
     pipe(p);
     
     if(fork() == 0){
	prime(p);
     } else{
	transmit_data(left,p,first);
        wait(0);
     } 
  }
}

int
main(int argc, char *argv[])
{
  int p[2];
  
  pipe(p);
  for(int i = 2;i <= 35;i++){
    write(p[1],&i,sizeof(int));
  }
  
  if(fork() == 0){
    prime(p);
  }else{
    close(p[0]);
    close(p[1]);
    wait(0);
  }
  exit(0);
}
