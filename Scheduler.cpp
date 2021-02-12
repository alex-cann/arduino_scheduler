/*
  Scheduler.cpp - Library for premptible scheduling.
  Created by Alex Cann, February, 2021.
  Released into the public domain.
*/
#include "Scheduler.h"


struct thread * curr=NULL;
jmp_buf kernel;

void setupInterrupt()
{
  //setup that doesn't have a specific time goes here
  //may change to taking a LL of setup information
  cli();// disable interrupts
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B = (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 = (1 << OCIE1A);
}

void linkThread(void(*func)(), size_t stackSize, int id)
{
  struct thread * threadStruct = (thread *)malloc(sizeof(struct thread));
  if (curr == NULL) {
    curr = threadStruct;
  }
  threadStruct->next = curr->next;
  curr->next = threadStruct;
  threadStruct->call = func;
  threadStruct->flag = 0;
  threadStruct->delayTime = 0;
  threadStruct->stackSize = stackSize;
  threadStruct->id = id;
}

//wrapper to prevent scheduler from pausing itself incase of weirdness
void runThread()
{
  sei();
  curr->call();
}

//disable interrupts while saving is happening as control is going to be released anyway
void pause(unsigned long delayAmount)
{
  cli();
  curr->delayTime = millis() + delayAmount;
  curr->flag = 1;
  if (setjmp(curr->env) == 0) {
    longjmp(kernel, 1);
  }
  sei();
}

void loop()
{
  cli();
  volatile uint16_t esp;
  struct thread* l_curr = curr;
  int memSize = 0;
  int offset = 0;
  //find the total requested memory usage
  do {
    memSize += l_curr->stackSize;
  } while ((l_curr = l_curr->next) != curr);
  //allocate memory for child threads
  char mem[memSize + 1]; //declared in loop() to prevent reclamation
  mem[memSize] = '@'; //prevent optimization from ignoring mem
  //distribute memory between threads
  do {
    l_curr->stack = (uint16_t) &mem[memSize - offset - 1]; 
    offset += l_curr->stackSize;
  } while ((l_curr = l_curr->next) != curr);
  while (1) {
    TCNT1=0; //reset counter
    if (curr->flag == 1) {
      if (setjmp(kernel) == 0) {
        longjmp(curr->env, 1);
      }
    } else {
      if (setjmp(kernel) == 0) {
        //don't have to fix this as longjmp will clober it
        SP = curr->stack;
        runThread();
      }
    }
    //busy wait for the next thread to run (could replace this with something a bit smarter)
    sei();
    while ((curr = curr->next)->delayTime > millis());
    cli();
  }
}

ISR(TIMER1_COMPA_vect,ISR_NOBLOCK) {
  //relinquish control to supervisor
  pause(1);
}