#include "GlitchDelayInterface.h"
#include "CompileSwitches.h"

  
GLITCH_DELAY_INTERFACE::GLITCH_DELAY_INTERFACE() :
  m_loop_size_dial(),
  m_loop_speed_dial(),
  m_feedback_dial(),
  m_low_mix_dial(),
  m_high_mix_dial(),
  m_mix_dial(),
  m_bpm_button( BPM_BUTTON_PIN, false ),
  m_mode_button( MODE_BUTTON_PIN, false ),
  m_tap_bpm( BPM_BUTTON_PIN ),
  m_beat_led(),
  m_mode_leds(),
  m_current_mode( 0 ),
  m_change_bit_depth_valid( true ),
  m_reduced_bit_depth( false )
{
  m_beat_led        = LED( LED_1_PIN, false );
  m_mode_leds[0]    = LED( LED_2_PIN, false );
  m_mode_leds[1]    = LED( LED_3_PIN, false ); 
}

void GLITCH_DELAY_INTERFACE::setup()
{
  m_bpm_button.setup();
  m_mode_button.setup();

  m_beat_led.setup();
  m_beat_led.set_brightness( 0.25f );

  for( int i = 0; i < NUM_MODES; ++i )
  {
    m_mode_leds[i].setup();
    m_mode_leds[i].set_brightness( 0.25f );
  }
}

void GLITCH_DELAY_INTERFACE::update( uint32_t time_in_ms )
{
  // start I2C with PIC chip
  Wire.requestFrom(123, 16);    // request

  // read each pot
  m_loop_size_dial.update();
  m_loop_speed_dial.update();
  m_feedback_dial.update();
  m_low_mix_dial.update();
  m_high_mix_dial.update();
  m_mix_dial.update();
  
  m_bpm_button.update( time_in_ms );
  m_mode_button.update( time_in_ms );

  m_tap_bpm.update( time_in_ms );
 
  if( m_tap_bpm.beat_type() != TAP_BPM::NO_BEAT )
  {
      m_beat_led.flash_on( time_in_ms, 100 );
  }
  m_beat_led.update( time_in_ms );

  /*
  if( m_mode_button.down_time_ms() > BIT_DEPTH_BUTTON_HOLD_TIME_MS && m_change_bit_depth_valid )
  {
    m_reduced_bit_depth = !m_reduced_bit_depth;

    // don't allow the mode to change until button is released
    m_change_bit_depth_valid = false;
  }

  if( !m_change_bit_depth_valid && !m_mode_button.active() )
  {
    // once the mode button has been released, we can change the mode again
    m_change_bit_depth_valid = true;
  }
  */

  if( m_mode_button.single_click() )
  {
    m_current_mode = ( m_current_mode + 1 ) % NUM_MODES;
  }

  for( int i = 0; i < NUM_MODES; ++i )
  {
    m_mode_leds[i].set_active( m_current_mode == i );
     
    m_mode_leds[i].update( time_in_ms );
  }

#ifdef DEBUG_OUTPUT
  /*
  if( m_speed_dial.update() )
  {
    Serial.print("Speed ");
    Serial.print(m_speed_dial.value());
    Serial.print("\n");
  }
  if( m_mix_dial.update() )
  {
    Serial.print("Mix ");
    Serial.print(m_mix_dial.value());
    Serial.print("\n");   
  }
  if( m_length_dial.update() )
  {
    Serial.print("Length ");
    Serial.print(m_length_dial.value());
    Serial.print("\n");
  }
  if( m_position_dial.update() )
  {
    Serial.print("Position ");
    Serial.print(m_position_dial.value());
    Serial.print("\n");   
  }
  m_freeze_button.update();

  if( m_freeze_button.active() )
  {
    Serial.print("on\n");
  }
  */
#endif // DEBUG_OUTPUT
}

const I2C_DIAL& GLITCH_DELAY_INTERFACE::loop_size_dial() const
{
  return m_loop_size_dial;
}

const I2C_DIAL& GLITCH_DELAY_INTERFACE::loop_speed_dial() const
{
  return m_loop_speed_dial;
}

const I2C_DIAL& GLITCH_DELAY_INTERFACE::feedback_dial() const
{
  return m_feedback_dial;
}

const I2C_DIAL& GLITCH_DELAY_INTERFACE::low_mix_dial() const
{
  return m_low_mix_dial;
}

const I2C_DIAL& GLITCH_DELAY_INTERFACE::hight_mix_dial() const
{
  return m_high_mix_dial;
}

const I2C_DIAL& GLITCH_DELAY_INTERFACE::mix_dial() const
{
  return m_mix_dial;
}

const TAP_BPM& GLITCH_DELAY_INTERFACE::tap_bpm() const
{
  return m_tap_bpm;
}

int GLITCH_DELAY_INTERFACE::mode() const
{
  return m_current_mode;
}

bool GLITCH_DELAY_INTERFACE::reduced_bit_depth() const
{
  return m_reduced_bit_depth;
}

