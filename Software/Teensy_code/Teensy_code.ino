#include "pinSetup.h"
#include "PacketSerial.h"
#include "SDcard.h"
#include <EEPROM.h>
#include <TimeLib.h> //Set RTC time and get time strings

//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT//////////////STRUCT

#pragma pack(1) //Remove alignment padding bytes in structs - https://forum.pjrc.com/threads/50536-problem-with-union-in-Teensy-3-5
struct configurationStruct{
  uint8_t prefix;
  char driver_name[16]; //Name of LED driver: "default name"
  char led_names[4][16]; //Name of each LED channel
  boolean led_active[4]; //Whether LED channel is in use: {false, false, false, false}
  uint16_t current_limit[4]; //Current limit for each channel on DAC values: {0,0,0,0}
  uint8_t led_channel[4]; //SSR channels used for each LED: {1,2,3,4}
  float resistor_values[4]; //Values of current sense resistors
  boolean resistor_active[4]; //Whether a specific resistor is used
  uint16_t warn_temp[3]; //Warn temp for transistor, resistor, and external respectively in ADC units: {0,0,0}
  uint16_t fault_temp[3]; //Fault temp for transistor, resistor, and external respectively in ADC units: {0,0,0}
  uint16_t driver_fan[2]; //Driver fan min and max temperatures in ADC units: {65535, 65535}
  uint16_t ext_fan[2]; //External fan min and max temperatures in ADC units: {65535, 65535}
  uint8_t fan_channel; //Ext output channel used to send fan PWM signal
  int ext_therm_resistance; //External thermistor nominal resistance at 25°C
  int ext_therm_beta; //Beta value of external thermistor
  uint8_t audio_volume[2]; //Status and alarm volumes for transducer: {10, 100}
  bool pushbutton_intensity; //LED intensity - on/off
  uint8_t pushbutton_mode; //LED illumination mode when alarm is active
  uint8_t checksum; //Checksum to confirm that configuration is valid
};

const struct defaultConfigurationStruct{
  uint8_t prefix = 2;
  char driver_name[16] = "Unnamed driver ";
  char led_names[4][16] = {"LED #1         ", "LED #2         ", "LED #3         ", "LED #4         "};
  boolean led_active[4] = {false, false, false, false}; //Whether LED channel is in use: {false, false, false, false}
  uint16_t current_limit[4] = {65535,65535,65535,65535}; //Current limit for each channel on DAC values: {0,0,0,0}
  uint8_t led_channel[4] = {0,1,2,3}; //SSR channels used for each LED: {1,2,3,4}
  float resistor_values[4] = {5, 10, 1000, 1000}; //Values of current sense resistors
  boolean resistor_active[4] = {true, true, false, false}; //Whether a specific resistor is used
  uint16_t warn_temp[3] = {14604, 14604, 14604}; //Warn at 60°C
  uint16_t fault_temp[3] = {8891, 8891, 8891}; //Fault at 80°C
  uint16_t driver_fan[2] = {29565, 14604}; //Fan on at 30°C, fan max at 60°C
  uint16_t ext_fan[2] = {29565, 14604}; //Fan on at 30°C, fan max at 60°C
  uint8_t fan_channel = 0; //Ext output channel used to send fan PWM signal
  int ext_therm_resistance = 4700; //External thermistor nominal resistance at 25°C
  int ext_therm_beta = 3545; //Beta value of external thermistor
  uint8_t audio_volume[2] = {10, 100}; //Status and alarm volumes for transducer: {10, 100}
  bool pushbutton_intensity = true; //LED intensity - on/off
  uint8_t pushbutton_mode = 0; //LED illumination mode when alarm is active
  uint8_t checksum = 112;
} defaultConfig;

struct syncStruct{ //158 bytes
  uint8_t prefix;
  uint8_t mode; //Type of sync - digital, analog, confocal, etc.
  uint8_t sync_output_channel; //Channel to output sync signal
  
  uint8_t digital_channel; //The input channel for the sync signal
  uint8_t digital_mode[2]; //The digital sync mode  in the LOW and HIGH trigger states respectively
  uint8_t digital_led[2]; //The active LED channel in the LOW and HIGH trigger states respectively
  uint16_t digital_pwm[2]; //The PWM value in the LOW and HIGH trigger states respectively
  uint16_t digital_current[2]; //The DAC value in the LOW and HIGH trigger states respectively
  uint32_t digital_duration[2]; //The maximum number of milliseconds to hold LED state

  uint8_t analog_channel; //The input channel for the sync signal
  uint8_t analog_led; //The active LED channel
  uint8_t analog_mode; //The analog sync mode
  uint16_t analog_pwm; //ADC averages per PWM update
  uint16_t analog_current; //ADC averages per DAC update
  
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
  uint8_t sync_output_channel = 0; //Channel to output sync signal
  
  uint8_t digital_channel = 0; //The input channel for the sync signal
  uint8_t digital_mode[2] = {0,0}; //The digital sync mode  in the LOW and HIGH trigger states respectively
  uint8_t digital_led[2] = {0,0}; //The active LED channel in the LOW and HIGH trigger states respectively
  uint16_t digital_pwm[2] = {0,0}; //The PWM value in the LOW and HIGH trigger states respectively
  uint16_t digital_current[2] = {0,0}; //The DAC value in the LOW and HIGH trigger states respectively
  uint32_t digital_duration[2] = {0,0}; //The maximum number of milliseconds to hold LED state
  
  uint8_t analog_channel = 0; //The input channel for the sync signal
  uint8_t analog_led = 0; //The active LED channel
  uint8_t analog_mode = 0; //The analog sync mode
  uint16_t analog_pwm = 1; //ADC averages per PWM update
  uint16_t analog_current = 1; //ADC averages per DAC update

  boolean shutter_polarity = true; //Shutter polarity when scan is active
  uint8_t confocal_channel = 0; //The input channel for the line sync signal
  boolean confocal_sync_mode = false; //Whether the line sync is digital (false) or analog (true)
  boolean confocal_sync_polarity[2] = {true, true}; //Sync polarity for digital and analog sync inputs
  uint16_t confocal_threshold = 2000; //Threshold for analog sync trigger
  boolean confocal_scan_mode = true; //Whether scan is unidirectional (false) or bidrectional (true)
  uint32_t confocal_mirror_period = 180000; //Time in µs for the scanning mirror to complete one cycle
  uint32_t confocal_delay[3] = {0,36000,0}; //Delay in clock cycles for each sync delay
  
  uint8_t confocal_mode[2] = {0,0}; //The digital sync mode  in the image and flyback states respectively
  uint8_t confocal_led[2] = {0,0}; //The active LED channel in the image and flyback states respectively
  uint16_t confocal_pwm[2] = {0,0}; //The PWM value (in clock cycles) in the image and flyback states respectively
  uint16_t confocal_current[2] = {0,0}; //The DAC value in the image and flyback states respectively
  uint32_t confocal_duration[2] = {0,0}; //The maximum number of milliseconds to hold LED state

  uint8_t checksum = 18; //Checksum to confirm that configuration is valid
} defaultSync;

struct sequenceHeaderStruct{
  uint8_t prefix;
  uint8_t file_index;
  uint32_t buffer_size;
};

struct sequenceStruct{
  uint8_t led_id; //LED channel
  uint16_t led_pwm; //LED PWM (analog out units)
  uint16_t led_current; //LED current (DAC units)
  uint32_t led_duration; //LED duration (microseconds)
};

struct statusStruct{
  uint8_t led_channel; //Active LED channel
  uint16_t led_pwm; //PWM value for internal and external fan respectively
  uint16_t led_current; //DAC value for active LED
  uint8_t mode; //0=Sync, 1=PWM, 2=Current, 3=Off
  boolean state; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  boolean driver_control; //True = driver controls itself, False = GUI controls driver
  uint16_t temp[3]; //ADC temp reading of mosfet, resistor, and external respectively
  uint16_t fan_speed[2]; //PWM value for internal and external fan respectively
};

struct defaultStatusStruct{
  uint8_t led_channel=0; //Active LED channel
  uint16_t led_pwm=0; //PWM value for internal and external fan respectively
  uint16_t led_current=0; //DAC value for active LED
  uint8_t mode=0; //0=Sync, 1=PWM, 2=Current, 3=Off
  boolean state=0; //0=Standby (confocal), LOW (digital), etc. 1 = Scanning (confocal), HIGH (digital), etc.
  boolean driver_control=0; //True = driver controls itself, False = GUI controls driver
  uint16_t temp[3] = {0,0,0}; //ADC temp reading of mosfet, resistor, and external respectively
  uint16_t fan_speed[2] = {0,0}; //PWM value for internal and external fan respectively
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
char MAGIC_SEND[] = "-kvlWfsBplgasrsh3un5K"; //Magic number reply from Teensy verifying it is an LED driver "-" is for providing byte prefix in serial message
const static char MAGIC_RECEIVE[] = "kc1oISEIZ60AYJqH4J1P"; //Magic number received from GUI to verify this is an LED driver
char temp_buffer[COBS_BUFFER_SIZE]; //Temporary buffer for preparing packets immediately before transmission
int temp_size; //Size of temporary packet to transmit
byte sequence_buffer[2][10001*sizeof(sequenceStruct)+10]; //Add buffer padding for prefix info on transmission
uint32_t send_stream_index = 0; //Current index position of stream that is being sent
uint32_t send_stream_size = 0; //Total size of file to be streamed
const static uint16_t DEFUALT_TIMEOUT = 500; //Default timeout for serial communication in ms
uint8_t status_index = 0; //Index counter for incrementally updating and transmitting status
elapsedMillis status_update_timer; //Status timer to track when to transmit the next update
const uint8_t status_update_interval = 5; //The minimum time (in ms) between serial updates - prevents over-streaming of serial data and constantly accelerating fan
const uint32_t status_step_time_duration =  9; //Minimum time needed (in µs) to complete one status check
const uint32_t status_step_clock_duration =  status_step_time_duration*180; //Minimum time needed (in clock cycles) to complete one status check
elapsedMillis heartbeat; //Heartbeat timer to confirm that GUI is still connected
const static uint32_t HEARTBEAT_TIMEOUT = 10000; //Driver will assume connection has closed if heartbeat not received within this time 
uint32_t cpu_cycles = 0; //Track the number of CPU cycles for sub-microseconds timing precision
const static uint32_t cycle_offset = 7; //Number of cycles needed to check cycles ellapsed
boolean serial_connection_active = false; //Whether to send updates over serial - used to block updates during critical communication
uint8_t manual_mode = 1; //Store value of manual mode when status goes to sync (mode = 0)
boolean fault_active = false; //Whether the led driver is currently in a fault state (such as over-heated).
STATUSUNION stored_status; //Temporarily store operating status when status is over-ridden, such as during a thermal fault
elapsedMicros audio; //Timer controlling audio volume and frequency
elapsedMillis pulse; //Timer controlling tone pule interval
boolean external_analog = false; //Whether to use the DAC or external analog input
uint16_t seq_steps[2]; //Number of setps in each active sync sequence
uint8_t active_channel; //Currently active LED channel
uint8_t update_flag = false; //Whether an update needs to be processed
uint32_t ext_avg = 65535; //Summing variable for performing rolling average on the external thermistor to denoise it
const uint16_t ext_avg_samples = 1024; //Size of sliding window for external average
//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS//////////////CLASS
pinSetup pin;
SDcard sd;
PacketSerial_<COBS, 0, COBS_BUFFER_SIZE> usb; //Sets Encoder, framing character, buffer size

void setup() {
//  EEPROM.update(0,0); //Uncomment to reset EEPROM to defaults - re-comment and the upload code again
//  sd.formatSdCard(); //Uncomment to format SD card - re-comment and the upload code again

  for(size_t a=0; a<sizeof(current_status.byte_buffer); a++) *(current_status.byte_buffer+a)=0; //Initialize status buffer to a known state of all 0
  
  //Count cpu cycles for submircrosecond delay precision - https://forum.pjrc.com/threads/28407-Teensyduino-access-to-counting-cpu-cycles?p=71036&viewfull=1#post71036
  ARM_DEMCR |= ARM_DEMCR_TRCENA;
  ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
  cpu_cycles = ARM_DWT_CYCCNT;
  sequence_buffer[0][0]= 0;
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600); //Needed for non-COBS streaming, such as large sequence files
  usb.begin(115200);
  usb.setPacketHandler(&onPacketReceived);
  if(!sd.initializeSD()){ //Initiazlize SD card first so the sequence files can be retrieved on initializeConfigurations()
    digitalWriteFast(LED_BUILTIN, HIGH);
    if(!sd.formatSdCard()){ ; //Try reformatting the card
      sd.message_buffer[0] = prefix.message;
      usb.send((const unsigned char*) sd.message_buffer, sd.message_size);
    }
    else{
      if(!sd.initializeSD()){ //Re-initialize sd card
        sd.message_buffer[0] = prefix.message;
        usb.send((const unsigned char*) sd.message_buffer, sd.message_size);
      }
      else{
        temp_size = sprintf(temp_buffer, "-Warning: SD card fomat was invalid.  Card was successfully reformatted to FAT16/32.  All files were cleared.");
        temp_buffer[0] = prefix.message;
        usb.send((const unsigned char*) temp_buffer, temp_size);
      }
    }
  }
  initializeConfigurations(); //Initialize configurations only after initializing SD card, as SD card is needed to load seqences
  status_index = 0;
  ledOff(); //For safety, boot to LED off
  for(size_t a=0; a==status_index; a++) checkStatus(); //Perform full round of status checks to get starting status of driver
  
  digitalWriteFast(pin.INTERLINE, LOW);
  digitalWriteFast(pin.ANALOG_SELECT, LOW);
  digitalWriteFast(pin.FAN_PWM, LOW);
  analogWrite(pin.DAC0, 0);
}
elapsedMillis t = 0;
uint32_t d = 10;

void loop() {
  interrupts();
  if(sync.s.sync_output_channel) digitalWriteFast(pin.OUTPUTS[sync.s.sync_output_channel-1], LOW); //Set sync output low when in manual mode
  update_flag = false; //Reset update flag on return on main loop
  if(current_status.s.mode) checkStatus();
  if(!current_status.s.mode) syncRouter();
  delayMicroseconds(10);
}

void debug(){
  temp_size = sprintf(temp_buffer, "LED#%d PWM:%d Current:%d Mode:%s State:%s Control:%s", current_status.s.led_channel+1, current_status.s.led_pwm, current_status.s.led_current, !!current_status.s.mode ? "Manual":"Sync", current_status.s.state ? "LOW":"HIGH", current_status.s.driver_control ? "GUI":"Manual");
  if(t>500){
    if(serial_connection_active){
      temp_buffer[0] = prefix.message;
      usb.send((const unsigned char*) temp_buffer, temp_size);
    }
    else{
      Serial.println(temp_buffer);
    }
    t=0;
  }
}

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
  memcpy(seq.byte_buffer, sequence_buffer[current_status.s.state]+sync_step*sizeof(seq.byte_buffer), sizeof(seq.byte_buffer)); //Get current sequence step 
  memcpy(current_status.byte_buffer, seq.byte_buffer, sizeof(seq.byte_buffer) - sizeof(seq.s.led_duration)); //Copy LED channel, PWM, and current to status
  if(current_status.s.led_channel >= 4) current_status.s.led_channel = active_channel;
}

void digitalSync(){ //5 µs jitter + 2 µs phase delay
  elapsedMicros duration;
  uint16_t sync_step;
  
  pinMode(pin.INPUTS[sync.s.digital_channel], INPUT); //Set sync input pin to input
  while(!current_status.s.mode && sync.s.mode == 0){
    sync_step = 0;
    current_status.s.state = digitalReadFast(pin.INPUTS[sync.s.digital_channel]); //Get state of sync input
    if(sync.s.sync_output_channel) digitalWriteFast(pin.OUTPUTS[sync.s.sync_output_channel-1], current_status.s.state); //Drive output sync signal
    active_channel = current_status.s.led_channel;
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
  int a; //Loop counter
  uint16_t n_avg; //Number of ADC samples to take per recording
  uint32_t adc_average; //Used to sum ADC readings
  boolean output_state; //Used to flip sync output for external syncing
  uint16_t sample_interval = 1800; //Interval at which to take ADC recording
  float sample_freq; //Frequency in Hz that samples are taken
  current_status.s.state = 0; //There is only one sync state - set to default
  noInterrupts();
  if(sync.s.analog_mode != 2){ //If using internal analog
    if(sync.s.analog_led) current_status.s.led_channel = sync.s.analog_led-1; //Change LED channel if specified
    if(sync.s.analog_mode){ //Mode 1 is current
      n_avg = 1<<sync.s.analog_current; 
      current_status.s.led_pwm = 65535; //Set PWM to max
    }
    else{ //Mode 0 is PWM
      n_avg = 1<<sync.s.analog_pwm; 
      current_status.s.led_current = conf.c.current_limit[current_status.s.led_channel]; //Set current to current limit
    }
    
    //Set the PWM frequency to match the sample frequency for maximum PWM precision - capped at a min of 915 HZ since this gives full 16-bit precision at 180 MHz - https://www.pjrc.com/teensy/td_pulse.html
    sample_freq = 1/((float) n_avg * (float) sample_interval * 5.555e-9); //Calculate the sample frequency
    if(sample_freq > 915.527) analogWriteFrequency(pin.INTERLINE, sample_freq);
    else analogWriteFrequency(pin.INTERLINE, 915.527); //Cap min at 
    
    pinMode(pin.INPUTS[sync.s.analog_channel], INPUT);
    cpu_cycles = ARM_DWT_CYCCNT;
    while(!current_status.s.mode && sync.s.mode == 1 && sync.s.analog_mode != 2 && !update_flag){
      adc_average = 0;
      if(sync.s.sync_output_channel){ //Flip output sync per recording cycle
        output_state = !output_state;
        digitalWriteFast(pin.OUTPUTS[sync.s.sync_output_channel-1], output_state);
      }
      for(a=0; a<n_avg-1; a++){
        analogRead(pin.INPUTS[sync.s.analog_channel]); //Clear the ADC
        adc_average += analogRead(pin.INPUTS[sync.s.analog_channel]); //Record ADC value
        checkStatus();
        if(update_flag) goto quit;
        while(ARM_DWT_CYCCNT-cpu_cycles < sample_interval); //Ensure ADC recordings are at fixed 10 µs interval
        cpu_cycles = cpu_cycles + sample_interval;
      }
      analogRead(pin.INPUTS[sync.s.analog_channel]); //Clear the ADC
      adc_average += analogRead(pin.INPUTS[sync.s.analog_channel]); //Record the last ADC value
      if(sync.s.analog_mode){ //If current sync
        current_status.s.led_current = adc_average >> sync.s.analog_current; //Mode 1 is current - take average value
        if(conf.c.current_limit[current_status.s.led_channel] < current_status.s.led_current) current_status.s.led_current = conf.c.current_limit[current_status.s.led_channel]; //Cap current to current limit
      }
      else{
        current_status.s.led_pwm = adc_average >> sync.s.analog_pwm; //Mode 1 is current - take average value
      }
      updateIntensity(); //Update intensity in place of status check to maintain fixed intervals
      while(ARM_DWT_CYCCNT-cpu_cycles < sample_interval); //Ensure ADC recordings are at fixed 10 µs interval
      cpu_cycles = cpu_cycles + sample_interval;
    }
  }
  else{ //If using external analog
    pinMode(pin.ANALOG_SELECT, OUTPUT);
    external_analog = true;
    pinMode(pin.INTERLINE, OUTPUT);
    digitalWriteFast(pin.ANALOG_SELECT, HIGH); //Set external analog input
    digitalWriteFast(pin.INTERLINE, HIGH); //Set LED on
    while(!current_status.s.mode && sync.s.mode == 1 && sync.s.analog_mode != 2 && !update_flag) checkStatus();
  }
  quit:
    pinMode(pin.ANALOG_SELECT, OUTPUT);
    digitalWriteFast(pin.ANALOG_SELECT, LOW); //Set internal analog input
    external_analog = false;
    interrupts();
    analogWriteFrequency(pin.INTERLINE, pin.LED_FREQ); //Restore the interline timer to its defaul value: https://www.pjrc.com/teensy/td_pulse.html
}

void confocalSync(){
  elapsedMicros duration; //Duration timer for sequence steps
  uint16_t sync_step; //sequence step counter
  uint32_t interline_timeout = 720000000; //Timeout to stop looking for mirror sync - 4 second.
  uint32_t pwm_clock_cycles; //The number of clock cycles equivalent to the PWM duration
  uint32_t unidirectional_status_window = sync.s.confocal_delay[0] + sync.s.confocal_delay[1] + sync.s.confocal_delay[2] + 2*status_step_clock_duration; //Number of clock cycles between end of interline sequence and next trigger
  float pwm_freq = 180000000/(float) sync.s.confocal_mirror_period; //Get the frequency of the mirror in Hz
  float pwm_ratio = (float) sync.s.confocal_delay[1] / (float) sync.s.confocal_mirror_period; //Ratio of LED on time to total mirror period
  boolean shutter_state; //Logical state of shutter input
  boolean sync_pol; //Track polarity of sync output
  uint8_t timeout = 0; //Flag for whether the line sync has timed out waiting for trigger - 0: no timeout, 1: new timeout - report error, 2: on going timeout - error already reported.  Flag resets when shutter closes.
  
  if(sync.s.confocal_scan_mode){ //If the scan is bidirectional
    pwm_freq *= 2; //Double the pwm freq since the LED flashes twice per period
    pwm_ratio *= 2; //Double the pwm ratio since the ratio is now delay[1] / (0.5 * mirror period)
  }
  analogWriteFrequency(pin.INTERLINE, pwm_freq); //Set interline PWM freq to match mirror freq, also sets analog_select PWM freq (on same timer): https://www.pjrc.com/teensy/td_pulse.html
  
  if(sync.s.confocal_mirror_period > unidirectional_status_window) unidirectional_status_window = sync.s.confocal_mirror_period - unidirectional_status_window;
  else unidirectional_status_window = 0;
  noInterrupts(); //Turn off interrupts for exact interline timing

  //Lamda trigger sync funtions ---------------------------------------------------------------------------------------------------------------
  auto waitForTrigger = [&] (){ //Wait for the trigger event
    cpu_cycles = ARM_DWT_CYCCNT; //Reset inerline timer
    if(sync.s.confocal_sync_mode){ //If analog sync
      analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
      if(sync.s.confocal_sync_polarity[1]){
        while(analogRead(pin.INPUTS[sync.s.confocal_channel]) < sync.s.confocal_threshold){ //Wait for input to rise above threshold - timeout after two mirror periods
          if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
            if(!timeout) timeout = 1; //Flag timeout
            break;
          }
        }
      }
      else{
        while(analogRead(pin.INPUTS[sync.s.confocal_channel]) > sync.s.confocal_threshold){
          if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){
            if(!timeout) timeout = 1; //Wait for trigger to match polarity
            break;
          }
        }
      }
    }
    else{ //If digital sync
      while(digitalReadFast(pin.INPUTS[sync.s.confocal_channel]) != sync.s.confocal_sync_polarity[0]){ //What for line sync trigger
        if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
          if(!timeout) timeout = 1; //Flag timeout
          break;
        }
      }
    }
    cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer
  };

  auto waitForTriggerReset = [&] (){ //Wait for the trigger event to reset - used to initially sync the LED driver to the trigger input
    cpu_cycles = ARM_DWT_CYCCNT; //Reset inerline timer
    if(sync.s.confocal_sync_mode){ //If analog sync
      analogRead(pin.INPUTS[sync.s.confocal_channel]); //Clear the ADC
      if(sync.s.confocal_sync_polarity[1]){
        while(analogRead(pin.INPUTS[sync.s.confocal_channel]) > sync.s.confocal_threshold){ //Wait for input to fall below threshold - timeout after two mirror periods
          if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
            if(!timeout) timeout = 1; //Flag timeout
            break;
          }
        }
      }
      else{
        while(analogRead(pin.INPUTS[sync.s.confocal_channel]) > sync.s.confocal_threshold){
          if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){
            if(!timeout) timeout = 1; //Wait for trigger to match polarity
            break;
          }
        }
      }
    }
    else{ //If digital sync
      while(digitalReadFast(pin.INPUTS[sync.s.confocal_channel]) == sync.s.confocal_sync_polarity[0]){ //What for line sync trigger to reset
        if(ARM_DWT_CYCCNT-cpu_cycles >= interline_timeout){ //Check if line sync has timed out
          if(!timeout) timeout = 1; //Flag timeout
          break;
        }
      }
    }
    cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer
  };
  //-----------------------------------------------------------------------------------------------------------------------------------------------------
  
  pinMode(pin.INPUTS[0], INPUT); //Set shutter input pin to input
  pinMode(pin.INPUTS[sync.s.confocal_channel], INPUT); //Set sync input pin to input
  pinMode(pin.INTERLINE, OUTPUT); //Disconnect the 
  while(!current_status.s.mode && sync.s.mode == 2){ //This loop is maintained as long as in confocal sync mode - checked each time the status state changes (imaging/standby)
    sync_step = 0;
    timeout = 0; //Reset the timrout flag when scan state changes.
    shutter_state = digitalReadFast(pin.INPUTS[0]); //Get state of shutter
    current_status.s.state = (shutter_state == sync.s.shutter_polarity);
    
    active_channel = current_status.s.led_channel;
    getSeqStep(sync_step); //Get first sequence step
    duration = 0; //Reset seq timer
    cpu_cycles = ARM_DWT_CYCCNT; //Reset interline timer

//    interline_timeout = 180000000; //Timeout to stop looking for mirror sync - wait one full second as there can be a delay between the shutter and the start of the mirror.
    waitForTrigger();  //Catch first trigger to resync timing - prevents starting stim later 
//    interline_timeout = 2*sync.s.confocal_mirror_period; //One the first trigger is caught, wait no longer than 2x one mirror period for the next trigger
    
    checkStatus(); //Check status at least once per mirror cycle
    if(update_flag) goto quit; //Exit on update
    
    while(shutter_state == digitalReadFast(pin.INPUTS[0]) && !update_flag){ //While shutter state doesn't change and driver still in digital sync mode - checked each time a seq step is complete
      if(sync_step < seq_steps[current_status.s.state]){ //If the end of the sequence list has not been reached
        if(sync.s.confocal_mode[current_status.s.state] == 3){ //If sync uses external analog , set external analog pin HIGH
          pinMode(pin.ANALOG_SELECT, OUTPUT);
          external_analog = true;
          digitalWriteFast(pin.ANALOG_SELECT, HIGH); //Set external analog input
        }
        else{ //For all other modes, set led intensity to new values
          pinMode(pin.ANALOG_SELECT, OUTPUT);
          external_analog = false;
          digitalWriteFast(pin.ANALOG_SELECT, LOW); //Set internal analog input
          updateIntensity(); 
        }
        
        if(current_status.s.state){ //If scanning, convert PWM to clock cycles
          pwm_clock_cycles = round(((float) current_status.s.led_pwm * (float) sync.s.confocal_delay[1])/65535); //Calculate the number of clock cycles to leave the LED on during delay #2 to match the needed % PWM
          pinMode(pin.INTERLINE, OUTPUT); //Disconnect interline pin from PWM bus
        }
        else{ //If standby, convert PWM to proportion of total mirror period
          pwm_clock_cycles = round((float) current_status.s.led_pwm * pwm_ratio);
          analogWrite(pin.INTERLINE, (uint16_t) pwm_clock_cycles);
        }
        while(shutter_state == digitalReadFast(pin.INPUTS[0]) && !update_flag && (duration < seq.s.led_duration || seq.s.led_duration == 0)){ //Loop until shutter changes, update, or seq duration times out (0 = hold - no timeout) - Interline loop
          checkStatus(); //Check status at least once per mirror cycle
          if(update_flag) goto quit;
          if(current_status.s.state){ //If shutter is open (actively scanning) perform interline modulation
            waitForTriggerReset(); //Wait for trigger to reset - this insures the driver will always only sync to the start of a trigger, and not mid trigger
            if(sync.s.sync_output_channel) digitalWriteFast(pin.OUTPUTS[sync.s.sync_output_channel-1], sync_pol); //Toggle Sync
            sync_pol = !sync_pol; //Flip sync_pol polarity
            waitForTrigger();
            if(timeout){
              if(timeout == 1){
                temp_size = sprintf(temp_buffer, "-Error: Confocal Sync timed out waiting for line trigger. Check connection and re-measure mirror period.");
                temp_buffer[0] = prefix.message;
                usb.send((const unsigned char*) temp_buffer, temp_size);
                timeout = 2;
              }
            }
            else{
              while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[0]); //Wait for delay #1
              if(current_status.s.led_current) digitalWriteFast(pin.INTERLINE, HIGH);
              cpu_cycles += sync.s.confocal_delay[0]; //Increment interline timer
              while(ARM_DWT_CYCCNT - cpu_cycles < pwm_clock_cycles); //Wait for PWM delay
              digitalWriteFast(pin.INTERLINE, LOW);
              while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[1]); //Wait for end of delay #2
              cpu_cycles += sync.s.confocal_delay[1]; //Increment interline timer
              if(sync.s.confocal_delay[2] > status_step_clock_duration){ //See if there is enough time to check status during delay #3
                while(sync.s.confocal_delay[2] - (ARM_DWT_CYCCNT - cpu_cycles) > status_step_clock_duration){ //If there is enough time, perform status checks during delay #3
                  checkStatus(); //Check status while there is time to do so during the mirror sweep to the interline pulse
                  if(update_flag) goto quit;
                }
              }
              while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[2]); //Wait for end of delay #3
              if(sync.s.confocal_scan_mode){ //If scan is bidirectional, perform flyback interline
                if(current_status.s.led_current) digitalWriteFast(pin.INTERLINE, HIGH);
                cpu_cycles += sync.s.confocal_delay[2]; //Increment interline timer
                while(ARM_DWT_CYCCNT - cpu_cycles < pwm_clock_cycles); //Wait for PWM delay
                digitalWriteFast(pin.INTERLINE, LOW);
                while(ARM_DWT_CYCCNT - cpu_cycles < sync.s.confocal_delay[1]); //Wait for end of delay #2
              }
              else{ //If unidirectional, perform status checks if there is enough time before the next trigger
                if(unidirectional_status_window > status_step_clock_duration){ //See if there is enough time to check status during delay #3
                  while((unidirectional_status_window - (ARM_DWT_CYCCNT - cpu_cycles)) > status_step_clock_duration){ //If there is enough time, perform status checks during delay #3
                    checkStatus(); //Check status while there is time to do so during the mirror sweep to the interline pulse
                    if(update_flag) goto quit;
                  }
                }
              }
            }
          }
        }
        duration -= seq.s.led_duration; //Reset duration timer 
        sync_step++; //Increment the sync step counter
        getSeqStep(sync_step); //Get next sequence step
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
    analogWriteFrequency(pin.INTERLINE, pin.LED_FREQ); //Restore the interline timer to its defaul value: https://www.pjrc.com/teensy/td_pulse.html
    interrupts();
    pinMode(pin.ANALOG_SELECT, OUTPUT);
    digitalWriteFast(pin.ANALOG_SELECT, LOW);
    external_analog = false;
}

void customSync(){ //Two channel interline sequence, with external trigger between steps
  elapsedMicros duration;
  uint16_t sync_step;
  boolean stim_start = false; 
  const uint32_t stim_delay = 10e6; //How long to wait from start of movie to start of stim (in µs)
  const uint32_t stim_duration = 1e6; //Duration of stim (in µs)
  const uint32_t total_duration = 20e6; //Total duration of movie (in µs)
  const uint32_t reset_time = total_duration + 5e6; //Time from start to sync reset - needs to be > maximum possible duration of movie (in µs)
  elapsedMicros stim_timer; //Needs to be outside loop to retain value
    
  pinMode(pin.INPUTS[sync.s.digital_channel], INPUT); //Set sync input pin to input
  sync.s.mode = 0; //Get digital sequence
  initializeSeq();
  sync.s.mode = 4;
  while(!current_status.s.mode && sync.s.mode == 4){ //While in custom sync
    sync_step = 0;
    current_status.s.state = digitalReadFast(pin.INPUTS[sync.s.digital_channel]); //Get state of sync input
    if(sync.s.sync_output_channel) digitalWriteFast(pin.OUTPUTS[sync.s.sync_output_channel-1], current_status.s.state); //Drive output sync signal
    active_channel = current_status.s.led_channel;
    getSeqStep(sync_step); //Get first sequence step
    duration = 0;
    if(current_status.s.state && !stim_start){ //Restart stim timer if stim has not yet been started and trigger goes high.
      stim_timer = 0;
      stim_start = true;
    }         
    while(current_status.s.state == digitalReadFast(pin.INPUTS[sync.s.digital_channel]) && !update_flag && stim_start){ //While trigger state doesn't change and driver still in digital sync mode 
      if(current_status.s.state || (stim_timer >= stim_delay && stim_timer < stim_delay+stim_duration)){ //If trigger high
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
            while(current_status.s.state == digitalReadFast(pin.INPUTS[sync.s.digital_channel]) && !current_status.s.mode && sync.s.mode == 4){
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
          pinMode(pin.INPUTS[sync.s.digital_channel], INPUT_DISABLE);
          return;
        }
      }
      else{
        digitalWriteFast(pin.INTERLINE, LOW);
        checkStatus();
        if(update_flag) return; //Exit on update
      }    
      if(stim_timer > reset_time){
        stim_start = false;
        playStatusTone();
      }
    }
    digitalWriteFast(pin.INTERLINE, LOW);
    checkStatus();
    if(update_flag) return; //Exit on update
  }
  pinMode(pin.INPUTS[sync.s.digital_channel], INPUT_DISABLE); 
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
        seq.s.led_id = 255; //Don't change LED channel
        seq.s.led_pwm = 0; //Turn off led PWM
        seq.s.led_current = 0; //Turn off led current
        seq.s.led_duration = 0; //Hold at off
        memcpy(sequence_buffer[a], seq.byte_buffer, sizeof(seq.byte_buffer));
        seq_steps[a] = 1;
      }
      else if((sync.s.mode == 0 && sync.s.digital_mode[a] == 1) || (sync.s.mode == 2 && sync.s.confocal_mode[a] == 1)){ //If single event
        memcpy(&seq.s.led_id, sync_pointer+a, sizeof(seq.s.led_id));
        seq.s.led_id -= 1; //Shift from id# to list index
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
          for(int b=0; b<seq_steps[a]; b++) --*(sequence_buffer[a]+b*sizeof(seq.byte_buffer));  //Decrement LED IDs 
        }
      }
      else if(sync.s.mode == 2 && sync.s.confocal_mode[a] == 3){ //If confocal sync with external analog
        seq.s.led_id = 255; //Don't change LED channel
        seq.s.led_pwm = 65535; //Set PWM to max
        seq.s.led_current = 0; //Turn off led current
        seq.s.led_duration = 0; //Hold at off
        memcpy(sequence_buffer[a], seq.byte_buffer, sizeof(seq.byte_buffer));
        seq_steps[a] = 1;
      }
      memcpy(seq.byte_buffer, sequence_buffer[a]+(seq_steps[a]-1)*sizeof(seq.byte_buffer), sizeof(seq.byte_buffer)); //Get the last sequence step
      if(seq.s.led_duration){ //If the last step is not a hold (duration > 0) then add a hold to the end of the sequence
        seq.s.led_id = 255; //Don't change LED channel
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
  switch(status_index){
    case 0: //Check driver temperatures - 3.68 µs
      status_index++;
      analogRead(pin.MOSFET_TEMP);
      current_status.s.temp[0] = analogRead(pin.MOSFET_TEMP);
      break;
    case 1:
      status_index++;
      analogRead(pin.RESISTOR_TEMP);
      current_status.s.temp[1] = analogRead(pin.RESISTOR_TEMP);
      break;
    case 2: //Check external thermistor with rolling average - 3.1 µs to 4.5 µs max
      status_index++;
      analogRead(pin.EXTERNAL_TEMP);
      current_status.s.temp[2] = analogRead(pin.EXTERNAL_TEMP);
      break;
    case 3:
      status_index++;
      ext_avg = (ext_avg*(ext_avg_samples-1) + current_status.s.temp[2])/ext_avg_samples;
      current_status.s.temp[2] = ext_avg;
      break;
    case 4: //Check if any of the temperatures is past the fault temperature - 0.59 µs
      status_index++;
      if(!fault_active) thermalFault();
      break;
    case 5: //Update internal fan speed - 0.85 µs
      status_index++;
      if(current_status.s.temp[0] < current_status.s.temp[1]) setFan(current_status.s.temp[0], 0); //Update fan based on highest internal temperature (lowest ADC value)
      else setFan(current_status.s.temp[1], 0);
      break;
    case 6: //Update external fan speed - 0.85 µs
      status_index++;
      setFan(current_status.s.temp[2], 1); //Update external fan
      break;
    case 7: //Check toggle switch - 0.28 µs
      status_index++;
      if(current_status.s.driver_control && !fault_active){ //Only check toggle if driver control
        if(digitalReadFast(pin.TOGGLE) == !current_status.s.mode){ //Check if toggle state has changed - xor comparison by boolean inference (!) of mode
          analogWrite(pin.DAC0, 0);
          pinMode(pin.INTERLINE, OUTPUT);
          digitalWriteFast(pin.INTERLINE, LOW); //Turn of LED while driver transitions between sync and manual modes
          delay(pin.DEBOUNCE);
          if(digitalReadFast(pin.TOGGLE)) current_status.s.mode = manual_mode;
          else current_status.s.mode = 0;
          update_flag = true; //Toggle update flag
        }
      }
      break;
    case 8: //Check pot value - 3.74 µs
      status_index++;
      if(current_status.s.driver_control && !fault_active && current_status.s.mode){ //Only check pot if driver control and in manual mode
        if(current_status.s.mode == 1){
          analogRead(pin.POT);
          current_status.s.led_pwm = 65535-analogRead(pin.POT);
          current_status.s.led_current = conf.c.current_limit[current_status.s.led_channel]; //Set current to LED current limit
        }
        else if(current_status.s.mode == 3){
          ledOff();
        }
        else return; //If sync - do no update intensity
        updateIntensity(); //Update the LED intensity with the new values
      }
      break;
    case 9: //Check pushbuttons and update LEDs - 1.05 µs
      status_index++;
      if(!fault_active){
        if(current_status.s.mode && current_status.s.driver_control){ //If in manual mode and driver control, check for button presses
          for(int a=0; a<4; a++){
            if(digitalReadFast(pin.PUSHBUTTON[a])){
              delay(pin.DEBOUNCE);
              if(conf.c.led_active[a]){ //confirm that channel can be selected 
                if(current_status.s.led_channel == a && manual_mode == 1){
                  ledOff();
                  manual_mode = 3;
                }
                else{
                  current_status.s.led_channel = a; //Update channel status
                  manual_mode = 1; 
                }
                current_status.s.mode = manual_mode; //Update mode
              }
              while(digitalReadFast(pin.PUSHBUTTON[a])) delay(10); //Wait for button release
              delay(pin.DEBOUNCE);
              updateIntensity(); //Update the LED intensity with the new values
              break; //Only process one pushbutton if several are pressed        
            }
          }
        }
        for(int a=0; a<4; a++){ //Update indicator LEDs
          if(current_status.s.led_channel == a && current_status.s.mode < 3 && conf.c.pushbutton_intensity) digitalWriteFast(pin.LED[a], HIGH);
          else digitalWriteFast(pin.LED[a], LOW);
        }
      }
      break;
    case 10: //Send current status to led driver - 4.76 µs
      status_index++;
      if(status_update_timer >= status_update_interval){ //Status timer to track when to transmit the next update
        status_update_timer = 0; //Reset the status update timer
        if(heartbeat >= HEARTBEAT_TIMEOUT && serial_connection_active){ //Default to LED off if connection is lost
          ledOff();
          updateIntensity();
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
      if(current_status.s.mode){ //If in manual mode, use internal analog
        external_analog = false;
        digitalWriteFast(pin.ANALOG_SELECT, LOW); //Set external analog input
      }
      else{ //If in sync mode
        if((sync.s.mode == 0 && sync.s.digital_mode[current_status.s.state] == 3) || (sync.s.mode == 2 && sync.s.confocal_mode[current_status.s.state] == 3) || (sync.s.mode == 1 && sync.s.analog_mode == 2)){ //If state requires ext. analog
          external_analog = true;
          pinMode(pin.ANALOG_SELECT, OUTPUT);
          digitalWriteFast(pin.ANALOG_SELECT, HIGH); //Set external analog input
        }
      }
      break;
    default: //Check if a serial packet has been received - 0.37 µs
      usb.update();
      status_index = 0; //Reset status index if no cases match
      break;
  }

  if((sync.s.mode==1 || sync.s.mode==2) && !current_status.s.mode) noInterrupts(); //Disable interrupts if in confocal mode, as the scan mirror is used as the interrupt clock
}

void ledOff(){
  current_status.s.led_pwm = 0;
  current_status.s.led_current = 0;
  manual_mode = 3;
  current_status.s.mode = manual_mode;
  external_analog = false; //Set input to internal analog
}

void updateIntensity(){
  //Turn off LED before updating intensity.
  pinMode(pin.INTERLINE, OUTPUT); //ensure pin is disconnected from PWM bus
  digitalWriteFast(pin.INTERLINE, LOW);
  analogWrite(pin.DAC0, 0);
  
  if(conf.c.led_active[current_status.s.led_channel]){ //Check if the channel is active
    
    for(int a=0; a<4; a++){ //Toggle channel relays
      if(conf.c.led_channel[a] == current_status.s.led_channel) digitalWriteFast(pin.RELAY[a], pin.RELAY_CLOSE);
      else digitalWriteFast(pin.RELAY[a], !pin.RELAY_CLOSE);
    }
    active_channel = current_status.s.led_channel;

    digitalWriteFast(pin.ANALOG_SELECT, external_analog); //Set external analog input
    if(external_analog){
      pinMode(pin.INTERLINE, OUTPUT); //ensure pin is disconnected from PWM bus
      analogWrite(pin.DAC0, 0);
    }
    else{
      analogWrite(pin.DAC0, current_status.s.led_current);
      if(!(sync.s.mode == 2 && !current_status.s.mode)){ //If in confocal mode, do not apply PWM to interline pin
        if(!(sync.s.mode == 1 && sync.s.analog_mode && !current_status.s.mode)){ //If analog mode without PWM, do not apply PWM to interline pin
          if(current_status.s.led_pwm == 65535){ //Write pin constant high at 100% PWM
            pinMode(pin.INTERLINE, OUTPUT);
            digitalWriteFast(pin.INTERLINE, HIGH);
          }
          else analogWrite(pin.INTERLINE, current_status.s.led_pwm);
        }
        else{
          pinMode(pin.INTERLINE, OUTPUT); //Otherwise, ensure pin is constant on
          digitalWriteFast(pin.INTERLINE, HIGH);
        }
      }
      else pinMode(pin.INTERLINE, OUTPUT); //Otherwise, ensure pin is disconnected from PWM bus
    }
  }
  else{ //If LED channel is inactive, set output to 0 to not stress op-amp inputs
    current_status.s.led_pwm = 0;
    current_status.s.led_current = 0;
    analogWrite(pin.DAC0, 0);
    pinMode(pin.INTERLINE, OUTPUT);
    digitalWriteFast(pin.INTERLINE, LOW);
  }
}


void setFan(uint16_t temp, uint8_t fan_index){
  uint8_t out_pin;
  float temp_min;
  uint16_t temp_max;
  float delta_temp;
  if(fan_index){
    if(conf.c.fan_channel){
      out_pin = pin.OUTPUTS[conf.c.fan_channel-1];
      temp_min = conf.c.ext_fan[0];
      temp_max = conf.c.ext_fan[1];
      delta_temp =  conf.c.ext_fan[0] - conf.c.ext_fan[1];
    }
    else return;
  }
  else{
    out_pin = pin.FAN_PWM;
    temp_min = conf.c.driver_fan[0];
    temp_max = conf.c.driver_fan[1];
    delta_temp =  conf.c.driver_fan[0] - conf.c.driver_fan[1];
  }
  
  if(temp >= temp_min){
    pinMode(out_pin, OUTPUT);
    digitalWriteFast(out_pin, LOW); //If temp is below min fan temp, turn fan off
    current_status.s.fan_speed[fan_index] = 0;
  }
  else if(temp <= temp_max){
    pinMode(out_pin, OUTPUT);
    digitalWriteFast(out_pin, HIGH); //If temp is above max fan temp, run fan at full speed
    current_status.s.fan_speed[fan_index] = 65535;
  }
  else{
    current_status.s.fan_speed[fan_index] =  round(((temp_min - (float) temp)/delta_temp)*65535.0);
    analogWrite(out_pin, current_status.s.fan_speed[fan_index]);
  }
}

void thermalFault(){
  if(!fault_active){ //If fault is not active, check if any temp is above the fault temperature
    for(int a=0; a<3; a++){
      if(current_status.s.temp[a] <= conf.c.fault_temp[a]){
        fault_active = true;
        break;   
      }  
    }
  }
  if(fault_active){
    memcpy(stored_status.byte_buffer, current_status.byte_buffer, sizeof(stored_status.byte_buffer)); //Save current status to restore state after fault.

    //Turn off LED circuit completely
    pinMode(pin.INTERLINE, OUTPUT); //Disconnect interline pin from PWM mux
    digitalWriteFast(pin.INTERLINE, LOW); //Apply negative voltage input to op-amp
    analogWrite(pin.DAC0, 0); //Set analog input to 0
    for(size_t b=0; b<sizeof(pin.RELAY)/sizeof(pin.RELAY[0]); b++) digitalWriteFast(pin.RELAY[b], !pin.RELAY_CLOSE); //Open all relays

    //Send fault warning to GUI
    temp_size = sprintf(temp_buffer, "-Warning: Fault temperature has been exceeded.  The driver will turn off LED until below warning temperature.");
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);

    //Update status
    ledOff();
    current_status.s.driver_control = true;
    while(fault_active){
      playAlarmTone();
      fault_active = false; //If all temps are below warn temp, clear the fault
      for(int a=0; a<3; a++) if(current_status.s.temp[a] <= conf.c.warn_temp[a]) fault_active = true; //If any temp is above warn temp, maintain fault      
    }
    digitalWriteFast(pin.RELAY[current_status.s.led_channel], pin.RELAY_CLOSE); //Close corresponding relay
    delay(10); //Wait for SSR to fully close (needs 2 ms max)

    //Restore driver to previous state
    current_status.s.led_pwm = stored_status.s.led_pwm;
    current_status.s.led_current = stored_status.s.led_current;
    current_status.s.mode = stored_status.s.mode;
    current_status.s.driver_control = stored_status.s.driver_control;
    if(current_status.s.driver_control) manual_mode = current_status.s.mode; //Update manual mode if driver control
    
    pinMode(pin.INTERLINE, OUTPUT); //Disconnect interline pin from PWM mux
    if(stored_status.s.led_pwm == 0 || (sync.s.mode == 2 && stored_status.s.mode == 0)) digitalWriteFast(pin.INTERLINE, LOW); //Set to digital low if no PWM or in confocal sync mode (which doesn't use PWM)
    else analogWrite(pin.INTERLINE, stored_status.s.led_pwm); //Restore PWM on LED
    analogWrite(pin.DAC0, stored_status.s.led_current); //Set analog input to 0
    update_flag = true; //Toggle update flag
  }
}

void playAlarmTone(){
  static uint8_t led_index = 0;
  audio = 0;
  pulse = 0;
  for(int led=0; led<4; led++){
    if(conf.c.pushbutton_mode == 1 || conf.c.pushbutton_mode == 3 || (conf.c.pushbutton_mode == 2 && led == led_index)) digitalWriteFast(pin.LED[led], HIGH);
    else digitalWriteFast(pin.LED[led], LOW);
  }
  if(++led_index >= 4) led_index = 0;
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
  for(int led=0; led<4; led++){
    if(conf.c.pushbutton_mode == 3 || (conf.c.pushbutton_mode == 2 && led == led_index)) digitalWriteFast(pin.LED[led], HIGH);
    else digitalWriteFast(pin.LED[led], LOW);
  }
  if(++led_index >= 4) led_index = 0;
  while(pulse < 1024){
    checkStatus();
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
  sd.initializeSD();
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
    pinMode(pin.INTERLINE, OUTPUT);
    digitalWriteFast(pin.INTERLINE, 0); //Turn of LED while driver transitions between sync and manual modes
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
  else if(buffer_prefix == prefix.calibration) driverCalibration(buffer, size);
  else if(buffer_prefix == prefix.gui_disconnect) disconnectSerial();
  else if(buffer_prefix == prefix.measure_period) measurePeriod(buffer, size);
  else if(buffer_prefix == prefix.test_current) testCurrent(buffer, size);
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
  if(size == sizeof(recv_status.byte_buffer)+1){
    memcpy(recv_status.byte_buffer, buffer+1, sizeof(recv_status.byte_buffer));
    current_status.s.led_channel = recv_status.s.led_channel;
    current_status.s.driver_control = recv_status.s.driver_control;
    if(current_status.s.mode) manual_mode = recv_status.s.mode; //Set manual mode to recv'd mode if not in sync
    if(!current_status.s.driver_control){
      current_status.s.mode = recv_status.s.mode;
      current_status.s.led_pwm = recv_status.s.led_pwm;
      current_status.s.led_current = recv_status.s.led_current;
    }
    else{
      if(!manual_mode) manual_mode = 1;
    }
    memcpy(stored_status.byte_buffer, current_status.byte_buffer, sizeof(stored_status.byte_buffer)); //Update the stored status
    updateIntensity();
    update_flag = true;
  }
  else{
    temp_size = sprintf(temp_buffer, "-Error: LED  driver received an invalid status packet.  Expected %d bytes and received %d bytes.", sizeof(recv_status.byte_buffer)+1, size);
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
  }
}

static void driverCalibration(const uint8_t* buffer, size_t size){
  BYTE16UNION calibration_current;
  memcpy(calibration_current.bytes, buffer + 1, sizeof(calibration_current.bytes));
  temp_size = pin.captureWave(calibration_current.bytes_var, (uint8_t*) (temp_buffer+1));
  temp_buffer[0] = prefix.calibration;
  usb.send((const unsigned char*) temp_buffer, (size_t) (temp_size+1));
  //usb.send((const unsigned char*) cobs_buffer, cobs_size);
}

void disconnectSerial(){
  serial_connection_active = false; //Stop sending status packets
  ledOff(); //Set LED off on disconnect
  updateIntensity();
  manual_mode = 3;
  update_flag = true;
}

void measurePeriod(const uint8_t* buffer, size_t size){
  SYNCUNION temp_sync;
  float delta_cycles; //Number of cycles between triggers
  float sum_of_squares = 0; //Used to calcualte variance - https://www.thoughtco.com/sum-of-squares-formula-shortcut-3126266
  float sum_cycles = 0; //Used to calcualte variance - https://www.thoughtco.com/sum-of-squares-formula-shortcut-3126266
  uint32_t prev_cycles; //Number of cycles at previous trigger
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
    if(pin.OUTPUTS[temp_sync.s.sync_output_channel]) digitalWriteFast(pin.OUTPUTS[temp_sync.s.sync_output_channel-1], HIGH);
    checkStatus();
    if(a > 0){
      delta_cycles = cpu_cycles - prev_cycles; //Calcualte number of elapsed cycles
      sum_of_squares += delta_cycles * delta_cycles; //add to sum of squares
      sum_cycles += delta_cycles; //Used for stdev and mean
      n_measurements += 1;
    }
    prev_cycles = cpu_cycles;
    if(pin.OUTPUTS[temp_sync.s.sync_output_channel]) digitalWriteFast(pin.OUTPUTS[temp_sync.s.sync_output_channel-1], LOW);
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
    mean = (sum_cycles/n_measurements)/180.0;
    stdev = (sum_cycles*sum_cycles);
    stdev /= n_measurements;
    stdev = abs(sum_of_squares-stdev);
    stdev /= n_measurements;
    stdev = sqrt(stdev);
    stdev /= 180.0;
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

void testCurrent(const uint8_t* buffer, size_t size){
  STATUSUNION stored_status;
  CONFIGUNION stored_config;
  float current_array[4]; //Array to store current measurements
  bool enable_array[4]; //Array to store whether LED channel can be enabled
  uint32_t current_measurement; //Store current measurements
  uint16_t n_samples = 256; //Number of samples to take of each LED current 

  if(size == sizeof(stored_config.byte_buffer)){
    memcpy(stored_status.byte_buffer, current_status.byte_buffer, sizeof(stored_status.byte_buffer)); //Save current status
    memcpy(stored_config.byte_buffer, conf.byte_buffer, sizeof(stored_config.byte_buffer)); //Store copy of saved configuration
    memcpy(conf.byte_buffer, buffer, sizeof(conf.byte_buffer)); //Load temp configuration
    analogWrite(pin.INTERLINE, 0); //Turn off LED
    digitalWriteFast(pin.ANALOG_SELECT, LOW); //Set to internal analog
    pinMode(pin.ISENSE, INPUT);
    temp_size = 1; //Initialize temp_buffer index after prefix
    for(int a=0; a<4; a++){
      if(conf.c.led_active[a]){ //If LED channel is active
        current_status.s.led_channel = a; //Set channel
        current_status.s.led_current = conf.c.current_limit[a]; //Set to current limit
        current_status.s.led_pwm = 65535; //Set PWM to max
        current_status.s.mode = 1; //Assume manual mode to override sync related intensities
        updateIntensity();
        delay(10); //Wait for current and SSRs to settle
        analogRead(pin.ISENSE); //Clear adc
        current_measurement = 0;
        for(int b=0; b<n_samples; b++) current_measurement += analogRead(pin.ISENSE);
        analogWrite(pin.INTERLINE, 0);
        current_array[a] = (float) current_measurement / (float) n_samples; //Divide by n_samples to get average current
        if(current_array[a] + (0.5/3.3)*65535 < conf.c.current_limit[a] || current_array[a] - (0.5/3.3)*65535 > conf.c.current_limit[a]){ //Disable channel if Isense voltage is outside DAC voltage +/- 0.5V to prevent damage to op-amp
          enable_array[a] = false;
          if(stored_config.c.led_active[a]){ //Force over-ride under-voltage led configuration to prevent op-amp damage
            stored_config.c.led_active[a] = false; 
            stored_config.c.checksum += 1; //Increment checksum
          }
        }
        else enable_array[a] = true;
        current_array[a] /= (float) conf.c.current_limit[a] * 0.01; //Convert current to percentage of current limit
        temp_size += sprintf(temp_buffer+temp_size, "LED #%d: %.1f%% output - %s\n", a+1, current_array[a], enable_array[a] ? "ok":"DISABLED");
      }
      else{ //If untested - report as disabled with no current
        current_array[a] = 0;
        enable_array[a] = false;
        temp_size += sprintf(temp_buffer+temp_size, "LED #%d: Inactive - not tested\n", a+1);
      }
    }
    temp_buffer[0] = prefix.message;
    usb.send((const unsigned char*) temp_buffer, temp_size);
    delay(100);
    memcpy(temp_buffer+1, enable_array, sizeof(enable_array));
    temp_buffer[0] = prefix.test_current;
    usb.send((const unsigned char*) temp_buffer, sizeof(enable_array)+1);
    memcpy(current_status.byte_buffer, stored_status.byte_buffer, sizeof(stored_status.byte_buffer)); //Restore status
    memcpy(conf.byte_buffer, stored_config.byte_buffer, sizeof(stored_config.byte_buffer)); //Restore configuraiton
    digitalWriteFast(pin.ANALOG_SELECT, external_analog); //Restore analog input selection
    updateIntensity(); //Restore led intensity
  }
  else{
    temp_size = sprintf(temp_buffer, "-Error: LED  driver received an invalid test current packet.  Expected %d bytes and received %d bytes.", sizeof(stored_config.byte_buffer), size);
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
