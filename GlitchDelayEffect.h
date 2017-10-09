#pragma once

#include "TeensyJuce.h"
#include "Util.h"

#define DELAY_BUFFER_SIZE_IN_BYTES     1024*240      // 240k

////////////////////////////////////

class DELAY_BUFFER;

////////////////////////////////////

class PLAY_HEAD
{
  const DELAY_BUFFER&         m_delay_buffer;     // TODO pass in to save storage?
  
  float                       m_current_play_head;
  float                       m_destination_play_head;
  float                       m_play_speed;
  int                         m_fade_samples_remaining;

  int                         m_loop_start;
  int                         m_loop_end;
  int                         m_unjittered_loop_start;
  int                         m_shift_speed;

  float                       m_next_loop_size_ratio;
  float                       m_next_shift_speed_ratio;
  float                       m_jitter_ratio;

  bool                        m_initial_loop_crossfade_complete;

  int                         play_head_to_write_head_buffer_size() const;
  int16_t                     read_sample_with_cross_fade();
   
public:

  PLAY_HEAD( const DELAY_BUFFER& delay_buffer, float play_speed );

  int                         current_position() const;
  int                         destination_position() const;

  int                         loop_start() const;
  int                         loop_end() const;
  int                         buffered_loop_start() const;
  int                         current_loop_size() const;

  bool                        position_inside_section( int position, int start, int end ) const;
  bool                        position_inside_next_read( int position, int read_size ) const;
  bool                        crossfade_active() const;
  bool                        initial_loop_crossfade_complete() const;

  void                        set_loop_size( float loop_size_in_samples );
  void                        set_shift_speed( float speed );
  void                        set_jitter( float jitter );
  void                        set_play_head( int offset_from_write_head );
  void                        set_next_loop();

  void                        set_loop_behind_write_head();
  
  void                        read_from_play_head( int16_t* dest, int size );  

  void                        enable_loop( int start, int end );
  void                        disable_loop();

#ifdef DEBUG_OUTPUT
  void                        debug_output();
#endif
};

////////////////////////////////////

class DELAY_BUFFER
{
  friend PLAY_HEAD;
  
  uint8_t                     m_buffer[DELAY_BUFFER_SIZE_IN_BYTES];
  int                         m_buffer_size_in_samples;
  int                         m_sample_size_in_bits;

  int                         m_write_head;

  int                         m_fade_samples_remaining;

public:

  DELAY_BUFFER();

  int                         position_offset_from_head( int offset ) const;
  int                         delay_offset_from_ratio( float ratio ) const;
  int                         delay_offset_from_time( int time_in_ms ) const;
  int                         write_head() const;
  int                         wrap_to_buffer( int position ) const;
  bool                        write_buffer_fading_in() const;

  void                        write_sample( int16_t sample, int index );
  int16_t                     read_sample( int index ) const;
  int16_t                     read_sample_with_speed( float index, float speed ) const;

  void                        increment_head( int& head ) const;
  void                        increment_head( float& head, float speed ) const;
  
  void                        write_to_buffer( const int16_t* source, int size );

  void                        set_bit_depth( int sample_size_in_bits );

  void                        fade_in_write();

  void                        set_write_head( int new_head );

#ifdef DEBUG_OUTPUT
  void                        debug_output();
#endif
};

////////////////////////////////////

class GLITCH_DELAY_EFFECT : public TEENSY_AUDIO_STREAM_WRAPPER
{
  static const int NUM_PLAY_HEADS = 3;
  
  DELAY_BUFFER          m_delay_buffer;

  PLAY_HEAD             m_play_heads[NUM_PLAY_HEADS];

  float                 m_speed_ratio;
  int                   m_speed_in_samples;

  float                 m_loop_size_ratio;
  int                   m_loop_size_in_samples;

  bool                  m_loop_moving;

  // store 'next' values, otherwise interrupt could be called during calculation of values
  int                   m_next_sample_size_in_bits;
  bool                  m_next_loop_moving;
  bool                  m_next_beat;
    
protected:
    
    void                process_audio_in_impl( int channel, const int16_t* sample_data, int num_samples ) override;
    void                process_audio_out_impl( int channel, int16_t* sample_data, int num_samples ) override;
  
public:

  GLITCH_DELAY_EFFECT();
    
  int                   num_input_channels() const override;
  int                   num_output_channels() const override;

  void                  update() override;

  void                  set_bit_depth( int sample_size_in_bits );
  void                  set_speed( float speed );
  void                  set_loop_size( float loop_size );
  void                  set_loop_moving( bool moving );

  void                  set_beat();
};


