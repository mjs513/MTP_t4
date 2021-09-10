#include "Arduino.h"

#include "SD.h"
#include <MTP.h>

#define USE_SD  0         // SDFAT based SDIO and SPI
#ifdef ARDUINO_TEENSY41
#define USE_LFS_RAM 0     // T4.1 PSRAM (or RAM)
#else
#define USE_LFS_RAM 0     // T4.1 PSRAM (or RAM)
#endif
#ifdef ARDUINO_TEENSY_MICROMOD
#define USE_LFS_QSPI 0    // T4.1 QSPI
#define USE_LFS_PROGM 1   // T4.4 Progam Flash
#define USE_LFS_SPI 0     // SPI Flash
#define USE_LFS_NAND 0
#define USE_LFS_QSPI_NAND 0
#define USE_LFS_FRAM 0
#else
#define USE_LFS_QSPI 0    // T4.1 QSPI
#define USE_LFS_PROGM 1   // T4.4 Progam Flash
#define USE_LFS_SPI 0     // SPI Flash
#define USE_LFS_NAND 1
#define USE_LFS_QSPI_NAND 0
#define USE_LFS_FRAM 0
#endif
#define USE_MSC 3    // set to > 0 experiment with MTP (USBHost.t36 + mscFS)


extern "C" {
  extern uint8_t external_psram_size;
}

bool g_lowLevelFormat = true;
uint32_t last_storage_index = (uint32_t)-1;


//=============================================================================
// Global defines
//=============================================================================


                            MTPStorage_SD storage;
                            MTPD    mtpd(&storage);

#include "TimeLib.h"

//=============================================================================
// MSC & SD classes
//=============================================================================
#if USE_SD==1
#include <SDMTPClass.h>

#define USE_BUILTIN_SDCARD
#if defined(USE_BUILTIN_SDCARD) && defined(BUILTIN_SDCARD)
#define CS_SD  BUILTIN_SDCARD
#else
#define CS_SD 10
#endif
#define SPI_SPEED SD_SCK_MHZ(16)  // adjust to sd card 

#define COUNT_MYFS  1  // could do by count, but can limit how many are created...
SDMTPClass myfs[] = {
//                      {mtpd, storage, "SDIO", CS_SD}, 
                      {mtpd, storage, "SD8", 8, 9, SHARED_SPI, SPI_SPEED}
                    };
//SDMTPClass myfs(mtpd, storage, "SD10", 10, 0xff, SHARED_SPI, SPI_SPEED);

#endif

//=========================================================================
// USB MSC Class setup
//=========================================================================
#if USE_MSC > 0
#include <USB_MSC_MTP.h>
USB_MSC_MTP usbmsc(mtpd, storage);
FS* mscDisk;
#endif

#if USE_LFS_RAM==1 ||  USE_LFS_PROGM==1 || USE_LFS_QSPI==1 || USE_LFS_SPI==1 || USE_LFS_NAND==1 ||  USE_LFS_QSPI_NAND==1
#include "LittleFS.h"
//=============================================================================
//LittleFS classes
//=============================================================================
// Setup a callback class for Littlefs storages..
#include <LFS_MTP_Callback.h>  //callback for LittleFS format
LittleFSMTPCB lfsmtpcb;
#endif

#if USE_SD == 1
//========================================================================
//This puts a the index file in memory as opposed to an SD Card in memory
//========================================================================
#define LFSRAM_SIZE 65536  // probably more than enough...
LittleFS_RAM lfsram;
#endif

// =======================================================================
// Set up LittleFS file systems on different storage media
// =======================================================================
#if USE_LFS_RAM==1
const char *lfs_ram_str[] = {"RAM1", "RAM2"};  // edit to reflect your configuration
const int lfs_ram_size[] = {200'000,4'000'000}; // edit to reflect your configuration
                            const int nfs_ram = sizeof(lfs_ram_str)/sizeof(const char *);
                            LittleFS_RAM ramfs[nfs_ram];
#endif

#if USE_LFS_QSPI==1
                            const char *lfs_qspi_str[]={"QSPI"};     // edit to reflect your configuration
                            const int nfs_qspi = sizeof(lfs_qspi_str)/sizeof(const char *);

                            LittleFS_QSPIFlash qspifs[nfs_qspi];
#endif

#if USE_LFS_PROGM==1
                            const char *lfs_progm_str[]={"PROGM"};     // edit to reflect your configuration
                            const int lfs_progm_size[] = {1'000'000}; // edit to reflect your configuration
                            const int nfs_progm = sizeof(lfs_progm_str)/sizeof(const char *);

                            LittleFS_Program progmfs[nfs_progm];
#endif

#if USE_LFS_SPI==1
                            const char *lfs_spi_str[]={"sflash8", "sflash9"}; // edit to reflect your configuration
                            const int lfs_cs[] = {8, 9}; // edit to reflect your configuration
                            const int nfs_spi = sizeof(lfs_spi_str)/sizeof(const char *);

                            LittleFS_SPIFlash spifs[nfs_spi];
#endif
#if USE_LFS_NAND == 1
                            const char *nspi_str[]={"WINBOND1G", "WINBOND2G","WINBONDxG", "WINBONDyG"};     // edit to reflect your configuration
                            const int nspi_cs[] = {3, 4, 5, 6}; // edit to reflect your configuration
                            const int nspi_nsd = sizeof(nspi_cs)/sizeof(int);
                            LittleFS_SPINAND nspifs[nspi_nsd]; // needs to be declared if LittleFS is used in storage.h
#endif



void storage_configure()
{
  
  Serial.printf("Date: %u/%u/%u %u:%u:%u\n", day(), month(), year(),
                hour(), minute(), second());

  storage.setIndexStore(0);
  
  // lets initialize a RAM drive. 
/*  if (lfsram.begin (LFSRAM_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    lfsmtpcb.set_formatLevel(true);  //sets formating to lowLevelFormat
    uint32_t istore = storage.addFilesystem(lfsram, "RAM", &lfsmtpcb, (uint32_t)(LittleFS*)&lfsram);
    if (istore != 0xFFFFFFFFUL) storage.setIndexStore(istore);
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
*/    
    
#if USE_SD == 1
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
  
    Serial.println("SD initialized.");
#endif


#if USE_LFS_RAM==1
  for(int ii=0; ii<nfs_ram;ii++)
  {
    if(!ramfs[ii].begin(lfs_ram_size[ii]))
    { Serial.printf("Ram Storage %d %s failed or missing",ii,lfs_ram_str[ii]); Serial.println();
    }
    else
    {
      storage.addFilesystem(ramfs[ii], lfs_ram_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&ramfs[ii]);
      uint64_t totalSize = ramfs[ii].totalSize();
      uint64_t usedSize  = ramfs[ii].usedSize();
      Serial.printf("RAM Storage %d %s ",ii,lfs_ram_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
    }
  }
#endif

#if USE_LFS_PROGM==1
  for(int ii=0; ii<nfs_progm;ii++)
  {
    if(!progmfs[ii].begin(lfs_progm_size[ii]))
    { Serial.printf("Program Storage %d %s failed or missing",ii,lfs_progm_str[ii]); Serial.println();
    }
    else
    {
      storage.addFilesystem(progmfs[ii], lfs_progm_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&progmfs[ii]);
      uint64_t totalSize = progmfs[ii].totalSize();
      uint64_t usedSize  = progmfs[ii].usedSize();
      Serial.printf("Program Storage %d %s ",ii,lfs_progm_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
    }
  }
#endif

#if USE_LFS_QSPI==1
  for(int ii=0; ii<nfs_qspi;ii++)
  {
    if(!qspifs[ii].begin())
    { Serial.printf("QSPI Storage %d %s failed or missing",ii,lfs_qspi_str[ii]); Serial.println();
    }
    else
    {
      storage.addFilesystem(qspifs[ii], lfs_qspi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&qspifs[ii]);
      uint64_t totalSize = qspifs[ii].totalSize();
      uint64_t usedSize  = qspifs[ii].usedSize();
      Serial.printf("QSPI Storage %d %s ",ii,lfs_qspi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
    }
  }
#endif

#if USE_LFS_SPI==1
  for(int ii=0; ii<nfs_spi;ii++)
  {
    pinMode(lfs_cs[ii],OUTPUT); digitalWriteFast(lfs_cs[ii],HIGH);
    if(!spifs[ii].begin(lfs_cs[ii], SPI))
    { Serial.printf("SPIFlash Storage %d %d %s failed or missing",ii,lfs_cs[ii],lfs_spi_str[ii]); Serial.println();
    }
    else
    {
      storage.addFilesystem(spifs[ii], lfs_spi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&spifs[ii]);
      uint64_t totalSize = spifs[ii].totalSize();
      uint64_t usedSize  = spifs[ii].usedSize();
      Serial.printf("SPIFlash Storage %d %d %s ",ii,lfs_cs[ii],lfs_spi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
    }
  }
#endif
#if USE_LFS_NAND == 1
  for(int ii=0; ii<nspi_nsd;ii++) {
    pinMode(nspi_cs[ii],OUTPUT); digitalWriteFast(nspi_cs[ii],HIGH);
    if(!nspifs[ii].begin(nspi_cs[ii], SPI)) 
    { Serial.printf("SPIFlash NAND Storage %d %d %s failed or missing",ii,nspi_cs[ii],nspi_str[ii]); Serial.println();
    }
    else
    {
      storage.addFilesystem(nspifs[ii], nspi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&nspifs[ii]);

      uint64_t totalSize = nspifs[ii].totalSize();
      uint64_t usedSize  = nspifs[ii].usedSize();
      Serial.printf("Storage %d %d %s ",ii,nspi_cs[ii],nspi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
    }
  }
#endif

#if USE_LFS_QSPI_NAND == 1
  for(int ii=0; ii<qnspi_nsd;ii++) {
    if(!qnspifs[ii].begin()) 
    { Serial.printf("QSPI NAND Storage %d %s failed or missing",ii,qnspi_str[ii]); Serial.println();
    }
    else
    {
      storage.addFilesystem(qnspifs[ii], qnspi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&qnspi_str[ii]);

      uint64_t totalSize = qnspifs[ii].totalSize();
      uint64_t usedSize  = qnspifs[ii].usedSize();
      Serial.printf("Storage %d %s ",ii,qnspi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
  }
  }
#endif

// Start USBHost_t36, HUB(s) and USB devices.
#if USE_MSC > 0
  Serial.println("\nInitializing USB MSC drives...");
  usbmsc.checkUSBStatus(true);
#endif

}


void setup()
{
#if USE_MSC_FAT > 0
  // let msusb stuff startup as soon as possible
  usbmsc.begin();
#endif 
  
  // Open serial communications and wait for port to open:
#if defined(USB_MTPDISK_SERIAL)
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
#else
  //while(!Serial.available()); // comment if you do not want to wait for terminal (otherwise press any key to continue)
  while (!Serial && !Serial.available() && millis() < 5000) 
  myusb.Task(); // or third option to wait up to 5 seconds and then continue
#endif

  Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(3000);
  
  storage_configure();
  
  Serial.println("\nSetup done");
  
}

int ReadAndEchoSerialChar() {
  int ch = Serial.read();
  if (ch >= ' ') Serial.write(ch);
  return ch;
}

void loop()
{

  mtpd.loop();
  // Call code to detect if MSC status changed
  #if USE_MSC > 0
  usbmsc.checkUSBStatus(false);
  #endif
  
  if (Serial.available())
  {
    char pathname[MAX_FILENAME_LEN]; 
    uint32_t storage_index = 0;
    uint32_t file_size = 0;

    // Should probably use Serial.parse ...
    Serial.printf("\n *** Command line: ");
    int cmd_char = ReadAndEchoSerialChar();
    int ch = ReadAndEchoSerialChar();
    while (ch == ' ') ch = ReadAndEchoSerialChar();
    if (ch >= '0' && ch <= '9') {
      storage_index = ch - '0';
      ch = ReadAndEchoSerialChar();
      if (ch == 'x') {
        ch = ReadAndEchoSerialChar();
        for(;;) {
          if  (ch >= '0' && ch <= '9') storage_index = storage_index*16 + ch - '0';
          else if  (ch >= 'a' && ch <= 'f') storage_index = storage_index*16 + 10 + ch - 'a';
          else break;
          ch = ReadAndEchoSerialChar();
        }
      }
      else 
      {
        while (ch >= '0' && ch <= '9') {
          storage_index = storage_index*10 + ch - '0';
          ch = ReadAndEchoSerialChar();
        }
      }
      while (ch == ' ') ch = ReadAndEchoSerialChar();
      last_storage_index = storage_index;
    } else {
      storage_index = last_storage_index;
    }
    char *psz = pathname;
    while (ch > ' ') {
      *psz++ = ch;
      ch = ReadAndEchoSerialChar();
    }
    *psz = 0;
    while (ch == ' ') ch = ReadAndEchoSerialChar();
    while (ch >= '0' && ch <= '9') {
      file_size = file_size*10 + ch - '0';
      ch = ReadAndEchoSerialChar();
    }


    while(ReadAndEchoSerialChar() != -1) ;
    Serial.println();
    switch (cmd_char) 
    {
      case'r':
        Serial.println("Reset");
        mtpd.send_DeviceResetEvent();
        break;
      case 'd':
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

      case 'f':
        g_lowLevelFormat = !g_lowLevelFormat;
        if (g_lowLevelFormat) Serial.println("low level format of LittleFS disks selected");
        else Serial.println("Quick format of LittleFS disks selected");
        break;
#if USE_LFS_RAM==1
      case 'b':
        {
          Serial.println("Add Files");
          static int next_file_index_to_add = 100;
          uint32_t store = storage.getStoreID("PROGM");
          for (int ii = 0; ii < 10; ii++)
          { char filename[80];
            snprintf(filename, sizeof(filename),"/test_%d.txt", next_file_index_to_add++);
            Serial.println(filename);
            File file = ramfs[0].open(filename, FILE_WRITE_BEGIN);
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.println("This is a test line");
            file.close();
            mtpd.send_addObjectEvent(store, filename);
          }
          //  Notify PC on added files
          Serial.print("Store "); Serial.println(store);
          mtpd.send_StorageInfoChangedEvent(store);
        }
        break;
      case 'y':
        {
          Serial.println("Delete Files");
          static int next_file_index_to_delete = 100;
          uint32_t store = storage.getStoreID("RAM1");
          for (int ii = 0; ii < 10; ii++)
          { char filename[80];
            snprintf(filename, sizeof(filename), "/test_%d.txt", next_file_index_to_delete++);
            Serial.println(filename);
            if (ramfs[0].remove(filename))
            {
              mtpd.send_removeObjectEvent(store, filename);
            }
          }
          // attempt to notify PC on added files (does not work yet)
          Serial.print("Store "); Serial.println(store);
          mtpd.send_StorageInfoChangedEvent(store);
        }
        break;
#elif USE_SD==1
      case'a':
        {
          Serial.println("Add Files");
          static int next_file_index_to_add = 100;
          uint32_t store = storage.getStoreID("sdio");
          for (int ii = 0; ii < 10; ii++)
          { char filename[80];
            snprintf(filename, sizeof(filename), "/test_%d.txt", next_file_index_to_add++);
            Serial.println(filename);
            File file = myfs[0].open(filename, FILE_WRITE_BEGIN);
            file.println("This is a test line");
            file.close();
            mtpd.send_addObjectEvent(store, filename);
          }
          // attempt to notify PC on added files (does not work yet)
          Serial.print("Store "); Serial.println(store);
          mtpd.send_StorageInfoChangedEvent(store);
        }
        break;
      case 'x':
        {
          Serial.println("Delete Files");
          static int next_file_index_to_delete = 100;
          uint32_t store = storage.getStoreID("sdio");
          for (int ii = 0; ii < 10; ii++)
          { char filename[80];
            snprintf(filename, sizeof(filename), "/test_%d.txt", next_file_index_to_delete++);
            Serial.println(filename);
            if (myfs[0].remove(filename))
            {
              mtpd.send_removeObjectEvent(store, filename);
            }
          }
          // attempt to notify PC on added files (does not work yet)
          Serial.print("Store "); Serial.println(store);
          mtpd.send_StorageInfoChangedEvent(store);
        }
        break;
#endif        
      case 'e':
        Serial.printf("Sending event: %x\n", storage_index);
        mtpd.send_Event(storage_index);
        break;
      default:
        // show list of commands.
        Serial.println("\nCommands");
        Serial.println("  r - Reset mtp connection");
        Serial.println("  d - Dump storage list");
#if USE_SD == 1
        Serial.println("  a - Add some dummy files");
        Serial.println("  x - delete dummy files");
#endif
#if USE_LFS_RAM==1
        Serial.println("  b - Add some dummy files");
        Serial.println("  y - delete dummy files");
#endif
        Serial.println("  f - toggle storage format type");
        Serial.println("  e <event> - Send some random event");
        break;    

      }
    }
  }