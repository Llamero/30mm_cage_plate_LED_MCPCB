#include "Arduino.h"
#include "SDcard.h"
#include "SdFat.h"
#include "sdios.h"
#include <TimeLib.h>

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50)

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif  ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif  ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

//Assume FAT16/FAT32 format
SdFat32 card;
File32 f;

//Classes for formatting card
// SdCardFactory constructs and initializes the appropriate card.
SdCardFactory cardFactory;
// Pointer to generic SD card.
SdCard* m_card = nullptr;

char SDcard::message_buffer[512]; //Temporary buffer for preparing packets immediately before transmission
size_t SDcard::message_size; //Size of temporary packet to transmit
size_t SDcard::file_size; //Size of file on SD card
boolean SDcard::card_active; //Whether card has been activated
const char SDcard::seq_bin_dir[] = "seq_bin"; //Directory to save boot log files into - max length 8 char
const char SDcard::seq_files[][13] = {"dig_low.bin", "dig_high.bin", "con_stby.bin", "con_scan.bin"};

SDcard::SDcard()
{
}

void SDcard::init(){
  card_active = false;
}

// call back for file timestamps - from: https://forum.arduino.cc/index.php?topic=348562.0
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {

  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(year(), month(), day());

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(hour(), minute(), second());

  // Return low time bits in units of 10 ms.
  *ms10 = second() & 1 ? 100 : 0;
}

boolean SDcard::initializeSD(){
  message_size = 0;
  // set date time callback function for applying RTC synced time stamps to SD card file time stamps
  FsDateTime::setCallback(dateTime);

  //Test SD card
    //////////////////////CARD MISSING///////////////////////////
  if(!card_active){
    if (!card.begin(SD_CONFIG)) { //Check if SD card is present
      message_size = sprintf(message_buffer, "-Warning: SD card not found. Sequence files will not be permanently saved to the LED driver, and will be lost on reboot.");
      return false;
    }
    else card_active = true;
  }
  
  /////////////////INVALID PARTITION/////////////////////////////////////
  if (!card.volumeBegin()) { //Check that there is a FAT32 partition
    message_size = sprintf(message_buffer, "-Warning: SD card is not FAT32 formatted. Sequence files will not be permanently saved to the LED driver, and will be lost on reboot..");
    return false;
  }

  /////////////////CREATE DIRECTORIES IF NECESSARY/////////////////////////////////////
  //Create directories for saving log files and boot info
  if(!card.exists(seq_bin_dir)){ //Don't run begin again if it has already been run - known bug in SD.h library: https://arduino.stackexchange.com/questions/3850/sd-begin-fails-second-time
    if(!card_active){
      if(!card.begin(SD_CONFIG)){
        message_size = sprintf(message_buffer, "-Warning: SD initialization failed. Sequence files will not be permanently saved to the LED driver, and will be lost on reboot.");
        return false;
      }
      else card_active = true;
    } 
    if(!card.exists(seq_bin_dir)){ 
      if(!card.mkdir(seq_bin_dir)){
        message_size = sprintf(message_buffer, "-Warning: Could not access directory on SD card. Sequence files will not be permanently saved to the LED driver, and will be lost on reboot.");
        return false;
      }
    }
  }
  /////////////////CREATE EMPTY SEQ FILES IF NECESSARY///////////////////////////
  for(int a = 0; a<N_SEQ_FILES; a++){
    sprintf(message_buffer, "%s/%s", seq_bin_dir, seq_files[a]); //Path to file
    if(!card.exists(message_buffer)){ //If file doesn't exist, create an empty file
      f = card.open(message_buffer, FILE_WRITE);
      f.close();
    }
  }
  return message_size == 0;
}

//Save data to the SD card
bool SDcard::saveToSD(char *data_array, uint32_t start_index, uint32_t end_index, const char (*file_name), const char (*file_dir)){
  file_size = 0;
  if(end_index > start_index && end_index != (uint32_t) -1) file_size = end_index-start_index; //If buffer is not empty calcualte log size
  sprintf(message_buffer, "%s/%s", file_dir, file_name); //Path to file to save
   
  if(card.exists(file_dir)){ //Make sure that the save directories exist before trying to save to it - without this check open() will lock without SD card  
    //Open file
    card.remove(message_buffer); //Delete file to ensure clean sequence write
    f = card.open(message_buffer, FILE_WRITE); //Overwrite existing file if exists https://pubs.opengroup.org/onlinepubs/7908799/xsh/open.html
    if(f){
      if(file_size){ //Write file if there is data to write
        f.write((data_array + start_index), file_size);
      }
      f.close(); //File timestamp applied on close (save)
      message_size = 0;
    }
    else message_size = sprintf(message_buffer, "-Warning: Could not save file to SD card. Sequence files will not be permanently saved to the LED driver, and will be lost on reboot.");
  }
  else message_size = sprintf(message_buffer, "-Warning: Could not find SD card. Sequence files will not be permanently saved to the LED driver, and will be lost on reboot.");
  
  return bool(!message_size);
}

//Read data from the SD card
boolean SDcard::readFromSD(char *data_array, uint32_t start_index, uint32_t end_index, const char (*file_name), const char (*file_dir)){
  file_size = 0;
  if(end_index > start_index && end_index != (uint32_t) -1) file_size = end_index-start_index; //If buffer is not empty calcualte log size
  sprintf(message_buffer, "%s/%s", file_dir, file_name); //Path to file to save
  
  if(card.exists(seq_bin_dir)){ //Make sure that the save directories exist before trying to save to it - without this check open() will lock without SD card  
    //Open file
    f = card.open(message_buffer, FILE_READ);
    if(f){
      if(file_size == 0){
        file_size = f.size(); //Get the size of the file being read if one wasn't specified
        end_index = file_size;
      }
      f.readBytes((data_array + start_index), file_size);
      file_size = end_index;
      f.close(); //File timestamp applied on close (save)
      message_size = 0;
    }
    else message_size = sprintf(message_buffer, "-Warning: Could not read file on SD card. Sequence file \"%s\" was not loaded.", file_name);
  }
  else message_size = sprintf(message_buffer, "-Warning: Could not find SD card. Please check SD card and restart Teensy.");
  
  return bool(!message_size);
}

// call back for file timestamps - from: https://forum.arduino.cc/index.php?topic=348562.0
void SDcard::dateTime(uint16_t* date, uint16_t* time) {
 time_t unix_t = now();
 // return date using FAT_DATE macro to format fields
 *date = FAT_DATE(year(unix_t), month(unix_t), day(unix_t));
 // return time using FAT_TIME macro to format fields
 *time = FAT_TIME(hour(unix_t), minute(unix_t), second(unix_t));
}

//Delete all files and folders on SD card
boolean SDcard::formatSdCard(){
  uint32_t const ERASE_SIZE = 262144L;
  uint32_t firstBlock = 0;
  uint32_t lastBlock;
  uint32_t cardSectorCount = 0;
  FatFormatter fatFormatter;
  
  // Select and initialize proper card driver.
  m_card = cardFactory.newCard(SD_CONFIG);
  if (!m_card || m_card->errorCode()) {
    message_size = sprintf(message_buffer, "-FORMAT error: card init failed.");
    return false;
  }

  //Erase all data on card
  cardSectorCount = m_card->sectorCount();
  if (!cardSectorCount) {
    message_size = sprintf(message_buffer, "-FORMAT error: Get sector count failed.");
    return false;
  }
  
  do {
    lastBlock = firstBlock + ERASE_SIZE - 1;
    if (lastBlock >= cardSectorCount) {
      lastBlock = cardSectorCount - 1;
    }
    if (!m_card->erase(firstBlock, lastBlock)) {
      message_size = sprintf(message_buffer, "-FORMAT error: erase failed");
      return false;
    }
    firstBlock += ERASE_SIZE;
  } while (firstBlock < cardSectorCount);

  if (!m_card->readSector(0, (uint8_t*) message_buffer)) {
    message_size = sprintf(message_buffer, "-FORMAT error: readBlock");
    return false;
  }

  //Format card to FAT16/32
  fatFormatter.format(m_card, (uint8_t*) message_buffer);
  return true;
}


//Functions below are for debugging purposes

void SDcard::getFileList(){
  card.ls(LS_DATE | LS_SIZE);
}
