#include "GlitchDelayInterface.h"
#include "CompileSwitches.h"

const int I2C_ADDRESS(111); 
const int I2C_DATA_SIZE_IN_BYTES(12);
  
GLITCH_DELAY_INTERFACE::GLITCH_DELAY_INTERFACE() :
  m_loop_size_dial(true),
  m_loop_speed_dial(true),
  m_feedback_dial(true),
  m_low_mix_dial(true),
  m_high_mix_dial(true),
  m_mix_dial(true),
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

  Wire.begin();
}

void GLITCH_DELAY_INTERFACE::update( uint32_t time_in_ms )
{
  // start I2C with PIC chip
  Wire.requestFrom(I2C_ADDRESS, I2C_DATA_SIZE_IN_BYTES); 

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

  auto debug_dial = []( const char* dial_name, const DIAL_BASE& dial )
  {
      Serial.print( dial_name );
      Serial.print( dial.value() );
      Serial.print( "\t");
  };

  debug_dial( "loop size:", m_loop_size_dial );
  debug_dial( "loop speed/jitter:", m_loop_speed_dial );
  debug_dial( "feedback:", m_feedback_dial );
  debug_dial( "low mix:", m_low_mix_dial );
  debug_dial( "high mix:", m_high_mix_dial );
  debug_dial( "mix:", m_mix_dial );
  Serial.println();
  
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

