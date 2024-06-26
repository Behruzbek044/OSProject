#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "kalloc.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void 
updateNFUState(){

  struct proc *p;
  int i;
  pte_t *pte, *pde;

  acquire(&ptable.lock);
  for(p = ptable.proc; p< &ptable.proc[NPROC]; p++){
    if((p->state == RUNNING || p->state == RUNNABLE || p->state == SLEEPING)){
      for(i=0; i<MAX_PSYC_PAGES; i++){
        if(p->free_pages[i].va == (char*)0xffffffff)
          continue;
        p->free_pages[i].age++;
        p->swap_space_pages[i].age++;

        pde = &p->pgdir[PDX(p->free_pages[i].va)];
        if(*pde & PTE_P){
          pte_t *pgtab;
          pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
          pte = &pgtab[PTX(p->free_pages[i].va)];
        }
        else pte = 0;
        if(pte){
          if(*pte & PTE_A){
            p->free_pages[i].age = 0;
          }
        }
      }
    }
  }
  release(&ptable.lock);
}
void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  //cprintf("called allocproc\n");
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

  // initialize process's page data
  #ifndef NONE
    int i;
    for (i = 0; i < MAX_PSYC_PAGES; i++) {
      p->swap_space_pages[i].va = (char*)0xffffffff;
      p->swap_space_pages[i].swaploc = 0;
      p->swap_space_pages[i].age = 0;
      p->free_pages[i].va = (char*)0xffffffff;
      p->free_pages[i].next = 0;
      p->free_pages[i].prev = 0;
      p->free_pages[i].age = 0;
    }
    p->page_fault_count = 0;
    p->page_swapped_count = 0;
    p->main_mem_pages = 0;
    p->swap_file_pages = 0;
    p->head = 0;
    p->tail = 0;
  #endif

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  //cprintf("called userinit\n");
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

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
  //cprintf("called before userinit\n");
  p->tf->eip = 0;  // beginning of initcode.S

  //cprintf("called before userinit\n");

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    //cprintf("\ncalled n>0\n");
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0){
      //cprintf("value of size = %d",sz);
      return -1;
    }
      
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  #ifndef NONE
    np->main_mem_pages = curproc->main_mem_pages;
    np->swap_file_pages = curproc->swap_file_pages;
  #endif

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  #ifndef NONE
    if(curproc->pid > 2)
    {
      createSwapFile(np);
      char buf[PGSIZE/2] = "";
      int offset = 0;
      int nread = 0;
      while((nread == readFromSwapFile(curproc,buf, offset, PGSIZE/2))!=0)
        if(writeToSwapFile(np, buf, offset, nread) == -1){
          panic("fork: error while copying the parent's swap file to the child");
        offset +=nread;
      }
    }

    for(i=0;i<MAX_PSYC_PAGES;i++){
      np->free_pages[i].va = curproc->free_pages[i].va;
      np->free_pages[i].age = curproc->free_pages[i].age;
      np->swap_space_pages[i].va = curproc->swap_space_pages[i].va;
      np->swap_space_pages[i].age = curproc->swap_space_pages[i].age;
      np->swap_space_pages[i].swaploc = curproc->swap_space_pages[i].swaploc;
      }

    int j;
    for(i = 0; i<MAX_PSYC_PAGES; i++){
      for(j=0;j<MAX_PSYC_PAGES;++j){
        if(np->free_pages[j].va == curproc->free_pages[i].next->va)
          np->free_pages[i].next = &np->free_pages[j];
        if(np->free_pages[j].va == curproc->free_pages[i].prev->va)
          np->free_pages[i].prev = &np->free_pages[j];
      }
    }  
    #if SCFIFO
      for (i = 0; i < MAX_PSYC_PAGES; i++) {
        if (curproc->head->va == np->free_pages[i].va){
          np->head = &np->free_pages[i];
        }
        if (curproc->tail->va == np->free_pages[i].va){
          np->tail = &np->free_pages[i];
        }
      }
    #elif FIFO
      for(i = 0;i<MAX_PSYC_PAGES;i++){
        if(curproc->head->va == np->free_pages[i].va)
          np->head = &np->free_pages[i];
        if(curproc->tail->va == np->free_pages[i].va)
          np->tail = &np->free_pages[i];
      }
    #endif
  #endif

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.



void custom_proc_print(struct proc *proc)
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
  char *state;
  uint pc[10];

  if(proc->state >= 0 && proc->state < NELEM(states) && states[proc->state])
    state = states[proc->state];
  else
    state = "???";
  cprintf("\npid:%d state:%s name:%s", proc->pid, state, proc->name);
  if(proc->state == SLEEPING){
      getcallerpcs((uint*)proc->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
  }
  cprintf("\nNo. of pages currently in physical memory: %d,\n", proc->main_mem_pages);
  cprintf("No. of pages currently in swap space: %d,\n", proc->swap_file_pages);
  cprintf("Count of page faults: %d,\n", proc->page_fault_count);
  cprintf("Count of paged out pages: %d,\n\n", proc->page_swapped_count);
  
 }

int 
cuscmp(const char *p, const char *q){
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

void
exit(void)
{
  struct proc *curproc = myproc();
  //cprintf("called_exit %s", curproc->name);
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }
  #ifndef NONE
    if(curproc->pid >2 &&  curproc->swapFile!=0 && curproc->swapFile->ref > 0)
    {
      removeSwapFile(curproc);
    }
  #endif

  #if TRUE
    if(cuscmp(curproc->name,"sh") != 0)
      custom_proc_print(curproc);
  #endif
    

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  //cprintf("called2\n");
  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
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
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
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
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      //cprintf("done executing: pid = %d\n",p->pid);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
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
    struct proc *p;
    int percentage;

    for(p = ptable.proc; p<&ptable.proc[NPROC]; p++){
      if(p->state == UNUSED)
        continue;
      custom_proc_print(p);
    }
    percentage = (free_page_counts.num_curr_free_pages*100)/free_page_counts.num_init_free_pages;
    cprintf("\n\n Number of free physical pages: %d/%d ~ %d%% \n",free_page_counts.num_curr_free_pages,free_page_counts.num_init_free_pages, percentage);
}