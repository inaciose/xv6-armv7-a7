#include "types.h"
#include "defs.h"
//#include "arm.h"
#include "memlayout.h"

#include "uart_io.h"
#include "macro.h"

void uartputc(int c)
{
  while ((mmio_read32(UART0_LSR+KERNBASE) & 0x20) == 0) continue;
  mmio_write32(UART0_THR+KERNBASE, c);
}

void uart_puts(const char *s)
{
  while (*s) {
    if (*s == '\n')
      uartputc('\r');
    uartputc(*s++);
  }
}

int uartgetc()
{
  if ((mmio_read32(UART0_LSR+KERNBASE) & 0x01) == 0) {
    return -1;
  } else {
    return mmio_read32(UART0_RBR+KERNBASE);
  }
}

// *****************************************************************************

void isr_uart (struct trapframe *tf, int idx)
{
  
  if ((mmio_read32(UART0_LSR+KERNBASE) & 0x01) != 0) {
    consoleintr(uartgetc);
  }
    /*
    if (uart_base[UART_MIS] & UART_RXI) {
        consoleintr(uartgetc);
    }

    // clear the interrupt
    uart_base[UART_ICR] = UART_RXI | UART_TXI;
    */
}

void uart_enable_rx ()
{

    mmio_write32(UART0_IER+KERNBASE, 5 | mmio_read32(UART0_IER)); // ERBFI + ELSI
    
    pic_enable(UART0_IRQNO, isr_uart);
    
    // enable uart interrupts for gic
    // 0 = no int | 1 = rx int | 2 = rx/tx int
    /*
    switch (mode) {
      case 1:
        mmio_write32(UART0_IER, 1);
        break;
      case 2:
        mmio_write32(UART0_IER, 5 | mmio_read32(UART0_IER)); // ERBFI + ELSI
        break;
      default:
        mmio_write32(UART0_IER, 0);
        break;
    }
    */
}

// *****************************************************************************

void print_hex(uint val) {
    char digit[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char number[8] = {'0','0','0','0','0','0','0','0'};
    uint base = 16;
    int i = 7;
    uartputc('0');
    uartputc('x');

    while(val > 0) {
        number[i--] = digit[val % base];
        val /= base;

    }
    for(i=0;i<8;++i) {
        uartputc(number[i]);
    }
    uartputc('\r');
    uartputc('\n');
}

// *****************************************************************************
// bof my debug
// *****************************************************************************

void hexstrings ( unsigned int d )
{
  unsigned int rb;
  unsigned int rc;

  rb=32;
  while(1)
  {
    rb-=4;
    rc=(d>>rb)&0xF;
    if(rc>9) rc+=0x37; else rc+=0x30;
    uartputc(rc);
    if(rb==0) break;
  }
  uartputc(0x20);
}

void hexstring ( unsigned int d )
{
  hexstrings(d);
  uartputc(0x0D);
  uartputc(0x0A);
}
// eof my debug