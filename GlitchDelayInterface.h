#pragma once

#include "Interface.h"
#include "TapBPM.h"

class GLITCH_DELAY_INTERFACE
{
  static const int        MODE_BUTTON_PIN                 = 1;
  static const int        BPM_BUTTON_PIN                  = 2;
  static const int        LED_1_PIN                       = 29;
  static const int        LED_2_PIN                       = 11;
  static const int        LED_3_PIN                       = 7;

  static const int        NUM_MODES                       = 2;

  static const bool       FREEZE_BUTTON_IS_TOGGLE         = true;
  static const int        NUM_DIALS                       = 6;
  static const int        NUM_LEDS                        = 3;

  static constexpr float  HEAD_MIX_INITIAL_VALUE          = 0.25f;
  static constexpr float  FEEDBACK_INITIAL_VALUE          = 0.7f;
 
  static const int32_t    BIT_DEPTH_BUTTON_HOLD_TIME_MS   = 2000;

  CV_DIAL                 m_dials[NUM_DIALS];

  BUTTON                  m_bpm_button;
  BUTTON                  m_mode_button;
  TAP_BPM                 m_tap_bpm;        // same button as mode
  
  LED                     m_beat_led;
  LED                     m_mode_leds[NUM_MODES];

  PUSH_AND_TURN           m_head_mix_push_and_turn;
  PUSH_AND_TURN           m_feedback_push_and_turn;

  int                     m_current_mode;
  bool                    m_change_bit_depth_valid;
  bool                    m_reduced_bit_depth;

public:

  GLITCH_DELAY_INTERFACE();

  void                    setup();
  void                    update( ADC& adc, uint32_t time_in_ms );

  float                   loop_size() const;
  float                   loop_speed() const;
  float                   feedback() const;
  float                   low_mix() const;
  float                   normal_mix() const;
  float                   high_mix() const;
  float                   reverse_mix() const;
  float                   dry_wet_mix() const;

  float                   head_mix() const;

  const TAP_BPM&          tap_bpm() const;

  int                     mode() const;
  bool                    reduced_bit_depth() const;
};

