#ifndef Scheduler_h
#define Scheduler_h
#include "WConstants.h"
#include "Arduino.h"
#include <avr/interrupt.h>
#include <setjmp.h>
struct thread {
  jmp_buf env;
  struct thread * next;
  /*
     0 -> hasn't been run yet
     1 -> sleeping
     2 -> not in use
  */
  volatile int flag;
  void (*call)(); //thread body
  volatile unsigned long delay_time; //time till next wakeup in microseconds
  volatile uint16_t stack; //stack pointer
  size_t stack_size;
  int id;
};

class Scheduler
{
  public:
    Scheduler();
    void link_thread(void(*func)(), size_t stack_size, int id);
    void run_sched();
    void pause()
  private:
    void run_thread();
    struct thread * curr = NULL;
    jmp_buf kernel;
};


//never blocks interrupts so no need to do explicitly do reti?
ISR(TIMER1_COMPA_vect,ISR_NOBLOCK) {
  //relinquish control to supervisor
  pause(1);
}
#endif
