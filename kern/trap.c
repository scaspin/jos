#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

extern uintptr_t gdtdesc_64;
struct Taskstate ts;
extern struct Segdesc gdt[];
extern long gdt_pd;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {0,0};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.

	extern void XTRAPX_divzero();
	extern void XTRAPX_debug();
	extern void XTRAPX_nonmask();
	extern void XTRAPX_breakpoint();
	extern void XTRAPX_overflow();
	extern void XTRAPX_bound();
	extern void XTRAPX_illop();
	extern void XTRAPX_device();
	extern void XTRAPX_dblflt();
	
	extern void XTRAPX_tss();
	extern void XTRAPX_segnp();
	extern void XTRAPX_stack();
	extern void XTRAPX_gpflt();
	extern void XTRAPX_pgflt();
	
	extern void XTRAPX_fperr();
	extern void XTRAPX_align();
	extern void XTRAPX_mchk();
	extern void XTRAPX_simderr();

	extern void XTRAPX_syscall();
	extern void XTRAPX_default();
	
	extern void XTRAPX_irq_timer();
	extern void XTRAPX_irq_kbd();
	extern void XTRAPX_irq_serial();
	extern void XTRAPX_irq_spurious();
	extern void XTRAPX_irq_ide();
	extern void XTRAPX_irq_error();


	SETGATE(idt[T_DIVIDE], 0, GD_KT, XTRAPX_divzero, 0);  
    	SETGATE(idt[T_DEBUG], 0, GD_KT, XTRAPX_debug, 0);  
    	SETGATE(idt[T_NMI], 0, GD_KT, XTRAPX_nonmask, 0);  
    	SETGATE(idt[T_BRKPT], 0, GD_KT, XTRAPX_breakpoint, 3);  
    	SETGATE(idt[T_OFLOW], 0, GD_KT, XTRAPX_overflow, 0);  
    	SETGATE(idt[T_BOUND], 0, GD_KT, XTRAPX_bound, 0);  
    	SETGATE(idt[T_ILLOP], 0, GD_KT, XTRAPX_illop, 0);  
    	SETGATE(idt[T_DEVICE], 0, GD_KT, XTRAPX_device, 0);  
   	SETGATE(idt[T_DBLFLT], 0, GD_KT, XTRAPX_dblflt, 0);  
    	
	SETGATE(idt[T_TSS], 0, GD_KT, XTRAPX_tss, 0);  
    	SETGATE(idt[T_SEGNP], 0, GD_KT, XTRAPX_segnp, 0);  
    	SETGATE(idt[T_STACK], 0, GD_KT, XTRAPX_stack, 0);  
    	SETGATE(idt[T_GPFLT], 0, GD_KT, XTRAPX_gpflt, 0);  
    	SETGATE(idt[T_PGFLT], 0, GD_KT, XTRAPX_pgflt, 0);  
    	
	SETGATE(idt[T_FPERR], 0, GD_KT, XTRAPX_fperr, 0);  
    	SETGATE(idt[T_ALIGN], 0, GD_KT, XTRAPX_align, 0);  
    	SETGATE(idt[T_MCHK], 0, GD_KT, XTRAPX_mchk, 0);  
    	SETGATE(idt[T_SIMDERR], 0, GD_KT, XTRAPX_simderr, 0);  
    	
	SETGATE(idt[T_SYSCALL], 0, GD_KT, XTRAPX_syscall, 3);
    	SETGATE(idt[T_DEFAULT], 0, GD_KT, XTRAPX_default, 0);
	
    	SETGATE(idt[IRQ_OFFSET+IRQ_TIMER], 0, GD_KT, XTRAPX_irq_timer, 0);
    	SETGATE(idt[IRQ_OFFSET+IRQ_KBD], 0, GD_KT, XTRAPX_irq_kbd, 0);
    	SETGATE(idt[IRQ_OFFSET+IRQ_SERIAL], 0, GD_KT, XTRAPX_irq_serial, 0);
    	SETGATE(idt[IRQ_OFFSET+IRQ_SPURIOUS], 0, GD_KT, XTRAPX_irq_spurious, 0);
    	SETGATE(idt[IRQ_OFFSET+IRQ_IDE], 0, GD_KT, XTRAPX_irq_ide, 0);
    	SETGATE(idt[IRQ_OFFSET+IRQ_ERROR], 0, GD_KT, XTRAPX_irq_error, 0);

	idt_pd.pd_lim = sizeof(idt)-1;
	idt_pd.pd_base = (uint64_t)idt;
	// Per-CPU setup
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + 2*i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:


	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.


	int cpu_id = thiscpu->cpu_id;
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpu_id * (KSTKSIZE+KSTKGAP);

	// Initialize the TSS slot of the gdt.
	//SETTSS((struct SystemSegdesc64 *)((gdt_pd>>16)+40),STS_T64A, (uint64_t) (&ts),sizeof(struct Taskstate), 0);
	
	//doesn't work welp
	//SETTSS((struct SystemSegdesc64 *)gdt[(GD_TSS0>>3)+(2*cpu_id)],STS_T64A, (uint64_t) (&thiscpu->cpu_ts),sizeof(struct Taskstate), 0);
	
	/* for ref

	#define SEG64(type, base, lim, dpl) (struct Segdesc)\
	{ ((lim) >> 12) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,\
    type, 1, dpl, 1, (unsigned) (lim) >> 28, 0, 1, 0, 1,\
    (unsigned) (base) >> 24 }

	*/

	//gdt[(GD_TSS0>>3)+(2*cpu_id)]=SEG64(STS_T64A, (uint64_t) (&thiscpu->cpu_ts),sizeof(struct Taskstate), 0);
	
	//NOT HAVING THIS GAVE INFINITE LOOOP, SYSTEM, not application (difference between SETTSS and straight up assignment
	//gdt[(GD_TSS0>>3)+(2*cpu_id)].sd_s = 0;

	SETTSS((struct SystemSegdesc64 *)((gdt_pd>>16)+40+16*cpu_id),STS_T64A, (uint64_t) (&thiscpu->cpu_ts),sizeof(struct Taskstate), 0);

	gdt[(GD_TSS0>>3)+(2*cpu_id)].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0+(cpu_id*16));

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  rip  0x%08x\n", tf->tf_rip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  rsp  0x%08x\n", tf->tf_rsp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  r15  0x%08x\n", regs->reg_r15);
	cprintf("  r14  0x%08x\n", regs->reg_r14);
	cprintf("  r13  0x%08x\n", regs->reg_r13);
	cprintf("  r12  0x%08x\n", regs->reg_r12);
	cprintf("  r11  0x%08x\n", regs->reg_r11);
	cprintf("  r10  0x%08x\n", regs->reg_r10);
	cprintf("  r9  0x%08x\n", regs->reg_r9);
	cprintf("  r8  0x%08x\n", regs->reg_r8);
	cprintf("  rdi  0x%08x\n", regs->reg_rdi);
	cprintf("  rsi  0x%08x\n", regs->reg_rsi);
	cprintf("  rbp  0x%08x\n", regs->reg_rbp);
	cprintf("  rbx  0x%08x\n", regs->reg_rbx);
	cprintf("  rdx  0x%08x\n", regs->reg_rdx);
	cprintf("  rcx  0x%08x\n", regs->reg_rcx);
	cprintf("  rax  0x%08x\n", regs->reg_rax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	
	if (tf->tf_trapno == T_PGFLT) {
		page_fault_handler(tf);
		return;
	}
	if (tf->tf_trapno ==T_BRKPT){
		monitor(tf);
	}
	if (tf->tf_trapno==T_SYSCALL){
		uint64_t call_num = tf->tf_regs.reg_rax;
		uint64_t arg1 = tf->tf_regs.reg_rdx;
		uint64_t arg2 = tf->tf_regs.reg_rcx;
		uint64_t arg3 = tf->tf_regs.reg_rbx;
		uint64_t arg4 = tf->tf_regs.reg_rdi;
		uint64_t arg5 = tf->tf_regs.reg_rsi;
		
		tf->tf_regs.reg_rax = syscall(call_num, arg1, arg2, arg3, arg4, arg5);
		return;
	
	}


	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.

	if (tf->tf_trapno==IRQ_OFFSET+IRQ_TIMER){
		lapic_eoi();
		sched_yield();
		return;
	}	 

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	//struct Trapframe *tf = &tf_;
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();

		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint64_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if ((tf->tf_cs & 0x03) == 0){
		panic("kernel mode page fault");	
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.

	if (curenv->env_pgfault_upcall)	
	{
		//set up new trap frame
		uint64_t old_rsp = tf->tf_rsp;
		uint64_t new_rsp;
		
		//not first time
		if (tf->tf_rsp <= UXSTACKTOP-1 && tf->tf_rsp >= (UXSTACKTOP-PGSIZE)){
			tf->tf_rsp = old_rsp - 8;
			new_rsp = old_rsp - 8;
		}else{ //first ime
			tf->tf_rsp=UXSTACKTOP;
			new_rsp = UXSTACKTOP;
		}
		
		new_rsp = new_rsp- sizeof(struct UTrapframe);
		struct UTrapframe *uxstack;
		user_mem_assert(curenv, (void *)new_rsp,1, PTE_W);

		uxstack = (struct UTrapframe *)new_rsp;

		//put input into new stack
		uxstack->utf_fault_va = fault_va;
		uxstack->utf_err = tf->tf_err;
		uxstack->utf_regs = tf->tf_regs;
		uxstack->utf_rip = tf->tf_rip;
		uxstack->utf_eflags = tf->tf_eflags;
		uxstack->utf_rsp = old_rsp;

		//branch to curenv->env_pgfault_upcall
		curenv->env_tf.tf_rip = (uintptr_t)curenv->env_pgfault_upcall;
		curenv->env_tf.tf_rsp = new_rsp;
		env_run(curenv); 
	
	}
	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_rip);
	print_trapframe(tf);
	env_destroy(curenv);
}

