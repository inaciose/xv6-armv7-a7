// BSP support routine
#include "types.h"
#include "defs.h"
#include "param.h"
#include "arm.h"
#include "proc.h"
#include "memlayout.h"
#include "mmu.h"

#define GIC_BASE (0x01C80000)

extern void* end;

struct cpu	cpus[NCPU];
struct cpu	*cpu;

#define MB (1024*1024)

void kmain (void)
{
    uint vectbl;

    // Configure port PH24 (green led) for output
    asm ("\n"
      "mov r0,  #0x00000001 \n"
      "ldr r3, =0x01C20908 \n"
      "str r0, [r3] \n");

    // only one cpu for now
    cpu = &cpus[0];

    // uart init to be moved from start.c (after move it to device/uart.c)
    //uart_init (P2V(UART0));

    // interrrupt vector table is in the middle of first 1MB. We use the left
    // over for page tables
    vectbl = P2V_WO ((VEC_TBL & PDE_MASK) + PHY_START);
    
    init_vmm ();
    kpt_freerange (align_up(&end, PT_SZ), vectbl);
    kpt_freerange (vectbl + PT_SZ, P2V_WO(INIT_KERNMAP));
    paging_init (INIT_KERNMAP, PHYSTOP);
    
    kmem_init ();
    kmem_init2(P2V(INIT_KERNMAP), P2V(PHYSTOP));
    
    trap_init ();				// vector table and stacks for models
    
    gic_init(P2V(GIC_BASE));    // arm v2 gic init

    uart_enable_rx ();			// interrupt for uart
    consoleinit ();				// console
    pinit ();					// process (locks)

    binit ();					// buffer cache
    fileinit ();				// file table
    iinit ();					// inode cache
    ideinit ();					// ide (memory block device)
    
    timer_init (HZ);			// the timer (ticker)

    sti ();
    userinit();					// first user process
    scheduler();				// start running processes
}
