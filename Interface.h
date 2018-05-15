#pragma once

#include <ADC.h>
#include <Bounce.h>

//////////////////////////////////////

class DIAL_BASE
{  
  int           m_current_value;
  bool          m_invert;

protected:

  bool         set_current_value( int new_value );

public:

  DIAL_BASE( bool invert );
  virtual ~DIAL_BASE();

  float         value() const;
};

//////////////////////////////////////

class DIAL : public DIAL_BASE
{  
  int           m_data_pin;
  
public:

  DIAL( int data_pin, bool invert = false );

  bool          update( ADC& adc );
};

//////////////////////////////////////

class I2C_DIAL : public DIAL_BASE
{
public:

  I2C_DIAL( bool invert );

  bool          update();
};

//////////////////////////////////////

class BUTTON
{
  int16_t       m_data_pin;
  int16_t       m_is_toggle : 1;
  int16_t       m_prev_is_active : 1;
  int16_t       m_is_active : 1;
  int16_t       m_down_time_valid : 1;
  uint32_t       m_down_time_stamp;
  uint32_t       m_down_time_curr;

  Bounce        m_bounce;

public:

  BUTTON( int data_pin, bool is_toggle );

  bool          active() const;
  bool          single_click() const;

  int32_t       down_time_ms() const;

  void          setup();
  void          update( uint32_t time_ms );
};

//////////////////////////////////////

class LED
{
  short         m_data_pin;
  byte          m_brightness;
  byte          m_is_active       : 1;
  byte          m_flash_active    : 1;
  byte          m_analog          : 1;
  uint32_t      m_flash_off_time_ms;

public:

  LED();              // to allow for arrays
  LED( int data_pin, bool analog );

  void          set_active( bool active );
  void          flash_on( uint32_t time_ms, uint32_t flash_duration );
  void          set_brightness( float brightness );
  
  void          setup();
  void          update( uint32_t time_ms );       
};

