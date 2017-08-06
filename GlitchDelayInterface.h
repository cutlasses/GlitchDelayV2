#pragma once

#include "Interface.h"
#include "TapBPM.h"

class GLITCH_DELAY_INTERFACE
{
  static const int      LOOP_SIZE_DIAL_PIN              = 20;
  static const int      LOOP_SPEED_DIAL_PIN             = 17;
  static const int      FEEDBACK_DIAL_PIN               = 21;
  static const int      MIX_DIAL_PIN                    = 16;
  static const int      MODE_BUTTON_PIN                 = 1;
  static const int      BPM_BUTTON_PIN                  = 2;
  static const int      LED_1_PIN                       = 4;
  static const int      LED_2_PIN                       = 3;
  static const int      LED_3_PIN                       = 5;

  static const int      NUM_MODES                       = 2;

  static const bool     FREEZE_BUTTON_IS_TOGGLE         = true;
  static const int      NUM_LEDS                        = 3;
 
  static const int32_t  BIT_DEPTH_BUTTON_HOLD_TIME_MS   = 2000;
  
  DIAL              m_loop_size_dial;
  DIAL              m_loop_speed_dial;
  DIAL              m_feedback_dial;
  DIAL              m_mix_dial;

  BUTTON            m_bpm_button;
  BUTTON            m_mode_button;
  TAP_BPM           m_tap_bpm;        // same button as mode
  
  LED               m_beat_led;
  LED               m_mode_leds[NUM_MODES];

  int               m_current_mode;
  bool              m_change_bit_depth_valid;
  bool              m_reduced_bit_depth;

public:

  GLITCH_DELAY_INTERFACE();

  void            setup();
  void            update( uint32_t time_in_ms );

  const DIAL&     loop_size_dial() const;
  const DIAL&     loop_speed_dial() const;
  const DIAL&     feedback_dial() const;
  const DIAL&     mix_dial() const;

  const TAP_BPM&  tap_bpm() const;

  int             mode() const;
  bool            reduced_bit_depth() const;
};

