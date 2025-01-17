/*
  Different sync configurations for controlling the LED output
*/
#ifndef SDcard_h
#define SDcard_h

#include "Arduino.h"
#include "SDcard.h"
#include "SdFat.h"
#include "sdios.h"
#include <TimeLib.h>

class SDcard
{
  public:
    SDcard();
    static boolean initializeSD();
    static bool saveToSD(char *data_array, uint32_t start_index, uint32_t end_index, const char (*file_name), const char (*file_dir) = seq_bin_dir);
    static boolean readFromSD(char *data_array, uint32_t start_index, uint32_t end_index, const char (*file_name), const char (*file_dir) = seq_bin_dir);
    static boolean formatSdCard();
    static void getFileList();
    static char message_buffer[512]; //Temporary buffer for preparing packets immediately before transmission
    static size_t message_size; //Size of temporary packet to transmit
    static size_t file_size; //Size of file that is being read
    const static char seq_bin_dir[]; //Directory to save boot log files into - max length 8 char
    const static char seq_files[][13]; //file names foro the four sequence files - max length 8 char name + 4 char ext
    const static uint8_t N_SEQ_FILES = 4;
    static boolean card_active;
    
  private:
    const static int chipSelect = BUILTIN_SDCARD;
    static void dateTime(uint16_t* date, uint16_t* time); 
    static void init();
};

#endif
