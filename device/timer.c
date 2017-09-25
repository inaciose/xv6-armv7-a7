#include "types.h"
#include "memlayout.h"
#include "defs.h"
#include "spinlock.h"
#include "macro.h"
#include "timer_io.h"

void isr_timer (struct trapframe *tf, int irq_idx);

unsigned int timer_base;

volatile unsigned int icount;
//int ledstate = 0;

static inline void _delay(unsigned int count)
{
  asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
     : "=r"(count): [count]"0"(count) : "cc");
}

struct spinlock tickslock;
uint ticks;

static void ack_timer() {
    // clean his status register bit
    mmio_write32or(timer_base + TMR_IRQ_STA, 1);
}

void timer_init(int hz) {
    
    //void hz; // do the hz stuf later, lets void it for now
    
    timer_base =  P2V_WO(SUNXI_TIMER_BASE);
    
    initlock(&tickslock, "time");
    
    // stops timer to allow change on his registers
    mmio_write32(timer_base + TMR_0_CTRL, 0x4); // timer 0 control : stop
    
    _delay(150); // need to wait to take effect

    // start value decrementing to zero.. 
    // then set its bit to 1 on IRQ_STA timer register (check and set it to 0)
    // and generate an interrupt if its bit is enabled TMR_IRQ_EN register 
    mmio_write32(timer_base + TMR_0_INTR_VAL, 0xFFF000);  // timer 0 interval (start value)
    
    // start timer countdown
    mmio_write32(timer_base + TMR_0_CTRL, 0x7);  // timer 0 control : start
    
    // enable interrupt generation
    mmio_write32or(timer_base + TMR_IRQ_EN, 0x1);  // timer 0

    pic_enable(TMR_0_IRQNO, isr_timer);
    
    // led blinking
    icount=0;
  
}

void isr_timer (struct trapframe *tf, int irq_idx) {
    (void)tf;
    
    // led blinking
    icount++;
    if(icount&1) {
      // Turn green led on PH24
      asm (" \n"
        "mov r0,  #0x01000000 \n"
        "ldr r3, =0x41C2090C \n"
        "str r0, [r3]");

    } else {
      // Turn green led off PH24
      asm (" \n"
        "mov r0,  #0x00000000 \n"
        "ldr r3, =0x41C2090C \n"
        "str r0, [r3]");

    }
    
    // process
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
    ack_timer();
}
