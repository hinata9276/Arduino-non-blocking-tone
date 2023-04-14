#include "pitches.h"
 // Define pin
#define buzzer 3

char data;                                      //data to be received from UART                     
volatile unsigned int timer1_counter0 = 0;      //virtual timer1.0, for non-blocking tone playback control
volatile unsigned int timer1_counter1 = 0;      //virtual timer1.1, for monitoring tone loop
volatile bool timer1_overflow0 = false;
volatile bool timer1_overflow1 = false;
unsigned int timer1_target0 = 0;
unsigned int timer1_target1 = 0;
int column_count = 0;                           //for tone plaback control
bool tone_end = true;                           //tone length flag

int canonD[2][56]={ {NOTE_A6,NOTE_FS6,NOTE_G6,NOTE_A6,NOTE_FS6,NOTE_G6,NOTE_A6,NOTE_A5,NOTE_B5,NOTE_CS6,NOTE_D6,NOTE_E6,NOTE_FS6,NOTE_G6,\
                     NOTE_FS6,NOTE_D6,NOTE_E6,NOTE_FS6,NOTE_FS5,NOTE_G5,NOTE_A5,NOTE_B5,NOTE_A5,NOTE_G5,NOTE_A5,NOTE_FS5,NOTE_G5,NOTE_A5,\
                     NOTE_G5,NOTE_B5,NOTE_A5,NOTE_G5,NOTE_FS5,NOTE_E5,NOTE_FS5,NOTE_E5,NOTE_D5,NOTE_E5,NOTE_FS5,NOTE_G5,NOTE_A5,NOTE_B5,\
                     NOTE_G5,NOTE_B5,NOTE_A5,NOTE_B5,NOTE_CS6,NOTE_D6,NOTE_A5,NOTE_B5,NOTE_CS6,NOTE_D6,NOTE_E6,NOTE_FS6,NOTE_G6,NOTE_A6}, \
                    { 4,8,8,4,8,8,8,8,8,8,8,8,8,8,\
                      4,8,8,4,8,8,8,8,8,8,8,8,8,8,\
                      4,8,8,4,8,8,8,8,8,8,8,8,8,8,\
                      4,8,8,4,8,8,8,8,8,8,8,8,8,8} };

//Mario main theme melody
int mario_main[2][12]={ {NOTE_E6, NOTE_E6, MUTE, NOTE_E6, MUTE, NOTE_C6, NOTE_E6, MUTE, NOTE_G6, MUTE, NOTE_G5, MUTE},\
                        {8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4}};
int mario_one[2][22]={  {NOTE_C6, MUTE, NOTE_G5, MUTE, NOTE_E5, MUTE, NOTE_A5, NOTE_B5, NOTE_AS5, NOTE_A5,\
                         NOTE_G5, NOTE_E6, NOTE_G6, NOTE_A6, NOTE_F6, NOTE_G6, MUTE, NOTE_E6, NOTE_C6, NOTE_D6, NOTE_B5, MUTE},\
                        {4, 8, 4, 8, 4, 8, 4, 4, 8, 4, 4, 4, 4, 4, 8, 8, 8, 4, 8, 8, 4, 8}};
int mario_two[2][15]={  {NOTE_C4, NOTE_G6, NOTE_FS6, NOTE_F6, NOTE_DS6, NOTE_C5, NOTE_E6,\
                         NOTE_F5, NOTE_GS5, NOTE_A5, NOTE_C6, NOTE_C5, NOTE_A5, NOTE_C6, NOTE_D6},\
                        {4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8}};
int mario_three[2][12]={{NOTE_C4, NOTE_G6, NOTE_FS6, NOTE_F6, NOTE_DS6, NOTE_C5, NOTE_E6,\
                         MUTE, NOTE_C7, NOTE_C7, NOTE_C7, NOTE_G5},\
                        {4, 8, 8, 8, 8, 8, 8, 8, 4, 8, 4, 4}};
int mario_four[2][10]={{ MUTE, NOTE_DS6, MUTE, NOTE_D6, MUTE, NOTE_C6, MUTE, NOTE_G5, NOTE_G5, NOTE_C5},\
                        {4, 4, 8, 4, 8, 4, 8, 8, 4, 4}};
int mario_five[2][13]={{ NOTE_C6, NOTE_C6, MUTE, NOTE_C6, MUTE, NOTE_C6, NOTE_D6, NOTE_E6, NOTE_C6, MUTE, NOTE_A5, NOTE_G5, NOTE_G3},\
                        {8, 8, 8, 8, 8, 8, 4, 8, 8, 8, 8, 4, 4}};
int mario_six[2][13]={{ NOTE_C6, NOTE_C6, MUTE, NOTE_C6, MUTE, NOTE_C6, NOTE_D6, NOTE_E6, NOTE_G4, MUTE, NOTE_C4, MUTE, NOTE_G3},\
                        {8, 8, 8, 8, 8, 8, 8, 8, 4, 8, 8, 4, 4}};

int tone_buffer[2][220];                         //max 219 notes at a time, last column is reserved as tone ending marker

void setup() {
  // put your setup code here, to run once:
  pinMode(buzzer, OUTPUT);
  Serial.begin(9600);
  for(int i = 0; i<57; i++){                    //clear buffer
    tone_buffer[0][i] = 0;
    tone_buffer[1][i] = 0;
  }
  TCCR1A = 0;                                   // set entire TCCR1A register to 0
  TCCR1B = 0;                                   // same for TCCR1B
  TCNT1  = 0;                                   //initialize counter value to 0
  // set compare match register for 128hz increments
  OCR1A = 15624;                                // = (16*10^6) / (128*8) - 1 (must be <65536)
  TCCR1B |= (1 << WGM12);                       // turn on CTC mode
  TCCR1B |= (1 << CS11);                        // Set CS12 bits for 8 prescaler
  TIMSK1 |= (1 << OCIE1A);                      // enable timer compare interrupt
}
ISR(TIMER1_COMPA_vect){                         //trigger at 128Hz
  //Virtual timer is created with timer 1 since Timer 2 is being used for tone() library so it is not possible
  //to utilize Timer 2 for other purposes. Virtual timer is useful to execute delay without blocking the main program flow.
  //Tone of higher note length will experience quantization error, but the slight change in note lenght will not likely be
  //audible, same thing goes to the program flow part, only use it for program that is insensitive to timing.
  //Lowering quantization error requires higher ISR frequency, but may affect the performance of the main program. However  
  //ISR of lower frequency can accomodate more virtual timers since it has more time to check for all virtual timer parameters.
  //It is technically no limit on how much virtual timers can be created with this approach, but do keep in mind that there 
  //is a tradeoff between virtual timer quantization error and main program performance.

  if(timer1_counter0 < timer1_target0){         //for tone delay control 
    timer1_counter0++;
    timer1_overflow0 = false;
  }
  else{
    timer1_overflow0 = true;
  }

  if(timer1_counter1 < timer1_target1){         //for program flow control if any
    timer1_counter1++;
    timer1_overflow1 = false;
  }
  else{
    timer1_overflow1 = true;
  }
}
void loop() {
  if(Serial.available()){
    data = Serial.read();
  }
  else{
    data = '0';                                 //prevent it from executing if no match character is found
  }
  switch (data){
    case 'a':                                   //play canonD music
      if(tone_end == true){
        music2_1();
        music2_2();
        music2_1();
      }
      break;
    case 'b':                                   //play SuperMario muusic
      if(tone_end == true){
        music();
      }
      break;
    case 'k':                                   //key pressed
      if(tone_end == true){
        play(NOTE_A5, 8);
      }
      break;
    case 'r':                                   //error tone
      if(tone_end == true){
        play(NOTE_A5, 2);
      }
      break;
    case 's':                                   //Parameter set tone
      if(tone_end == true){
        play(NOTE_A5, 16);
        play(MUTE, 16);
        play(NOTE_A5, 16);
      }
      break;
    case 'x':                                   //exit editing mode
      if(tone_end == true){
        play(NOTE_A6, 16);
        play(MUTE, 16);
        play(NOTE_A5, 16);
      }
      break;
    default:
      break;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////
  if(timer1_overflow0 == true){                         //if virtual timer1.0 elapsed 
    if(tone_end == false){                              //if tone is not ended  
      column_count++;                                   //proceed to scan next column
      if(tone_buffer[0][column_count-1] > 1){           //if the specific column has data
        tone(buzzer,tone_buffer[0][column_count-1],tone_buffer[1][column_count-1]); //play note
        timer1_delay(0, tone_buffer[1][column_count-1]);  //delay for the specified note length
      }
      else if(tone_buffer[0][column_count-1] == 1){     //if the specific column is 1
        noTone(buzzer);                                 //mute
        timer1_delay(0, tone_buffer[1][column_count-1]);  //delay for the specified note length
      }
      else{
        tone_end = true;                          //stop ask to play tone
        column_count = 0;                         //reset column counter for next tone playback
        for(int i = 0; i<220; i++){               //clear buffer
          tone_buffer[0][i] = 0;
          tone_buffer[1][i] = 0;
        }
      }
    }
    else{                                       //if tone is ended, force tone end just in case and clear all buffer
      tone_end = true;                          //stop ask to play tone
      column_count = 0;                         //reset column counter for next tone playback
      for(int i = 0; i<220; i++){               //clear buffer
        tone_buffer[0][i] = 0;
        tone_buffer[1][i] = 0;
      }
    }                                           
  }
}

void play(int pitch, int note){                 //calculate and buffer up the tone to be played
  int noteDuration = 1000 / note;
  if(pitch == 0){                               //if a silent note exists
    pitch = 1;                                 //overwrite it so that to use no_tone()
  }
  if(tone_end == true){                         //if first time play will store to 1st column data
    tone_buffer[0][0] = pitch;
    tone_buffer[1][0] = noteDuration;
    tone_end = false;                           //tone length not ended, till tone end flag being marked as true again in main loop
  }
  else{                                         //if play() command keep coming after that will queue up the tone to be generated
    for(int i = 1; i<219; i++){                 //if found zero array will push in the number
      if(tone_buffer[0][i] == 0){               //then break the check loop
        tone_buffer[0][i] = pitch;
        tone_buffer[1][i] = noteDuration;
        break;
      }                                         //last column is reserved for tone ending marker, max 219 tones at a time
    }
  }
  return;
}

void music(){                                   //function is pretty much self-explanatory
  for(int i=0; i<56; i++){
    play(canonD[0][i],canonD[1][i]);
  }
  return;
}             
void music2_1(){                                  //play mario music main
  for(int i=0; i<12; i++){
    play(mario_main[0][i],mario_main[1][i]);
  }
}
void music2_2(){                                  //play mario music chorus
  for(int i=0; i<2; i++){
    for(int i=0; i<22; i++){
      play(mario_one[0][i],mario_one[1][i]);
    }
  }
  for(int i=0; i<2; i++){
    for(int i=0; i<15; i++){
      play(mario_two[0][i],mario_two[1][i]);
    }
    for(int i=0; i<12; i++){
      play(mario_three[0][i],mario_three[1][i]);
    }
    for(int i=0; i<15; i++){
      play(mario_two[0][i],mario_two[1][i]);
    }
    for(int i=0; i<10; i++){
      play(mario_four[0][i],mario_four[1][i]);
    }
  }
  for(int i=0; i<13; i++){
    play(mario_five[0][i],mario_five[1][i]);
  }
  for(int i=0; i<13; i++){
    play(mario_six[0][i],mario_six[1][i]);
  }
  for(int i=0; i<13; i++){
    play(mario_five[0][i],mario_five[1][i]);
  }
}

void timer1_delay(int no, int timex){
  switch(no){
    case 0:                                      //Virtual timer 1.0 in ms resolution
      timer1_target0 = timex * 10 / 77 ;         //need to consider value rounding of integer division, avoid floating point operation to speed up machine
      timer1_counter0 = 0;
      timer1_overflow0 = false;
      break;
    case 1:
      timer1_target1 = timex * 128;               //Virtual timer1.1 in sec resolution
      timer1_counter1 = 0;
      timer1_overflow1 = false;
    default:
      break;
  }
}
