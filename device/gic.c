#include "types.h"
#include "param.h"
#include "arm.h"
#include "defs.h"
#include "memlayout.h"

#include "proc.h"

#include <stdint.h>
#include "macro.h"
#include "gicv2_io.h"
#include "timer_io.h"

#define NUM_INTSRC    128 // numbers of interrupt source supported

// for yield calling on interrupt
extern struct proc *proc;

// declare an array of ISR functions to handle interrupts
static ISR isrs[NUM_INTSRC];

// global variables
static void *gicc_base, *gicd_base;

static void default_isr(struct trapframe *tf, int n) {
    (void)tf;
    cprintf("unhandled interrupt: %d\n", n);
}

void gic_init(void *gicbase) {
    
    //void *gicbase = get_gic_baseaddr();
    gicd_base = gicbase + GICD_OFFSET; // global variable
    gicc_base = gicbase + GICC_OFFSET; // global variable
    
    // Global enable forwarding interrupts from distributor to cpu interface
    mmio_write32(gicd_base + GICD_CTLR, GICD_CTLR_GRPEN1);
    
    // Global enable signalling of interrupt from the cpu interface
    mmio_write32(gicc_base + GICC_CTLR, GICC_CTLR_GRPEN1);
    mmio_write32(gicc_base + GICC_PMR, GICC_PMR_DEFAULT);
    
    // init the interrupt service routice sources array
    int i;
    for (i=0; i< NUM_INTSRC; i++)
      isrs[i] = default_isr;

}

// not used for now
void* get_gic_baseaddr(void)
{
    unsigned val;
    asm volatile("mrc p15, 4, %0, c15, c0, 0;"
        : "=r" (val)
        );
    /* FIXME Here we handle only 32 bit addresses. But there are 40 bit addresses with lpae */
    val >>= 15;
    val <<= 15;
    return (void*)val;
}

void *gic_v2_gicd_get_address(void)
{
    return gicd_base;
}

void *gic_v2_gicc_get_address(void)
{
    return gicc_base;
}

void gic_v2_irq_enable(unsigned int irqn)
{
    unsigned m ,n, val;
    void *address;
    m = irqn;
    n = m / 32;
    address = gicd_base + GICD_ISENABLER + 4*n;
    val = mmio_read32(address);
    val |= 1 << (m % 32);
    mmio_write32(address, val);
}

void gic_v2_irq_set_prio(int irqno, int prio)
{
    /* See doc: ARM Generic Interrupt Controller: Architecture Specification version 2.0
     * section 4.3.11
     */
    int n, m, offset;
    volatile uint8_t *gicd = gic_v2_gicd_get_address();
    m = irqno;
    n = m / 4;
    offset = GICD_IPRIORITYR + 4*n;
    offset += m % 4; /* Byte offset */
    
    gicd[offset] = 0xff;
    
    gicd[offset] = prio << 4;  
}

void gic_irq_enable(int irqno)
{
    volatile uint8_t *gicd = gic_v2_gicd_get_address() + GICD_ITARGETSR;
    int n, m, offset;
    m = irqno;
    n = m / 4;
    offset = 4*n;
    offset += m % 4;
    
    gicd[offset] |= gicd[0];
    
    gic_v2_irq_set_prio(irqno, LOWEST_INTERRUPT_PRIORITY);  
    gic_v2_irq_enable(irqno);  
}

void gic_eoi(int intn)
{
    mmio_write32(gicc_base + GICC_EOIR, intn); 
}

int gic_getack( void )
{
    return mmio_read32(gicc_base + GICC_IAR);
}

void pic_enable(int n, ISR isr) {
    if ((n<0) || (n>=NUM_INTSRC)) {
        panic("invalid interrupt source");
    }
    if(n < NUM_INTSRC) {
      // configure irq n on gicv2
      gic_irq_enable(n);
      // add the function to ISRS array
      isrs[n] = isr;
    }
}

// not used
void pic_disable(int n) {
    if ((n<0) || (n>=NUM_INTSRC)) {
        panic("invalid interrupt source");
    }
    //gic_irq_disable(n);
    isrs[n] = default_isr;
}

// dispatch the interrupt
void pic_dispatch (struct trapframe *tp) {
    int intno;
    
    // get & ack the irqno from gic register
    intno = gic_getack();
  
    // execute the registerd ISR function
    isrs[intno](tp, intno);
    
    // clear the irqno from gic register
    gic_eoi(intno);
    
    // Force process exit if it has been killed and is in user space.
    // (If it is still executing in the kernel, let it keep running
    // until it gets to the regular system call return.)
    //
    // on xv6 x86
    //if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    // 
    // on arm: (r14_svc == pc if SWI) 
    // - the proc in swi is in kernel space
    // - the proc not in swi is in user space
    // so: we need to compare tp->r14_svc with tp->pc
    // they need to be diferent to proc be in user space
    /*
    if(proc && proc->killed && (tp->r14_svc) != (tp->pc)) {
      exit();
    }
    */

    // Force process to give up CPU on clock tick.
    // If interrupts were on while locks held, would need to check nlock.
    if(proc && proc->state == RUNNING && intno == TMR_0_IRQNO) {
      yield();
    }
    
    // Check if the process has been killed since we yielded
    if(proc && proc->killed && (tp->r14_svc) != (tp->pc)) {
      exit();
    }
    
}
