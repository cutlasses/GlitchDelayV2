#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>     // Arduino compiler can get confused if you don't include include all required headers in this file?!?

#include "CompileSwitches.h"
#include "GlitchDelayEffect.h"
#include "GlitchDelayInterface.h"
#include "TapBPM.h"
#include "Util.h"


// Use these with the audio adaptor board
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

// wrap in a struct to ensure initialisation order
struct IO
{
#ifndef I2C_INTERFACE
  ADC                         adc;
#endif
  AudioInputAnalog            audio_input;
  AudioOutputAnalog           audio_output;

  IO() :
#ifndef I2C_INTERFACE
    adc(),
#endif
    audio_input(A0),
    audio_output()
  {
  }
};

IO io;
//AudioInputAnalog            audio_input(A0);
//AudioOutputAnalog           audio_output;


GLITCH_DELAY_EFFECT      glitch_delay_effect;
AudioMixer4              delay_mixer;
AudioMixer4              glitch_mixer;
AudioMixer4              wet_dry_mixer;
//AudioEffectDelay         audio_delay;

const int DRY_CHANNEL( 0 );
const int WET_CHANNEL( 1 );
const int FEEDBACK_CHANNEL( 1 );

const float MAX_FEEDBACK( 0.95f );

#ifdef STANDALONE_AUDIO
AudioPlaySdRaw           raw_player;
//AudioConnection          patch_cord_L1( raw_player, 0, audio_output, 0 );
//AudioConnection          patch_cord_R1( raw_player, 1, audio_output, 1 );
AudioConnection          patch_cord_L1( raw_player, 0, delay_mixer, 0 );
AudioConnection          patch_cord_L2( delay_mixer, 0, glitch_delay_effect, 0 );
AudioConnection          patch_cord_L3( glitch_delay_effect, 0, glitch_mixer, 0 );
AudioConnection          patch_cord_L4( glitch_delay_effect, 1, glitch_mixer, 1 );
AudioConnection          patch_cord_L5( glitch_delay_effect, 2, glitch_mixer, 2 );
AudioConnection          patch_cord_L6( glitch_delay_effect, 3, glitch_mixer, 3 );
AudioConnection          patch_cord_L7( glitch_mixer, 0, delay_mixer, FEEDBACK_CHANNEL );
AudioConnection          patch_cord_L8( glitch_mixer, 0, wet_dry_mixer, WET_CHANNEL );
AudioConnection          patch_cord_L9( raw_player, 0, wet_dry_mixer, DRY_CHANNEL );
AudioConnection          patch_cord_L10( wet_dry_mixer, 0, audio_output, 0 );
#else // STANDALONE_AUDIO
AudioConnection          patch_cord_L1( io.audio_input, 0, delay_mixer, 0 );
AudioConnection          patch_cord_L2( delay_mixer, 0, glitch_delay_effect, 0 );
AudioConnection          patch_cord_L3( glitch_delay_effect, 0, glitch_mixer, 0 );
AudioConnection          patch_cord_L4( glitch_delay_effect, 1, glitch_mixer, 1 );
AudioConnection          patch_cord_L5( glitch_delay_effect, 2, glitch_mixer, 2 );
AudioConnection          patch_cord_L6( glitch_delay_effect, 3, glitch_mixer, 3 );
AudioConnection          patch_cord_L7( glitch_mixer, 0, delay_mixer, FEEDBACK_CHANNEL );
AudioConnection          patch_cord_L8( glitch_mixer, 0, wet_dry_mixer, WET_CHANNEL );
AudioConnection          patch_cord_L9( io.audio_input, 0, wet_dry_mixer, DRY_CHANNEL );
AudioConnection          patch_cord_L10( wet_dry_mixer, 0, io.audio_output, 0 );
//AudioConnection          patch_cord_L1( audio_input, 0, audio_output, 0 );    // left channel passes straight through (for testing)
//AudioConnection          patch_cord_R1( io.audio_input, 1, io.audio_output, 1 );      // right channel passes straight through
#endif // !STANDALONE_AUDIO

GLITCH_DELAY_INTERFACE   glitch_delay_interface;


//////////////////////////////////////

void set_adc1_to_3v3()
{
  ADC1_SC3 = 0; // cancel calibration
  ADC1_SC2 = ADC_SC2_REFSEL(0); // vcc/ext ref 3.3v

  ADC1_SC3 = ADC_SC3_CAL;  // begin calibration

  uint16_t sum;

  //serial_print("wait_for_cal\n");

  while( (ADC1_SC3 & ADC_SC3_CAL))
  {
    // wait
  }

  __disable_irq();

    sum = ADC1_CLPS + ADC1_CLP4 + ADC1_CLP3 + ADC1_CLP2 + ADC1_CLP1 + ADC1_CLP0;
    sum = (sum / 2) | 0x8000;
    ADC1_PG = sum;
    sum = ADC1_CLMS + ADC1_CLM4 + ADC1_CLM3 + ADC1_CLM2 + ADC1_CLM1 + ADC1_CLM0;
    sum = (sum / 2) | 0x8000;
    ADC1_MG = sum;

  __enable_irq();
  
}

void setup()
{
  Serial.begin(9600);

#ifdef DEBUG_OUTPUT
  serial_port_initialised = true;

  Serial.print("Setup started!\n");
#endif // DEBUG_OUTPUT

  AudioMemory(16);

  analogReference(INTERNAL);

  set_adc1_to_3v3();

#ifdef STANDALONE_AUDIO
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  //if (!(SD.begin(SDCARD_CS_PIN)))
  if (!(SD.begin(BUILTIN_SDCARD)))
  {
    // stop here, but print a message repetitively
    while (1)
    {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
#endif // STANDALONE_AUDIO

  glitch_delay_interface.setup();

  //audio_delay.delay(0, 300);

  wet_dry_mixer.gain( DRY_CHANNEL, 0.5f );
  wet_dry_mixer.gain( WET_CHANNEL, 0.5f );

  delay_mixer.gain( 0, 0.5f );
  delay_mixer.gain( 1, 0.25f );

  glitch_mixer.gain( 0, 0.3f );
  glitch_mixer.gain( 1, 0.5f );
  glitch_mixer.gain( 2, 0.2f );
  
#ifdef DEBUG_OUTPUT
  Serial.print("Setup finished!\n");
#endif // DEBUG_OUTPUT
}

void loop()
{
  uint32_t time_in_ms = millis();

#ifdef STANDALONE_AUDIO
  if( !raw_player.isPlaying() )
  {
    Serial.println("START RAW");
    raw_player.play( "GUITAR.RAW" ); 
    //raw_player.play( "DRUMLOOP.RAW" );
    Serial.println("RAW Playing");
  }
#endif

  /*
  wet_dry_mixer.gain( DRY_CHANNEL, 0.0f );
  wet_dry_mixer.gain( WET_CHANNEL, 1.0f );

  delay_mixer.gain( FEEDBACK_CHANNEL, MAX_FEEDBACK );
  //delay_mixer.gain( FEEDBACK_CHANNEL, 0.0f );

  glitch_delay_effect.set_speed( 0.3f );

  glitch_delay_effect.set_loop_size( 0.2f );
  */

#ifdef I2C_INTERFACE
  glitch_delay_interface.update( time_in_ms );
#else// !I2C_INTERFACE
  glitch_delay_interface.update( io.adc, time_in_ms );
#endif  

  const float wet_dry = clamp( glitch_delay_interface.dry_wet_mix(), 0.0f, 1.0f );
  wet_dry_mixer.gain( DRY_CHANNEL, 1.0f - wet_dry );
  wet_dry_mixer.gain( WET_CHANNEL, wet_dry );
  
  const float feedback = glitch_delay_interface.feedback();
  delay_mixer.gain( FEEDBACK_CHANNEL, feedback * MAX_FEEDBACK );

  const float jitter  = clamp( glitch_delay_interface.loop_speed(), 0.0f, 1.0f );
  const float size  = clamp( glitch_delay_interface.loop_size(), 0.0f, 1.0f );

  for( int h = 0; h < GLITCH_DELAY_EFFECT::NUM_PLAY_HEADS; ++h )
  {
    glitch_delay_effect.set_jitter( h, jitter );
    glitch_delay_effect.set_loop_size( h, size );
  }

  //const bool move_loop = glitch_delay_interface.mode() == 0;
  glitch_delay_effect.set_loop_moving( false );

  const bool freeze = glitch_delay_interface.mode() == 1;
  glitch_delay_effect.set_freeze_active( freeze );

  const float head_mix = glitch_delay_interface.head_mix();
  glitch_mixer.gain( 0, glitch_delay_interface.low_mix() * head_mix );
  glitch_mixer.gain( 1,glitch_delay_interface.normal_mix() * head_mix );
  glitch_mixer.gain( 2, glitch_delay_interface.high_mix() * head_mix );
  glitch_mixer.gain( 3, glitch_delay_interface.reverse_mix() * head_mix );

  if( glitch_delay_interface.tap_bpm().beat_type() == TAP_BPM::AUTO_BEAT )
  {
    glitch_delay_effect.set_beat();
  }

#ifdef DEBUG_OUTPUT
  /*
  static int count = 0;
  if( ++count % 1000 == 0 )
  {

    Serial.print("Size ");
    Serial.print(size);
    Serial.print("\t");
    
    Serial.print("speed ");
    Serial.print(speed);
    Serial.print("\t");

    Serial.print("feedback ");
    Serial.print(feedback * MAX_FEEDBACK);
    Serial.print("\t");
      
    Serial.print("mix ");
    Serial.print(wet_dry);
    Serial.print("\n");
  
    Serial.print("****\n");
  }
  */

#endif // DEBUG_OUTPUT
    
#ifdef PERF_CHECK
  const int processor_usage = AudioProcessorUsage();
  if( processor_usage > 85 )
  {
    Serial.print( "Performance spike: " );
    Serial.print( processor_usage );
    Serial.print( "\n" );
  }
#endif
}




