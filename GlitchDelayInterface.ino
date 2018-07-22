#include "GlitchDelayInterface.h"
#include "CompileSwitches.h"

#ifdef I2C_INTERFACE
const int I2C_ADDRESS(111); 
const int I2C_DATA_SIZE_IN_BYTES(12);
#endif
  
GLITCH_DELAY_INTERFACE::GLITCH_DELAY_INTERFACE() :
#ifdef I2C_INTERFACE
  m_dials( { I2C_DIAL( true ), I2C_DIAL( true ), I2C_DIAL( true ), I2C_DIAL( true ), I2C_DIAL( true ), I2C_DIAL( true ) } ),
#else // !I2C_INTERFACE
  m_dials( { DIAL( A20 ), DIAL( A19 ), DIAL( A18 ), DIAL( A17 ), DIAL( A16 ), DIAL( A13 ) } ),
#endif // !I2C_INTERFACE
  m_bpm_button( BPM_BUTTON_PIN, false ),
  m_mode_button( MODE_BUTTON_PIN, false ),
  m_tap_bpm( BPM_BUTTON_PIN ),
  m_beat_led(),
  m_mode_leds(),
  m_head_mix_push_and_turn( m_dials[0], m_bpm_button, HEAD_MIX_INITIAL_VALUE ),
  m_feedback_push_and_turn( m_dials[0], m_mode_button, FEEDBACK_INITIAL_VALUE ),
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

#ifdef I2C_INTERFACE
  Wire.begin();
#endif
}

#ifdef I2C_INTERFACE
void GLITCH_DELAY_INTERFACE::update( uint32_t time_in_ms )
#else
void GLITCH_DELAY_INTERFACE::update( ADC& adc, uint32_t time_in_ms )
#endif
{
#ifdef I2C_INTERFACE
  // start I2C with PIC chip
  Wire.requestFrom(I2C_ADDRESS, I2C_DATA_SIZE_IN_BYTES);
#endif

  // read each pot
  for( int d = 0; d < NUM_DIALS; ++d )
  {
#ifdef I2C_INTERFACE
    m_dials[d].update();
#else
    m_dials[d].update( adc );
#endif
  }
  
  m_bpm_button.update( time_in_ms );
  m_mode_button.update( time_in_ms );

  m_tap_bpm.update( time_in_ms );

  m_head_mix_push_and_turn.update();
  m_feedback_push_and_turn.update();
 
  if( m_tap_bpm.beat_type() != TAP_BPM::NO_BEAT )
  {
      m_beat_led.flash_on( time_in_ms, 100 );
  }
  m_beat_led.update( time_in_ms );

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

  auto debug_dial = []( int dial_index, const DIAL_BASE& dial )
  {
      Serial.print( dial_index );
      Serial.print( ":" );
      Serial.print( dial.value() );
      Serial.print( "\t");
  };

  for( int d = 0; d < NUM_DIALS; ++d )
  {
    debug_dial( d, m_dials[d] );
  }
  Serial.println();
  
#endif // DEBUG_OUTPUT
}

float GLITCH_DELAY_INTERFACE::loop_size() const
{
  return m_dials[0].value();
}

float GLITCH_DELAY_INTERFACE::loop_speed() const
{
  return m_dials[1].value();
}

float GLITCH_DELAY_INTERFACE::feedback() const
{
  return m_feedback_push_and_turn.secondary_value();
}

float GLITCH_DELAY_INTERFACE::low_mix() const
{
  return m_dials[2].value();
}

float GLITCH_DELAY_INTERFACE::normal_mix() const
{
  return m_dials[3].value();
}

float GLITCH_DELAY_INTERFACE::high_mix() const
{
  return m_dials[4].value();
}

float GLITCH_DELAY_INTERFACE::reverse_mix() const
{
  return m_dials[5].value();
}

float GLITCH_DELAY_INTERFACE::dry_wet_mix() const
{
  return 1.0f; // fully wet
}

float GLITCH_DELAY_INTERFACE::head_mix() const
{
  return m_head_mix_push_and_turn.secondary_value();
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

