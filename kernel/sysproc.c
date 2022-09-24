#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fcntl.h"
#include "fs.h"
#include "file.h"


uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd, offset;
  struct file *f;
  if(argaddr(0, &addr) < 0) // should be always treated as NULL
    return -1;
  if(argint(1, &length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &offset) < 0)
    return -1;
  // check fd
  struct proc* p = myproc();
  if(fd < 0 || fd >= NOFILE || (f=p->ofile[fd]) == 0)
    return -1;
  // boarder checker
  int idx = -1;
  for(int i = 0;i < MAX_VMA;i++){
    if(p->vma_table[i].file == 0){
      idx = i;
      break;
    }
  }
  if(idx == -1) 
    return -1;
  // prot checker(only function when `MAP_SHARED` is set)
  if((flags & MAP_SHARED)&&((!f->readable && (prot & PROT_READ)) || (!f->writable && (prot & PROT_WRITE)))){
    return -1;
  }
  // write vma
  addr = p->sz;
  p->vma_table[idx].addr = p->sz;
  p->vma_table[idx].len = length;
  p->vma_table[idx].flags = flags; // MAP_SHARED / MAP_PRIVATE
  p->vma_table[idx].file = f;     // file pointer
  p->vma_table[idx].offset = offset;  // offset
  p->vma_table[idx].prot = prot;  // prot
  // p->vma_cnt += 1;          // add vma_cnt
  p->sz += length;          // this has been occupied
  // should filedup
  filedup(f);
  return addr; // return addr it's has been allocated
}

uint64
sys_munmap(void)
{
  uint64 addr;
  if(argaddr(0, &addr) < 0)
    return -1;
  int len;
  if(argint(1, &len) < 0)
    return -1;
  struct proc* p = myproc();
  int vma_idx = -1;
  for(int i = 0;i < MAX_VMA;i++){
    if(p->vma_table[i].file && addr >= p->vma_table[i].addr 
        && (addr + len) <= (p->vma_table[i].addr + p->vma_table[i].len)){
          vma_idx = i;
          break; // should break
    }  
  }
  // write-back checker
  if((p->vma_table[vma_idx].flags & MAP_SHARED) && (p->vma_table[vma_idx].prot & PROT_WRITE)){
    filewrite(p->vma_table[vma_idx].file, addr, len); // write back then
  }
  // please check whether file is fully released
  if(p->vma_table[vma_idx].len <= len){
    // it's fully released
    fileclose(p->vma_table[vma_idx].file); // file close
    p->vma_table[vma_idx].file = 0; // cancel mapping
  } else{
    // just change vma_table item
    // there will only be 2 possibilities
    if(p->vma_table[vma_idx].addr == addr){
      // it's at head
      p->vma_table[vma_idx].addr = addr + len;
    }     
    // it's at tail
    p->vma_table[vma_idx].len -= len;
  }
  // then unmap the page included
  uint64 va = PGROUNDUP(addr);
  uint64 npages = len / PGSIZE;
  uvmunmap(p->pagetable, va, npages, 1);
  return 0;
}