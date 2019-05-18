#include "SevSeg.h"

SevSeg sevseg;

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

  resetTimer(20 * 60);
  
  Serial.begin(115200);
}

long countdownSeconds;
long t0;

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

// manage "no change" exit 
long setting_mode_timeout = 1;
long setting_mode_last_used_at_millis = 0;

// manage "repeat" of held-down time change
long setting_delta_timeout_ms = 500;
long setting_delta_last_used_at_millis = 0;

// scratch for debugging
char line[50];

void resetTimer(int t) {
  sprintf(line, "Reset to %d", t);
  Serial.println(line);
  countdownSeconds = t;
  t0 = millis();
}

boolean isJoystickYNeutral(int y) {
  return (y < y_center + joystick_dead_zone && y > y_center - joystick_dead_zone);
}

void loop() {
  int x, y;
  long minutes, seconds;
  long wantCountdownSeconds;

  // monitor joystick for changes to timer setting
  x = analogRead(X_pin);
  y = 1023 - analogRead(Y_pin);

  sprintf(line, "x = %d\ny = %d", x, y);
  //Serial.println(line);

  // we activate joystick setting by moving it out of "center"
  if (current_mode == running_timer && !isJoystickYNeutral(y)) {
      Serial.println("Entering setting mode");
      current_mode = setting_timer;
      wantCountdownSeconds = countdownSeconds;
      setting_delta_last_used_at_millis = 0;  // make sure it fires immediately once we enter this mode
      setting_mode_last_used_at_millis = millis();
      
  }

  sprintf(line, "current_mode: %d\n", current_mode);
  //Serial.println(line);
  
  signed long deltaS = 0;
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
      //delay(100);
      break;
      
    case running_timer:
      long secondsElapsed = (millis() - t0) / 1000;
      long secondsLeft = countdownSeconds - secondsElapsed;
      minutes = secondsLeft / 60;
      seconds = secondsLeft % 60;
      
      sevseg.setNumber(minutes * 100 + seconds, 2);
      sevseg.refreshDisplay();
      break;
  }

}
