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


File dataFile;  // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

uint8_t current_store = 0;

// Add in MTPD objects
MTPStorage_SD storage;
MTPD       mtpd(&storage);

SDMTPClass sdmtpSPI(mtpd, storage, "SPI8", 8, 9, SHARED_SPI, SPI_SPEED);

// Experiment add memory FS to mainly hold the storage index
// May want to wrap this all up as well
#include <LittleFS.h>
#include <LFS_MTP_Callback.h>
#define LFSRAM_SIZE 65536  // probably more than enough...
LittleFS_RAM lfsram;
LittleFSMTPCB lfsmtpcb;

LittleFS_Program lfsProg;  // Used to create FS on the Flash memory of the chip
LittleFSMTPCB    lfsmtpProgcb;

FS *myfs = &lfsProg; // current default FS...

static const uint32_t file_system_size = 1024 * 512;

#define DBGSerial Serial

// Quick and Dirty memory Stream...
class RAMStream : public Stream {
  public:
  // overrides for Stream
  virtual int available() {return (tail_ - head_); }
  virtual int read() {return (tail_ != head_)? buffer_[head_++] : -1;  }
  virtual int peek() {return (tail_ != head_)? buffer_[head_] : -1;  }

  // overrides for Print
  virtual size_t write(uint8_t b) {
    if (tail_ < sizeof(buffer_)) {
      buffer_[tail_++] = b;
      return 1;
    }
    return 0;
  }

  enum {BUFFER_SIZE=32768};
  uint8_t buffer_[BUFFER_SIZE];
  uint32_t head_ = 0;
  uint32_t tail_ = 0;
};

void setup()
{
  // setup to do quick and dirty ram stream until Serial or like is up...
  RAMStream rstream;
  // start up MTPD early which will if asked tell the MTP
  // host that we are busy, until we have finished setting
  // up...
  DBGSerial.begin(2000000);
  mtpd.PrintStream(&rstream);   // Setup which stream to use...

  mtpd.begin();

  // Open serial communications and wait for port to open:
  while (!DBGSerial && millis() < 5000) {
    // wait for serial port to connect.
  }
  
  // set to real stream
  mtpd.PrintStream(&DBGSerial);   // Setup which stream to use...
  int ch;
  while ((ch = rstream.read()) != -1) DBGSerial.write(ch);

  if (CrashReport) {
    DBGSerial.print(CrashReport);
  }
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  DBGSerial.printf("%u Initializing MTP Storage list ...", millis());

  DBGSerial.printf("Date: %u/%u/%u %u:%u:%u\n", day(), month(), year(),
                   hour(), minute(), second());

  // Lets add the Prorgram memory version:
  // checks that the LittFS program has started with the disk size specified
  if (lfsProg.begin(file_system_size)) {
    lfsmtpProgcb.set_formatLevel(true);  //sets formating to lowLevelFormat
    storage.addFilesystem(lfsProg, "Program", &lfsmtpProgcb, (uint32_t)(LittleFS*)&lfsProg);
  } else {
    Serial.println("Error starting Program Flash storage");
  }

// Lets add the external SD card.
  sdmtpSPI.init(true);

  // lets initialize a RAM drive.
  if (lfsram.begin (LFSRAM_SIZE)) {
    DBGSerial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    lfsmtpcb.set_formatLevel(true);  //sets formating to lowLevelFormat
    uint32_t istore = storage.addFilesystem(lfsram, "RAM", &lfsmtpcb, (uint32_t)(LittleFS*)&lfsram);
    if (istore != 0xFFFFFFFFUL) storage.setIndexStore(istore);
    DBGSerial.printf("Set Storage Index drive to %u\n", istore);
  }

  DBGSerial.printf("%u SD initialized.\n", millis());
  menu();

}

void loop()
{
  if ( DBGSerial.available() ) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
    uint8_t storage_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ') ch = DBGSerial.read();

    switch (command) {
    case '1':
    {
      // first dump list of storages:
      uint32_t fsCount = storage.getFSCount();
      DBGSerial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        DBGSerial.printf("store:%u storage:%x name:%s fs:%x\n", ii, mtpd.Store2Storage(ii), storage.getStoreName(ii), (uint32_t)storage.getStoreFS(ii));
      }
      DBGSerial.println("\nDump Index List");
      storage.dumpIndexList();
    }
    break;
    case '2':
    {
      if (storage_index < storage.getFSCount()) {
        DBGSerial.printf("Storage Index %u Name: %s Selected\n", storage_index, storage.getStoreName(storage_index));
        myfs = storage.getStoreFS(storage_index);
        current_store = storage_index;
      } else {
        DBGSerial.printf("Storage Index %u out of range\n", storage_index);
      }
    }
    break;

    case 'l': listFiles(); break;
    case 'e': eraseFiles(); break;
    case 's':
    {
      DBGSerial.println("\nLogging Data!!!");
      write_data = true;   // sets flag to continue to write data until new command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append data to
      // to the file created.
      dataFile = myfs->open("datalog.txt", FILE_WRITE);
      logData();
    }
    break;
    case 'x': stopLogging(); break;
    case'r':
      DBGSerial.println("Reset");
      mtpd.send_DeviceResetEvent();
      break;
    case 'd': dumpLog(); break;
    case '\r':
    case '\n':
    case 'h': menu(); break;
    }
    while (DBGSerial.read() != -1) ; // remove rest of characters.
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
    DBGSerial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    DBGSerial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging()
{
  DBGSerial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  DBGSerial.printf("Records written = %d\n", record_count);
  mtpd.send_DeviceResetEvent();
}


void dumpLog()
{
  DBGSerial.println("\nDumping Log!!!");
  // open the file.
  dataFile = myfs->open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      DBGSerial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    DBGSerial.println("error opening datalog.txt");
  }
}

void menu()
{
  DBGSerial.println();
  DBGSerial.println("Menu Options:");
  DBGSerial.println("\t1 - List Drives (Step 1)");
  DBGSerial.println("\t2 - Select Drive for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\te - Erase files on disk");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tr - Reset MTP");
  DBGSerial.println("\th - Menu");
  DBGSerial.println();
}

void listFiles()
{
  DBGSerial.print("\n Space Used = ");
  DBGSerial.println(myfs->usedSize());
  DBGSerial.print("Filesystem Size = ");
  DBGSerial.println(myfs->totalSize());

  printDirectory(myfs);
}

extern PFsLib pfsLIB;
void eraseFiles()
{

#if 1
  // Lets try asking storage for enough stuff to call it's format code
  bool send_device_reset = false;
  MTPStorageInterfaceCB *callback = storage.getCallback(current_store);
  uint32_t user_token = storage.getUserToken(current_store);
  if (callback) {
    send_device_reset = (callback->formatStore(&storage, current_store, user_token, 
          0, true) == MTPStorageInterfaceCB::FORMAT_SUCCESSFUL);
  } 
  if (send_device_reset) {
    DBGSerial.println("\nFiles erased !");
    mtpd.send_DeviceResetEvent();    
  } else {
    DBGSerial.println("\n failed !");
  }

#else  
  PFsVolume partVol;
  if (!partVol.begin(myfs->sdfs.card(), true, 1)) {
    DBGSerial.println("Failed to initialize partition");
    return;
  }
  if (pfsLIB.formatter(partVol)) {
    DBGSerial.println("\nFiles erased !");
    mtpd.send_DeviceResetEvent();
  }
#endif  
}

void printDirectory(FS *pfs) {
  DBGSerial.println("Directory\n---------");
  printDirectory(pfs->open("/"), 0);
  DBGSerial.println();
}

void printDirectory(File dir, int numSpaces) {
  while (true) {
    File entry = dir.openNextFile();
    if (! entry) {
      //DBGSerial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    DBGSerial.print(entry.name());
    if (entry.isDirectory()) {
      DBGSerial.println("/");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      printSpaces(36 - numSpaces - strlen(entry.name()));
      DBGSerial.print("  ");
      DBGSerial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    DBGSerial.print(" ");
  }
}


uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ') ch = DBGSerial.read();
  if ((ch < '0') || (ch > '9')) return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = DBGSerial.read();
  }
  return return_value;
}
