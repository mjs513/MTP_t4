#include <SDMTPClass.h>
#include <mscFS.h>

#if defined(MAINTAIN_FREE_CLUSTER_COUNT) && (MAINTAIN_FREE_CLUSTER_COUNT == 0)
#warning "###############################################################"
#warning "# MAINTAIN_FREE_CLUSTER_COUNT is 0 in SdFatConfig.h           #"
#warning "# Large Fat32 SD cards will likely fail to work properly      #"
#warning "###############################################################"
#endif

//=============================================================================
// try to get the right FS for this store and then call it's format if we have one...
// Here is for LittleFS...
PFsLib pfsLIB;
uint8_t SDMTPClass::formatStore(MTPStorage_SD *mtpstorage, uint32_t store, uint32_t user_token, uint32_t p2, bool post_process)
{
  // Lets map the user_token back to oint to our object...
  Serial.printf("Format Callback: user_token:%x store: %u p2:%u post:%u \n", user_token, store, p2, post_process);
  SDClass *psd = (SDClass*)user_token;

  if (psd->sdfs.fatType() == FAT_TYPE_FAT12) {
    Serial.printf("    Fat12 not supported\n");
    return MTPStorageInterfaceCB::FORMAT_NOT_SUPPORTED;
  }

  // For all of these the fat ones will do on post_process
  if (!post_process) return MTPStorageInterfaceCB::FORMAT_NEEDS_CALLBACK;

  PFsVolume partVol;
  if (!partVol.begin(psd->sdfs.card(), true, 1)) return MTPStorageInterfaceCB::FORMAT_NOT_SUPPORTED;
  bool success = pfsLIB.formatter(partVol);
  return success ? MTPStorageInterfaceCB::FORMAT_SUCCESSFUL : MTPStorageInterfaceCB::FORMAT_NOT_SUPPORTED;
}

uint64_t SDMTPClass::usedSizeCB(MTPStorage_SD *mtpstorage, uint32_t store, uint32_t user_token)
{
  // Courious how often called and how long it takes...
  Serial.printf("\n\n}}}}}}}}} SDMTPClass::usedSizeCB called %x %u %u cs:%u ", (uint32_t)mtpstorage, store, user_token, csPin_);
  if (!disk_valid_) {
    Serial.println("* not inserted *");
    return 0l;
  }
  Serial.printf("ft:%u\n", sdfs.vol()->fatType());

  elapsedMillis em = 0;
  uint64_t us = usedSize();
  Serial.println(em, DEC);
  return us;
}

uint64_t SDMTPClass::totalSizeCB(MTPStorage_SD *mtpstorage, uint32_t store, uint32_t user_token)
{
  // Courious how often called and how long it takes...
  Serial.printf("\n\n{{{{{{{{{ SDMTPClass::totalSizeCB called %x %u %u cs:%u ", (uint32_t)mtpstorage, store, user_token, csPin_);
  if (!disk_valid_) {
    Serial.println("* not inserted *");
    return 0l;
  }
  Serial.printf("ft:%u\n", sdfs.vol()->fatType());
  elapsedMillis em = 0;
  uint64_t us = totalSize();
  Serial.println(em, DEC);
  return us;
}




#include "TimeLib.h"
void SDMTPClass::dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10)
{
  *date = FS_DATE(year(), month(), day());
  *time = FS_TIME(hour(), minute(), second());
  *ms10 = second() & 1 ? 100 : 0;
  Serial.printf("### dateTime: called %x %x %x\n", *date, *time, *ms10);
}


//===================================================
// SD Testing for disk insertion.
#if defined(ARDUINO_TEENSY41)
#define _SD_DAT3 46
#elif defined(ARDUINO_TEENSY40)
#define _SD_DAT3 38
#elif defined(ARDUINO_TEENSY_MICROMOD)
#define _SD_DAT3 38
#elif defined(ARDUINO_TEENSY35) || defined(ARDUINO_TEENSY36)
#define _SD_DAT3 62
#endif


bool  SDMTPClass::init(bool add_if_missing) {
#ifdef BUILTIN_SDCARD
  if (csPin_ == BUILTIN_SDCARD) {
    if (!(disk_valid_ = sdfs.begin(SdioConfig(FIFO_SDIO)))) {
      if (!add_if_missing) return false; // not here and don't add...
#ifdef _SD_DAT3
      cdPin_ = _SD_DAT3;
      pinMode(_SD_DAT3, INPUT_PULLDOWN);
      check_disk_insertion_ = true;
      disk_inserted_time_ = 0;
      //return true;
#else
      return false;
#endif
    }
  }
  else
#endif
  {
    Serial.printf("Trying to open SPI config: %u %u %u %u\n", csPin_, cdPin_, opt_, maxSpeed_);
    if (!(disk_valid_ = sdfs.begin(SdSpiConfig(csPin_, opt_, maxSpeed_, port_)))) {
      Serial.println("    Failed to open");
      if (!add_if_missing || (cdPin_ == 0xff)) return false; // Not found and no add or not detect
      disk_inserted_time_ = 0;
      //return true;
    }
    // if cd Pin defined configure it to check in the looop function. 
    if (cdPin_) {
      check_disk_insertion_ = true;
      pinMode(cdPin_, INPUT_PULLUP);  // connects I think to SDCard frame ground
    }
  }
  addFSToStorage(false); // do the work to add us to the storage list.
  return true;
}


bool SDMTPClass::loop(MTPStorage_SD *mtpstorage, uint32_t store, uint32_t user_token) {
  //Serial.printf("SDMTPClass::loop %u\n", csPin_);

  if (!check_disk_insertion_) return true; // bail quick
  // delayMicroseconds(5);
  if (digitalRead(cdPin_)) {
    delay(1); // give it some time to settle; 
    if (!digitalRead(cdPin_)) return true; // double check.
    if (disk_inserted_time_ == 0) disk_inserted_time_ = millis(); // when did we detect it...
    // looks like SD Inserted. so disable the pin for now...
    // BUGBUG for SPI ones with extra IO pins can do more...

    delay(25);  // time to stabilize 
    Serial.printf("\n*** SD Card Inserted ***");
    #ifdef BUILTIN_SDCARD
    if (csPin_ == BUILTIN_SDCARD) {
      Serial.println("Trying to begin SDIO config");
      if (!(disk_valid_ = sdfs.begin(SdioConfig(FIFO_SDIO)))) {
        #ifdef _SD_DAT3
        //pinMode(_SD_DAT3, INPUT_PULLDOWN);  // it failed try to reinit again
        #endif
        Serial.println("    Begin Failed");
        if ((millis() - disk_inserted_time_) > DISK_INSERT_TEST_TIME) {
          pinMode(cdPin_, INPUT_DISABLE);
          check_disk_insertion_ = false; // only try this once        
          Serial.println("    Time Out");
        }
        return true; // bail
      }

      //pinMode(cdPin_, INPUT_DISABLE);  // shoot self in foot..
      check_disk_insertion_ = false; // only try this once
    }
    else
    #endif
    {
      Serial.printf("Trying to begin SPI config: %u %u %u %u\n", csPin_, cdPin_, opt_, maxSpeed_);
      if (!(disk_valid_ = sdfs.begin(SdSpiConfig(csPin_, opt_, maxSpeed_, port_)))) {
        Serial.println("    Begin Failed");
        if ((millis() - disk_inserted_time_) > DISK_INSERT_TEST_TIME) {
          pinMode(cdPin_, INPUT_DISABLE);
          check_disk_insertion_ = false; // only try this once        
          Serial.println("    Time Out");
        }
        // pinMode(cdPin_, INPUT_PULLDOWN);  // BUGBUG retury?  But first probably only when other removed?
        return true;
      }
      // not sure yet.. may not do this after it works. 
      pinMode(cdPin_, INPUT_DISABLE);
      check_disk_insertion_ = false; // only try this once
    }
    addFSToStorage(true); // do the work to add us to the storage list.
  } else {
    //
  }
  return true;
}

void SDMTPClass::addFSToStorage(bool send_events)
{
  // Lets first get size so won't hold up later
  if (disk_valid_) {
    // only called if the disk is actually there...
    uint64_t ts = totalSize();
    uint64_t us  = usedSize();
    Serial.print("Total Size: "); Serial.print(ts);
    Serial.print(" Used Size: "); Serial.println(us);
  }

  // The SD is valid now...
  uint32_t store = storage_.getStoreID(sdc_name_);
  if (store != 0xFFFFFFFFUL)
  {
    //if(send_events) mtpd_.send_StoreRemovedEvent(store);
    delay(50);
    //mtpd_.send_StorageInfoChangedEvent(store);
    //if(send_events) mtpd_.send_StoreAddedEvent(store);
    Serial.println("SDMTPClass::addFSToStorage - Disk in list, try reset event");
    if(send_events) mtpd_.send_DeviceResetEvent();

  } else {
    // not in our list, try adding it
    Serial.println("addFSToStorage: Added FS"); 
    store_ = storage_.addFilesystem(*this, sdc_name_, this, (uint32_t)(void*)this);
    if(send_events) mtpd_.send_StoreAddedEvent(store_);
  }



}
