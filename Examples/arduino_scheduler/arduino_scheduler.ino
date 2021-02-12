//an example illustrating how the scheduler would work if it isn't a library
//functions are generally the same as those in the library
#include <LiquidCrystal.h>
#include <setjmp.h>
#include <dht.h>
#include <avr/interrupt.h>
#define DHT11_PIN A0     // Analog Pin A0 of Arduino is connected to DHT11 out pin
#define BUZZER_PIN 4

const int rs = 7, en = 8, d4 = 9, d5 = 10, d6 = 11, d7 = 12;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
dht DHT;

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

struct thread * curr = NULL;
jmp_buf kernel;

void link_thread(void(*func)(), size_t stack_size, int id) {

  struct thread * thread_struct = (thread *)malloc(sizeof(struct thread));
  if (curr == NULL) {
    curr = thread_struct;
  }
  thread_struct->next = curr->next;
  curr->next = thread_struct;
  thread_struct->call = func;
  thread_struct->flag = 0;
  thread_struct->delay_time = 0;
  thread_struct->stack_size = stack_size;
  thread_struct->id = id;
}

void initialize_threads() {
  //link_thread(&buzzer_func, 400, 0); //disabled because of how annoying it is
  link_thread(&display_func, 400, 1);
  link_thread(&led_func, 200, 2);
}

//disable interrupts while saving is happening as control is going to be released anyway
//noinline isn't actually necessary as setjmp longjmp can't be inlined
void __attribute__ ((noinline)) pause(unsigned long delay_amount) {
  cli();
  curr->delay_time = millis() + delay_amount;
  curr->flag = 1;
  if (setjmp(curr->env) == 0) {
    longjmp(kernel, 1);
  }
  sei();
}


void setup() {
  Serial.begin(9600); // open the serial port at 9600 bps:
  pinMode(13, OUTPUT);
  initialize_threads();
  lcd.begin(16, 2);
  lcd.print("Text!");
  lcd.setCursor(0, 1);
  lcd.print("BOTTOM TEXT");
  pinMode(BUZZER_PIN, OUTPUT); //initialize the buzzer pin as an output
  cli();
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

void display_func() {
  while (1) {
    pause(1000);
    int chk = DHT.read11(DHT11_PIN);
    lcd.clear();
    lcd.print("temp: ");
    lcd.print(DHT.temperature);
    lcd.setCursor(0, 1);
    lcd.print("humidity: ");
    lcd.print(DHT.humidity);
    pause(1000);
    lcd.clear();
    lcd.print("Hello!");
    lcd.setCursor(0, 1);
    lcd.print("World");
  }
}


void led_func() {
  while (1) {
    digitalWrite(13, LOW);
    pause(100);
    digitalWrite(13, HIGH);
    pause(100);
  }
}

//some 
void buzzer_func() {
  unsigned char i;
  while (1) {
    for (i = 0; i < 80; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      pause(4);//wait for 4ms
      digitalWrite(BUZZER_PIN, LOW);
      pause(4);//wait for 4ms
    }
    pause(1000);
  }
}

//wrapper to prevent scheduler from pausing itself incase of weirdness
void run(){
  sei();
  curr->call();
}


//main program body
void loop() {
  cli();
  volatile uint16_t esp;
  struct thread* l_curr = curr;
  int mem_size = 0;
  int offst = 0;
  //find the total requested memory usage
  do {
    mem_size += l_curr->stack_size;
  } while ((l_curr = l_curr->next) != curr);
  //allocate memory for child threads
  char mem[mem_size + 1]; //declared in loop() to prevent reclamation
  mem[mem_size] = '@'; //prevent optimization from ignoring mem
  //distribute memory between threads
  do {
    l_curr->stack = (uint16_t) &mem[mem_size - offst - 1]; 
    offst += l_curr->stack_size;
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
        run();
      }
    }
    //busy wait for the next thread to run (could replace this with something a bit smarter)
    sei();
    while ((curr = curr->next)->delay_time > millis());
    cli();
  }
}
//never blocks interrupts so no need to do explicitly do reti?
ISR(TIMER1_COMPA_vect,ISR_NOBLOCK) {
  //relinquish control to supervisor
  pause(1);
}
