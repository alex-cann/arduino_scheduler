#include<Scheduler.h>


void setup() {
  Serial.begin(9600);
  linkThread(&hi,400,1);
  linkThread(&bye,400,2);
  setupInterrupt();
}

void hi(){
  //write one hi every 100 milliseconds
  Serial.print("hi");
  pause(100);
}

void bye(){
  //write one bye every 300 milliseconds
  Serial.print("bye");
  pause(300);
}
