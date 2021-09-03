/*
  SF  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include "SD.h"
#include <MTP.h>
#include <SDMTPClass.h>
#include "TimeLib.h"


#define USE_BUILTIN_SDCARD
#if defined(USE_BUILTIN_SDCARD) && defined(BUILTIN_SDCARD)
#define CS_SD  BUILTIN_SDCARD
#else
#define CS_SD 10
#endif
#define SPI_SPEED SD_SCK_MHZ(16)  // adjust to sd card 


// LittleFS supports creating file systems (FS) in multiple memory types.  Depending on the
// memory type you want to use you would uncomment one of the following constructors

//SDClass myfs;  // Used to create FS on SD Card either built-in or external

File dataFile;  // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

uint8_t active_storage = 0;

// Add in MTPD objects
MTPStorage_SD storage;
MTPD       mtpd(&storage);

#define COUNT_MYFS  2  // could do by count, but can limit how many are created...
SDMTPClass myfs[] = {
                      {mtpd, storage, "SDIO", CS_SD}, 
                      {mtpd, storage, "SPI8", 8, 9, SHARED_SPI, SPI_SPEED}
                    };
//SDMTPClass myfs(mtpd, storage, "SD10", 10, 0xff, SHARED_SPI, SPI_SPEED);

void setup()
{

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  if (CrashReport) {
    Serial.print(CrashReport);
  }
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  Serial.println("Initializing SD ...");

  mtpd.begin();
  Serial.printf("Date: %u/%u/%u %u:%u:%u\n", day(), month(), year(),
                hour(), minute(), second());

  // Try to add all of them. 
  bool storage_added = false;
  for (uint8_t i = 0 ; i < COUNT_MYFS; i++) {
    storage_added |= myfs[i].init(true);
  }
  if (!storage_added) {
    Serial.println("Failed to add any valid storage objects");
    pinMode(13, OUTPUT);
    while (1) {
      digitalToggleFast(13);
      delay(250);
    }
  }

  //myfs2.init(true);

  Serial.println("SD initialized.");

  menu();

}

void loop()
{
  if ( Serial.available() ) {
    uint8_t command = Serial.read();
    int ch = Serial.read();
    uint8_t temp = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ') ch = Serial.read();

    switch (command) {
      case '1':
        {
          // first dump list of storages:
          uint32_t fsCount = storage.getFSCount();
          Serial.printf("\nDump Storage list(%u)\n", fsCount);
          for (uint32_t ii = 0; ii < fsCount; ii++) {
            Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii, mtpd.Store2Storage(ii), storage.getStoreName(ii), (uint32_t)storage.getStoreFS(ii));
          }
          Serial.println("\nDump Index List");
          storage.dumpIndexList();
        }
        break;  
      case '2':
        Serial.printf("Drive # %d Selected\n", active_storage);
        active_storage = temp;
        break;
      case 'l': listFiles(); break;
      case 'e': eraseFiles(); break;
      case 's':
      {
        Serial.println("\nLogging Data!!!");
        write_data = true;   // sets flag to continue to write data until new command is received
        // opens a file or creates a file if not present,  FILE_WRITE will append data to
        // to the file created.
        dataFile = myfs[active_storage].open("datalog.txt", FILE_WRITE);
        logData();
      }
      break;
      case 'x': stopLogging(); break;
      case'r':
        Serial.println("Reset");
        mtpd.send_DeviceResetEvent();
        break;
      case 'd': dumpLog(); break;
      case '\r':
      case '\n':
      case 'h': menu(); break;
    }
    while (Serial.read() != -1) ; // remove rest of characters.
  }
  else 
  { 
    mtpd.loop();
  }

  if (write_data) logData();
}

void logData()
{
  // make a string for assembling the data to log:
  String dataString = "";

  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    // print to the serial port too:
    Serial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging()
{
  Serial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  Serial.printf("Records written = %d\n", record_count);
  mtpd.send_DeviceResetEvent();
}


void dumpLog()
{
  Serial.println("\nDumping Log!!!");
  // open the file.
  dataFile = myfs[active_storage].open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void menu()
{
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List Drives (Step 1)");
  Serial.println("\t2 - Select Drive for Logging (Step 2)");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk");
  Serial.println("\ts - Start Logging data (Restarting logger will append records to existing log)");
  Serial.println("\tx - Stop Logging data");
  Serial.println("\td - Dump Log");
  Serial.println("\tr - Reset MTP");
  Serial.println("\th - Menu");
  Serial.println();
}

void listFiles()
{
  Serial.print("\n Space Used = ");
  Serial.println(myfs[active_storage].usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(myfs[active_storage].totalSize());

  printDirectory(myfs[active_storage]);
}

extern PFsLib pfsLIB;
void eraseFiles()
{

  PFsVolume partVol;
  if (!partVol.begin(myfs[active_storage].sdfs.card(), true, 1)) {
    Serial.println("Failed to initialize partition");
    return;
  }
  if (pfsLIB.formatter(partVol)) {
    Serial.println("\nFiles erased !");
    mtpd.send_DeviceResetEvent();
  }
}

void printDirectory(FS &fs) {
  Serial.println("Directory\n---------");
  printDirectory(fs.open("/"), 0);
  Serial.println();
}

void printDirectory(File dir, int numSpaces) {
  while (true) {
    File entry = dir.openNextFile();
    if (! entry) {
      //Serial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      printSpaces(36 - numSpaces - strlen(entry.name()));
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}


uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ') ch = Serial.read();
  if ((ch < '0') || (ch > '9')) return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = Serial.read(); 
  }
  return return_value;  
}