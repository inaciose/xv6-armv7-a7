#include "arm.h"
#include "memlayout.h"

.text
.code 32

.global _start

_start:
    # disable interrupts (FIQ and IRQ), also set the cpu to SVC32 mode,
    # except if in HYP mode already
    mrs  r0, cpsr
    and  r1, r0, #0x1f    @ mask mode bits
    teq  r1, #0x1a    @ test for HYP mode
    bicne  r0, r0, #0x1f    @ clear all mode bits
    orrne  r0, r0, #0x13    @ set SVC mode
    orr  r0, r0, #0xc0    @ disable FIQ and IRQ
    msr  cpsr,r0

    # clear the entry bss section, the svc stack, and kernel page table
    LDR     r1, =edata_entry
    LDR     r2, =end_entry
    MOV     r3, #0x00

1:
    CMP     r1, r2
    STMLTIA r1!, {r3}
    BLT     1b

    # initialize stack pointers for svc modes
    //MSR     CPSR_cxsf, #(SVC_MODE|NO_INT)
    //LDR     sp, =svc_stktop

    @ Disable MMU.
    MRC p15, 0, r1, c1, c0, 0                 @ Read Control Register configuration data.
    BIC r1, r1, #0x1
    MCR p15, 0, r1, c1, c0, 0                 @ Write Control Register configuration data.
      
    @ Disable L1 Caches.
    MRC p15, 0, r1, c1, c0, 0                 @ Read Control Register configuration data.
    BIC r1, r1, #(0x1 << 12)                 @ Disable I Cache.
    BIC r1, r1, #(0x1 << 2)                 @ Disable D Cache.
    MCR p15, 0, r1, c1, c0, 0                 @ Write Control Register configuration data

    @ Invalidate L1 Caches.
    @ Invalidate Instruction cache.
    MOV r1, #0
    MCR p15, 0, r1, c7, c5, 0

    @ Invalidate Data cache.
    @ To make the code general purpose, calculate the
    @ cache size first and loop through each set + way.

    MRC p15, 1, r0, c0, c0, 0                 @ Read Cache Size ID.
    LDR r3, =0x1ff
    AND r0, r3, r0, LSR #13                 @ r0 = no. of sets - 1.

    MOV r1, #0                 @ r1 = way counter way_loop.
way_loop:
    MOV r3, #0                 @ r3 = set counter set_loop.
set_loop:
    MOV r2, r1, LSL #30
    ORR r2, r3, LSL #5                 @ r2 = set/way cache operation format.
    MCR p15, 0, r2, c7, c6, 2                @ Invalidate the line described by r2.
    ADD r3, r3, #1                 @ Increment set counter.
    CMP r0, r3                 @ Last set reached yet?
    BGT set_loop                 @ If not, iterate set_loop, 
    ADD r1, r1, #1                 @ else, next.
    CMP r1, #4                 @ Last way reached yet?
    BNE way_loop                 @ if not, iterate way_loop.

    @ Invalidate TLB
    MCR p15, 0, r1, c8, c7, 0

    @ Branch Prediction Enable.
    MOV r1, #0
    MRC p15, 0, r1, c1, c0, 0                 @ Read Control Register configuration data.
    ORR r1, r1, #(0x1 << 11)                 @ Global BP Enable bit.
    MCR p15, 0, r1, c1, c0, 0                 @ Write Control Register configuration data.
    
    @ Turn on unaligned memory access
    mrc p15, #0, r4, c1, c0, #0
    orr r4, #0x400000  ;@ 1<22 
    mcr p15, #0, r4, c1, c0, #0

    @ Setup SVC mode stack
    mov r0, #(SVC_MODE | NO_INT)
    msr cpsr_c, r0
    ldr     sp, =svc_stktop

    BL      start
    B .

# during startup, kernel stack uses user address, now switch it to kernel addr
.global jump_stack
jump_stack:
    MOV     r0, sp
    ADD     r0, r0, #KERNBASE
    MOV     sp, r0
    MOV     pc, lr