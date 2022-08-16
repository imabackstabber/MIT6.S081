#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXSZ 512

enum state{
  S_WAIT, 
  S_ARG,
  S_ARG_END,
  S_ARG_LINE_END,
  S_LINE_END,
  S_END
};

enum state state_transfer(enum state cur,char ch){
    switch(cur){
      case S_WAIT:
	if (ch == ' ') return S_WAIT;
        if (ch == '\n') return S_LINE_END;
        return S_ARG;
	break;
      case S_ARG:
	if (ch == ' ') return S_ARG_END;
	if (ch == '\n') return S_ARG_LINE_END;
        return S_ARG;
	break;
      case S_ARG_END:
      case S_ARG_LINE_END:
      case S_LINE_END:
	if (ch == ' ') return S_WAIT;
	if (ch == '\n') return S_LINE_END;
	return S_ARG;
	break;
      default:
	break;
    }
    return S_END;
}

int
main(int argc, char* argv[]){
  if(argc - 1 >= MAXARG){
    fprintf(2,"xargs: too many arguments.\n");
    exit(1);
  } 
  char lines[MAXSZ] = {0};
  char* p = lines;
  char *xargv[MAXARG] = {0};
  
  for(int i = 1;i < argc;i++){
    xargv[i-1] = argv[i];
  }
  int begin = 0;
  int end = 0;
  int cnt = argc - 1; 
  enum state st = S_WAIT;

  while(st != S_END){
    if(read(0,p,sizeof(char)) != sizeof(char)){
      st = S_END;
    }else{
      st = state_transfer(st,*p);
    }
    
    if(++end >= MAXSZ){
      fprintf(2, "xargs: arguments too long.\n");
      exit(1);
    }

    switch(st){
      case S_WAIT:
        ++begin;
	break;
      case S_ARG_END:
	xargv[cnt++] = &lines[begin];
	begin = end;
        *p = '\0';
	break;
      case S_ARG_LINE_END:
        xargv[cnt++] = &lines[begin];
      case S_LINE_END:
	begin = end;
        *p = '\0';
	if(fork() == 0){
	  exec(argv[1],xargv);
	}
        cnt = argc - 1;
        for(int i = cnt;i < MAXARG;i++){
	  xargv[i] = 0;
	}
	wait(0);
      default:
	break;
    }
    ++p;
  }
  
  exit(0);
}
