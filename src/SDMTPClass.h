#ifndef SDMTPCLASS_H
#define SDMTPCLASS_H

#include <SD.h>
#include <mscFS.h>
#include <MTP.h>

// Setup a callback class for Littlefs storages..
// Plus helper functions for handling SD disk insertions. 
class SDMTPClass : public SDClass, public MTPStorageInterfaceCB
{
public:
  // constructor
  SDMTPClass(MTPD &mtpd, MTPStorage_SD &storage, const char *sdc_name, uint8_t csPin, 
      uint8_t cdPin=0xff, uint8_t opt=SHARED_SPI, 
      uint32_t maxSpeed=SD_SCK_MHZ(50), SpiPort_t* port=&SPI) 
      : mtpd_(mtpd), storage_(storage), sdc_name_(sdc_name), csPin_(csPin), 
      cdPin_(cdPin), opt_(opt), maxSpeed_(maxSpeed), port_(port) {
        // Make sure we have a date time callback set...
        FsDateTime::callback = dateTime;
  };

  //---------------------------------------------------------------------------
  // Callback function overrides.
  uint8_t formatStore(MTPStorage_SD *mtpstorage, uint32_t store, uint32_t user_token, uint32_t p2, bool post_process);

  // Support functions for SD Insertions.
  bool init(bool add_if_missing);
  void loop();

  // date time callback from SD library
  static void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10);



private:
  // helper functions
  void addFSToStorage(bool send_events);  

  // local variables. 
  MTPD &mtpd_;
  MTPStorage_SD &storage_;
  const char *sdc_name_;
  uint8_t csPin_;
  uint8_t cdPin_; // chip detect
  uint8_t opt_;
  uint32_t maxSpeed_;
  SpiPort_t *port_;

  // Currently SDIO only, may add. SPI ones later
  bool check_disk_insertion_ = false;
};

#endif
