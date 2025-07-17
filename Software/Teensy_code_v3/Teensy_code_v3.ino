#include <SPI.h>
#include "pinSetup.h"
#include "DAC.h"
#include "PacketSerial.h"
#include "SDcard.h"
#include <EEPROM.h>
#include <TimeLib.h> //Set RTC time and get time strings
#include <eFlexPwm.h>

//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT

#pragma pack(1) //Remove alignment padding bytes in structs - https://forum.pjrc.com/threads/50536-problem-with-union-in-Teensy-3-5
const uint8_t N_BOARDS = 3; //Number of boards connected to the LED driver
const uint8_t N_LEDS = 4; //Number of LEDs per board

struct configurationStruct{ //259 bytes
  uint8_t prefix;
  char driver_name[16]; //Name of LED driver: "default name"
  char led_names[3][4][16]; //Name of each LED channel
  boolean led_active[3][4]; //Whether LED channel is in use: {false, false, false, false}
  uint16_t current_limit[3][4]; //Current limit for each channel on DAC values: {0,0,0,0}
  boolean simul_led; //Whether LED boards can be turned on simultaneously
  uint16_t warn_temp; //Warn temp for three LED boards respectively in ADC units: {0,0,0}
  uint16_t fault_temp; //Fault temp for transistor, resistor, and external respectively in ADC units: {0,0,0}
  uint16_t driver_fan[2]; //Driver fan min and max temperatures in ADC units: {65535, 65535}
  uint8_t audio_volume[2]; //Status and alarm volumes for transducer: {10, 100}
  bool pushbutton_intensity; //LED intensity - on/off
  uint8_t pushbutton_mode; //LED illumination mode when alarm is active
  uint8_t checksum; //Checksum to confirm that configuration is valid
};

const struct defaultConfigurationStruct{ //259 bytes
  uint8_t prefix = 2;
  char driver_name[16] = "Unnamed driver "; //Name of LED driver: "default name"
  char led_names[3][4][16] = {{"LED #1         ", "LED #2         ", "LED #3         ", "LED #4         "}, //Name of each LED channel
                              {"LED #5         ", "LED #6         ", "LED #7         ", "LED #8         "},
                              {"LED #9         ", "LED #10        ", "LED #11        ", "LED #12        "}};
  boolean led_active[3][4] = {{false, false, false, false}, //Whether LED channel is in use: {false, false, false, false}
                              {false, false, false, false},
                              {false, false, false, false}};
  uint16_t current_limit[3][4] = {{65535,65535,65535,65535}, //Current limit for each channel on DAC values: {0,0,0,0}
                                  {65535,65535,65535,65535},
                                  {65535,65535,65535,65535}};
  boolean simul_led = false; //Whether LED boards can be turned on simultaneously
  uint16_t warn_temp = 14604; //Warn at 60°C
  uint16_t fault_temp = 8891; //Fault at 80°C
  uint16_t driver_fan[2] = {33963, 27958}; //Fan on at 30°C, fan max at 40°C
  uint8_t audio_volume[2] = {2, 100}; //Status and alarm volumes for transducer: {10, 100}
  bool pushbutton_intensity = true; //LED intensity at full intensity
  uint8_t pushbutton_mode = 1; //LED illumination mode when alarm is active
  uint8_t checksum = 227; //Checksum to confirm that configuration is valid
} defaultConfig;

struct syncStruct{ //158 bytes
  uint8_t prefix;
  uint8_t mode; //Type of sync - digital, analog, confocal, etc.
  
  uint8_t digital_channel; //The input channel for the sync signal
  uint8_t digital_mode[2]; //The digital sync mode  in the LOW and HIGH trigger states respectively
  uint8_t digital_led[2]; //The active LED channel in the LOW and HIGH trigger states respectively
  uint16_t digital_pwm[2]; //The PWM value in the LOW and HIGH trigger states respectively
  uint16_t digital_current[2]; //The DAC value in the LOW and HIGH trigger states respectively
  uint32_t digital_duration[2]; //The maximum number of milliseconds to hold LED state

  uint8_t analog_led[3]; //The active LED channel for each board

  boolean shutter_polarity; //Shutter polarity when scan is active
  uint8_t confocal_channel; //The input channel for the line sync signal
  boolean confocal_sync_mode; //Whether the line sync is digital (true) or analog (false)
  boolean confocal_sync_polarity[2]; //Sync polarity for digital and analog sync inputs
  uint16_t confocal_threshold; //Threshold for analog sync trigger
  boolean confocal_scan_mode; //Whether scan is unidirectional (true) or bidrectional (false)
  uint32_t confocal_mirror_period; //Time in clock cycles for the scanning mirror to complete one cycle
  uint32_t confocal_delay[3]; //Delay in clock cycles for each sync delay

  uint8_t confocal_mode[2]; //The digital sync mode  in the image and flyback states respectively
  uint8_t confocal_led[2]; //The active LED channel in the image and flyback states respectively
  uint16_t confocal_pwm[2]; //The PWM value in the image and flyback states respectively
  uint16_t confocal_current[2]; //The DAC value in the image and flyback states respectively
  uint32_t confocal_duration[2]; //The maximum number of milliseconds to hold LED state

  uint8_t checksum; //Checksum to confirm that configuration is valid
};

const struct defaultSyncStruct{ //158 bytes
  uint8_t prefix = 4;
  uint8_t mode = 0; //Type of sync - digital, analog, confocal, etc.
  
  uint8_t digital_channel = 0; //The input channel for the sync signal
  uint8_t digital_mode[2] = {0,0}; //The digital sync mode  in the LOW and HIGH trigger states respectively
  uint8_t digital_led[2] = {0,0}; //The active LED channel in the LOW and HIGH trigger states respectively
  uint16_t digital_pwm[2] = {0,0}; //The PWM value in the LOW and HIGH trigger states respectively
  uint16_t digital_current[2] = {0,0}; //The DAC value in the LOW and HIGH trigger states respectively
  uint32_t digital_duration[2] = {0,0}; //The maximum number of milliseconds to hold LED state
  
  uint8_t analog_led[3] = {0, 0, 0}; //The active LED channel for each board

  boolean shutter_polarity = true; //Shutter polarity when scan is active
  uint8_t confocal_channel = 0; //The input channel for the line sync signal
  boolean confocal_sync_mode = false; //Whether the line sync is digital (false) or analog (true)
  boolean confocal_sync_polarity[2] = {true, true}; //Sync polarity for digital and analog sync inputs
  uint16_t confocal_threshold = 2000; //Threshold for analog sync trigger
  boolean confocal_scan_mode = true; //Whether scan is unidirectional (false) or bidrectional (true)
  uint32_t confocal_mirror_period = 600000; //Time in µs for the scanning mirror to complete one cycle
  uint32_t confocal_delay[3] = {0,36000,0}; //Delay in clock cycles for each sync delay
  
  uint8_t confocal_mode[2] = {0,0}; //The digital sync mode  in the image and flyback states respectively
  uint8_t confocal_led[2] = {0,0}; //The active LED channel in the image and flyback states respectively
  uint16_t confocal_pwm[2] = {0,0}; //The PWM value (in clock cycles) in the image and flyback states respectively
  uint16_t confocal_current[2] = {0,0}; //The DAC value in the image and flyback states respectively
  uint32_t confocal_duration[2] = {0,0}; //The maximum number of milliseconds to hold LED state

  uint8_t checksum = 20; //Checksum to confirm that configuration is valid
} defaultSync;

struct sequenceHeaderStruct{ //6 bytes
  uint8_t prefix;
  uint8_t file_index;
  uint32_t buffer_size;
};

struct sequenceStruct{ //9 bytes
  uint8_t led_id; //LED channel
  uint16_t led_pwm; //LED PWM (analog out units)
  uint16_t led_current; //LED current (DAC units)
  uint32_t led_duration; //LED duration (microseconds)
};

struct statusStruct{
  uint8_t led_channel[3]; //Active LED channel
  uint16_t led_pwm[3]; //PWM value for internal and external fan respectively
  uint16_t led_current[3]; //DAC value for active LED
  uint8_t mode; //0=Sync, 1=PWM, 2=Current, 3=Off
  boolean state; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  boolean driver_control; //True = driver controls itself, False = GUI controls driver
  uint16_t temp[3]; //ADC temp reading of mosfet, resistor, and external respectively
  uint16_t fan_speed[3]; //PWM value for internal and external fan respectively
};

const struct defaultStatusStruct{
  uint8_t led_channel[3] = {N_LEDS, N_LEDS, N_LEDS}; //Set active LED channel to off
  uint16_t led_pwm[3] = {0, 0, 0}; //PWM value for internal and external fan respectively
  uint16_t led_current[3] = {0, 0, 0}; //DAC value for active LED
  uint8_t mode = 3; //0=Sync, 1=PWM, 2=Current, 3=Off
  boolean state = 0; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  boolean driver_control = true; //True = driver controls itself, False = GUI controls driver
  uint16_t temp[3] = {defaultConfig.driver_fan[0], defaultConfig.driver_fan[0], defaultConfig.driver_fan[0]}; //ADC temp reading of mosfet, resistor, and external respectively
  uint16_t fan_speed[3] = {0, 0, 0}; //PWM value for internal and external fan respectively
} defaultStatus;

const struct prefixStruct{
  uint8_t message = 0; //Send error, warning, and notification messages to be diplayed as pop-up in GUI
  uint8_t connection = 1; //Recv magic number at connection start and confirm with magic reply
  uint8_t send_config = 2; //Send active configuration byte file
  uint8_t recv_config = 3; //Recv and apply new configuration byte file
  uint8_t send_sync = 4; //Send active configuration byte file
  uint8_t recv_sync = 5; //Recv and apply new configuration byte file
  uint8_t send_seq = 6; //Send specified seq byte file from SD card if available
  uint8_t recv_seq = 7; //Recv specified seq byte file and save on SD card if available
  uint8_t send_id = 8; //Send the driver ID only
  uint8_t recv_time = 9; //Receive the unix time of the GUI to sync driver RTC
  uint8_t recv_stream = 10; //Signal the driver is ready to receive stream that is queued
  uint8_t send_stream = 11; //If recv - signals that ready for next packet
  uint8_t status_update = 12; //Status update packet for driver or GUI
  uint8_t calibration = 13; //Send a plot of LED square wave test output for calibrating op-amp compensation trimpots
  uint8_t gui_disconnect = 14; //GUI is disconnecting form LED driver
  uint8_t measure_period = 15; //Measure the mirror period - return as 
  uint8_t test_current = 16; //Measure the output for each channel - return as 4x uint16_t - disable channels with output below 51633 (0.7 V diff) to protect opamp.
  uint8_t test_volume = 17; //Test status or indicator volume
  uint8_t long_off = 148; //Prefix sent when computer has been off for a while
} prefix;

//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION//////////////UNION

//Convert between byte list and float
union FLOATUNION
{
 float float_var;
 uint8_t bytes[4];
}floatUnion;

//Convert between byte list and int
union INTUNION
{
 int int_var;
 uint8_t bytes[4];
}intUnion;

//Convert between byte list and int
union BYTE16UNION
{
 uint16_t bytes_var;
 uint8_t bytes[2];
}uint16Union;

union BYTE32UNION
{
 uint32_t bytes_var;
 uint8_t bytes[4];
}uint32Union;

//From: https://forum.arduino.cc/index.php?topic=263107.0
union CONFIGUNION //Convert binary buffer <-> config setup
{
   configurationStruct c;
   byte byte_buffer[sizeof(defaultConfigurationStruct)];
} conf;

union SYNCUNION //Convert binary buffer <-> sync setup
{
   syncStruct s;
   byte byte_buffer[sizeof(defaultSyncStruct)];
} sync;

union SEQHEADERUNION //Convert binary buffer <-> sync setup
{
   sequenceHeaderStruct s;
   byte byte_buffer[sizeof(sequenceHeaderStruct)];
} seq_header;

union SEQUNION //Convert binary buffer <-> sync setup
{
   sequenceStruct s;
   byte byte_buffer[sizeof(sequenceStruct)];
}seq;

union STATUSUNION //Convert binary buffer <-> sync setup
{
   statusStruct s;
   byte byte_buffer[sizeof(defaultStatusStruct)];
} current_status;

//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF//////////////TYPEDEF

//https://forum.arduino.cc/index.php?topic=171069.0
typedef void (* GenericFP)(const uint8_t*, size_t); //function pointer prototype to a function which takes an 'int' an returns 'void'
GenericFP function_router[256]; //create an array of 'GenericFP' function pointers. Notice the '&' operator

//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE//////////////VARIABLE

const static uint32_t COBS_BUFFER_SIZE = 4096; //Size of the COBS buffer
char MAGIC_SEND[] = "-A5DihJ3v5bbXKmAmmhQl"; //Magic number reply from Teensy verifying it is an LED driver "-" is for providing byte prefix in serial message
const static char MAGIC_RECEIVE[] = "51ERrUAT6ZWlThiltxJK"; //Magic number received from GUI to verify this is an LED driver
char temp_buffer[COBS_BUFFER_SIZE]; //Temporary buffer for preparing packets immediately before transmission
int temp_size; //Size of temporary packet to transmit
DMAMEM byte sequence_buffer[2][28001*sizeof(sequenceStruct)+10]; //Add buffer padding for prefix info on transmission
uint32_t send_stream_index = 0; //Current index position of stream that is being sent
uint32_t send_stream_size = 0; //Total size of file to be streamed
const static uint16_t DEFUALT_TIMEOUT = 500; //Default timeout for serial communication in ms
uint8_t status_index = 0; //Index counter for incrementally updating and transmitting status
elapsedMillis status_update_timer; //Status timer to track when to transmit the next update ########################################################################################
const uint8_t status_update_interval = 5; //The minimum time (in ms) between serial updates - prevents over-streaming of serial data and constantly accelerating fan
const uint32_t status_step_time_duration =  9; //Minimum time needed (in µs) to complete one status check
const uint32_t status_step_clock_duration =  status_step_time_duration*600; //Minimum time needed (in clock cycles) to complete one status check
elapsedMillis heartbeat; //Heartbeat timer to confirm that GUI is still connected ###########################################################################################################
const static uint32_t HEARTBEAT_TIMEOUT = 10000; //Driver will assume connection has closed if heartbeat not received within this time 
volatile uint32_t cpu_cycles = 0; //Track the number of CPU cycles for sub-microseconds timing precision
const static uint32_t cycle_offset = 7; //Number of cycles needed to check cycles ellapsed
boolean serial_connection_active = false; //Whether to send updates over serial - used to block updates during critical communication
uint8_t manual_mode = 1; //Store value of manual mode when status goes to sync (mode = 0)
boolean fault_active = false; //Whether the led driver is currently in a fault state (such as over-heated).
STATUSUNION stored_status; //Temporarily store operating status when status is over-ridden, such as during a thermal fault
STATUSUNION prev_status; //Tracks previous status state - allowing driver to disinguish if changes to status have happened
elapsedMicros audio; //Timer controlling audio volume and frequency ###########################################################################
elapsedMillis pulse; //Timer controlling tone pule interval ###################################################################################
boolean external_analog = false; //Whether to use the DAC or external analog input
uint16_t seq_steps[2]; //Number of setps in each active sync sequence
uint8_t active_channel; //Currently active LED channel
uint8_t update_flag = false; //Whether an update needs to be processed
uint32_t ext_avg = 65535; //Summing variable for performing rolling average on the external thermistor to denoise it
const uint16_t ext_avg_samples = 1024; //Size of sliding window for external average
bool update_current = false; //Whether the LED current needs to be updated.

//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS
pinSetup pin;
SDcard sd;
PacketSerial_<COBS, 0, COBS_BUFFER_SIZE> usb; //Sets Encoder, framing character, buffer size
DAC dac;

void setup() {
  //EEPROM.update(0,0); //Uncomment to reset EEPROM to defaults - re-comment and the upload code again
  //sd.formatSdCard(); sd.initializeSD();//Uncomment to format SD card - re-comment and the upload code again
  
  //Count cpu cycles for submircrosecond delay precision - https://forum.pjrc.com/threads/28407-Teensyduino-access-to-counting-cpu-cycles?p=71036&viewfull=1#post71036
  ARM_DEMCR |= ARM_DEMCR_TRCENA;
  ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
  cpu_cycles = ARM_DWT_CYCCNT;

  //Setup pin configurations
  pin.configurePins();
  dac.initiazize();

  //Setup usb communication
  sequence_buffer[0][0]= 0;
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600); //Needed for non-COBS streaming, such as large sequence files
  usb.begin(115200);
  usb.setPacketHandler(&onPacketReceived);

  //Setup SD card
  if(!sd.initializeSD()){ //Initiazlize SD card first so the sequence files can be retrieved on initializeConfigurations()
    if(!sd.formatSdCard()){ ; //Try reformatting the card
      sd.message_buffer[0] = prefix.message;
      usb.send((const unsigned char*) sd.message_buffer, sd.message_size);
      return;
    }
    if(!sd.initializeSD()){ //Re-initialize sd card
      sd.message_buffer[0] = prefix.message;
      usb.send((const unsigned char*) sd.message_buffer, sd.message_size);
      return;
    }
    else{
      temp_size = sprintf(temp_buffer, "-Warning: SD card fomat was invalid.  Card was successfully reformatted to FAT16/32.  All files were deleted.");
      temp_buffer[0] = prefix.message;
      usb.send((const unsigned char*) temp_buffer, temp_size);
    }
  }

  initializeConfigurations(); //Initialize configurations only after initializing SD card, as SD card is needed to load seqences
  status_index = 0;
  ledOff(); //For safety, boot to LED off
  
  //Initialize driver status
  for(uint8_t a=0; a<N_BOARDS; a++){
    current_status.s.led_channel[a] = defaultStatus.led_channel[a]; //Active LED channel
    current_status.s.led_pwm[a] = defaultStatus.led_pwm[a]; //PWM value for internal and external fan respectively
    current_status.s.led_current[a] = defaultStatus.led_current[a]; //DAC value for active LED
  }
  current_status.s.mode = defaultStatus.mode; //0=Sync, 1=PWM, 2=Current, 3=Off
  current_status.s.state = defaultStatus.state; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  current_status.s.driver_control = defaultStatus.driver_control; //True = driver controls itself, False = GUI controls driver
  for(uint8_t b=0; b<N_BOARDS; b++) current_status.s.temp[b] = pin.boardTemp(b) << 4; //Initialize board temperatures 
  for(uint8_t a=0; a==status_index; a++) checkStatus(); //Perform full round of status checks to get starting status of driver
}

void loop() {
  interrupts();
  update_flag = false; //Reset update flag on return on main loop
  if(current_status.s.mode) checkStatus();
  else syncRouter();
  delayMicroseconds(10);
}

// elapsedMillis t = 0;
// void debug(){
//   temp_size = sprintf(temp_buffer, "LED#%d PWM:%d Current:%d Mode:%s State:%s Control:%s", current_status.s.led_channel+1, current_status.s.led_pwm, current_status.s.led_current, !!current_status.s.mode ? "Manual":"Sync", current_status.s.state ? "LOW":"HIGH", current_status.s.driver_control ? "GUI":"Manual");
//   if(t>500){
//     if(serial_connection_active){
//       temp_buffer[0] = prefix.message;
//       usb.send((const unsigned char*) temp_buffer, temp_size);
//     }
//     else{
//       Serial.println(temp_buffer);
//     }
//     t=0;
//   }
// }

void syncRouter(){
  update_flag = false; //Clear the update flag
  switch(sync.s.mode){
    case 0: //Digital sync
      digitalSync();
      break;
    case 1: //Analog sync
      analogSync();
      break;
    case 2: //Confocal sync
      confocalSync();
      break;
//    case 3: //Serial sync
//      serialSync();
//      break;
    case 4: //Custom sync
      customSync();
      break;
    default:
      break;
  }
}

void getSeqStep(uint16_t sync_step){
  uint8_t board_number;
  uint8_t led_number;
  memcpy(seq.byte_buffer, sequence_buffer[current_status.s.state]+sync_step*sizeof(seq.byte_buffer), sizeof(seq.byte_buffer)); //Get current sequence step
  board_number = (seq.s.led_id) / N_LEDS;
  led_number = (seq.s.led_id) % N_LEDS;
  for(uint8_t i=0; i<N_BOARDS; i++){
    if(i==board_number){
      current_status.s.led_channel[i] = led_number;
      current_status.s.led_pwm[i] = seq.s.led_pwm;
      current_status.s.led_current[i] = seq.s.led_current; 
    }
    else{
      current_status.s.led_channel[i] = N_LEDS;
      current_status.s.led_pwm[i] = 0;
      current_status.s.led_current[i] = 0; 
    }
  }
}

void digitalSync(){ //5 µs jitter + 2 µs phase delay
  elapsedMicros duration;
  uint16_t sync_step;
  
  pinMode(pin.INPUTS[sync.s.digital_channel], INPUT); //Set sync input pin to input
  while(!current_status.s.mode && sync.s.mode == 0){
    sync_step = 0;
    current_status.s.state = digitalReadFast(pin.INPUTS[sync.s.digital_channel]); //Get state of sync input
    getSeqStep(sync_step); //Get first sequence step
    duration = 0;
    while(current_status.s.state == digitalReadFast(pin.INPUTS[sync.s.digital_channel]) && !update_flag){ //While trigger state doesn't change and driver still in digital sync mode 
      if(sync_step < seq_steps[current_status.s.state]){ //If the end of the sequence list has not been reached
        updateIntensity(); //Set led intensity to new values     
        if(seq.s.led_duration){ //If hold for a specific duration
          checkStatus(); //Check status at least once
          if(update_flag) return; //Exit on update
          while(current_status.s.state == digitalReadFast(pin.INPUTS[sync.s.digital_channel]) && !update_flag && duration < (seq.s.led_duration-status_step_time_duration)){ //Perform status checks during step is enough time
            checkStatus();
            if(update_flag) return; //Exit on update
          }
          while(current_status.s.state == digitalReadFast(pin.INPUTS[sync.s.digital_channel]) && duration < seq.s.led_duration); //Only check time during the last few microseconds for a precise incremental step
          duration -= seq.s.led_duration; //Reset duration timer
        }
        else{
          checkStatus(); //Check status at least once
          if(update_flag) return; //Exit on update
          while(current_status.s.state == digitalReadFast(pin.INPUTS[sync.s.digital_channel]) && !current_status.s.mode && sync.s.mode == 0){
            checkStatus(); //Hold until trigger changes
            if(update_flag) return; //Exit on update
          }
          break;
        }
        sync_step++; //Increment the sync step counter
        getSeqStep(sync_step); //Get next sequence step        
      }
      else{ //Report error if driver ran off the end of the sequence list (i.e. never encountered a hold)
        temp_size = sprintf(temp_buffer, "-Error: Digital Sync - %s reached the end of the sequence without encountering a hold. Step #%d of %d steps.", current_status.s.state ? "LOW":"HIGH", sync_step, seq_steps[current_status.s.state]);
        temp_buffer[0] = prefix.message;
        usb.send((const unsigned char*) temp_buffer, temp_size);
        duration = 0;
        initializeSeq(); //Try reloading seq
        while(duration < 200000){
          checkStatus(); //This can happen if there was rapid bounce in the trigger, so pause to avoid spamming this error for every bounce
          if(update_flag) return; //Exit on update
        }
        return;
      }
    }
  }
  pinMode(pin.INPUTS[sync.s.digital_channel], INPUT_DISABLE);   
}

void analogSync(){
  for(uint8_t a=0; a<N_BOARDS; a++){
    if(sync.s.analog_led[a] != current_status.s.led_channel[a]){ //Update channel first to avoid LED flahsing on changes
      for(uint8_t b=0; b<N_LEDS; b++){
        if(sync.s.analog_led[a] == b) digitalWriteFast(pin.RELAY[a][b], pin.RELAY_CLOSE);
        else digitalWriteFast(pin.RELAY[a][b], !pin.RELAY_CLOSE);
      }
      current_status.s.led_channel[a] = sync.s.analog_led[a];
      prev_status.s.led_channel[a] = current_status.s.led_channel[a];
    }
  }
}

void confocalSync(){
  // elapsedMicros duration; //Duration timer for sequence steps
  // uint16_t sync_step; //sequence step counter
  // const uint32_t interline_timeout = 180e6; //Timeout to stop looking for mirror sync - 1 second.
  // uint32_t pwm_clock_cycles; //The number of clock cycles equivalent to the PWM duration
  // uint32_t unidirectional_status_window = sync.s.confocal_delay[0] + sync.s.confocal_delay[1] + sync.s.confocal_delay[2] + 2*status_step_clock_duration; //Number of clock cycles between end of interline sequence and next trigger
  // float pwm_freq = 180000000/(float) sync.s.confocal_mirror_period; //Get the frequency of the mirror in Hz
  // float pwm_ratio = (float) sync.s.confocal_delay[1] / (float) sync.s.confocal_mirror_period; //Ratio of LED on time to total mirror period
  // boolean shutter_state; //Logical state of shutter input
  // boolean sync_pol; //Track polarity of sync output
  // uint8_t timeout = 0; //Flag for whether the line sync has timed out waiting for trigger - 0: no timeout, 1: new timeout - report error, 2: on going timeout - error already reported.  Flag resets when shutter closes.
  
  // if(sync.s.confocal_scan_mode){ //If the scan is bidirectional
  //   pwm_freq *= 2; //Double the pwm freq since the LED flashes twice per period
  //   pwm_ratio *= 2; //Double the pwm ratio since the ratio is now delay[1] / (0.5 * mirror period)
  // }
  // analogWriteFrequency(pin.INTERLINE, pwm_freq); //Set interline PWM freq to match mirror freq, also sets analog_select PWM freq (on same timer): https://www.pjrc.com/teensy/td_pulse.html
  
  // if(sync.s.confocal_mirror_period > unidirectional_status_window) unidirectional_status_window = sync.s.confocal_mirror_period - unidirectional_status_window;
  // else unidirectional_status_window = 0;
  // noInterrupts(); //Turn off interrupts for exact interline timing

  // //Lamda trigger sync funtions ---------------------------------------------------------------------------------------------------------------
  // auto waitForTrigger = [&] (){ //Wait for the trigger event
  //   cpu_cycles = ARM_DWT_CYCCNT; //Reset inerline timer
  //   if(sync.s.confocal_sync_mode){ //If analog sync
  //     analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
  //     if(sync.s.confocal_sync_polarity[1]){
  //       while(analogRead(pin.INPUTS[sync.s.confocal_channel]) < sync.s.confocal_threshold){ //Wait for input to rise above threshold - timeout after two mirror periods
  //         if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
  //           if(!timeout) timeout = 1; //Flag timeout
  //           break;
  //         }
  //         if(shutter_state != digitalReadFast(pin.INPUTS[0])) break; //Check if shutter changed
  //       }
  //     }
  //     else{
  //       while(analogRead(pin.INPUTS[sync.s.confocal_channel]) > sync.s.confocal_threshold){
  //         if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
  //           if(!timeout) timeout = 1; //Flag timeout
  //           break;
  //         }
  //         if(shutter_state != digitalReadFast(pin.INPUTS[0])) break; //Check if shutter changed
  //       }
  //     }
  //   }
  //   else{ //If digital sync
  //     while(digitalReadFast(pin.INPUTS[sync.s.confocal_channel]) != sync.s.confocal_sync_polarity[0]){ //What for line sync trigger
  //       if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
  //         if(!timeout) timeout = 1; //Flag timeout
  //         break;
  //       }
  //       if(shutter_state != digitalReadFast(pin.INPUTS[0])) break; //Check if shutter changed
  //     }
  //   }
  //   cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer
  // };

  // auto waitForTriggerReset = [&] (){ //Wait for the trigger event to reset - used to initially sync the LED driver to the trigger input
  //   cpu_cycles = ARM_DWT_CYCCNT; //Reset inerline timer
  //   if(sync.s.confocal_sync_mode){ //If analog sync
  //     analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
  //     if(sync.s.confocal_sync_polarity[1]){
  //       while(analogRead(pin.INPUTS[sync.s.confocal_channel]) > sync.s.confocal_threshold){ //Wait for input to fall below threshold - timeout after two mirror periods
  //         if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
  //           if(!timeout) timeout = 1; //Flag timeout
  //           break;
  //         }
  //         if(shutter_state != digitalReadFast(pin.INPUTS[0])) break; //Check if shutter changed
  //       }
  //     }
  //     else{
  //       while(analogRead(pin.INPUTS[sync.s.confocal_channel]) < sync.s.confocal_threshold){
  //         if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
  //           if(!timeout) timeout = 1; //Flag timeout
  //           break;
  //         }
  //         if(shutter_state != digitalReadFast(pin.INPUTS[0])) break; //Check if shutter changed
  //       }
  //     }
  //   }
  //   else{ //If digital sync
  //     while(digitalReadFast(pin.INPUTS[sync.s.confocal_channel]) == sync.s.confocal_sync_polarity[0]){ //What for line sync trigger to reset
  //       if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
  //         if(!timeout) timeout = 1; //Flag timeout
  //         break;
  //       }
  //       if(shutter_state != digitalReadFast(pin.INPUTS[0])) break; //Check if shutter changed
  //     }
  //   }
  //   cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer
  // };
  // //-----------------------------------------------------------------------------------------------------------------------------------------------------
  
  // pinMode(pin.INPUTS[0], INPUT); //Set shutter input pin to input
  // pinMode(pin.INPUTS[sync.s.confocal_channel], INPUT); //Set sync input pin to input
  // pinMode(pin.INTERLINE, OUTPUT); //Disconnect the 
  // while(!current_status.s.mode && sync.s.mode == 2){ //This loop is maintained as long as in confocal sync mode - checked each time the status state changes (imaging/standby)
  //   sync_step = 0;
  //   timeout = 0; //Reset the timrout flag when scan state changes.
  //   shutter_state = digitalReadFast(pin.INPUTS[0]); //Get state of shutter
  //   current_status.s.state = (shutter_state == sync.s.shutter_polarity);
    
  //   active_channel = current_status.s.led_channel;
  //   getSeqStep(sync_step); //Get first sequence step
  //   duration = 0; //Reset seq timer
  //   cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer

  //   if(current_status.s.state){ //If shutter is open wait for first trigger
  //     cpu_cycles = ARM_DWT_CYCCNT; //Reset inerline timer
  //     if(sync.s.confocal_sync_mode){ //If analog sync
  //       analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
  //       if(sync.s.confocal_sync_polarity[1]){
  //         while(analogRead(pin.INPUTS[sync.s.confocal_channel]) < sync.s.confocal_threshold){ //Wait for input to rise above threshold - timeout after two mirror periods
  //           checkStatus();
  //           if(shutter_state != digitalReadFast(pin.INPUTS[0]) || update_flag) break; //Check if shutter or status changed
  //           analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
  //         }
  //       }
  //       else{
  //         while(analogRead(pin.INPUTS[sync.s.confocal_channel]) > sync.s.confocal_threshold){
  //           checkStatus();
  //           if(shutter_state != digitalReadFast(pin.INPUTS[0]) || update_flag) break; //Check if shutter or status changed
  //           analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
  //         }
  //       }
  //     }
  //     else{ //If digital sync
  //       while(digitalReadFast(pin.INPUTS[sync.s.confocal_channel]) != sync.s.confocal_sync_polarity[0]){ //What for line sync trigger
  //         checkStatus();
  //         if(shutter_state != digitalReadFast(pin.INPUTS[0]) || update_flag) break; //Check if shutter or status changed
  //       }
  //     }
  //     cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer
  //   }
  //   checkStatus(); //Check status at least once per mirror cycle
  //   if(update_flag) goto quit; //Exit on update
    
  //   while(shutter_state == digitalReadFast(pin.INPUTS[0]) && !update_flag){ //While shutter state doesn't change and driver still in digital sync mode - checked each time a seq step is complete
  //     if(sync_step < seq_steps[current_status.s.state]){ //If the end of the sequence list has not been reached
  //       if(sync.s.confocal_mode[current_status.s.state] == 3){ //If sync uses external analog , set external analog pin HIGH
  //         pinMode(pin.ANALOG_SELECT, OUTPUT);
  //         external_analog = true;
  //         digitalWriteFast(pin.ANALOG_SELECT, HIGH); //Set external analog input
  //       }
  //       else{ //For all other modes, set led intensity to new values
  //         pinMode(pin.ANALOG_SELECT, OUTPUT);
  //         external_analog = false;
  //         digitalWriteFast(pin.ANALOG_SELECT, LOW); //Set internal analog input
  //         updateIntensity(); 
  //       }
        
  //       if(current_status.s.state){ //If scanning, convert PWM to clock cycles
  //         pwm_clock_cycles = round(((float) current_status.s.led_pwm * (float) sync.s.confocal_delay[1])/65535); //Calculate the number of clock cycles to leave the LED on during delay #2 to match the needed % PWM
  //         pinMode(pin.INTERLINE, OUTPUT); //Disconnect interline pin from PWM bus
  //       }
  //       else{ //If standby, convert PWM to proportion of total mirror period
  //         pwm_clock_cycles = round((float) current_status.s.led_pwm * pwm_ratio);
  //         analogWrite(pin.INTERLINE, (uint16_t) pwm_clock_cycles);
  //       }
  //       while(shutter_state == digitalReadFast(pin.INPUTS[0]) && !update_flag && (duration < seq.s.led_duration || seq.s.led_duration == 0)){ //Loop until shutter changes, update, or seq duration times out (0 = hold - no timeout) - Interline loop
  //         checkStatus(); //Check status at least once per mirror cycle
  //         if(update_flag) goto quit;
  //         if(current_status.s.state){ //If shutter is open (actively scanning) perform interline modulation
  //           waitForTriggerReset(); //Wait for trigger to reset - this insures the driver will always only sync to the start of a trigger, and not mid trigger
  //           if(sync.s.sync_output_channel) digitalWriteFast(pin.OUTPUTS[sync.s.sync_output_channel-1], sync_pol); //Toggle Sync
  //           sync_pol = !sync_pol; //Flip sync_pol polarity
  //           waitForTrigger();
  //           if(timeout){
  //             if(timeout == 1){
  //               temp_size = sprintf(temp_buffer, "-Error: Confocal Sync timed out waiting for line trigger. Check connection and re-measure mirror period.");
  //               temp_buffer[0] = prefix.message;
  //               usb.send((const unsigned char*) temp_buffer, temp_size);
  //               timeout = 2;
  //             }
  //           }
  //           else{
  //             while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[0]); //Wait for delay #1
  //             if(current_status.s.led_current) digitalWriteFast(pin.INTERLINE, HIGH);
  //             cpu_cycles += sync.s.confocal_delay[0]; //Increment interline timer
  //             while(ARM_DWT_CYCCNT - cpu_cycles < pwm_clock_cycles); //Wait for PWM delay
  //             digitalWriteFast(pin.INTERLINE, LOW);
  //             while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[1]); //Wait for end of delay #2
  //             cpu_cycles += sync.s.confocal_delay[1]; //Increment interline timer
  //             if(sync.s.confocal_delay[2] > status_step_clock_duration){ //See if there is enough time to check status during delay #3
  //               while(sync.s.confocal_delay[2] - (ARM_DWT_CYCCNT - cpu_cycles) > status_step_clock_duration){ //If there is enough time, perform status checks during delay #3
  //                 checkStatus(); //Check status while there is time to do so during the mirror sweep to the interline pulse
  //                 if(update_flag) goto quit;
  //               }
  //             }
  //             while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[2]); //Wait for end of delay #3
  //             if(sync.s.confocal_scan_mode){ //If scan is bidirectional, perform flyback interline
  //               if(current_status.s.led_current) digitalWriteFast(pin.INTERLINE, HIGH);
  //               cpu_cycles += sync.s.confocal_delay[2]; //Increment interline timer
  //               while(ARM_DWT_CYCCNT - cpu_cycles < pwm_clock_cycles); //Wait for PWM delay
  //               digitalWriteFast(pin.INTERLINE, LOW);
  //               while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[1]); //Wait for end of delay #2
  //             }
  //             else{ //If unidirectional, perform status checks if there is enough time before the next trigger
  //               if(unidirectional_status_window > status_step_clock_duration){ //See if there is enough time to check status during delay #3
  //                 while((unidirectional_status_window - (ARM_DWT_CYCCNT - cpu_cycles)) > status_step_clock_duration){ //If there is enough time, perform status checks during delay #3
  //                   checkStatus(); //Check status while there is time to do so during the mirror sweep to the interline pulse
  //                   if(update_flag) goto quit;
  //                 }
  //               }
  //             }
  //           }
  //         }
  //       }
  //       duration -= seq.s.led_duration; //Reset duration timer 
  //       sync_step++; //Increment the sync step counter
  //       getSeqStep(sync_step); //Get next sequence step
  //     }
  //     else{ //Report error if driver ran off the end of the sequence list (i.e. never encountered a hold)
  //       temp_size = sprintf(temp_buffer, "-Error: Confocal Sync - %s reached the end of the sequence without encountering a hold.", current_status.s.state ? "STANDBY":"SCANNING");
  //       temp_buffer[0] = prefix.message;
  //       usb.send((const unsigned char*) temp_buffer, temp_size);
  //       duration = 0;
  //       while(duration < 200000){
  //         checkStatus(); //This can happen if there was rapid bounce in the trigger, so pause to avoid spamming this error for every bounce
  //         if(update_flag) goto quit; //Exit on update
  //       }
  //       goto quit;
  //     }
  //   }
  // }
  // quit:
  //   analogWriteFrequency(pin.INTERLINE, pin.LED_FREQ); //Restore the interline timer to its defaul value: https://www.pjrc.com/teensy/td_pulse.html
  //   interrupts();
  //   pinMode(pin.ANALOG_SELECT, OUTPUT);
  //   digitalWriteFast(pin.ANALOG_SELECT, LOW);
  //   external_analog = false;
}

void customSync(){ //Two channel interline sequence, with external trigger between steps
  //const float mask_duration_cycles[8] = {433440, 216720, 108360, 54180, 42300, 42300, 42300, 42300}; //Duration of each mask MSB->LSB in clock cycles 
  //const float mask_duration_cycles[8] = {75600, 75600, 75600, 75600, 75600, 75600, 75600, 75600};
  const float mask_duration_cycles[8] = {742435, 371217, 185609, 141000, 141000, 141000, 141000, 141000};
  //const float mask_exp_cycles[8] = {361440, 180720, 90360, 45180, 22590, 11295, 5648, 2824}; //Max duration of LED exposure on each mask
  //const float mask_exp_cycles[8] = {70000, 70000, 70000, 70000, 70000, 70000, 70000, 70000};
  const float mask_exp_cycles[8] = {622435, 311217, 155609, 77804, 38902, 19451, 9726, 4863};
  //const uint32_t intermask_timeout = 1800; //Number of clock cycles before timing out between masks in a 24 bit sequence
  const uint32_t intermask_timeout = 260*600; //Number of clock cycles before timing out between masks in a 24 bit sequence
  //const uint32_t min_end_frame_duration = 200*600; //Number of clock cycles that marks the end of a frame
  const uint32_t min_end_frame_duration = 500*600; //Number of clock cycles that marks the end of a frame
  //const uint32_t frame_timeout = 600*600; //Number of clock cycles before timing out at end of 24 bit sequence
  //const uint32_t frame_timeout = 1500*600; //Number of clock cycles before timing out at end of 24 bit sequence
  const uint32_t resync_timeout = 20000*600; //Number of clock cycles before timing waiting to catch start of a new frame
  const uint32_t RELAY_OPEN_DURATION = 3; //duration, in µs, to pulse mosfets open to speed up off time of LED

  uint32_t duration; //Duration timer for sequence steps
  uint8_t sync_step; //sequence step counter
  uint32_t cpu_cycles = 0; //Timer from LED on to LED off - solves issue with line clock edge occuring during the flyback.
  //const uint32_t check_channel_cycles = 500; //The maximum number of clock cycles it takes to check and change the DMD channel
  //uint32_t frame_cycle_timer = 0; //Timer to track from the start of the previous blue - this eliminates multiple toggling 
  //bool relay_bit;
  uint8_t led_channel;
  uint8_t board_number;
  uint8_t led_number;
  bool resync = false; //Flag for whether sync with the DMD has been lost, and the driver needs to resync.
  bool led_state = false; //Whether an LED is on or off
  uint32_t pwm_clock_list[2][24]; //The number of clock cycles equivalent to the PWM duration for all 3 channels
  uint32_t pwm_delay_list[2][24]; //The number of clock cycles equivalent to the PWM duration for all 3 channels
  uint8_t seq_offset = 24;
  bool color_phase = 0; //Whether on RGB (LOW table) or OCV (HIGH table) phase
  uint32_t mask_index; //Current index to use when going through mask clock cycles
  
  //load the digital sync sequence
  sync.s.mode = 0;
  initializeSeq();
  sync.s.mode = 4;

  //Precalculate PWM values
  current_status.s.state = 1;
  for (uint8_t a = 0; a < 2; a++){
    current_status.s.state = (bool) a;
    sync_step = seq_offset;
    while(sync_step){ //Calculate the new PWM durations
      sync_step--;
      getSeqStep(sync_step); //Get sequence step
      mask_index = sync_step/3;
      board_number = seq.s.led_id/N_LEDS;
      pwm_clock_list[current_status.s.state][sync_step] = round(((float) current_status.s.led_pwm[board_number] * mask_exp_cycles[mask_index])/65535); //Calculate the number of clock cycles to leave the LED on during delay #2 to match the needed % PWM
      pwm_delay_list[current_status.s.state][sync_step] = round((mask_duration_cycles[mask_index] - pwm_clock_list[current_status.s.state][sync_step])/2); //Calculate delay for symmetric PWM
      pwm_clock_list[current_status.s.state][sync_step] += pwm_delay_list[current_status.s.state][sync_step]; //Add delay to end time to keep duration
    }
  }
  current_status.s.state = 0;

  //Disconnect interline pin from PWM mux and open all LED mux fets
  for(uint8_t a=0; a<N_BOARDS; a++){
    pinMode(pin.INTERLINE[a], OUTPUT);
    digitalWriteFast(pin.INTERLINE[a], LOW);
    for(uint8_t b=0; b<N_LEDS; b++) digitalWriteFast(pin.RELAY[a][b], !pin.RELAY_CLOSE);
    prev_status.s.led_channel[a] = current_status.s.led_channel[a];
  }
  
  pinMode(pin.INPUTS[0], INPUT); //Set sync input pin to input
  pinMode(pin.INPUTS[1], INPUT); //Set sync input pin to input
  pinMode(pin.INPUTS[2], INPUT); //Set sync input pin to input
  pinMode(pin.INPUTS[3], INPUT_DISABLE);

  noInterrupts(); //Turn off interrupts for exact interline timing
 
  auto checkChannel = [&] (){ //Check which DMD channel is active - 2.4 µs per cycle
    if(!resync){
      if(digitalReadFast(pin.INPUTS[0])){ 
        if(sync_step%3 == 0){
          if(ARM_DWT_CYCCNT - cpu_cycles > pwm_clock_list[current_status.s.state][sync_step] && led_state){
            digitalWriteFast(pin.INTERLINE[board_number], LOW);  //Turn off LED if at end of PWM cycle
            digitalWriteFast(pin.RELAY[board_number][led_number], !pin.RELAY_CLOSE); //Open Mosfet for faster off time
            delayMicroseconds(RELAY_OPEN_DURATION);
            digitalWriteFast(pin.RELAY[board_number][led_number], pin.RELAY_CLOSE); //Open Mosfet
            led_state = false;
          }
          else if(ARM_DWT_CYCCNT - cpu_cycles > pwm_delay_list[current_status.s.state][sync_step] && !led_state){
            digitalWriteFast(pin.INTERLINE[board_number], HIGH); //Turn on LED at start of PWM cycle
            digitalWriteFast(pin.RELAY[board_number][led_number], pin.RELAY_CLOSE); //Open Mosfet
            led_state = true;
          } 
          return; //No change so return
        }
        else if(sync_step%3 == 2){ //If channel changed in order
          cpu_cycles = ARM_DWT_CYCCNT; //Reset clock cycle timer
          sync_step++;
        } 
        else{
          playStatusTone(); //Warn that sync was lost 
          resync = true; //If active sync channel is out of order, resync to DMD
        } 
      }
      else if(digitalReadFast(pin.INPUTS[1])){
        if(sync_step%3 == 1){
          if(ARM_DWT_CYCCNT - cpu_cycles > pwm_clock_list[current_status.s.state][sync_step] && led_state){
            digitalWriteFast(pin.INTERLINE[board_number], LOW);  //Turn off LED if at end of PWM cycle
            digitalWriteFast(pin.RELAY[board_number][led_number], !pin.RELAY_CLOSE); //Open Mosfet for faster off time
            delayMicroseconds(RELAY_OPEN_DURATION);
            digitalWriteFast(pin.RELAY[board_number][led_number], pin.RELAY_CLOSE); //Open Mosfet
            led_state = false;
          }
          else if(ARM_DWT_CYCCNT - cpu_cycles > pwm_delay_list[current_status.s.state][sync_step] && !led_state){
            digitalWriteFast(pin.INTERLINE[board_number], HIGH); //Turn on LED at start of PWM cycle
            digitalWriteFast(pin.RELAY[board_number][led_number], pin.RELAY_CLOSE); //Open Mosfet
            led_state = true;
          } 
          return; //No change so return
        }
        else if(sync_step%3 == 0){ //If channel changed in order
          cpu_cycles = ARM_DWT_CYCCNT; //Reset clock cycle timer
          sync_step++;
        } 
        else{
          playStatusTone(); //Warn that sync was lost 
          resync = true; //If active sync channel is out of order, resync to DMD
        } 
      }
      else if(digitalReadFast(pin.INPUTS[2])){
        if(sync_step%3 == 2){
          if(ARM_DWT_CYCCNT - cpu_cycles > pwm_clock_list[current_status.s.state][sync_step] && led_state){
            digitalWriteFast(pin.INTERLINE[board_number], LOW);  //Turn off LED if at end of PWM cycle
            digitalWriteFast(pin.RELAY[board_number][led_number], !pin.RELAY_CLOSE); //Open Mosfet for faster off time
            delayMicroseconds(RELAY_OPEN_DURATION);
            digitalWriteFast(pin.RELAY[board_number][led_number], pin.RELAY_CLOSE); //Open Mosfet
            led_state = false;
          }
          else if(ARM_DWT_CYCCNT - cpu_cycles > pwm_delay_list[current_status.s.state][sync_step] && !led_state){
            digitalWriteFast(pin.INTERLINE[board_number], HIGH); //Turn on LED at start of PWM cycle
            digitalWriteFast(pin.RELAY[board_number][led_number], pin.RELAY_CLOSE); //Open Mosfet
            led_state = true;
          }
          return; //No change so return
        }
        else if(sync_step%3 == 1){ //If channel changed in order
          cpu_cycles = ARM_DWT_CYCCNT; //Reset clock cycle timer
          sync_step++;
        } 
        else{
          playStatusTone(); //Warn that sync was lost 
          resync = true; //If active sync channel is out of order, resync to DMD
        } 
      }
      else{ //If all channels are LOW, turn off LED to dark blank between frames.  This is essential for proper image encoding.; 
        for(uint8_t a=0; a<N_BOARDS; a++) digitalWriteFast(pin.INTERLINE[a], LOW);  //Turn off LED
        cpu_cycles = ARM_DWT_CYCCNT; //Reset clock cycle timer
        if(sync_step < seq_offset-1){ //If not at the last frame wait for next frame
          while(ARM_DWT_CYCCNT - cpu_cycles < intermask_timeout){ //wait for a pin to go high
            if(digitalReadFast(pin.INPUTS[0]) || digitalReadFast(pin.INPUTS[1]) || digitalReadFast(pin.INPUTS[2])) return; //if a pin goes high, exit 
          }
          playStatusTone(); //Warn that sync was lost       
        }
        resync = true; //If timed out waiting for next mask, resync to the DMD
      }
    }

    if(resync){ //If resync, wait for the extended dark frame at the end of the pattern sequence to find the start of the next pattern sequence
      digitalWriteFast(pin.INTERLINE[board_number], LOW);  //Turn off LED
      if(sync_step >= seq_offset) playStatusTone();  //Play status tone if resyncing without reaching the end of seq list - indicates that sync was lost 
      cpu_cycles = ARM_DWT_CYCCNT; //Reset clock cycle timer
      color_phase = !color_phase; //Flip the color phase
      current_status.s.state = color_phase; //Apply color phase
      checkStatus();

      while(resync){
        while(digitalReadFast(pin.INPUTS[0]) || digitalReadFast(pin.INPUTS[1]) || digitalReadFast(pin.INPUTS[2])){ //Wait for a dark frame
          if(ARM_DWT_CYCCNT-cpu_cycles > resync_timeout) return; //Quit if frame duration is too long - prevent while loop from blocking in the event of a lost connection
        }
        cpu_cycles = ARM_DWT_CYCCNT; //Reset timer
        while(!(digitalReadFast(pin.INPUTS[0]) || digitalReadFast(pin.INPUTS[1]) || digitalReadFast(pin.INPUTS[2])) && resync){
          if(ARM_DWT_CYCCNT-cpu_cycles > min_end_frame_duration){ //If extended dark frame is found
            while(!(digitalReadFast(pin.INPUTS[0]) || digitalReadFast(pin.INPUTS[1]) || digitalReadFast(pin.INPUTS[2]))){ //Wait for end of dark frame
              if(ARM_DWT_CYCCNT-cpu_cycles > resync_timeout) return; //Quit if frame duration is too long - prevent while loop from blocking in the event of a lost connection
            } 
            sync_step = 0; //Move to first patternsince this is the end of the pattern sequence
            resync = false; //Clear resync flag
            cpu_cycles = ARM_DWT_CYCCNT; //Reset timer for seq step 0
          }
        }
      }
    }
    if(!resync){ //If LED color changed - update LED
      digitalWriteFast(pin.INTERLINE[board_number], LOW);  //Turn off LED
      getSeqStep(sync_step); //Get first sequence step

      //Switch mux board to the correct channel
      led_channel = seq.s.led_id;
      board_number = led_channel/N_LEDS;
      led_number = led_channel%N_LEDS;
      for(uint8_t a = 0; a<N_BOARDS; a++){
        if(a == board_number){
          current_status.s.led_channel[a] = led_number;
          current_status.s.led_current[a] = seq.s.led_current; 
        } 
        else{
          current_status.s.led_channel[a] = N_LEDS;
          current_status.s.led_current[a] = 0; 
        } 
      }
      for(uint8_t a = 0; a<N_BOARDS; a++){
        if(prev_status.s.led_current[a] != current_status.s.led_current[a]){ //Update current last to avoid LED flahsing on changes
          dac.setSingleCurrent(a, current_status.s.led_current[a]);
          prev_status.s.led_current[a] = current_status.s.led_current[a];
          if(pin.FAN_PWM[a] == 1){
            pinMode(1, OUTPUT);
            analogWrite(pin.FAN_PWM[a], current_status.s.fan_speed[a]); //_________________________________________________________________________________________________________________________________
          }
        } 
      }
    }
  };
  //-----------------------------------------------------------------------------------------------------------------------------------------------------

  while(!current_status.s.mode && sync.s.mode == 4){ //This loop is maintained as long as in custom sync mode - checked each time the status state changes (imaging/standby)
    checkStatus(); //Check status at least once per mirror cycle
    if(update_flag) goto quit; //Exit on update
    noInterrupts();
    resync = true;
    checkChannel(); //Catch start of next frame
    while(!update_flag){ //While shutter state doesn't change and driver still in digital sync mode - checked each time a seq step is complete
      if(sync_step < seq_steps[current_status.s.state]){ //If the end of the sequence list has not been reached
        if(update_flag) goto quit;     
        noInterrupts();
        checkChannel();
      }
      else{ //Report error if driver ran off the end of the sequence list (i.e. never encountered a hold)
        temp_size = sprintf(temp_buffer, "-Error: Confocal Sync - %s reached the end of the sequence without encountering a hold.", current_status.s.state ? "STANDBY":"SCANNING");
        temp_buffer[0] = prefix.message;
        usb.send((const unsigned char*) temp_buffer, temp_size);
        duration = 0;
        while(duration < 200000){
          checkStatus(); //This can happen if there was rapid bounce in the trigger, so pause to avoid spamming this error for every bounce
          if(update_flag) goto quit; //Exit on update
        }
        goto quit;
      }
    }
  }
  quit:
    interrupts();
    external_analog = false;
    pinMode(pin.INPUTS[0], INPUT_DISABLE); //Set sync input pin to input
    pinMode(pin.INPUTS[1], INPUT_DISABLE); //Set sync input pin to input
    pinMode(pin.INPUTS[2], INPUT_DISABLE); //Set sync input pin to input
    pinMode(pin.INPUTS[3], INPUT_DISABLE); //Set sync input pin to input
}

void initializeSeq(){ //Setup seq
  uint8_t *sync_pointer; //Pointer to whether digital sync or confocal sync set
  uint8_t file_index; //Index of file name
  if(sync.s.mode == 0 || sync.s.mode == 2){ //if doing a digital or confocal sync
    if(sync.s.mode == 0){
      sync_pointer = &sync.s.digital_led[0]; //Point to start of digital sync info
      file_index = 0; //Index offset to sd card file name
    }
    else{
      sync_pointer = &sync.s.confocal_led[0]; //Point to start of confocal sync info
      file_index = 2; //Index offset to sd card file name
    }
    for(uint8_t a=0; a<2; a++){
      if((sync.s.mode == 0 && sync.s.digital_mode[a] == 0) || (sync.s.mode == 2 && sync.s.confocal_mode[a] == 0)){ //If mode is off
        seq.s.led_id = 0; //Don't change LED channel
        seq.s.led_pwm = 0; //Turn off led PWM
        seq.s.led_current = 0; //Turn off led current
        seq.s.led_duration = 0; //Hold at off
        memcpy(sequence_buffer[a], seq.byte_buffer, sizeof(seq.byte_buffer));
        seq_steps[a] = 1;
      }
      else if((sync.s.mode == 0 && sync.s.digital_mode[a] == 1) || (sync.s.mode == 2 && sync.s.confocal_mode[a] == 1)){ //If single event
        memcpy(&seq.s.led_id, sync_pointer+a, sizeof(seq.s.led_id));
        //seq.s.led_id -= 1; //Shift from id# to list index
        memcpy(&seq.s.led_pwm, sync_pointer+2+2*a, sizeof(seq.s.led_pwm));
        memcpy(&seq.s.led_current, sync_pointer+6+2*a, sizeof(seq.s.led_current));
        memcpy(&seq.s.led_duration, sync_pointer+10+4*a, sizeof(seq.s.led_duration));
        memcpy(sequence_buffer[a], seq.byte_buffer, sizeof(seq.byte_buffer));
        seq_steps[a] = 1;
      }
      else if((sync.s.mode == 0 && sync.s.digital_mode[a] == 2) || (sync.s.mode == 2 && sync.s.confocal_mode[a] == 2)){ //If sequence of events
        if(!sd.readFromSD((char*) sequence_buffer[a], 0, 0, sd.seq_files[file_index+a])){ //If no SD is found, send error message 
          temp_size = sprintf(temp_buffer, "%s", sd.message_buffer);  
          temp_buffer[0] = prefix.message;
          usb.send((const unsigned char*) temp_buffer, temp_size);
        }
        else{
          seq_steps[a] = sd.file_size/sizeof(seq.byte_buffer); //Calculate number of sequence steps given file size
          //for(int b=0; b<seq_steps[a]; b++) --*(sequence_buffer[a]+b*sizeof(seq.byte_buffer));  //Decrement LED IDs 
        }
      }
      else if(sync.s.mode == 2 && sync.s.confocal_mode[a] == 3){ //If confocal sync with external analog
        seq.s.led_id = 0; //Don't change LED channel
        seq.s.led_pwm = 65535; //Set PWM to max
        seq.s.led_current = 0; //Turn off led current
        seq.s.led_duration = 0; //Hold at off
        memcpy(sequence_buffer[a], seq.byte_buffer, sizeof(seq.byte_buffer));
        seq_steps[a] = 1;
      }
      memcpy(seq.byte_buffer, sequence_buffer[a]+(seq_steps[a]-1)*sizeof(seq.byte_buffer), sizeof(seq.byte_buffer)); //Get the last sequence step
      if(seq.s.led_duration){ //If the last step is not a hold (duration > 0) then add a hold to the end of the sequence
        seq.s.led_id = 0; //Don't change LED channel
        seq.s.led_pwm = 0; //Turn off led PWM
        seq.s.led_current = 0; //Turn off led current
        seq.s.led_duration = 0; //Hold at off
        memcpy(sequence_buffer[a] + seq_steps[a]*sizeof(seq.byte_buffer), seq.byte_buffer, sizeof(seq.byte_buffer)); //Copy led off hold to end of sequence.
        seq_steps[a] += 1;
      }
    }
  }
}

void checkStatus(){
  interrupts(); //Activate interrupts to allow serial to be monitored and sent
  uint16_t a;
  uint16_t b;
  uint16_t c;
  switch(status_index){
    case 0: //Check driver temperatures - 1.5 µs
      status_index++;
      current_status.s.temp[0] = (current_status.s.temp[0] >> 4) * 15 + pin.boardTempFast(0);
      break;
    case 1:
      status_index++;
      current_status.s.temp[1] = (current_status.s.temp[1] >> 4) * 15 + pin.boardTempFast(1);
      break;
    case 2: //Check external thermistor with rolling average - 3.1 µs to 4.5 µs max
      status_index++;
      current_status.s.temp[2] = (current_status.s.temp[2] >> 4) * 15 + pin.boardTempFast(2);
      break;
    case 3: //Check if any of the temperatures is past the fault temperature - 0.2 µs
      status_index++;
      if(!fault_active) thermalFault();
      break;
    case 4: //Set fan speeds - 0.4 µs
      status_index++;
      for(a=0; a<N_BOARDS; a++) setFan(a); //Update fan based on highest internal temperature (lowest ADC value)
      break;
    case 5: //Check pot positions if in manual mode - 1,300 µs per pot - upto 4 ms if all channels simultaneously active
      status_index++;
      if(current_status.s.driver_control && !fault_active && current_status.s.mode){ //Only check pot if driver control and in manual mode
        for(a=0; a<N_BOARDS; a++){
          if(current_status.s.led_channel[a] < N_LEDS){
            if(current_status.s.mode == 1){
              current_status.s.led_pwm[a] = pin.potValue(a);
              current_status.s.led_current[a] = conf.c.current_limit[a][current_status.s.led_channel[a]];
              updateIntensity(a); //Update the LED intensity with the new values
            }
            else if(current_status.s.mode == 2){
              current_status.s.led_current[a] = ((uint32_t) pin.potValue(a) * (uint32_t) conf.c.current_limit[a][current_status.s.led_channel[a]])>>16;
              current_status.s.led_pwm[a] = 65535; 
              updateIntensity(a); //Update the LED intensity with the new values
            }
            else{
              ledOff();
            }          
          }
        }
      }
      break;
    case 6: //Check pushbuttons and update LEDs - 1.05 µs
      status_index++; 
      if(!fault_active){ 
        if(current_status.s.driver_control){ //If in driver control and in manual mode, check for button presses
          for(a=0; a<N_BOARDS; a++){ //Check if any pushbutton is pressed
            if(!digitalReadFast(pin.PUSHBUTTON[a])){
              delay(pin.DEBOUNCE);
              for(b=0; b<1000; b++){
                if(digitalReadFast(pin.PUSHBUTTON[a])) break; //Wait for button release
                for(c=0; c<N_BOARDS; c++){ //Check if another button is also pressed
                    if(!digitalReadFast(pin.PUSHBUTTON[c]) && c != a){ //If two buttons are pressed simultaneously - switch to sync mode
                      playStatusTone();
                      if(current_status.s.mode){
                        ledOff(); //ledOff overrides mode, so check mode before turning off leds
                        manual_mode = 0;
                        current_status.s.mode = manual_mode;
                        update_flag = true;
                      } 
                      else{ //Play second tone to indicate manual mode
                        ledOff();
                        delay(100);
                        playStatusTone();
                        update_flag = true;
                      } 
                      while(!digitalReadFast(pin.PUSHBUTTON[a]) || !digitalReadFast(pin.PUSHBUTTON[c])) delay(10);
                      delay(pin.DEBOUNCE);
                      return;
                    }
                  }
                delay(1);
              }
              if(current_status.s.mode){ //If not in sync mode, update LED intensity
                if(b >= 1000){ //If button was held for 1 second, turn off LED
                  playStatusTone();
                  if(conf.c.simul_led) ledOff(a); //For simul, only turn off a single LED
                  else ledOff(); //If not simul led, one off means all off = mode 3
                  while(!digitalReadFast(pin.PUSHBUTTON[a])) delay(10);
                  delay(pin.DEBOUNCE);
                }
                else{ //Otherwise increment LED channel
                  delay(pin.DEBOUNCE);
                  current_status.s.led_channel[a]++;
                  if(current_status.s.led_channel[a] > N_LEDS) current_status.s.led_channel[a] = 0; //Roll over first LED at end of cycle.
                  while(current_status.s.led_channel[a] < N_LEDS){ //Check that LED channel is active
                    if(conf.c.led_active[a][current_status.s.led_channel[a]]) break;
                    else current_status.s.led_channel[a]++; //If not, skip to next channel
                  }
                  if(current_status.s.led_channel[a] >= N_LEDS){ //Turn LED off when finished cycling through all available LEDs
                    if(conf.c.simul_led) ledOff(a); //For simul, only turn off a single LED
                    else ledOff(); //If not simul led, one off means all off = mode 3
                  }
                  else{
                    manual_mode = 1;
                    current_status.s.mode = manual_mode; //Update mode
                    current_status.s.led_current[a] = conf.c.current_limit[a][current_status.s.led_channel[a]];
                    updateIntensity(a); //Update the LED intensity with the new values 
                    if(!conf.c.simul_led){ //If LEDs cannot be turned on simultaneously, turn off other LED boards.
                      for(c=0; c<N_BOARDS; c++){
                        if(c != a) ledOff(c);
                      }
                    } 
                  }         
                  if(conf.c.pushbutton_intensity){
                    pin.setButtonColor(a, current_status.s.led_channel[a]);
                  } 
                  else pin.setButtonColor(a, N_LEDS);      
                }
              }
            }
            else if(conf.c.pushbutton_intensity){
               pin.setButtonColor(a, current_status.s.led_channel[a]);
            }
          }
        }
        else if(conf.c.pushbutton_intensity){
          for(a=0; a<N_BOARDS; a++) pin.setButtonColor(a, current_status.s.led_channel[a]);
        }     
      }
      break;
    case 7: //Send current status to led driver - 4.76 µs
      status_index++;
      if(status_update_timer >= status_update_interval){ //Status timer to track when to transmit the next update
        status_update_timer = 0; //Reset the status update timer
        if(heartbeat >= HEARTBEAT_TIMEOUT && serial_connection_active){ //Default to LED off if connection is lost
          ledOff();
          manual_mode = 3;
          serial_connection_active = false; //Timeout update transmissions if serial is no longer received
        }
        if(serial_connection_active){ //If connection is active, send status update
          temp_buffer[0] = prefix.status_update;
          memcpy(temp_buffer+1, current_status.byte_buffer, sizeof(current_status.byte_buffer));
          usb.send((const unsigned char*) temp_buffer, sizeof(current_status.byte_buffer)+1);
        }
      }
      if(!serial_connection_active) current_status.s.driver_control = true;
      break;
    case 11: //Set ananlog_select pin based on driver configuration
      status_index++;
      if(current_status.s.mode && external_analog){ //If in manual mode, use internal analog
        external_analog = false;
        dac.allOff(); //Set external analog input
      }
      else{ //If in sync mode
        if(((sync.s.mode == 0 && sync.s.digital_mode[current_status.s.state] == 3) || (sync.s.mode == 2 && sync.s.confocal_mode[current_status.s.state] == 3) || sync.s.mode == 1) && !external_analog){ //If state requires ext. analog
          external_analog = true;
          dac.externalSignal();
        }
      }
      break;
    default: //Check if a serial packet has been received - 0.37 µs
      usb.update();
      memcpy(prev_status.byte_buffer, current_status.byte_buffer, sizeof(current_status.byte_buffer)); 
      status_index = 0; //Reset status index if no cases match
      break;
  }

  if((sync.s.mode==1 || sync.s.mode==2) && !current_status.s.mode) noInterrupts(); //Disable interrupts if in confocal mode, as the scan mirror is used as the interrupt clock
}

void updateIntensity(){
  for(uint8_t a = 0; a<N_BOARDS; a++){
    if(prev_status.s.led_channel[a] != current_status.s.led_channel[a]){ //Update channel first to avoid LED flahsing on changes
      for(uint8_t b=0; b<N_LEDS; b++){
        if(current_status.s.led_channel[a] == b) digitalWriteFast(pin.RELAY[a][b], pin.RELAY_CLOSE);
        else digitalWriteFast(pin.RELAY[a][b], !pin.RELAY_CLOSE);
      }
      prev_status.s.led_channel[a] = current_status.s.led_channel[a];
    }
    if(prev_status.s.led_pwm[a] != current_status.s.led_pwm[a]){
      dac.setSinglePWM(a, current_status.s.led_pwm[a]);
      prev_status.s.led_pwm[a] = current_status.s.led_pwm[a];
    }
    if(prev_status.s.led_current[a] != current_status.s.led_current[a]){ //Update current last to avoid LED flahsing on changes
      dac.setSingleCurrent(a, current_status.s.led_current[a]);
      prev_status.s.led_current[a] = current_status.s.led_current[a];
      if(pin.FAN_PWM[a] == 1){
        pinMode(1, OUTPUT);
        analogWrite(pin.FAN_PWM[a], current_status.s.fan_speed[a]); //_________________________________________________________________________________________________________________________________
      }
    } 
  }
}

void updateIntensity(uint8_t board_id){
  if(prev_status.s.led_current[board_id] != current_status.s.led_current[board_id]){
    dac.setSingleCurrent(board_id, current_status.s.led_current[board_id]);
    prev_status.s.led_current[board_id] = current_status.s.led_current[board_id];
  } 
  if(prev_status.s.led_pwm[board_id] != current_status.s.led_pwm[board_id]){
    dac.setSinglePWM(board_id, current_status.s.led_pwm[board_id]);
    prev_status.s.led_current[board_id] = current_status.s.led_current[board_id];
  } 
  if(prev_status.s.led_channel[board_id] != current_status.s.led_channel[board_id]){
    for(uint8_t a=0; a<N_LEDS; a++){
      if(current_status.s.led_channel[board_id] == a) digitalWriteFast(pin.RELAY[board_id][a], pin.RELAY_CLOSE);
      else digitalWriteFast(pin.RELAY[board_id][a], !pin.RELAY_CLOSE);
    }
    if(pin.FAN_PWM[board_id] == 1){
      pinMode(1, OUTPUT);
      analogWrite(pin.FAN_PWM[board_id], current_status.s.fan_speed[board_id]); //_________________________________________________________________________________________________________________________________
    }
    prev_status.s.led_channel[board_id] = current_status.s.led_channel[board_id];
  }
}

void playStatusTone(){
  audio = 0;
  pulse = 0;
  while(pulse < 100){ //Play tone for 0.2 seconds
    if(audio < conf.c.audio_volume[0]){
      digitalWriteFast(pin.ALARM[0], HIGH);
      digitalWriteFast(pin.ALARM[1], LOW);
    }
    else if(audio <= 256){
      digitalWriteFast(pin.ALARM[0], LOW);
      digitalWriteFast(pin.ALARM[1], HIGH);
    }
    else audio = 0; //Reset audio cycle timer
  }
}

void playAlarmTone(){
  static uint8_t led_index = 0;
  audio = 0;
  pulse = 0;
  for(int led=0; led<N_BOARDS; led++){
    if(conf.c.pushbutton_mode == 1 || conf.c.pushbutton_mode == 3 || (conf.c.pushbutton_mode == 2 && led == led_index)) pin.setButtonColor(led, 1);
    else pin.setButtonColor(led, N_LEDS);
  }
  if(++led_index >= N_BOARDS) led_index = 0;
  while(pulse < 512){ //Play tone for 0.5 seconds
    if(audio < conf.c.audio_volume[1]){
      digitalWriteFast(pin.ALARM[0], HIGH);
      digitalWriteFast(pin.ALARM[1], LOW);
    }
    else if(audio <= 256){
      digitalWriteFast(pin.ALARM[0], LOW);
      digitalWriteFast(pin.ALARM[1], HIGH);
      if(audio < 250) checkStatus(); //Check status if there is sufficient time
    }
    else audio = 0; //Reset audio cycle timer
  }
  for(int led=0; led<N_BOARDS; led++){
    if(conf.c.pushbutton_mode == 3 || (conf.c.pushbutton_mode == 2 && led == led_index)) pin.setButtonColor(led, 1);
    else pin.setButtonColor(led, N_LEDS);
  }
  if(++led_index >= N_BOARDS) led_index = 0;
  while(pulse < 1024){
    checkStatus();
  }
}

void setFan(uint8_t fan_index){
  float delta_temp;
  delta_temp =  conf.c.driver_fan[0] - conf.c.driver_fan[1];
  if(current_status.s.temp[fan_index] >= conf.c.driver_fan[0]){
    pinMode(pin.FAN_PWM[fan_index], OUTPUT);
    digitalWriteFast(pin.FAN_PWM[fan_index], LOW); //If temp is below min fan temp, turn fan off
    current_status.s.fan_speed[fan_index] = 0;
  }
  else if(current_status.s.temp[fan_index] <= conf.c.driver_fan[1]){
    pinMode(pin.FAN_PWM[fan_index], OUTPUT);
    digitalWriteFast(pin.FAN_PWM[fan_index], HIGH); //If temp is above max fan temp, run fan at full speed
    current_status.s.fan_speed[fan_index] = 65535;
  }
  else{
    current_status.s.fan_speed[fan_index] =  round((((float) conf.c.driver_fan[0] - (float) current_status.s.temp[fan_index])/delta_temp)*65535.0);
    pinMode(pin.FAN_PWM[fan_index], OUTPUT);
    analogWrite(pin.FAN_PWM[fan_index], current_status.s.fan_speed[fan_index]);
  }
}

void thermalFault(){
  uint8_t a = 0;
  const uint16_t DISCONNECT_TEMP = 63000;
  uint8_t disconnect_channel = N_BOARDS;
  if(!fault_active){ //If fault is not active, check if any temp is above the fault temperature
    for(a=0; a<N_BOARDS; a++){
      if(current_status.s.temp[a] <= conf.c.fault_temp){
        fault_active = true;
        break;   
      }
      else if(current_status.s.temp[a] >= DISCONNECT_TEMP){
        for(uint8_t b=0; b<N_LEDS; b++){
          if(conf.c.led_active[a][b]){
            fault_active = true;
            disconnect_channel = a;
            break;
          }
        }
        break;
      }  
    }
  }
  if(fault_active){
    memcpy(stored_status.byte_buffer, current_status.byte_buffer, sizeof(stored_status.byte_buffer)); //Save current status to restore state after fault.

    //Send fault warning to GUI
    if(current_status.s.temp[a] <= conf.c.fault_temp) temp_size = sprintf(temp_buffer, "-Warning: Fault temperature has been exceeded on LED board #%d.  The driver will turn off LED until below warning temperature.", a+1);
    else temp_size = sprintf(temp_buffer, "-Warning: No thermistor reading found on LED board #%d.  Please check board connection.", a+1);
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);

    //Update status
    ledOff();
    current_status.s.driver_control = true;
    while(fault_active){
      playAlarmTone();
      fault_active = false; //If all temps are below warn temp, clear the fault
      for(int a=0; a<N_BOARDS; a++) if(current_status.s.temp[a] <= conf.c.warn_temp) fault_active = true; //If any temp is above warn temp, maintain fault
      if(disconnect_channel < N_BOARDS && current_status.s.temp[disconnect_channel] >= DISCONNECT_TEMP) fault_active = true;      
    }
    
    //Restore driver to previous state
    for(uint8_t a=0; a<N_BOARDS; a++){
      current_status.s.led_pwm[a] = stored_status.s.led_pwm[a];
      current_status.s.led_current[a] = stored_status.s.led_current[a];
      current_status.s.led_channel[a] = stored_status.s.led_channel[a];
    }
    current_status.s.mode = stored_status.s.mode;
    current_status.s.driver_control = stored_status.s.driver_control;
    if(current_status.s.driver_control) manual_mode = current_status.s.mode; //Update manual mode if driver control
    
    updateIntensity();
    update_flag = true; //Toggle update flag
  }
}

void ledOff(){
  bool turned_off = false;
  for(uint8_t a=0; a<N_BOARDS; a++){
    if((current_status.s.led_current[a] || current_status.s.led_pwm[a]) && !turned_off){ //If LEDs are on
      //Turn off LED circuit completely
      dac.allOff();
      turned_off = true;
    }
    current_status.s.led_channel[a] = N_LEDS;
    current_status.s.led_current[a] = 0;
    current_status.s.led_pwm[a] = 0;
    pin.setButtonColor(a, N_LEDS);
  }
  manual_mode = 3;
  current_status.s.mode = manual_mode;
  memcpy(prev_status.byte_buffer, current_status.byte_buffer, sizeof(current_status.byte_buffer)); //Update prev status
}

void ledOff(uint8_t board_id){
  if(current_status.s.led_current[board_id] || current_status.s.led_pwm[board_id]){ //If LEDs are on
    //Turn off LED circuit completely
    dac.singleOff(board_id);
  }
  current_status.s.led_channel[board_id] = N_LEDS;
  current_status.s.led_current[board_id] = 0;
  current_status.s.led_pwm[board_id] = 0;
  pin.setButtonColor(board_id, N_LEDS);
  memcpy(prev_status.byte_buffer, current_status.byte_buffer, sizeof(current_status.byte_buffer)); //Update prev status
}
//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM//////////////EEPROM

//Check EEPROM to see if it has a saved configuration
void initializeConfigurations(){
  int a;
  uint8_t check_sum;
  uint16_t buffer_size = sizeof(MAGIC_RECEIVE);
  uint16_t EEPROM_address;

  //Set pin configurations
  pin.configurePins();
  
  //See if EEPROM has magic number
  for(a=0; a<buffer_size; a++){
    if(EEPROM.read(a) != MAGIC_RECEIVE[a]){
      break;
    }
  }
  //Verify EEPROM checksums
  if(a==buffer_size){
    auto verifyChecksum = [&] (){
      check_sum = 0;
      while(buffer_size--){
        check_sum += EEPROM.read(EEPROM_address--);
      }
    };   
    //Verify EEPROM check sums
    EEPROM_address = sizeof(MAGIC_RECEIVE) + sizeof(conf.byte_buffer) + sizeof(sync.byte_buffer) - 1;
    buffer_size = sizeof(sync.byte_buffer);
    verifyChecksum();
    if(!check_sum){
      buffer_size = sizeof(conf.byte_buffer);
      verifyChecksum();
      if(!check_sum){
        loadEEPROMtoStructs();
      }
      else loadDefaultsToEEPROM();
    }
    else loadDefaultsToEEPROM();
  }
  else loadDefaultsToEEPROM();
  initializeSeq();
  active_channel = 255; //Reset active channel so that channel gets actively set on updateIntensity()
  updateIntensity();
  playStatusTone();
  update_flag = true; //Toggle update flag
}

//https://forum.arduino.cc/index.php?topic=42850.0
void loadDefaultsToEEPROM(){
  uint8_t *buffer_ptr;
  uint16_t buffer_size;
  uint16_t EEPROM_address = 0;
  char message[] = "-A valid driver configuration was not found on EEPROM, so default settings will be loaded.";
  message[0] = prefix.message;
  usb.send((const unsigned char*) message, sizeof(message));
  
  //Lambda functions in C++11 rock! https://stackoverflow.com/questions/4324763/can-we-have-functions-inside-functions-in-c
  auto loadEEPROM = [&] (){
    while(buffer_size--){
      EEPROM.update(EEPROM_address++, *buffer_ptr++);
    }
  };
  buffer_ptr = (uint8_t *)&MAGIC_RECEIVE;
  buffer_size = sizeof(MAGIC_RECEIVE);
  loadEEPROM();
  buffer_ptr = (uint8_t *)&defaultConfig;
  buffer_size = sizeof(conf.byte_buffer);
  loadEEPROM();
  buffer_ptr = (uint8_t *)&defaultSync;
  buffer_size = sizeof(sync.byte_buffer);
  loadEEPROM();
  loadEEPROMtoStructs();

  //Clear the SD card as well in case it has invalid sequence files
  sd.formatSdCard();
  sd.initializeSD(); //Rebuild files
}

void loadEEPROMtoStructs(){
    uint16_t EEPROM_address = sizeof(MAGIC_RECEIVE) + sizeof(conf.byte_buffer) + sizeof(sync.byte_buffer) - 1;
    uint16_t buffer_size = sizeof(sync.byte_buffer);
    
    EEPROM_address = sizeof(MAGIC_RECEIVE) + sizeof(conf.byte_buffer) + sizeof(sync.byte_buffer);
    buffer_size = sizeof(sync.byte_buffer);
    while(buffer_size) sync.byte_buffer[--buffer_size] = EEPROM.read(--EEPROM_address);
    buffer_size = sizeof(conf.byte_buffer);
    while(buffer_size) conf.byte_buffer[--buffer_size] = EEPROM.read(--EEPROM_address);
}

//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL//////////////SERIAL

static void onPacketReceived(const uint8_t* buffer, size_t size){
  // Route decoded packet based on prefix byte
  heartbeat = 0; //Reset heartbeat timer as a serial packet has been received
  uint8_t buffer_prefix = buffer[0];
  if(buffer_prefix){ //Turn off LED for safety before processing packet if packet isn't a heartbeat update
    ledOff();
  }
  if(buffer_prefix == prefix.message) serial_connection_active = true; //Start/continue sending status packets; 
  else if(buffer_prefix == prefix.connection) magicExchange(buffer, size);
  else if(buffer_prefix == prefix.send_config) usb.send((const unsigned char*) conf.byte_buffer, sizeof(conf.byte_buffer));
  else if(buffer_prefix == prefix.recv_config) recvConfig(buffer, size);
  else if(buffer_prefix == prefix.send_sync) usb.send((const unsigned char*) sync.byte_buffer, sizeof(sync.byte_buffer));
  else if(buffer_prefix == prefix.recv_sync) recvSync(buffer, size);
  else if(buffer_prefix == prefix.send_seq) sendSeq(buffer, size);
  else if(buffer_prefix == prefix.recv_seq) recvSeq(buffer, size, true); //If serial notification of upload, it will be a single file
  else if(buffer_prefix == prefix.send_id) sendDriverId();
  else if(buffer_prefix == prefix.recv_time) syncRtcTime(buffer, size);
  else if(buffer_prefix == prefix.recv_stream);
  else if(buffer_prefix == prefix.send_stream);
  else if(buffer_prefix == prefix.status_update) updateStatus(buffer, size);
  //else if(buffer_prefix == prefix.calibration) driverCalibration(buffer, size);
  else if(buffer_prefix == prefix.gui_disconnect) disconnectSerial();
  else if(buffer_prefix == prefix.measure_period) measurePeriod(buffer, size);
  //else if(buffer_prefix == prefix.test_current) testCurrent(buffer, size);
  else if(buffer_prefix == prefix.test_volume) testVolume(buffer, size);
  else if(buffer_prefix == prefix.long_off);
  else{
    temp_size = sprintf(temp_buffer, "-Error: USB packet had invalid prefix: %d", buffer_prefix);  
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
  }
}

static void magicExchange(const uint8_t* buffer, size_t size){
  uint32_t a;
  if(size == sizeof(MAGIC_RECEIVE)){
    for(a=0; a<size; a++){
      if(buffer[a+1] != MAGIC_RECEIVE[a]){
        break;
      }
    }
    if(a==size-1){
      MAGIC_SEND[0] = prefix.connection;
      usb.send((const unsigned char*) MAGIC_SEND, size);
    }
  }
}

static void sendDriverId(){
  char driver_id[sizeof(conf.c.driver_name)+1];
  for(uint32_t a=0; a<sizeof(conf.c.driver_name); a++){
    driver_id[a+1] = conf.c.driver_name[a];
  }
  driver_id[0] = prefix.send_id;
  usb.send((const unsigned char*) driver_id, sizeof(driver_id));
}

static void recvConfig(const uint8_t* buffer, size_t size){
  uint8_t checksum = 0;
  temp_size = 0;
  if(size == sizeof(conf.byte_buffer)){
    for(int a = 0; a<(int) size; a++) checksum += buffer[a];
    if(!checksum){
      memcpy(conf.byte_buffer, buffer, sizeof(conf.byte_buffer));
      conf.byte_buffer[0] = prefix.send_config; //Switch prefix to sending prefix
      conf.byte_buffer[size-1] += (prefix.recv_config - prefix.send_config); //Fix corresponding checksum
      for(int a = 0; a<(int) size; a++) EEPROM.update(a + sizeof(MAGIC_RECEIVE), conf.byte_buffer[a]); //Copy configuration to EEPROM
      initializeConfigurations(); //Re-run the setup routine to update driver state
      temp_size = sprintf(temp_buffer, "-Configuration file was successfully uploaded.\nAlso upload \"Sync\" settings to apply changes.");
    }
    else temp_size = sprintf(temp_buffer, "-Error: Check sum is non-zero: %d", checksum); 
  }
  else temp_size = sprintf(temp_buffer, "-Error: Config packet is wrong size. Expected %d, got %d.", sizeof(conf.byte_buffer), size);
    
  if(temp_size){
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
  }
}

static void recvSync(const uint8_t* buffer, size_t size){
  uint8_t checksum = 0;
  temp_size = 0;
  uint16_t expected_size = sizeof(sync.byte_buffer);// - sizeof(sync.s.digital_sequence) - sizeof(sync.s.confocal_sequence);
  if(size == expected_size){
    for(int a = 0; a<(int) size; a++) checksum += buffer[a];
    if(!checksum){
      memcpy(sync.byte_buffer, buffer, sizeof(sync.byte_buffer));
      sync.byte_buffer[0] = prefix.send_sync; //Switch prefix to sending prefix
      sync.byte_buffer[size-1] += (prefix.recv_sync - prefix.send_sync); //Fix corresponding checksum
      for(int a = 0; a<(int) size; a++) EEPROM.update(a + sizeof(MAGIC_RECEIVE) + sizeof(conf.byte_buffer), sync.byte_buffer[a]); //Copy sync to EEPROM
      if(recvSeq(buffer, size, false)){
        initializeConfigurations(); //Re-run the setup routine to update driver state
        temp_size = sprintf(temp_buffer, "-Sync and sequence files were successfully uploaded.");
      }
      else{
        initializeConfigurations(); //Re-run the setup routine to update driver state
        temp_size = sprintf(temp_buffer, "-Only sync file was successfully uploaded.");
      }
    }
    else temp_size = sprintf(temp_buffer, "-Error: Check sum is non-zero: %d", checksum); 
  }
  else temp_size = sprintf(temp_buffer, "-Error: Sync packet is wrong size. Expected %d, got %d.", sizeof(sync.byte_buffer), size);  
  if(temp_size){
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
  }
}

static void syncRtcTime(const uint8_t* buffer, size_t size) {
  const unsigned long DEFAULT_TIME = 1609459200; // Jan 1 2021
  memcpy(uint32Union.bytes, buffer+1, sizeof(uint32Union.bytes));
  if(uint32Union.bytes_var >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
    setTime(uint32Union.bytes_var); // Sync Arduino clock to the time received on the serial port
  }
  else{
    temp_size = sprintf(temp_buffer, "-Warning: Epoch time %lu sync is invalid. Defaulting to January, 1 2021.", uint32Union.bytes_var);  
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
  }
}

static void sendSeq(const uint8_t* buffer, size_t size){
  uint8_t file_id; //Index of sequence file requested

  if(size == 2){ //Load file to stream buffer if command requesting file is sent from GUI
    if(buffer[1] <= 0 && buffer[1] >= sd.N_SEQ_FILES){
      temp_size = sprintf(temp_buffer, "-Error: Stream #%d is not a valid file identifier byte.", buffer[1]);  
      goto sendMessage;
    }
    else{
      file_id = buffer[1];
      if(!sd.readFromSD((char*) sequence_buffer[0]+2, 0, 0, sd.seq_files[file_id])){ //Offset 
        temp_size = sprintf(temp_buffer, "%s", sd.message_buffer);  
        goto sendMessage;
      }
      else{   
        //Initialize sequence file stream to GUI with callback prefix byte - tells GUI where to route the packet to once stream is complete
        temp_buffer[0] = prefix.send_seq;
        uint32Union.bytes_var = sd.file_size+2; //+2 byte for the callback routing byte and file ID prefix byte at the start of the packet
        memcpy(temp_buffer+1, uint32Union.bytes, sizeof(uint32Union.bytes));
        usb.send((const unsigned char*) temp_buffer, sizeof(prefix.send_seq) + sizeof(uint32Union.bytes));
      }
    }
    Serial.setTimeout(DEFUALT_TIMEOUT); //Set timeout for waiting for packet blocks
    temp_size = Serial.readBytes(temp_buffer, 3); //Wait for reply from GUI indicating ready for stream to be sent
    if(temp_size < 3){
      temp_size = sprintf(temp_buffer, "-Error: Driver timed out waiting for GUI to reply ready for stream, %d bytes received of 3.", temp_size);  
      goto sendMessage;
    }
    else if(temp_buffer[0] == 2 && temp_buffer[1] == prefix.send_seq && temp_buffer[2] == 0){ //If valid "ready for stream" requenst is received then send data stream
      sequence_buffer[0][0] = prefix.send_seq; //Send sequence prefix for callback routing of streamed packet
      sequence_buffer[0][1] = file_id; //Send ID of sequence file being streamed
      Serial.write(sequence_buffer[0], uint32Union.bytes_var); //Stream sequence file 
      if(file_id == 3) initializeSeq(); //Map active sequence data back onto sequence buffers.
    }
    else{
      temp_size = sprintf(temp_buffer, "-Error: Invalid \"ready for stream\" packet received from GUI. Expected [2, %d, 0] and got [%d, %d, %d].", prefix.send_stream, temp_buffer[0], temp_buffer[1], temp_buffer[2]);  
      goto sendMessage;
    }
  }
  else{
    temp_size = sprintf(temp_buffer, "-Error: Expected sendSeq request of 2 bytes and got %d bytes", size);  
    goto sendMessage;
  }
  return;
  sendMessage:
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
    initializeSeq(); //Map active sequence data back onto sequence buffers.
    return;
}

static bool recvSeq(const uint8_t* buffer, size_t size, bool single_file){
  uint32_t recv_packet_size = 0;
  Serial.setTimeout(DEFUALT_TIMEOUT); //Set timeout for waiting for packet blocks
  if(single_file){
    if(size == 2){ //Overrwite index counter "a" with requested file index if there is one
      if(buffer[1] <= 0 && buffer[1] >= sd.N_SEQ_FILES){
        temp_size = sprintf(temp_buffer, "-Error: Stream #%d is not a valid file identifier byte.", buffer[1]);  
        goto sendMessage;
      }
    }
    else{
      temp_size = sprintf(temp_buffer, "-Error: Invalid request size for single stream packet: %d bytes, instead of %d.", size, sizeof(seq_header.byte_buffer));  
      goto sendMessage;
    }
  }
  while(Serial.available()) Serial.read(); //Clear serial buffer
  for(int a = 0; a<sd.N_SEQ_FILES; a++){
    if(single_file) a=buffer[1]; //If a file was specified only recv that file
    temp_buffer[0] = prefix.recv_seq;
    temp_buffer[1] = a;
    usb.send((const unsigned char*) temp_buffer, 2); //send request for sequence file
    Serial.setTimeout(DEFUALT_TIMEOUT);
    recv_packet_size = Serial.readBytes((char*) seq_header.byte_buffer, sizeof(seq_header.byte_buffer));
    if(recv_packet_size < sizeof(seq_header.byte_buffer)){ //Timed out while waiting for header packet
      temp_size = sprintf(temp_buffer, "-Error: Timed out while waiting for sequence file stream #%d header packet. Only %lu bytes received of %d", a+1, recv_packet_size, sizeof(seq_header.byte_buffer));  
      goto sendMessage;
    }
    if(seq_header.s.prefix == prefix.recv_seq){ //Verify routing prefix
      if(seq_header.s.file_index == a){ //Correct sequence file is streaming   
        if(seq_header.s.buffer_size % sizeof(sequenceStruct)){ //Invalid stream length (not an integer multiple of 9 bytes)
          temp_size = sprintf(temp_buffer, "-Error: Invalid stream length, %lu is not an integer multiple of %d.", seq_header.s.buffer_size, sizeof(sequenceStruct));  
          goto sendMessage;
        }
        else if(seq_header.s.buffer_size > sizeof(sequence_buffer[0])){ //Invalid stream length - stream is longer than buffer
          temp_size = sprintf(temp_buffer, "-Error: Invalid stream length, %lu is larger than than the buffer size: %d.", seq_header.s.buffer_size, sizeof(sequence_buffer[0]));  
          goto sendMessage;
        }
        recv_packet_size = 0;
        if(seq_header.s.buffer_size > 0){ //Only request stream if there is a stream to recv
          Serial.setTimeout(int(seq_header.s.buffer_size >> 3)+DEFUALT_TIMEOUT);
          temp_buffer[0] = prefix.recv_stream;
          temp_buffer[1] = a;
          usb.send((const unsigned char*) temp_buffer, 2); //send request for sequence file 
          recv_packet_size = Serial.readBytes((char*) sequence_buffer[0], seq_header.s.buffer_size);
        }
        
        if(recv_packet_size == seq_header.s.buffer_size){ //If full packet was received, send it to the SD card
          if(!sd.saveToSD((char*) sequence_buffer[0], 0, seq_header.s.buffer_size, sd.seq_files[a])){
            temp_size = sprintf(temp_buffer, "%s", sd.message_buffer);  
            goto sendMessage;
          }
          if(single_file) return true; //If a specific file was requested then exit the loop on completion
        }
        else{ //Packet stream timed out
          temp_size = sprintf(temp_buffer, "-Error: Invalid for sequence file size for stream #%d.  Expected %lu bytes, received %lu", a+1, seq_header.s.buffer_size, recv_packet_size);  
          goto sendMessage;
        }
      }
      else{ //Wrong sequence file is being streamed
        temp_size = sprintf(temp_buffer, "-Error: Received sequence file #%d, while waiting for file #%d.", seq_header.s.file_index+1, a+1);  
        goto sendMessage;
      }
    }
    else{ //Invalid prefix
      temp_size = sprintf(temp_buffer, "-Error: \"%d\" is not a valid sequence packet prefix, looking for \"%d\".", seq_header.s.prefix, prefix.recv_seq);
      goto sendMessage;  
    }
  }
  return true;
  sendMessage:
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
    return false;
}

static void updateStatus(const uint8_t* buffer, size_t size){
  STATUSUNION recv_status;
  uint8_t a;
  if(size == sizeof(recv_status.byte_buffer)+1){
    memcpy(recv_status.byte_buffer, buffer+1, sizeof(recv_status.byte_buffer));
    for(a=0; a<N_BOARDS; a++) current_status.s.led_channel[a] = recv_status.s.led_channel[a];
    current_status.s.driver_control = recv_status.s.driver_control;
    if(current_status.s.mode) manual_mode = recv_status.s.mode; //Set manual mode to recv'd mode if not in sync
    if(!current_status.s.driver_control){
      current_status.s.mode = recv_status.s.mode;
      for(a=0; a<N_BOARDS; a++) current_status.s.led_pwm[a] = recv_status.s.led_pwm[a];
      for(a=0; a<N_BOARDS; a++) current_status.s.led_current[a] = recv_status.s.led_current[a];
    }
    else{
      if(!manual_mode){
        manual_mode = 1;
      }
    }
    memcpy(stored_status.byte_buffer, current_status.byte_buffer, sizeof(stored_status.byte_buffer)); //Update the stored status
    update_flag = true;
    updateIntensity();
  }
  else{
    temp_size = sprintf(temp_buffer, "-Error: LED  driver received an invalid status packet.  Expected %d bytes and received %d bytes.", sizeof(recv_status.byte_buffer)+1, size);
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
  }
}

void disconnectSerial(){
  serial_connection_active = false; //Stop sending status packets
  ledOff(); //Set LED off on disconnect
  manual_mode = 3;
  update_flag = true;
}

void measurePeriod(const uint8_t* buffer, size_t size){
  SYNCUNION temp_sync;
  float delta_cycles; //Number of cycles between triggers
  float sum_of_squares = 0; //Used to calcualte variance - https://www.thoughtco.com/sum-of-squares-formula-shortcut-3126266
  float sum_cycles = 0; //Used to calcualte variance - https://www.thoughtco.com/sum-of-squares-formula-shortcut-3126266
  uint32_t prev_cycles = 0; //Number of cycles at previous trigger
  float mean;
  float stdev;
  int a; //loop counter
  elapsedMillis timeout;
  elapsedMillis measure_duration;
  elapsedMicros debounce;
  float n_measurements=0;
  uint16_t record_timeout = 1000; //Time in ms to wait between line triggers during scan
  uint16_t measure_timeout = 3000; //Time in ms to measure mirror period

  //Lambda functions in C++11 rock! https://stackoverflow.com/questions/4324763/can-we-have-functions-inside-functions-in-c
  auto saveCounts = [&] (){
    checkStatus();
    if(a > 0){
      delta_cycles = cpu_cycles - prev_cycles; //Calcualte number of elapsed cycles
      sum_of_squares += delta_cycles * delta_cycles; //add to sum of squares
      sum_cycles += delta_cycles; //Used for stdev and mean
      n_measurements += 1;
    }
    prev_cycles = cpu_cycles;
    noInterrupts();
  };
  if(size == sizeof(temp_sync.byte_buffer)){
    memcpy(temp_sync.byte_buffer, buffer, sizeof(temp_sync.byte_buffer)); //Temporarily store copy of sync
    pinMode(pin.INPUTS[temp_sync.s.confocal_channel], INPUT); 
    temp_size = sprintf(temp_buffer, "-Measuring mirror period, please wait....");
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
    timeout = 0;
    analogRead(pin.INPUTS[temp_sync.s.confocal_channel]); //Clear ADC before reocording
    measure_duration = 0;
    for(a=-1; measure_duration < measure_timeout && n_measurements < 10000; a++){ //Measure period for 1 second
      if(temp_sync.s.confocal_sync_mode){ //If analog sync
        while(analogRead(pin.INPUTS[temp_sync.s.confocal_channel]) < temp_sync.s.confocal_threshold && timeout < record_timeout); //Wait for input to rise above threshold
        cpu_cycles = ARM_DWT_CYCCNT;
        if(a >= 0 && temp_sync.s.confocal_sync_polarity[1]) saveCounts(); //If rising trigger then save time point
        while(analogRead(pin.INPUTS[temp_sync.s.confocal_channel]) > temp_sync.s.confocal_threshold && timeout < record_timeout); //Wait for input to rise above threshold
        cpu_cycles = ARM_DWT_CYCCNT;
        if(a >= 0 && !temp_sync.s.confocal_sync_polarity[1]) saveCounts(); //If falling trigger then save time point 
      }
      else{ //If digital sync
        while(digitalReadFast(pin.INPUTS[temp_sync.s.confocal_channel]) !=  temp_sync.s.confocal_sync_polarity[0] && timeout < record_timeout); //Wait for trigger to match desired polarity
        cpu_cycles = ARM_DWT_CYCCNT;
        saveCounts();
        debounce = 0;
        while(debounce < 10) checkStatus();
        while(digitalReadFast(pin.INPUTS[temp_sync.s.confocal_channel]) ==  temp_sync.s.confocal_sync_polarity[0] && timeout < record_timeout); //Wait for trigger to reset
        debounce = 0;
        while(debounce < 10) checkStatus();
      }
      if(timeout >= record_timeout){ //If timed out, send error message.
        temp_size = sprintf(temp_buffer, "-Error: Measurement timed out waiting for line sync trigger.");    
        temp_buffer[0] = prefix.message;
        usb.send((const unsigned char*) temp_buffer, temp_size);
        return;
      }
      else{
        timeout = 0; //reset timeout timer
      }
    }
    mean = (sum_cycles/n_measurements)/600.0;
    stdev = (sum_cycles*sum_cycles);
    stdev /= n_measurements;
    stdev = abs(sum_of_squares-stdev);
    stdev /= n_measurements;
    stdev = sqrt(stdev);
    stdev /= 600.0;
    temp_size = sprintf(temp_buffer, "-Measurement Successful. Mirror period mean: %.2f µs, standard deviation: %.2f µs.", mean, stdev);
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
    memcpy(temp_buffer+1, &mean, sizeof(mean));
    temp_buffer[0] = prefix.measure_period;
    usb.send((const unsigned char*) temp_buffer, sizeof(mean)+1);
    updateIntensity(); //Restore led state  
  }
  else{
    temp_size = sprintf(temp_buffer, "-Error: LED  driver received an invalid measure period packet.  Expected %d bytes and received %d bytes.", sizeof(temp_sync.byte_buffer), size);
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
    return;
  }
}

void testVolume(const uint8_t* buffer, size_t size){
  uint8_t stored_volume;
  uint8_t stored_mode;

  stored_volume = conf.c.audio_volume[(bool) buffer[1]]; //Temporarily update volume to test volume
  conf.c.audio_volume[(bool) buffer[1]] = buffer[2];
  stored_mode = conf.c.pushbutton_mode; //Temporarily update led mode to test mode
  conf.c.pushbutton_mode = buffer[3];
   
  if(buffer[1] == 0){
    playStatusTone();
  }
  else{
    fault_active=true;
    playAlarmTone();
    playAlarmTone();
    fault_active=false;

  }
  conf.c.audio_volume[(bool) buffer[1]] = stored_volume; //Restore volume and mode
  conf.c.pushbutton_mode = stored_mode;
}
