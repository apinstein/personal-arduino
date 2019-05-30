#include "SevSeg.h"

SevSeg sevseg;

/**
 * Documentation
 * 
 * 1. Joystick: 
 * - X,Y to analog X_Pin, Y_Pin
 * - GND -> to any ground
 * - +5V -> to a +5V source pin
 * 2. Buzzer
 * - + to digital buzzerPin
 * - - to any GND
 * 3. 4-digit, 7-segment display, see setup() below
 * - 220 ohm resistors on digit pins (should be 4 of them)
 */

// buzzer
int buzzerPin = 0;

// joystick
const int X_pin = 0; // analog pin connected to X output
const int Y_pin = 1; // analog pin connected to Y output

const int x_center = 495;
const int y_center = 495;
const int joystick_dead_zone = 100;

enum run_modes { 
  setting_timer       = 1, 
  running_timer       = 2, 
  running_stopwatch   = 3
};
enum run_modes current_mode = running_timer;
long initialTimerSeconds = 5; // 20 * 60;

// manage "no change" exit 
long setting_mode_timeout = 1;
long setting_mode_last_used_at_millis = 0;

// manage "repeat" of held-down time change
long setting_delta_timeout_ms = 500;
long setting_delta_last_used_at_millis = 0;

// scratch for debugging
char line[50];

long countdownSeconds;
long wantCountdownSeconds;
long t0;

void setup() {
  // This is the 4 Digit 7-Segment Display that comes with Lewis' arduino kit:
  //  Top Row:    1 A F  2 3 B
  //  Bottom Row: E D DP C G 4
  // It's a COMMON_CATHODE part
  // Wire it up w/resistors on the digit pin circuits
  byte numDigits = 4;
  byte digitPins[] = {2, 3, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected. Then, you only need to specify 7 segmentPins[]

  sevseg.begin(
    hardwareConfig, 
    numDigits, 
    digitPins, 
    segmentPins, 
    resistorsOnSegments,
    updateWithDelays, 
    leadingZeros, 
    disableDecPoint
  );
  sevseg.setBrightness(90);

  pinMode(buzzerPin,OUTPUT);

  // verify hardware
  runBuzzer(250);
  sevseg.blank();
  sevseg.setNumber(8888);
  for (int i = 0; i < 1000; i++) {
    sevseg.refreshDisplay();
    delay(1);
  }
  sevseg.blank();
  delay(100);
    
  //Serial.begin(115200); // CANNOT be used along with anything on pins 0 or 1 or they conflict.
  resetTimer(initialTimerSeconds);
}

void resetTimer(int t) {
  sprintf(line, "Reset to %d", t);
  Serial.println(line);
  countdownSeconds = t;
  t0 = millis();
}

boolean isJoystickXNeutral(int x) {
  return (x < x_center + joystick_dead_zone && x > x_center - joystick_dead_zone);
}
boolean isJoystickYNeutral(int y) {
  return (y < y_center + joystick_dead_zone && y > y_center - joystick_dead_zone);
}

void loop() {
  int x, y;
  long minutes, seconds;

  // monitor joystick for changes to timer setting
  x = analogRead(X_pin);
  y = 1023 - analogRead(Y_pin);

  // we activate joystick setting by moving it out of "center"
  if (current_mode != setting_timer && !isJoystickYNeutral(y)) {
      Serial.println("Entering setting mode");
      current_mode = setting_timer;
      wantCountdownSeconds = countdownSeconds;
      setting_delta_last_used_at_millis = 0;  // make sure it fires immediately once we enter this mode
      setting_mode_last_used_at_millis = millis();
  }
  if (current_mode != setting_timer && !isJoystickXNeutral(x)) {
    if (x > x_center) {
      current_mode = running_stopwatch;
      resetTimer(0);
    } else if (x < x_center) {
      current_mode = running_timer;
      resetTimer(wantCountdownSeconds);
    }
  }

  sprintf(line, "current_mode: %d\n", current_mode);
  //Serial.println(line);
  
  signed long deltaS = 0;
  long secondsElapsed, secondsLeft;
  switch (current_mode) {
    case setting_timer:
      if (!isJoystickYNeutral(y)) {
        if (y > 1000) {
          deltaS = 5 * 60;
        } else if (y > 900) {
          deltaS = 1 * 60;
        } else if (y > 800) {
          deltaS = 30;
        } else if (y > 700) {
          deltaS = 10;
        } else if (y >= y_center) {
          deltaS = 1;
        } else if (y > 400) {
          deltaS = -1;
        } else if (y > 300) {
          deltaS = -10;
        } else if (y > 200) {
          deltaS = -30;
        } else if (y > 100) {
          deltaS = -1 * 60;
        } else if (y >= 0) {
          deltaS = -5 * 60;
        }
  
        // since the loop is tight, we need to throttle this...
        if (millis() - setting_delta_last_used_at_millis > setting_delta_timeout_ms) {
          wantCountdownSeconds += deltaS;
  
          // clip at [0, 99:59]
          wantCountdownSeconds = max(0, wantCountdownSeconds);
          wantCountdownSeconds = min(99 * 60 + 59, wantCountdownSeconds);
          sprintf(line, "new time: %lu, delta s: %ld, y: %i", wantCountdownSeconds, deltaS, y);
          Serial.println(line);
  
          setting_delta_last_used_at_millis = millis();
        }
      }
      
      // display current setting
      minutes = wantCountdownSeconds / 60;
      seconds = wantCountdownSeconds % 60;
      
      sevseg.setNumber(minutes * 100 + seconds, 2);
      sevseg.refreshDisplay();

      // exit timeout mode if we are in the middle for setting_mode_timeout
      if (isJoystickYNeutral(y)) {
        //Serial.println("in center");
        Serial.println(wantCountdownSeconds);
        //Serial.println(millis() - setting_mode_last_used_at_millis);
        if (millis() - setting_mode_last_used_at_millis > setting_mode_timeout * 1000) {
          current_mode = running_timer;
          Serial.println("exiting center mode");

          // initial new timer
          resetTimer(wantCountdownSeconds);
        }
      } else {
        setting_mode_last_used_at_millis = millis();
      }
      break;
      
    case running_timer:
      secondsElapsed = (millis() - t0) / 1000;
      secondsLeft = countdownSeconds - secondsElapsed;
     
      if (secondsLeft == 0) {
        runBuzzer(2000);
      }
      
      minutes = secondsLeft / 60;
      seconds = secondsLeft % 60;
      
      sevseg.setNumber(minutes * 100 + seconds, 2);
      sevseg.refreshDisplay();
      break;

        
    case running_stopwatch:
      secondsElapsed = (millis() - t0) / 1000;
      
      minutes = secondsElapsed / 60;
      seconds = secondsElapsed % 60;
      
      sevseg.setNumber(minutes * 100 + seconds, 2);
      sevseg.refreshDisplay();
      break;
  }
}

void runBuzzer(int playForMillis) {
  long t0 = millis();
  while (millis() - t0 < playForMillis) {
      digitalWrite(buzzerPin,HIGH);
      delay(1);//wait for 1ms
      digitalWrite(buzzerPin,LOW);
      delay(1);//wait for 1ms
    }
}
