/*
  Default configuration of teensy pins
*/
#ifndef pinSetup_h
#define pinSetup_h

#include "Arduino.h"

//NOTE: It seems that in this compiler lists longer than 4 need to be built in CPP while shorter lists need to be built in header with constexpr

class pinSetup
{
  public:
    DAC();
    