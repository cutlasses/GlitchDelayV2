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

AudioInputAnalog            audio_input(A0);
AudioOutputAnalog           audio_output;


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
AudioConnection          patch_cord_L6( glitch_mixer, 0, delay_mixer, FEEDBACK_CHANNEL );
AudioConnection          patch_cord_L7( glitch_mixer, 0, wet_dry_mixer, WET_CHANNEL );
AudioConnection          patch_cord_L8( raw_player, 0, wet_dry_mixer, DRY_CHANNEL );
AudioConnection          patch_cord_L9( wet_dry_mixer, 0, audio_output, 0 );
#else // STANDALONE_AUDIO
AudioConnection          patch_cord_L1( audio_input, 0, delay_mixer, 0 );
AudioConnection          patch_cord_L2( delay_mixer, 0, glitch_delay_effect, 0 );
AudioConnection          patch_cord_L3( glitch_delay_effect, 0, glitch_mixer, 0 );
AudioConnection          patch_cord_L4( glitch_delay_effect, 1, glitch_mixer, 1 );
AudioConnection          patch_cord_L5( glitch_delay_effect, 2, glitch_mixer, 2 );
AudioConnection          patch_cord_L6( glitch_mixer, 0, delay_mixer, FEEDBACK_CHANNEL );
AudioConnection          patch_cord_L7( glitch_mixer, 0, wet_dry_mixer, WET_CHANNEL );
AudioConnection          patch_cord_L8( audio_input, 0, wet_dry_mixer, DRY_CHANNEL );
AudioConnection          patch_cord_L9( wet_dry_mixer, 0, audio_output, 0 );
//AudioConnection          patch_cord_L1( audio_input, 0, audio_output, 0 );    // left channel passes straight through (for testing)
AudioConnection          patch_cord_R1( audio_input, 1, audio_output, 1 );      // right channel passes straight through
#endif // !STANDALONE_AUDIO

GLITCH_DELAY_INTERFACE   glitch_delay_interface;


//////////////////////////////////////

void setup()
{
  Serial.begin(9600);

#ifdef DEBUG_OUTPUT
  serial_port_initialised = true;

  Serial.print("Setup started!\n");
#endif // DEBUG_OUTPUT

  AudioMemory(16);

  analogReference(INTERNAL);

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

  glitch_delay_interface.update( time_in_ms );

  const float wet_dry = clamp( glitch_delay_interface.mix_dial().value(), 0.0f, 1.0f );
  wet_dry_mixer.gain( DRY_CHANNEL, 1.0f - wet_dry );
  wet_dry_mixer.gain( WET_CHANNEL, wet_dry );
  
  const float feedback = glitch_delay_interface.feedback_dial().value();
  delay_mixer.gain( FEEDBACK_CHANNEL, feedback * MAX_FEEDBACK );

  const float speed = clamp( glitch_delay_interface.loop_speed_dial().value(), 0.0f, 1.0f );
  glitch_delay_effect.set_speed( speed );

  const float size = clamp( glitch_delay_interface.loop_size_dial().value(), 0.0f, 1.0f );
  glitch_delay_effect.set_loop_size( size );

  const bool move_loop = glitch_delay_interface.mode() == 0;
  glitch_delay_effect.set_loop_moving( move_loop );

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




