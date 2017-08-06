#pragma once

#include <Bounce.h>

class DIAL
{
  int           m_data_pin;

  int           m_current_value;
  
public:

  DIAL( int data_pin );

  bool          update();
  float         value() const;  
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
  uint32_t      m_flash_off_time_ms;

public:

  LED();              // to allow for arrays
  LED( int data_pin );

  void          set_active( bool active );
  void          flash_on( uint32_t time_ms, uint32_t flash_duration );
  void          set_brightness( float brightness );
  
  void          setup();
  void          update( uint32_t time_ms );       
};

