#ifndef Scheduler_h
#define Scheduler_h
#include <avr/interrupt.h>
#include <setjmp.h>
#include <Arduino.h>
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
  volatile unsigned long delayTime; //time till next wakeup in microseconds
  volatile uint16_t stack; //stack pointer
  size_t stackSize;
  int id;
};


void linkThread(void(*func)(), size_t stackSize, int id);
void runThreads();
void pause(unsigned long delayAmmount);
void runScheduler();
void loop(); //defines loop for the user
void setupInterrupt();

//never blocks interrupts so no need to do explicitly do reti?
//this is probably bad but I can't find any direct reason why
//ISR(TIMER1_COMPA_vect,ISR_NOBLOCK);
#endif
