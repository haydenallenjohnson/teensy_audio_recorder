#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <TimeLib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1305.h>

//------------------------------------------------------------------
// define modifiable parameters to control behaviour
const int initialPause = 10000;               // ms (display status before starting recording)
const unsigned long long fileLength = 60000;  // ms
const int pauseLength = 100;                 // ms (between files)
// I am not responsible for issues caused by changes made outside
// of this block
//------------------------------------------------------------------

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=175,201
AudioRecordQueue         queue1;         //xy=352,219
AudioConnection          patchCord1(i2s1, 0, queue1, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=174,253
// GUItool: end automatically generated code

// specify audio input
const int audioInput = AUDIO_INPUT_LINEIN;

// specify SD card chip select
#define SDCARD_CS_PIN    BUILTIN_SDCARD  // 10 for audio board sd

// define OLED pins
#define OLED_CS 35
#define OLED_DC 34
#define OLED_RESET 33

// initialize display
#define OLED_ADDRESS 0x3C
Adafruit_SSD1305 display(128, 64, &SPI, OLED_DC, OLED_RESET, OLED_CS, 7000000UL);
char displayLine[22];

// define analog pin for reading battery
#define BATTERY_PIN  40
int batteryPercent;   // percent battery remaining

// initialize timers - note that the global timer will rollover 
// after about 50 days, but that's not a problem at the moment based
// on battery life and recording space
// unsigned long globalTimer;
unsigned long recordingTimer;
unsigned long pauseTimer;

// initialize recording specifier with value false
bool currentlyRecording = false;

// preallocate file objects and filename
File root;
File f;
File flog;
const char LOG_FILE_NAME[10] = {'L','O','G','.','T','X','T'};
File frec;
char fileName[16];
char logFileLine[50];

// preallocate datetime string
char timeString[22];

// initialize file number counters
int currentFileNumber = 0;
int newFileNumber;

// specify sd card size in bytes (determined using CardInfo example)
const unsigned long long cardSize = 31700000000;
const unsigned long long fileSize = 2*44100*fileLength/1000;
unsigned long long memoryUsed;
unsigned long long memoryAvail;

void setup() {

  // set battery monitor pin to input
  pinMode(BATTERY_PIN, INPUT);

  // initialize display
  display.begin(OLED_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Power on");
  display.display();
  delay(2000);

  // initialize clock
  setSyncProvider(getTeensy3Time);
  Serial.begin(115200);
  delay(100);
  if (Serial.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      Teensy3Clock.set(t); // set the RTC
      setTime(t);
    }
  }
  
  // allocate audio memory
  AudioMemory(60);

  //initialize audio board
  if (!sgtl5000_1.enable()) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Audio board error");
    display.display();
    while (1) { // stop
    }
  }
  sgtl5000_1.inputSelect(audioInput);

  // Initialize the SD card
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here if no SD card, but print a message
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("SD card error");
    display.display();
    while (1) { // stop
    }
  }

  // check existing recording files to determine file number
  root = SD.open("/");
  while (true) {
    f = root.openNextFile();
    if (! f) {
      break;
    }
    sscanf(f.name(), "%s", fileName);
    if ((fileName[0]=='R') & (fileName[1]=='E') & (fileName[2]=='C')) {
      newFileNumber = atoi(fileName + 3);
      if (newFileNumber > currentFileNumber) {
        currentFileNumber = newFileNumber;
      }
    }
    f.close();
  }
  currentFileNumber += 1;

  // check amount of memory used
  memoryUsed = dirSize(root);
  memoryAvail = cardSize-memoryUsed;
  
  root.close();  // close root directory

  // compute battery percent
  batteryPercent = computeBatteryPercent();
  
  if (memoryAvail<=fileSize) {  // if memory is full
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Memory full");
    display.display();
    while (true) { 
      // stop
    }
  } else {
    // display status
    display.clearDisplay();
    display.setCursor(0,0);
    sprintf(timeString,"%04d-%02d-%02d %02d:%02d PDT", year(), month(), day(), hour(), minute());
    display.println(timeString);
    sprintf(displayLine, "Memory used: %5llu MB", memoryUsed/1048576);
    display.println(displayLine);
    sprintf(displayLine, "Free space: %6llu MB", memoryAvail/1048576);
    display.println(displayLine);
    sprintf(displayLine, "Rec capacity: %4llu hr", memoryAvail/317520000);
    display.println(displayLine);
    sprintf(displayLine, "Battery: %9d %%", batteryPercent);
    display.println(displayLine);
    display.display();
    delay(initialPause);
  
    // update log file
    flog = SD.open(LOG_FILE_NAME, FILE_WRITE);
    flog.println("Start new deployment");
    flog.close();

    // start the first recording
    startRecording(currentFileNumber);
  }
}

void loop() {
  if (currentlyRecording) {
    if (millis()-recordingTimer < fileLength) {
      continueRecording();
    } 
    else {
      stopRecording();
    }
  }
  else { // if not currently recording
    // check amount of memory used
    root = SD.open("/");
    memoryUsed = dirSize(root);
    memoryAvail = cardSize-memoryUsed;
    root.close();  // close root directory

    if (millis()-pauseTimer < pauseLength) {
      delay(100); // wait before checking anything again
    } 
    else if (memoryAvail>fileSize) {
      startRecording(currentFileNumber);
    }
    else {  
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("Memory full");
      display.display();
      while (1) { // stop
      }
    }
  }
}

void startRecording(int fileNumber) {
  // create file name
  sprintf(fileName, "REC%05d.RAW", fileNumber);
  // display status
  display.clearDisplay();
  display.setCursor(0,0);
  sprintf(timeString,"%04d-%02d-%02d %02d:%02d PDT", year(), month(), day(), hour(), minute());
  display.println(timeString);
  sprintf(displayLine, "Memory used: %5llu MB", memoryUsed/1048576);
  display.println(displayLine);
  sprintf(displayLine, "Free space: %6llu MB", memoryAvail/1048576);
  display.println(displayLine);
  sprintf(displayLine, "Rec capacity: %4llu hr", memoryAvail/317520000);
  display.println(displayLine);
  batteryPercent = computeBatteryPercent();
  sprintf(displayLine, "Battery: %9d %%", batteryPercent);
  display.println(displayLine);
  sprintf(displayLine, "File number: %5d", fileNumber);
  display.println(displayLine);
  display.println("Recording...");
  display.display();

  // update log file
  flog = SD.open(LOG_FILE_NAME, FILE_WRITE);
  sprintf(timeString,"%04d-%02d-%02d %02d:%02d:%02d PDT", year(), month(), day(), hour(), minute(), second());
  flog.print(timeString);
  flog.print("    ");
  sprintf(logFileLine, "%5d    %3d %%", fileNumber, batteryPercent);
  flog.println(logFileLine);
  flog.close();
  
  // open recording file and start audio queue
  currentlyRecording = true;
  frec = SD.open(fileName, FILE_WRITE);  // open file
  recordingTimer = millis();             // start recording timer
  queue1.begin();                        // initialize audio queue
}

void continueRecording() {
  if (queue1.available() >= 2) {
    byte buffer[512];
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    frec.write(buffer, 512);  // write bytes to sd card
  }
}

void stopRecording() {
  queue1.end();  // stop recording blocks into the queue
  // read the remaining blocks into the file
  while (queue1.available() > 0) {
    frec.write((byte*)queue1.readBuffer(), 256);
    queue1.freeBuffer();
  }
  frec.close();                // close file
  currentlyRecording = false;  // set recording specifier to false
  pauseTimer = millis();       // start pause timer
  currentFileNumber += 1;      // increment file number
}

unsigned long long dirSize(File dir) {
  // function to compute directory size
  unsigned long long currentSize = 0;
  dir.rewindDirectory();
  while (true) {
    File entry = dir.openNextFile();
    if (! entry) {
      return currentSize;
      break;
    }
    if (entry.isDirectory()) {
      currentSize += dirSize(entry);
    } else {
      currentSize += entry.size();
    }
    entry.close();
  }
}

int computeBatteryPercent() {
  // function to compute battery percentage
  int batVal = analogRead(BATTERY_PIN);
  batVal -= 520;
  batVal /= 4;
  if (batVal>100) {
    return 100;
  } else if (batVal<0) {
    return 0;
  } else {
    return batVal;
  }
  return batVal;
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

#define TIME_HEADER  "T"   // Header tag for serial time sync message

unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     return pctime;
     if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
  }
  return pctime;
}

void updateTimeString() {
  sprintf(timeString,"%04d-%02d-%02d %02d:%02d PDT", year(), month(), day(), hour(), minute());
}
