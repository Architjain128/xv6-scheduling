#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
// int pizza=10;
// int burger=3;
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct proc *queue[5][NPROC];
int q_tail[5] = {-1, -1, -1, -1, -1}, q_ticks_max[5] = {1, 2, 4, 8, 16}, q_ticks[5] = {0,0,0,0,0};

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void *chan);

void ticking(){
	for(struct proc *p=ptable.proc; p<&ptable.proc[NPROC]; ++p){
		if(p->state == RUNNING){
			p->rtime++;
		}
    if(p->state == SLEEPING){
			p->iotime++;
		}
	}
}

int add_proc_to_q(struct proc *p, int q_no)
{	
  // checking queue
	for(int i=0; i < q_tail[q_no]; i++)
	{
		if(p->pid == queue[q_no][i]->pid)
			return -1;
	}
  // if(p->pid<=pizza && p->pid>=burger){
	// cprintf("Process with PID %d added to Queue %d at %d\n", p->pid, q_no,ticks);}
	p->enter = ticks;
	p -> queue = q_no;
	q_tail[q_no]++;
	queue[q_no][q_tail[q_no]] = p;
	return 1;
}

int remove_proc_from_q(struct proc *p, int q_no)
{
	int proc_found = 0, rem = 0;
	for(int i=0; i <= q_tail[q_no]; i++)
	{
		if(queue[q_no][i] -> pid == p->pid)
		{
			// cprintf("Process with PID %d found in Queue %d\n", p->pid, q_no);
			rem = i;
			proc_found = 1;
			break;
		}
	}
	if(proc_found  == 0)
	{
		// cprintf("ERROR : REMOVE_Q : no Process with pid %d found in Queue %d\n", p->pid, q_no);
		return -1;
	}
	for(int i = rem; i < q_tail[q_no]; i++)
	queue[q_no][i] = queue[q_no][i+1]; 
	q_tail[q_no] -= 1;
  //   if(p->pid<=pizza && p->pid>=burger){
	// cprintf("Process with PID %d is removed from Queue %d at %d\n", p->pid, q_no,ticks);}
	return 1;
}

void change_q_flag(struct proc* p)
{
	acquire(&ptable.lock);
	p-> change_q = 1;
	release(&ptable.lock);
}

void incr_curr_ticks(struct proc *p)
{
	acquire(&ptable.lock);
	p->curr_ticks++;
	p->qticks[p->queue]++;
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
  p->ctime = ticks; 
	p->etime = 0;
	p->rtime = 0;
	p->iotime = 0;
	p->waitshh = -1282128;
  p->num_run = 0;
	p->priority = 60; // default
  	#ifdef MLFQ
		p->curr_ticks = 0;
		p->queue = 0;
		p->enter = 0;
		for(int i=0; i<5; i++)
			p->qticks[i] = 0;
	#endif
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
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
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this make . the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  #ifdef MLFQ
  add_proc_to_q(p,0);
  #endif
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
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
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

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  #ifdef MLFQ
  		add_proc_to_q(np, 0);
	#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
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

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->etime = ticks;
  curproc->waitshh=curproc->etime - curproc->ctime;
	cprintf("-> EXIT MSG : Total Time for process [%s] with pid [%d] is [%d]\n",curproc->name,curproc->pid, curproc->etime - curproc->ctime);
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
        #ifdef MLFQ
					remove_proc_from_q(p, p->queue);
          p->queue=-1;
				#endif 
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


int
waitx(int *wtime, int *rtime)
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
        *rtime = p->rtime ;
				*wtime = p->etime - p->ctime - p->rtime - p->iotime;
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        #ifdef MLFQ
					remove_proc_from_q(p, p->queue);
          p->queue=-1;
				#endif 
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        // p->etime = ticks;
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
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    #ifdef RR
      struct proc *p;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        p->num_run++;
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    #else
    #ifdef FCFS

    struct proc *p;
    struct proc *to_run_proc = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		{
			if (p->state != RUNNABLE)
				continue;

			if (to_run_proc == 0)
				to_run_proc = p;
			else if (p->ctime < to_run_proc->ctime)
				to_run_proc = p;
		}
    if (to_run_proc != 0 && to_run_proc->state == RUNNABLE)
		{
			// cprintf("Process %s with PID %d and start time %d running\n",to_run_proc->name, to_run_proc->pid, to_run_proc->ctime);
			p = to_run_proc;
			c->proc = p;
			switchuvm(p);
			p->num_run++;
			p->state = RUNNING;

			swtch(&(c->scheduler), p->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
		}
    #else
		#ifdef PBS

      struct proc *p;
			struct proc *to_run_proc = 0;
			
			for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
			{
				if (p->state != RUNNABLE)
					continue;	

				if (to_run_proc == 0)
					to_run_proc = p;
				
				else if (p-> priority <= to_run_proc-> priority)
        {
          if(p-> priority == to_run_proc-> priority && p->ctime < to_run_proc->ctime)
          {
            to_run_proc = p;
          }
          else
          {
            to_run_proc = p;
          }
        }
			}

			if(to_run_proc == 0)
			{
				release(&ptable.lock);
				continue;		
			}

			for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
			{
				struct proc *q; int flag =0;
				for(q = ptable.proc; q < &ptable.proc[NPROC]; q++)
				{
					if (q->state != RUNNABLE)
						continue;
					
					if(q->priority < to_run_proc->priority)
						flag = 1;
          // if(p-> priority == to_run_proc-> priority && p->ctime < to_run_proc->ctime)
						// flag = 1;

				}

				if(flag == 1) break;
				
				if (p->state != RUNNABLE) continue;

				else if (p->priority == to_run_proc->priority)
				{
					// cprintf("Process %s with PID %d and priority %d running\n",p->name, p->pid, p->priority);
					c->proc = p;
					switchuvm(p);
					p->num_run++;
					p->state = RUNNING;

					swtch(&(c->scheduler), p->context);
					switchkvm();
					//if (p->state == RUNNABLE) break;

					// Process is done running for now.
					// It should have changed its p->state before coming back.
					// cprintf("PID %d done !!!\n\n", to_run_proc->pid);
					c->proc = 0;
				}
			}
    #else
		#ifdef MLFQ

      for(int i=1; i < 5; i++)
			{
				for(int j=0; j <= q_tail[i]; j++)
				{
					struct proc *p = queue[i][j];
          int age = ticks - p->enter;
					if(age > 30)
					{
						remove_proc_from_q(p, i);
    // if(p->pid<=pizza && p->pid>=burger){
		// 				cprintf("Process %d moved to queue %d from %d due to age %d at %d\n", p->pid, i-1,i, age, ticks);}
						add_proc_to_q(p, i-1);
					}
				}
			}

			struct proc *p = 0;
			for(int i=0; i < 5; i++)
			{
				if(q_tail[i] >=0)
				{
					p = queue[i][0];
					remove_proc_from_q(p, i);
					break;
				}
			}

			if(p!=0 && p->state==RUNNABLE)
			{
				p->curr_ticks++;
				p->num_run++;
        //   if(p->pid<=pizza && p->pid>=burger){
				// cprintf("Scheduling %s with PID %d from Queue %d with current tick %d at tick %d\n",p->name, p->pid, p->queue, p->curr_ticks,ticks);}
				p->qticks[p->queue]++;
				c->proc = p;
				switchuvm(p);
				p->state = RUNNING;
				swtch(&c->scheduler, p->context);
				switchkvm();
				c->proc = 0;

				if(p!=0 && p->state == RUNNABLE)
				{
					if(p->change_q == 1)
					{
						p->change_q = 0;
						p->curr_ticks = 0;
						if(p->queue != 4){
					  // remove_proc_from_q(p, p->queue);
						p->queue+=1;
            }
					}
					else p->curr_ticks = 0;
					  add_proc_to_q(p, p->queue);
				}
			}
    #endif
    #endif
    #endif
    #endif
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      #ifdef MLFQ
				p->curr_ticks = 0;
				add_proc_to_q(p, p->queue);
			#endif
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
        #ifdef MLFQ
          p->curr_ticks = 0;
          add_proc_to_q(p, p->queue);
        #endif
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


int
getps(void) 
{
	struct proc *p;
	int ret = -1;
  acquire(&ptable.lock);
  #ifndef MLFQ
  cprintf("PID\tPriority    State \tr_time\tw_time\ts_time\tName\n");
  #endif
  #ifdef MLFQ
  cprintf("PID\tPriority    State \tr_time\tw_time\ts_time\tn_run\tcur_q\t q0\tq1\tq2\tq3\tq4\tName\n");
  #endif
  for (p = ptable.proc; p < &ptable.proc[NPROC]; ++p)
  {
    #ifndef MLFQ
    if (p->state == SLEEPING)
    {
      cprintf("%d \t %d \t   SLEEPING \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, ticks - p->ctime - p->rtime -p->iotime,p->iotime,p->name);
    }
    else if (p->state == RUNNING)
    {
      cprintf("%d \t %d \t   RUNNING  \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, ticks - p->ctime - p->rtime -p->iotime,p->iotime,p->name);
    }
    else if (p->state == RUNNABLE)
    {
      cprintf("%d \t %d \t   RUNNABLE \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, ticks - p->ctime - p->rtime -p->iotime,p->iotime,p->name);
    }
    else if (p->state == ZOMBIE)
    {
      cprintf("%d \t %d \t    ZOMBIE \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, p->etime - p->ctime - p->rtime -p->iotime,p->iotime,p->name);
    }
    #endif

    #ifdef MLFQ
   if (p->state == SLEEPING)
    {
      cprintf("%d \t %d \t   SLEEPING \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, ticks - p->ctime - p->rtime -p->iotime,p->iotime,p->num_run,p->queue,p->qticks[0],p->qticks[1],p->qticks[2],p->qticks[3],p->qticks[4],p->name);
    }
    else if (p->state == RUNNING)
    {
      cprintf("%d \t %d \t   RUNNING  \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, ticks - p->ctime - p->rtime -p->iotime,p->iotime,p->num_run,p->queue,p->qticks[0],p->qticks[1],p->qticks[2],p->qticks[3],p->qticks[4],p->name);
    }
    else if (p->state == RUNNABLE)
    {
      cprintf("%d \t %d \t   RUNNABLE \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, ticks - p->ctime - p->rtime -p->iotime,p->iotime,p->num_run,p->queue,p->qticks[0],p->qticks[1],p->qticks[2],p->qticks[3],p->qticks[4],p->name);
    }
    else if (p->state == ZOMBIE)
    {
      cprintf("%d \t %d \t    ZOMBIE \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d\t %s\n",  p->pid, p->priority, p->rtime, p->etime - p->ctime - p->rtime -p->iotime,p->iotime,p->num_run,-1,p->qticks[0],p->qticks[1],p->qticks[2],p->qticks[3],p->qticks[4],p->name);
    }
    #endif

  }
  release(&ptable.lock);
	return ret;
}

int set_priority(int priority, int pid)
{
	struct proc *p;
	int to_yield = 0, old_priority = 0;

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	{
		if(p->pid == pid)
		{
			to_yield = 0;
			acquire(&ptable.lock);
			old_priority = p->priority;
  		p->priority = priority;
			cprintf("Changed priority of process with PID %d from %d to %d\n", p->pid, old_priority, p->priority);
			if (old_priority > p->priority)
				to_yield = 1;
			release(&ptable.lock);
			break;
		}
	}
  	if (to_yield == 1)
    yield();
  	return old_priority;
}
