#include "Arduino.h"

#include "SD.h"
#include <MTP.h>

#define USE_SD  1         // SDFAT based SDIO and SPI
#if defined(ARDUINO_TEENSY41)
#define USE_LFS_RAM 1     // T4.1 PSRAM (or RAM)
#define USE_LFS_QSPI 1    // T4.1 QSPI
#define USE_LFS_PROGM 1   // T4.4 Progam Flash
#define USE_LFS_NAND 1

#else
#define USE_LFS_RAM 0     // T4.1 PSRAM (or RAM)
#define USE_LFS_QSPI 0    // T4.1 QSPI
#define USE_LFS_PROGM 0   // T4.4 Progam Flash
#define USE_LFS_NAND 0
#endif
#define USE_LFS_SPI 1     // SPI Flash


#if USE_LFS_RAM==1 ||  USE_LFS_PROGM==1 || USE_LFS_QSPI==1 || USE_LFS_SPI==1
  #include <LittleFS.h>
  #include <LittleFS_NAND.h> 
#endif


/****  Start device specific change area  ****/
// SDClasses 
#if USE_SD==1
  // edit SPI to reflect your configuration (following is for T4.1)
  #define SD_MOSI 11
  #define SD_MISO 12
  #define SD_SCK  13

  #define SPI_SPEED SD_SCK_MHZ(16)  // adjust to sd card 

  #if defined (BUILTIN_SDCARD)
    const char *sd_str[]={"sdio","sd1"}; // edit to reflect your configuration
    const int cs[] = {BUILTIN_SDCARD,10}; // edit to reflect your configuration
  #else
    const char *sd_str[]={"sd1"}; // edit to reflect your configuration
    const int cs[] = {10}; // edit to reflect your configuration
  #endif
  const int nsd = sizeof(sd_str)/sizeof(const char *);

SDClass sdx[nsd];
#endif

//LittleFS classes
#if USE_LFS_RAM==1
  const char *lfs_ram_str[]={"RAM1","RAM2"};     // edit to reflect your configuration
  const int lfs_ram_size[] = {2'000'000,4'000'000}; // edit to reflect your configuration
  const int nfs_ram = sizeof(lfs_ram_str)/sizeof(const char *);

  LittleFS_RAM ramfs[nfs_ram]; 
#endif

#if USE_LFS_SLOW_RAM == 1 // like RAM but made to be SLOOOWWW
  const char *slfs_ram_str[]={"SLOW_RAM"};     // edit to reflect your configuration
  const int slfs_ram_size[] = {2'000'000}; // edit to reflect your configuration
  const int snfs_ram = sizeof(slfs_ram_str)/sizeof(const char *);

  class LittleFS_SLOW_RAM : public LittleFS_RAM
  {
  public:
    LittleFS_SLOW_RAM() : LittleFS_RAM() { }
    static uint32_t    WRITE_TIME_MS;  // time for writes to take. 

    bool begin(uint32_t size) {
      bool fRet = LittleFS_RAM::begin(size);

      // Lets overwrite the parent classes prog function
      parent_static_prog = config.prog; // remember the prevoius one. 
      config.prog = &static_prog;
      return fRet;
    }

    private:
      static int (*parent_static_prog)(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset, const void *buffer, lfs_size_t size);
      static int static_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t offset, const void *buffer, lfs_size_t size) {
        elapsedMillis em = 0;
        int ret = (*parent_static_prog)(c, block, offset, buffer, size);
        int delay_time = (int)(WRITE_TIME_MS - em);
        if (delay_time > 0) delay(delay_time);
        return ret;
      }
  };

  uint32_t LittleFS_SLOW_RAM::WRITE_TIME_MS = 2;
  int (*LittleFS_SLOW_RAM::parent_static_prog)(const struct lfs_config *c, lfs_block_t block, lfs_off_t offset, const void *buffer, lfs_size_t size) = nullptr;
  LittleFS_SLOW_RAM sramfs[snfs_ram]; 
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
#if USE_LFS_NAND==1
  const char *lfs_spi_str[]={"spif-5","spif-6"}; // edit to reflect your configuration
  const int lfs_cs[] = {5,6}; // edit to reflect your configuration
  const int nfs_spi = sizeof(lfs_spi_str)/sizeof(const char *);
  LittleFS_SPIFlash spifs[nfs_spi];

  const char *nlfs_spi_str[]={"nspif-3","nspif-4"}; // edit to reflect your configuration
  const int nlfs_cs[] = {3,4}; // edit to reflect your configuration
  const int nnfs_spi = sizeof(nlfs_spi_str)/sizeof(const char *);
  LittleFS_SPINAND nspifs[nnfs_spi];

#else
  const char *lfs_spi_str[]={"nspif-3","nspif-4"}; // edit to reflect your configuration
  const int lfs_cs[] = {5,6}; // edit to reflect your configuration
  const int nfs_spi = sizeof(lfs_spi_str)/sizeof(const char *);

LittleFS_SPIFlash spifs[nfs_spi];
#endif
#endif

MTPStorage_SD storage;
MTPD    mtpd(&storage);

void storage_configure()
{
  #if USE_SD==1
    #if defined SD_SCK
      SPI.setMOSI(SD_MOSI);
      SPI.setMISO(SD_MISO);
      SPI.setSCK(SD_SCK);
    #endif

    for(int ii=0; ii<nsd; ii++)
    { 
      #if defined(BUILTIN_SDCARD)
        if(cs[ii] == BUILTIN_SDCARD)
        {
          if(!sdx[ii].sdfs.begin(SdioConfig(FIFO_SDIO))) 
          { Serial.printf("SDIO Storage %d %d %s failed or missing",ii,cs[ii],sd_str[ii]);  Serial.println();
          }
          else
          {
            storage.addFilesystem(sdx[ii], sd_str[ii]);
            uint64_t totalSize = sdx[ii].totalSize();
            uint64_t usedSize  = sdx[ii].usedSize();
            Serial.printf("SDIO Storage %d %d %s ",ii,cs[ii],sd_str[ii]); 
            Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
          }
        }
        else if(cs[ii]<BUILTIN_SDCARD)
      #endif
      {
        pinMode(cs[ii],OUTPUT); digitalWriteFast(cs[ii],HIGH);
        if(!sdx[ii].sdfs.begin(SdSpiConfig(cs[ii], SHARED_SPI, SPI_SPEED))) 
        { Serial.printf("SD Storage %d %d %s failed or missing",ii,cs[ii],sd_str[ii]);  Serial.println();
        }
        else
        {
          storage.addFilesystem(sdx[ii], sd_str[ii]);
          uint64_t totalSize = sdx[ii].totalSize();
          uint64_t usedSize  = sdx[ii].usedSize();
          Serial.printf("SD Storage %d %d %s ",ii,cs[ii],sd_str[ii]); 
          Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
        }
      }
    }
    #endif

    #if USE_LFS_RAM==1
    for(int ii=0; ii<nfs_ram;ii++)
    {
      if(!ramfs[ii].begin(lfs_ram_size[ii])) 
      { Serial.printf("Ram Storage %d %s failed or missing",ii,lfs_ram_str[ii]); Serial.println();
      }
      else
      {
        storage.addFilesystem(ramfs[ii], lfs_ram_str[ii]);
        uint64_t totalSize = ramfs[ii].totalSize();
        uint64_t usedSize  = ramfs[ii].usedSize();
        Serial.printf("RAM Storage %d %s ",ii,lfs_ram_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
      }
    }

    #endif
    #if USE_LFS_SLOW_RAM==1
    for(int ii=0; ii<snfs_ram;ii++)
    {
      if(!sramfs[ii].begin(slfs_ram_size[ii])) 
      { Serial.printf("Slow Ram Storage %d %s failed or missing",ii,slfs_ram_str[ii]); Serial.println();
      }
      else
      {
        storage.addFilesystem(sramfs[ii], slfs_ram_str[ii]);
        uint64_t totalSize = sramfs[ii].totalSize();
        uint64_t usedSize  = sramfs[ii].usedSize();
        Serial.printf("Slow RAM Storage %d %s ",ii,slfs_ram_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
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
        storage.addFilesystem(progmfs[ii], lfs_progm_str[ii]);
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
        storage.addFilesystem(qspifs[ii], lfs_qspi_str[ii]);
        uint64_t totalSize = qspifs[ii].totalSize();
        uint64_t usedSize  = qspifs[ii].usedSize();
        Serial.printf("QSPI Storage %d %s ",ii,lfs_qspi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
      }
    }
    #endif

    #if USE_LFS_SPI==1
    for(int ii=0; ii<nfs_spi;ii++)
    {
      if(!spifs[ii].begin(lfs_cs[ii])) 
      { Serial.printf("SPIFlash Storage %d %d %s failed or missing",ii,lfs_cs[ii],lfs_spi_str[ii]); Serial.println();
      }
      else
      {
        storage.addFilesystem(spifs[ii], lfs_spi_str[ii]);
        uint64_t totalSize = spifs[ii].totalSize();
        uint64_t usedSize  = spifs[ii].usedSize();
        Serial.printf("SPIFlash Storage %d %d %s ",ii,lfs_cs[ii],lfs_spi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
      }
    }
    #if USE_LFS_NAND==1
    for(int ii=0; ii<nnfs_spi;ii++)
    {
      if(!nspifs[ii].begin(nlfs_cs[ii])) 
      { Serial.printf("SPIFlash NAND Storage %d %d %s failed or missing",ii,nlfs_cs[ii],nlfs_spi_str[ii]); Serial.println();
      }
      else
      {
        storage.addFilesystem(nspifs[ii], nlfs_spi_str[ii]);
        uint64_t totalSize = nspifs[ii].totalSize();
        uint64_t usedSize  = nspifs[ii].usedSize();
        Serial.printf("SPIFlash NAND Storage %d %d %s ",ii,nlfs_cs[ii],nlfs_spi_str[ii]); Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
      }
    }
    #endif
    #endif
}
/****  End of device specific change area  ****/

  // Call back for file timestamps.  Only called for file create and sync(). needed by SDFat-beta
   #include "TimeLib.h"
  void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) 
  { *date = FS_DATE(year(), month(), day());
    *time = FS_TIME(hour(), minute(), second());
    *ms10 = second() & 1 ? 100 : 0;
  }
elapsedMillis emLoop = 0;
  #ifdef ARDUINO_TEENSY41
  extern "C" {
    extern uint8_t external_psram_size;
  }
  #endif

void setup()
{ 
  #if defined(USB_MTPDISK_SERIAL) 
    while(!Serial && millis() < 5000); // comment if you do not want to wait for terminal
  #else
    while(!Serial.available() && millis() < 5000); // comment if you do not want to wait for terminal (otherwise press any key to continue)
  #endif
  Serial.begin(115200);
  delay(250);

  Serial.println("MTP_test");
  Serial.flush();
  delay(500);
#if USE_EVENTS==1
  mtpd.usb_init_events();
#endif
#if !__has_include("usb_mtp.h")
  usb_mtp_configure();
#endif
  storage_configure();

  #if USE_SD==1
  // Set Time callback // needed for SDFat
  FsDateTime::callback = dateTime;

  {
    const char *str = "test1.txt";
    if(sdx[0].exists(str)) sdx[0].remove(str);
    File file=sdx[0].open(str,FILE_WRITE_BEGIN);
        file.println("This is a test line");
    file.close();

    Serial.println("\n**** dir of sd[0] ****");
    sdx[0].sdfs.ls();
  }

  #endif
  #if USE_LFS_RAM==1
    for(int ii=0; ii<10;ii++)
    { char filename[80];
      sprintf(filename,"/test_%d.txt",ii);
      File file=ramfs[0].open(filename,FILE_WRITE_BEGIN);
        file.println("This is a test line");
      file.close();
    }
    ramfs[0].mkdir("Dir0");
    for(int ii=0; ii<10;ii++)
    { char filename[80];
      sprintf(filename,"/Dir0/test_%d.txt",ii);
      File file=ramfs[0].open(filename,FILE_WRITE_BEGIN);
        file.println("This is a test line");
      file.close();
    }
    ramfs[0].mkdir("Dir0/dir1");
    for(int ii=0; ii<10;ii++)
    { char filename[80];
      sprintf(filename,"/Dir0/dir1/test_%d.txt",ii);
      File file=ramfs[0].open(filename,FILE_WRITE_BEGIN);
        file.println("This is a test line");
      file.close();
    }
    uint32_t buffer[256];
    File file = ramfs[1].open("LargeFile.bin",FILE_WRITE_BEGIN);
    for(int ii=0;ii<3000;ii++)
    { memset(buffer,ii%256,1024);
      file.write(buffer,1024);
    }
    file.close();

  #endif

  #ifdef ARDUINO_TEENSY41
  Serial.printf("External RAM size: %u\n", external_psram_size);
  #endif

  Serial.println("\nSetup done");
  emLoop = 0;
}

int ReadAndEchoSerialChar() {
  int ch = Serial.read();
  if (ch >= ' ') Serial.write(ch);
  return ch;
}

uint32_t last_storage_index = (uint32_t)-1;
#define DEFAULT_FILESIZE 1024
void loop()
{ 
  #if 0
  if (emLoop > 1000) {
    emLoop = 0;
    Serial.print("*");
  }
  #endif
  mtpd.loop();

  if (Serial.available()) {
    char pathname[MAX_FILENAME_LEN]; 
    uint32_t storage_index = 0;
    uint32_t file_size = 0;

    // Should probably use Serial.parse ...
    Serial.printf("\n *** Command line: ");
    int cmd_char = ReadAndEchoSerialChar();
    int ch = ReadAndEchoSerialChar();
    while (ch == ' ') ch = ReadAndEchoSerialChar();
    if (ch >= '0' && ch <= '9') {
      while (ch >= '0' && ch <= '9') {
        storage_index = storage_index*10 + ch - '0';
        ch = ReadAndEchoSerialChar();
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

    switch (cmd_char) {
        case 'f': // New file
          {
            FS *pfs = storage.getFS(storage_index);
            if (pfs) {
              if (file_size == 0) file_size = DEFAULT_FILESIZE;

              File file = pfs->open(pathname,FILE_WRITE_BEGIN);
              if (file) {
                uint8_t buffer[256];
                for (int i=0; i < 256; i++) buffer[i] = i;  // random junk
                file.truncate();  // zero it out in case it was there. 
                uint32_t bytes_left_to_write = file_size;
                while (bytes_left_to_write) {
                  uint32_t write_size = min (file_size, sizeof(buffer));
                  file.write(buffer, write_size);
                  bytes_left_to_write -= write_size;
                }
                uint64_t fs_size = file.size();

                file.close();
                mtpd.notifyFileCreated(pfs, pathname, false, fs_size);
                Serial.printf("MTPD notified of file:%s size:%llu on %s created\n", pathname, fs_size, storage.getFSName(storage_index));
                break;
              } else {
                Serial.printf("Failed to create new file: %s on %s created\n", pathname, storage.getFSName(storage_index));
              }
          }
        }
        break;
      case 'd': // create a new directory
        {
          FS *pfs = storage.getFS(storage_index);
            if (pfs) {
              if (file_size == 0) file_size = DEFAULT_FILESIZE;

              if (pfs->mkdir(pathname)) {
                mtpd.notifyFileCreated(pfs, pathname, true);  // notify we created a directory
                Serial.printf("MTPD notified of new directory: %s on %s created\n", pathname, storage.getFSName(storage_index));
                break;
              } else {
                Serial.printf("Failed to create new directory: %s on %s created\n", pathname, storage.getFSName(storage_index));
              }
          }
        }
        break;
      case 'r': // remove/delete
        {
          FS *pfs = storage.getFS(storage_index);
          if (pfs) {
            Serial.printf("MTPD Try to removefile:%s from %s\n", pathname, storage.getFSName(storage_index));
            if (pfs->remove(pathname)) {
              Serial.println(" -- Succeeded try to notify");
              mtpd.notifyFileRemoved(pfs, pathname);
              Serial.printf("MTPD notified of file:%s on %s removed\n", pathname, storage.getFSName(storage_index));
            } else {
              Serial.println(" -- failed to remove");
            }
          }
        }
        break;
      case 'i': // Show index list
        Serial.println("Dump index list");
        storage.dumpIndexList();
        break;  
      default:
        // show list of commands and list of storages.
        Serial.println("\n*** List of Storages ***");
        for (storage_index = 0; storage_index < storage.getFSCount(); storage_index++) {
          Serial.printf("    %u: %s\n", storage_index, storage.getFSName(storage_index));
        }
        Serial.println("\nCommands");
        Serial.println("  Create File: f <storage index> pathname [size]");
        Serial.println("  Create directory: d <SI> pathname");
        Serial.println("  Remove file.dir: r <SI> pathname");
        Serial.println("  print storage index list: i");
        break;    

    }

  }
}
