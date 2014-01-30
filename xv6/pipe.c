#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "spinlock.h"

#define PIPESIZE 3072 // Made pipesize 6x bigger

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if((p = (struct pipe*)kalloc()) == 0)
    goto bad;
  p->readopen = 1;
  p->writeopen = 1;
  p->nwrite = 0;
  p->nread = 0;
  initlock(&p->lock, "pipe");
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;
  return 0;

//PAGEBREAK: 20
 bad:
  if(p)
    kfree((char*)p);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

void
pipeclose(struct pipe *p, int writable)
{
  acquire(&p->lock);
  if(writable){
    p->writeopen = 0;
    wakeup(&p->nread);
  } else {
    p->readopen = 0;
    wakeup(&p->nwrite);
  }
  if(p->readopen == 0 && p->writeopen == 0){
    release(&p->lock);
    kfree((char*)p);
  } else
    release(&p->lock);
}

//PAGEBREAK: 40
int
pipewrite(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock); // spins here until the lock is acquired
  /*for(i = 0; i < n; i++){ // loops over the bytes being written (addr[0]-addr[n-1])
    while(p->nwrite == p->nread + PIPESIZE){  //DOC: pipewrite-full
      if(p->readopen == 0 || proc->killed){
        release(&p->lock);
        return -1;
      }
      wakeup(&p->nread); // called when the pipe is full, then sleeps until some of the pipe is empty
      sleep(&p->nwrite, &p->lock);  //DOC: pipewrite-sleep
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i];
  }
  wakeup(&p->nread);  //DOC: pipewrite-wakeup1
  release(&p->lock);
  return n;*/
  
  // Need to make the counts wrap
  // this should do it
  // but I still need to check after each update
#if 0
  if(p->nwrite >= PIPESIZE)
    p->nwrite = p->nwrite - PIPESIZE;
#endif

  for(i = 0; (i+1) < n; i+=2){
    while(p->nwrite == p->nread + PIPESIZE){  //DOC: pipewrite-full
      if(p->readopen == 0 || proc->killed){
        release(&p->lock);
        return -1;
      }
      wakeup(&p->nread); // called when the pipe is full, then sleeps until some of the pipe is empty
      sleep(&p->nwrite, &p->lock);  //DOC: pipewrite-sleep
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i];
    if(p->nwrite >= PIPESIZE)
      p->nwrite = p->nwrite - PIPESIZE;
    p->data[p->nwrite++ % PIPESIZE] = addr[i+1];
    if(p->nwrite >= PIPESIZE)
      p->nwrite = p->nwrite - PIPESIZE;
  }
  if(n%2){
    while(p->nwrite == p->nread + PIPESIZE){  //DOC: pipewrite-full
      if(p->readopen == 0 || proc->killed){
        release(&p->lock);
        return -1;
      }
      wakeup(&p->nread); // called when the pipe is full, then sleeps until some of the pipe is empty
      sleep(&p->nwrite, &p->lock);  //DOC: pipewrite-sleep
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i];
  }
  wakeup(&p->nread);  //DOC: pipewrite-wakeup1
  release(&p->lock);  
  return n;// speed:170000ms
//old speed: 319951 ms
}

int
piperead(struct pipe *p, char *addr, int n)
{
  int i;
  
  acquire(&p->lock);

  // This should update the count so they wrap around
#if 0
  if(p->nread >= PIPESIZE)
    p->nread = p->nread - PIPESIZE;
#endif

  while(p->nread == p->nwrite && p->writeopen){  //DOC: pipe-empty
    if(proc->killed){
      release(&p->lock);
      return -1;
    }
    sleep(&p->nread, &p->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(p->nread == p->nwrite)
      break;
    addr[i] = p->data[p->nread++ % PIPESIZE];
    // This should update the count so they wrap around
#if 0
    if(p->nread >= PIPESIZE)
      p->nread = p->nread - PIPESIZE;
#endif
  
}
  wakeup(&p->nwrite);  //DOC: piperead-wakeup
  release(&p->lock);
  return i;

  /*for(i = 0; (i+1) < n; i+=2) {
    if(p->nread == p->nwrite)
      break;
    addr[i] = p->data[p->nread++ % PIPESIZE];
    if(p->nread == p->nwrite) {
      i++; //this probably doesn't even matter
      break;
    }
    addr[i+1] = p->data[p->nread++ % PIPESIZE];
  }
  //need to do case for if n%2 != 0
  if(n%2) {
    if(p->nread != p->nwrite)
    addr[i] = p->data[p->nread++ % PIPESIZE];
  }
  wakeup(&p->nwrite);  //DOC: piperead-wakeup
  release(&p->lock);
  return i;*/
}
