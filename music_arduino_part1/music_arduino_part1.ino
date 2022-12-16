/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be_1bit.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
#define TEST_MODE 1

#define SAMPLE_TIME 250  // ms, freq of 40000
#define TASK_TIME 100  // ms
#define SOUND_PIN  11
#define LED_PIN  13
#define BUTTON_PIN 7
#define BUF_SIZE 256

#define CLOCK_SYSTEM 16000000

/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;
bool mute;

bool value_old = false;


/**********************************************************
 * Function: play_bit
 *********************************************************/
void play_bit() {
  static int bitwise = 1;
  static unsigned char data = 0;
  static int music_count = 0;

    bitwise = (bitwise * 2);
    if (bitwise > 128) {
       bitwise = 1;
       #ifdef TEST_MODE 
          data = pgm_read_byte_near(music + music_count);
          music_count = (music_count + 1) % MUSIC_LEN;
       #else 
          if (Serial.available()>1) {
             data = Serial.read();
          }
       #endif
    }
    // OCR2A = data (byte)
    if(mute){ 
        digitalWrite(SOUND_PIN, 0);
    } else {

        digitalWrite(SOUND_PIN, (data & bitwise));
    }
}

/**********************************************************
 * Function: setup
 *********************************************************/
void setup() {
    // Initialize serial communications
    Serial.begin(115200);

    pinMode(SOUND_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);

    memset(buffer, 0, BUF_SIZE);

    timeOrig = millis();

    // Interrupt (Timer 1): CTC mode, compare with OCR1A, prescaling x1
	TCCR1A = 0;
	TCCR1B = 0;
	TCCR1B = _BV(WGM12) | _BV(CS10);
	TIMSK1 = _BV(OCIE1A);  // compare with R1A

	OCR1A = CLOCK_SYSTEM/(1 * (1000000 / SAMPLE_TIME)) - 1;  // 3999
}


ISR(TIMER1_COMPA_vect) {  // handler
    play_bit();
}


void task_button_n_led() {
    bool value = digitalRead(BUTTON_PIN);
    bool aux;
    if (value && !value_old) {
        noInterrupts();
        mute = !mute;
        aux = mute;

        interrupts();
    }

    digitalWrite(LED_PIN, aux);

    value_old = value;
}


/**********************************************************
 * Function: loop
 *********************************************************/
void loop() {
    unsigned long timeDiff;

    task_button_n_led();

    timeDiff = TASK_TIME - (millis() - timeOrig);
    timeOrig = timeOrig + TASK_TIME;

    delay(timeDiff);
}