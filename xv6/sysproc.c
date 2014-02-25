#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "traps.h"


int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;
  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


int sys_shmget(void)
{
  int token;
  unsigned long va_ptr;
  uint size;
  struct proc *p;
  pte_t *pte;
  uint pa;
  int i =0;

  if(argint(0,&token)<0 ||
     argulong(1,&va_ptr) < 0 ||
     arguint(2,&size) < 0){
    
    cprintf("Error getting vars in shmget()\n");
    return -1;
  }

  if(va_ptr >= KERNBASE && (va_ptr+size) >= KERNBASE && (va_ptr % PGSIZE)!=0){
    cprintf("Memory out of range or not modulo pagesize\nx");
    return -1;
  }



  //Allocating
  if(size != 0){
    if(allocAt(proc->pgdir,size,va_ptr)<0){
      cprintf("Error in shmget\n");
      return -1;
    }
    proc->shmem_size = size;
    proc->startaddr = va_ptr;
    proc->shmem_tok = token;
    switchuvm(proc);
    return 0;
  }
  //already mapped stuffs
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != UNUSED){
      if(p->shmem_tok == token){
	release(&ptable.lock);
	goto found;
      }
    }
  }
  release(&ptable.lock);
  return -1;
 found:
  size = PGROUNDUP(p->shmem_size);
  for(;i<size; i+=PGSIZE){
     pte = walkpgdir((pde_t*)p->pgdir, ((char *)p->startaddr+i), 0);
    if(!pte){
      cprintf("Major error in shmget trying to map already shared mem\n");
      return -1;
    }
    if((*pte & PTE_P)!= 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0){
	cprintf("Major error in shmget physical addr was 0\n");
	return -1;
	}
      mappages(proc->pgdir,(char*)va_ptr+i,PGSIZE,pa,PTE_W|PTE_U);
    }
  }
  switchuvm(proc);
  return 0;
}


int sys_gettime(void)
{
  unsigned long *msec;
  unsigned long *sec;
  uint ticks1;

  if((argptr(0,(char**)&msec, sizeof(unsigned long)) < 0) || 
     (argptr(1,(char**)&sec, sizeof(unsigned long)) < 0)){
    return -1;
  }
  
  //assert(msec != NULL);
  //assert(sec != NULL);

  acquire(&tickslock);
  ticks1 = ticks;
  release(&tickslock);
  
  //ticks occur every 10ms
  *msec = 10 * (ticks1 % 100);
  *sec = (ticks1 / 100);
  
  return 0;
  

}
