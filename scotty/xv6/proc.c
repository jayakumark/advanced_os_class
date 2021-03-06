#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

#define assert(exp) if (exp) ; else AssertionFailure( #exp, __FILE__, __LINE__ ) 

static void AssertionFailure(char *exp, char *file, int line)
{
  cprintf("Assertion '%s' failed at line %d of file %s\n", exp, line, file);
  panic("");
}


struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
int ready1(struct proc* process);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

void rQinit(void){
  int i;
  // initlock(&readyQ.lock, "readyQ");
  for(i=0;i<32;i++){
    readyQ.proc[i] = 0;
  }
}

/*CONTRACT REQUIRES PTABLE LOCK ALREADY HELD*/
int gethighestpriority(){
  int i;
  for(i=0;i<32;i++){
    if(readyQ.proc[i] != 0){
      return i;
    }
  }
  return 32;
}

int setpriority(int pid, int priority)
{
  int prev_pri = 31;
  struct proc *p;
  

  
  if(priority > 31 || priority < 0)
    return -1;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->pid == pid){
	  prev_pri = p->priority;
          p->priority = priority;

	  if(p->state == RUNNABLE){
	    if(p->next == 0 && p->prev==0){
	      readyQ.proc[prev_pri]=0;
	    }
	    else if(p->next == 0 && p->prev != 0){
	      //last of the list
	      p->prev->next = 0;
	      p->prev = 0;
	    }
	    else if(p->next !=0 && p->prev != 0){
	      p->prev->next = p->next;
	      p->next->prev = p->prev;
	      p->next = 0;
	      p->prev = 0;
	    }
	    else {
	      assert(1==2);
	    }
	    //I think thats all the cases?
	    ready1(p);
	  }
	  else if(p->state == RUNNING){
	    //cprintf("proc is currently running\n");
	    //Two cases
	    //Case one, Priority was increased (more cpu time) //do nothing, we're already running
	    
	    //Case two, we setpri on ourselves and we changed priority was lowered and there is a proc with higher priority
	    if(p->priority > gethighestpriority()){
	      
	      //I think I should yield to get THIS proc back on ready?
	      //sched();
	      release(&ptable.lock);
	      yield();
	      acquire(&ptable.lock);
	    }
	  }
	  //Okay, we changed the priority of ANOTHER process. Now we need to see if
	  //the priority we changed to is less (more cpu time) than what we are.
	  if(proc->priority > priority){
	    cprintf("Proc priority > priority\n");
	    release(&ptable.lock);
	    yield();
	    acquire(&ptable.lock);
	  }
	  //cprintf("Releasing and returning in set priority\n");
	  release(&ptable.lock);
          return 1;
        }
    }
  release(&ptable.lock);
  return -1;
}

int ready(struct proc* process){
  int ret;
  acquire(&ptable.lock);
  ret = ready1(process);
  release(&ptable.lock);
  return ret;
}

int ready1(struct proc* process){
  

  //  cprintf("in ready with process %d, and priority %d pid of %d\n",process,process->priority, process->pid);
 
 struct proc *proc_;

  //ready to place on table
  if(!process){
    return -1;
  }
  if((proc_ = readyQ.proc[process->priority])!=0){
   
    if(proc_->prev == 0){
      proc_->prev = process;
      proc_->next = process;
      process->prev=proc_;
      process->next=0;
    }
    else{
      process->prev = proc_->prev;
      proc_->prev->next = process;
      proc_->prev = process;
      process->next = 0;
    }
  }
  else{
    readyQ.proc[process->priority] = process;
    process->next = 0;
    process->prev = 0;
  }
  /*  proc_ = readyQ.proc[process->priority];
  cprintf("Priority is %d\n",process->priority);
  cprintf("list is %d \n",proc_->pid);
  while(proc_->next != 0){
    proc_ = proc_->next;
    cprintf("list is %d \n",proc_->pid);
    }*/
  return 1;
}


//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  //cprintf("in allocproc\n");
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  //cprintf("in user init");
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S
  p->priority = 14;
  p->prev = 0;
  p->next = 0;


  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  //cprintf("userinit setting on ready\n");
  ready(p);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  //cprintf("in fork\n");
  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  np->priority = proc->priority;
  ready(np);
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched(); 
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p = 0;
  int i;
  for(;;){
    // Enable interrupts on this processor.
    sti();


    acquire(&ptable.lock);
    for(i=0;i<32;i++){
      if(readyQ.proc[i]){
	p=readyQ.proc[i];
	if(p->next == 0){
	  readyQ.proc[i] = 0;
	}
	else{
	  readyQ.proc[i] = p->next;
	  if(p->next != p->prev){
	    p->next->prev = p->prev;  
	  }
	  else{
	    p->next->prev = 0;
	    p->next->next = 0;
	  }
	}
	p->next = 0;
	p->prev = 0;
	
	//	cprintf("Found proc to run\n");
	proc=p;
	switchuvm(p);
	p->state = RUNNING;
	//	cprintf("Dispatching priority %d\n",p->priority);
	swtch(&cpu->scheduler, proc->context);
	switchkvm();
	proc=p=0;
	break;
      }
    }
    release(&ptable.lock);
    

    /*
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);*/

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  ready1(proc);
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  //release(&ptable.lock);
  release(&ptable.lock);
  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    initlog();
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();
  
  // Tidy up.
  proc->chan = 0;
  //ready1(proc);
  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      ready1(p);
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
	ready1(p);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


