#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  
  case T_PGFLT:
    {
      // check if 0 <= fault_va < proc->sz manually
   
    // walkpgdir + no alloc (parameter = 0)
    // you get back pte
    // first, check flag on pte to make sure its not guard page --> if guard page, kill process (stack overflow)

    // kalloc, and assign page
    // then walkpage dir with alloc
    // (basically old allocuvm)
    // 

    // be careful of VA vs PA here
    uint fault_va = rcr2();
    uint rounded_fault_va = PGROUNDUP(fault_va);

    // make sure it is in bounds for process (0 <= fault_va < proc->sz) and not in the guard page
    pte_t *pte = walkpgdir(myproc()->pgdir, (const void *)(rounded_fault_va), 0);
    // & with user accessible bit
    // present flag PTE_P

    if(*pte & PTE_P && !(*pte & PTE_U)){
      cprintf("heap allocation failed - address out of bounds\n");
      goto kill_proc;
      return;
    }
    // obtain a free page
    char *mem = kalloc();
    if(mem == 0){
      cprintf("heap allocation failed - out of memory\n");
      goto kill_proc;
      return;
    }

    // zero out the page
    memset(mem, 0, PGSIZE);

    // update page table
    if(mappages(myproc()->pgdir, (char*)rounded_fault_va, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("heap allocation failed - out of memory\n");
      kfree(mem);
      goto kill_proc;
      return;
    }

    // flush tlb ?
    }

  //PAGEBREAK: 13
  default:
  kill_proc:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
