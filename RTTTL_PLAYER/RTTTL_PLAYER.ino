#include <ChibiOS_AVR.h>
#include <util/atomic.h>
#include "pitches.h"
#include "songs.h"

#define OCTAVE_OFFSET 0
volatile uint32_t count = 0;

const int tone_pin = 6;            //speaker output
const int trigPin = 2;    //Trig 핀 할당
const int echoPin = 4;    //Echo 핀 할당

int prev = 0;

const char* song  = {"MacGyver:d=4,o=6,b=150:8b4,8e5,8a5,8b5,a5,8e5,8b4,8p,8e5,8a5,8b5,8a5,8e5,b4,8p,8e5,8a5,8b5,a5,8e5,8b4,8p,8a5,8d,8c,8d,8c,8b5,8a5,8b4,8e5,8a5,8b5,a5,8e5,8b4,8p,8e5,8a5,8b5,8a5,8e5,b4,8p,8e5,8a5,8b5,a5,8e5,8b4,8p,8a5,8d,8c,8d,8c,8b5,8a5,b5,8p,2b5,8p,b5,8p,a5,d.,b5,8p,2b5,8p,8b5,8p,2a5,p,8c,8c,8c,8c,8c,8c,2b5,16p,8f#5,8a5,8p,2g5,8p,8c,8c,8p,b5,8a5,8b5,8a5,8g5,8p,e,2a5,16p,8c,8c,8p,2b5,8p,8f#5,8a5,8p,2g5,8p,8c,8c,8p,4b5,8a5,8b5,8a5,8g5,8p,4e,2a5,2b5,32p,8c,8b5,8a5,c,8b5,8a5,8d,8c,8b5,d,8c,8b5,e,8d,8e,f#,b5,g,8p,f#,f,b5,8g,8e,8b5,8f#,8d,8a5,8e,8c,8g5,8d,8b5,8g5,8c,8e5,8b5,8d5,8c,8b5,8a5,8g5,a#5,a5,8g,8g5,8d,8g5,8d#,8d#5,8a#5,8a5,8g5,8g4,8d5,8g4,8d#5,8g4,8a#4,8a4,8g4,8g4,8g4,8g4,8g4,8g4,8g4"};


bool bFound = false;
long duration, distance;

//------------------------------------------------------------------------------
// thread 1 
// 64 byte stack beyond task switch and interrupt needs

static THD_WORKING_AREA(waThread1, 64);

static msg_t Thread1(void *arg) {

  while (TRUE) {
#if   true
    //Trig 핀으로 10us의 pulse 발생
    digitalWrite(trigPin, LOW);        //Trig 핀 Low
    chThdSleepMicroseconds(2);            //2us 유지
    digitalWrite(trigPin, HIGH);    //Trig 핀 High
    chThdSleepMicroseconds(10);            //10us 유지
    digitalWrite(trigPin, LOW);        //Trig 핀 Low
 
    //Echo 핀으로 들어오는 펄스의 시간 측정
    duration = pulseIn(echoPin, HIGH);        //pulseIn함수가 호출되고 펄스가 입력될 때까지의 시간. us단위로 값을 리턴.
 
    //음파가 반사된 시간을 거리로 환산
    //음파의 속도는 340m/s 이므로 1cm를 이동하는데 약 29us.
    //따라서, 음파의 이동거리 = 왕복시간 / 1cm 이동 시간 / 2 이다.
    distance = duration / 29 / 2;        //센치미터로 환산
    Serial.println(distance);
    
    if(distance <= 6)
       bFound = true; 
    else
      bFound = false;
#else
   chThdSleepMilliseconds(50);
#endif
  }
  return 0;
}

//------------------------------------------------------------------------------
// thread 2 - high priority for playing RTTTL sound
// 200 byte stack beyond task switch and interrupt needs
static THD_WORKING_AREA(waThread2, 64);
 
static msg_t Thread2(void *arg) {
  int song_index;

  
  Serial.print("Thread2");  

  while (TRUE) {
    if(bFound){
      chThdSleepMilliseconds(10);
      song_index =0;
      song_index = random(119);
      strcpy_P(song,(char*)pgm_read_word(&(songs[song_index])));
      play_RTTTL(song);
      chThdSleepMilliseconds(500);
    }
    chThdSleepMilliseconds(500);
  }

  Serial.println("Thread2 return");  
  return 0;
}


void setup()
{

 Serial.begin(115200);
 randomSeed(analogRead(0));
 
 pinMode(trigPin, OUTPUT);    //Trig 핀 output으로 세팅
 pinMode(echoPin, INPUT);    //Echo 핀 input으로 세팅
    
 
  // initialize ChibiOS with interrupts disabled
  // ChibiOS will enable interrupts
   
  cli();
  halInit();
  chSysInit();


  // start blink thread
  chThdCreateStatic(waThread1, sizeof(waThread1),
        NORMALPRIO + 1, Thread1, NULL);
    
  // start print thread
  chThdCreateStatic(waThread2, sizeof(waThread2),
        NORMALPRIO + 2, Thread2, NULL);

}

#define isdigit(n) (n >= '0' && n <= '9')

void loop()
{
  // must insure increment is atomic
  // in case of context switch for print
}

void play_RTTTL(char* p){
 
  byte default_dur = 4;
  byte default_oct = 6;
  int bpm = 63;
  int num;
  long wholenote;
  long duration;
  byte note;
  byte scale;
 
   // Absolutely no error checking in here


  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  while(*p != ':') p++;    // ignore name
  p++;                     // skip ':'

  // get default duration
  if(*p == 'd')
  {
    p++; p++;              // skip "d="
    num = 0;
    while(isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    if(num > 0) default_dur = num;
    p++;                   // skip comma
  }

  // get default octave
  if(*p == 'o')
  {
    p++; p++;              // skip "o="
    num = *p++ - '0';
    if(num >= 3 && num <=7) default_oct = num;
    p++;                   // skip comma
  }

  // get BPM
  if(*p == 'b')
  {
    p++; p++;              // skip "b="
    num = 0;
    while(isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    bpm = num;
    p++;                   // skip colon
  }

  // BPM usually expresses the number of quarter notes per minute
  wholenote = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)


  // now begin note loop
  while(*p)
  {
    if(!bFound)
      return;
      
    // first, get note duration, if available
    num = 0;
    while(isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    
    if(num) duration = wholenote / num;
    else duration = wholenote / default_dur;  // we will need to check if we are a dotted note after

    // now get the note
    note = 0;

    switch(*p)
    {
      case 'c':
        note = 1;
        break;
      case 'd':
        note = 3;
        break;
      case 'e':
        note = 5;
        break;
      case 'f':
        note = 6;
        break;
      case 'g':
        note = 8;
        break;
      case 'a':
        note = 10;
        break;
      case 'b':
        note = 12;
        break;
      case 'p':
      default:
        note = 0;
    }
    p++;

    // now, get optional '#' sharp
    if(*p == '#')
    {
      note++;
      p++;
    }

    // now, get optional '.' dotted note
    if(*p == '.')
    {
      duration += duration/2;
      p++;
    }
  
    // now, get scale
    if(isdigit(*p))
    {
      scale = *p - '0';
      p++;
    }
    else
    {
      scale = default_oct;
    }

    scale += OCTAVE_OFFSET;

    if(*p == ',')
      p++;       // skip comma for next note (or we may be at the end)

    // now play the note

    if(note)
    {
      tone(tone_pin, notes[(scale - 4) * 12 + note]); // original - 4, not - 5 
        chThdSleepMilliseconds(duration);
//      delay(duration);    /* 일반 delay() 를 사용하면 멀티태스킹이 안된다.  chThdSleepMilliseconds()를 써야 한다. */
      noTone(tone_pin);
    }
    else
    {
      chThdSleepMilliseconds(duration);
 //     delay(duration);  /* 일반 delay() 를 사용하면 멀티태스킹이 안된다.  chThdSleepMilliseconds()를 써야 한다. */
    }
  }

  
}
