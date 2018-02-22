#include <Wire.h>
#include <Audio.h>
#include "Interface.h"

#define BOUNCE_TIME         10

//////////////////////////////////////

DIAL_BASE::DIAL_BASE( bool invert ) :
  m_current_value( 0 ),
  m_invert( invert )
{
  
}

DIAL_BASE::~DIAL_BASE()
{
  
}

bool DIAL_BASE::set_current_value( int new_value )
{
  if( new_value != m_current_value )
  {
    m_current_value = new_value;
    return true;
  }

  return false; 
}

float DIAL_BASE::value() const
{
  return m_current_value;
  /*
  const float vf = m_current_value / 1024.0f;

  if( m_invert )
  {
    return 1.0f - vf;
  }
  else
  {
    return vf;
  }
  */
}

DIAL::DIAL( int data_pin, bool invert ) :
  DIAL_BASE( invert ),
  m_data_pin( data_pin )
{

}

bool DIAL::update( ADC& adc )
{
  const int new_value = adc.analogRead( m_data_pin, ADC_1 );

  return set_current_value( new_value );
}

//////////////////////////////////////

I2C_DIAL::I2C_DIAL( bool invert ) :
  DIAL_BASE( invert )
{
  
}

bool I2C_DIAL::update()
{
  // I2C should already be setup by this point ( Wire.requestFrom() )
  const byte b1 = Wire.read();
  const byte b2 = Wire.read();

 int new_value = b1 | ( b2 << 8 );

  return set_current_value( new_value );
}

//////////////////////////////////////

BUTTON::BUTTON( int data_pin, bool is_toggle ) :
  m_data_pin( data_pin ),
  m_is_toggle( is_toggle ),
  m_prev_is_active( false ),
  m_is_active( false ),
  m_down_time_stamp( 0 ),
  m_down_time_curr( 0 ),
  m_bounce( m_data_pin, BOUNCE_TIME )
{
}

bool BUTTON::active() const
{
  return m_is_active;
}

bool BUTTON::single_click() const
{
  return m_is_active && !m_prev_is_active;
}

int32_t BUTTON::down_time_ms() const
{
  if( m_down_time_stamp > 0 )
  {
//#ifdef DEBUG_OUTPUT
//  Serial.print("Down time:");
//  Serial.print(m_down_time_curr);
//  Serial.print("\n");
//#endif // DEBUG_OUTPUT
    return m_down_time_curr;
  }
  else
  {
    return 0;
  }
}

void BUTTON::setup()
{
  pinMode( m_data_pin, INPUT_PULLUP );
}

void BUTTON::update( uint32_t time_ms )
{ 
  m_bounce.update();

  m_prev_is_active = m_is_active;

  if( m_bounce.fallingEdge() )
  {
    if( m_is_toggle )
    {
      m_is_active = !m_is_active;
    }
    else
    {
      m_is_active = true;
    }

    // time stamp when button is pressed
    m_down_time_stamp = time_ms;
  }
  else if( m_bounce.risingEdge() )
  {
    if( !m_is_toggle )
    {
      m_is_active = false;
    }

    // reset when button released
    m_down_time_stamp = 0;
  }

  if( m_down_time_stamp > 0 )
  {
    m_down_time_curr = time_ms - m_down_time_stamp;
  }
}

//////////////////////////////////////

LED::LED() :
  m_data_pin( 0 ),
  m_is_active( false ),
  m_flash_active( false ),
  m_analog( false ),
  m_flash_off_time_ms( 0 )
{
}

LED::LED( int data_pin, bool analog ) :
  m_data_pin( data_pin ),
  m_is_active( false ),
  m_flash_active( false ),
  m_analog( analog ),
  m_flash_off_time_ms( 0 )
{
}

void LED::set_active( bool active )
{
  m_is_active = active;
}

void LED::flash_on( uint32_t time_ms, uint32_t flash_duration_ms )
{
  m_flash_active      = true;
  m_flash_off_time_ms = time_ms + flash_duration_ms;

  m_is_active         = true;
}

void LED::set_brightness( float brightness )
{
  m_brightness = brightness * 255.0f;  
}

void LED::setup()
{
  pinMode( m_data_pin, OUTPUT );
}

void LED::update( uint32_t time_ms )
{  
  if( m_is_active && m_flash_active && time_ms > m_flash_off_time_ms )
  {
    m_is_active     = false;
    m_flash_active  = false;
  }

  if( m_analog )
  {
    if( m_is_active )
    {   
      analogWrite( m_data_pin, m_brightness );
    }
    else
    {
      analogWrite( m_data_pin, 0 );
    }
  }
  else
  {
    if( m_is_active )
    {   
      digitalWrite( m_data_pin, HIGH );
    }
    else
    {
      digitalWrite( m_data_pin, LOW );
    }
  }
}

