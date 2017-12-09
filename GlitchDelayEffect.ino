#ifdef TARGET_TEENSY
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#endif // TARGET_TEENSY

#include <string.h>
#include <math.h>
#include "GlitchDelayEffect.h"
#include "CompileSwitches.h"


const float MIN_SPEED( 0.25f );
const float MAX_SPEED( 4.0f );

#ifdef TARGET_JUCE
const int AUDIO_BLOCK_SAMPLES( 512 );           // TODO need a value in JUCE, is it even constant?
const int AUDIO_SAMPLE_RATE( 44100 );           // TODO need a value in JUCE
#endif

const int FIXED_FADE_TIME_SAMPLES( (AUDIO_SAMPLE_RATE / 1000.0f ) * 4 ); // 4ms cross fade
const int MIN_LOOP_SIZE_IN_SAMPLES( (FIXED_FADE_TIME_SAMPLES * 2) + AUDIO_BLOCK_SAMPLES );
const int MAX_LOOP_SIZE_IN_SAMPLES( AUDIO_SAMPLE_RATE * 0.5f );
const int MIN_SHIFT_SPEED( 0 );
const int MAX_SHIFT_SPEED( 100 );
const int MAX_JITTER_SIZE( AUDIO_SAMPLE_RATE * 0.2f );


/////////////////////////////////////////////////////////////////////

int delay_buffer_size_in_samples( int sample_size_in_bits )
{
    const float bytes_per_sample = sample_size_in_bits / 8.0f;
    return static_cast<int>(DELAY_BUFFER_SIZE_IN_BYTES / bytes_per_sample);
}

int convert_time_in_ms_to_samples( int time_in_ms )
{
    static int num_samples_per_ms = AUDIO_SAMPLE_RATE / 1000;
    return num_samples_per_ms * time_in_ms;
}

int fade_out_in( int x, int y, float t )
{
    // fade down then back up
    if( t < 0.5f )
    {
        // t = 0.0 -> 1, t = 0.5 -> 0
        t = 1.0f - ( t * 2.0f );
        
        return round( x * t );
    }
    else
    {
        // t = 0.5 -> 0, t = 1 -> 1
        t = ( t * 2.0f ) - 1.0f;
        
        return round( y * t );
    }
}

int cross_fade_samples( int x, int y, float t )
{
    return round( lerp<float>( x, y, t ) );
}

/////////////////////////////////////////////////////////////////////

PLAY_HEAD::PLAY_HEAD( const DELAY_BUFFER& delay_buffer, float play_speed ) :
    m_delay_buffer( delay_buffer ),
    m_current_play_head( 0.0f ),
    m_destination_play_head( 0.0f ),
    m_play_speed( play_speed ),
    m_fade_samples_remaining( 0 ),
    m_loop_start( -1 ),
    m_loop_end( -1 ),
    m_unjittered_loop_start( -1 ),
    m_shift_speed( 0 ),
    m_next_loop_size_ratio( 1.0f ),
    m_next_shift_speed_ratio( 0.0f ),
    m_jitter_ratio( 0.0f ),
    m_initial_loop_crossfade_complete(false)
{
    if( play_forwards() )
    {
        // all forward playing heads default to looping
        m_loop_start                    = 0;
        m_loop_end                      = MAX_LOOP_SIZE_IN_SAMPLES;
    }
    
    set_loop_behind_write_head();
    
    // set head immediately (don't want to crossfade initially)
    m_current_play_head                 = m_destination_play_head;
    m_initial_loop_crossfade_complete   = true;
    m_fade_samples_remaining            = 0;
}

int PLAY_HEAD::current_position() const
{
    return m_current_play_head;
}

int PLAY_HEAD::destination_position() const
{
    return m_destination_play_head;
}

int PLAY_HEAD::loop_start() const
{
    return m_loop_start;
}

int PLAY_HEAD::loop_end() const
{
    return m_loop_end;
}

int PLAY_HEAD::play_head_to_write_head_buffer_size() const
{
    const int half_jitter     = ( MAX_JITTER_SIZE / 2 ) + 1;
    const int buffer_samples  = FIXED_FADE_TIME_SAMPLES + FIXED_FADE_TIME_SAMPLES + AUDIO_BLOCK_SAMPLES + half_jitter;
    
    return buffer_samples;
}

int PLAY_HEAD::buffered_loop_start() const
{
    const int extended_start  = m_delay_buffer.wrap_to_buffer( m_loop_start - play_head_to_write_head_buffer_size() );
    return extended_start;
}

int PLAY_HEAD::current_loop_size() const
{
    if( m_loop_end > m_loop_start )
    {
        return m_loop_end - m_loop_start;
    }
    else
    {
        return ( m_delay_buffer.m_buffer_size_in_samples - m_loop_start ) + m_loop_end;
    }
}

bool PLAY_HEAD::looping() const
{
    if( m_loop_start < 0 )
    {
        ASSERT_MSG( m_loop_end < 0, "Inconsistent loop state" );
        return false;
    }
    
    return true;
}

bool PLAY_HEAD::position_inside_section( int position, int start, int end ) const
{
    if( end < start )
    {
        // current and destination are wrapped around
        if( position >= start || position <= end )
        {
            return true;
        }
    }
    else if( position >= start && position <= end )
    {
        return true;
    }
    
    return false;
}

bool PLAY_HEAD::position_inside_next_read( int position, int read_size ) const
{
    // standard delay (or crossfading into a loop)
    if( m_loop_end < 0 || (!m_initial_loop_crossfade_complete && m_fade_samples_remaining > 0) )
    {
        // cross-fading
        if( play_forwards() )
        {
            if( m_current_play_head != m_destination_play_head )
            {
                const int fade_read_size = min_val<int>( read_size, m_fade_samples_remaining ) - 1; // read_size -1 because if 1 sample is read start == end
                
                const int current_cf_end = m_delay_buffer.wrap_to_buffer( m_current_play_head + fade_read_size );
                if( position_inside_section( position, m_current_play_head, current_cf_end ) )
                {
                    // inside the cross fade from current to destination
                    return true;
                }
                
                const int destination_end = m_delay_buffer.wrap_to_buffer( m_destination_play_head + read_size - 1 ); // after fading, destination will become current, read_size samples will be read
                if( position_inside_section( position, m_destination_play_head, destination_end ) )
                {
                    // inside the cross fade from current to destination
                    return true;
                }
            }
            else
            {
                // not cross-fading
                const int read_end = m_delay_buffer.wrap_to_buffer( m_current_play_head + read_size - 1);
                if( position_inside_section( position, m_current_play_head, read_end ) )
                {
                    return true;
                }
            }
        }
        else
        {
            if( m_current_play_head != m_destination_play_head )
            {
                const int fade_read_size = min_val<int>( read_size, m_fade_samples_remaining ) - 1; // read_size -1 because if 1 sample is read start == end
                
                const int current_cf_start = m_delay_buffer.wrap_to_buffer( m_current_play_head - fade_read_size );
                if( position_inside_section( position, current_cf_start, m_current_play_head ) )
                {
                    // inside the cross fade from current to destination
                    return true;
                }
                
                const int destination_end = m_delay_buffer.wrap_to_buffer( m_destination_play_head - read_size - 1 ); // after fading, destination will become current, read_size samples will be read
                if( position_inside_section( position, destination_end, m_destination_play_head ) )
                {
                    // inside the cross fade from current to destination
                    return true;
                }
            }
            else
            {
                // not cross-fading
                const int read_end = m_delay_buffer.wrap_to_buffer( m_current_play_head - read_size - 1);
                if( position_inside_section( position, read_end, m_current_play_head ) )
                {
                    return true;
                }
            }
        }
    }
    // otherwise looping
    else
    {
        ASSERT_MSG( play_forwards(), "Loop not supported playing forwards" );
        
        // NOTE this tests entire loop NOT next read per-se
        const int loop_end_cf_end = m_delay_buffer.wrap_to_buffer( m_loop_end + FIXED_FADE_TIME_SAMPLES - 1 );
        if( position_inside_section( position, m_loop_start, loop_end_cf_end ) )
        {
            return true;
        }
        /*
         // cross-fading
         if( m_current_play_head != m_destination_play_head )
         {
         
         }
         
         const int loop_end_cf_end = m_delay_buffer.wrap_to_buffer( m_loop_end + FIXED_FADE_TIME_SAMPLES - 1 );
         int samples_left_of_loop( 0 );
         if( loop_end_cf_end > m_loop_start )
         {
         samples_left_of_loop = loop_end_cf_end - m_destination_play_head;
         }
         else
         {
         samples_left_of_loop = ( m_delay_buffer.m_buffer_size_in_samples - m_destination_play_head ) + loop_end_cf_end;
         }
         const int samples_to_read = min( read_size, samples_left_of_loop );
         const int read_end = m_delay_buffer.wrap_to_buffer( m_destination_play_head + samples_to_read );
         if( position_inside_section( position, m_destination_play_head, read_end ) )
         {
         // inside the cross fade from current to destination
         return true;
         }
         */
    }
    
    return false;
}

bool PLAY_HEAD::initial_loop_crossfade_complete() const
{
    return m_initial_loop_crossfade_complete;
}

bool PLAY_HEAD::play_forwards() const
{
    return m_play_speed > 0.0f;
}

bool PLAY_HEAD::crossfade_active() const
{
    return m_current_play_head != m_destination_play_head;
}

void PLAY_HEAD::set_next_loop()
{
    ASSERT_MSG( play_forwards(), "Loop not supported playing forwards" );
    ASSERT_MSG( m_loop_start >= 0, "PLAY_HEAD::read_from_play_head() invalid loop start" );
    ASSERT_MSG( m_initial_loop_crossfade_complete, "looping before we've finished the cross-fade into the loop\n" );
    ASSERT_MSG( !crossfade_active(), "starting new loop whist still cross fading" );
    
    // set next loop parameters
    float r                               = (random(1000) / 1000.0f) * 0.25f;
    r                                     = 1.0f + ( r - 0.125f ); // r = 0.875 => 1.125
    
    const int loop_size                   = round( lerp<float>( MIN_LOOP_SIZE_IN_SAMPLES, MAX_LOOP_SIZE_IN_SAMPLES, m_next_loop_size_ratio * r ) );
    
    if( m_next_shift_speed_ratio > 0.0f )
    {
        m_shift_speed                       = round( lerp<float>( MIN_SHIFT_SPEED, MAX_SHIFT_SPEED, m_next_shift_speed_ratio * r ) );
    }
    else
    {
        m_shift_speed                       = 0;
        
        r                                   = (random(1000) / 1000.0f);
        r                                   -= 0.5f; // r = -0.5 => 0.5
        int jitter_offset                   = MAX_JITTER_SIZE * r * m_jitter_ratio;
        
        m_loop_start                        = m_delay_buffer.wrap_to_buffer( m_unjittered_loop_start + jitter_offset );
    }
    
    m_loop_end                           = m_delay_buffer.wrap_to_buffer( m_loop_start + loop_size );
    
    ASSERT_MSG( current_loop_size() == loop_size, "Error in loop size calculation" );
    
    // check whether the write head is about to run over the read head, in which case cross fade read head to new position
    if( position_inside_section( m_delay_buffer.write_head(), buffered_loop_start(), m_loop_end ) )
    {
        set_loop_behind_write_head();
    }
    
    set_play_head( m_loop_start );
}

int16_t PLAY_HEAD::read_sample_with_cross_fade()
{
    ASSERT_MSG( m_fade_samples_remaining >= 0, "PLAY_HEAD::read_sample_with_cross_fade()" );
    
    int16_t sample(0);
    
    // cross-fading
    if( m_fade_samples_remaining > 0 )
    {
        int16_t current_sample            = m_delay_buffer.read_sample_with_speed( m_current_play_head, m_play_speed );
        
        int16_t destination_sample        = m_delay_buffer.read_sample_with_speed( m_destination_play_head, m_play_speed );
        
        const float t                     = static_cast<float>(m_fade_samples_remaining) / FIXED_FADE_TIME_SAMPLES; // t=0 at destination, t=1 at current
        --m_fade_samples_remaining;
        
        sample                            = cross_fade_samples( destination_sample, current_sample, t );
        
        m_delay_buffer.increment_head( m_current_play_head, m_play_speed );
        m_delay_buffer.increment_head( m_destination_play_head, m_play_speed );
    }
    // not cross-fading
    else
    {
        m_initial_loop_crossfade_complete = true;
        
        m_current_play_head               = m_destination_play_head;
        sample                            = m_delay_buffer.read_sample( m_current_play_head );
        
        m_delay_buffer.increment_head( m_current_play_head, m_play_speed );
        m_destination_play_head           = m_current_play_head;
    }
    
    return sample;
}

void PLAY_HEAD::set_loop_size( float loop_size_ratio )
{
    m_next_loop_size_ratio = loop_size_ratio;
}

void PLAY_HEAD::set_shift_speed( float speed )
{
    m_next_shift_speed_ratio = speed;
}

void PLAY_HEAD::set_jitter( float jitter )
{
    m_jitter_ratio = jitter;
}

void PLAY_HEAD::set_play_head( int new_play_head )
{
    // already at this offset (or currently fading to it)
    if( new_play_head == m_destination_play_head )
    {
        return;
    }
    
    // currently cross fading
    if( m_current_play_head != m_destination_play_head )
    {
        return;
    }
    
    m_destination_play_head       = new_play_head;
    
    m_fade_samples_remaining      = FIXED_FADE_TIME_SAMPLES;
}

void PLAY_HEAD::set_loop_behind_write_head()
{
    if( looping() )
    {
        const int loop_size                     = current_loop_size();
        int loop_end                            = m_delay_buffer.write_head() - ( play_head_to_write_head_buffer_size() + m_shift_speed );
        loop_end                                = m_delay_buffer.wrap_to_buffer( loop_end );
        const int loop_start                    = m_delay_buffer.wrap_to_buffer( loop_end - loop_size );
        
        ASSERT_MSG( loop_size + FIXED_FADE_TIME_SAMPLES + 1 < DELAY_BUFFER_SIZE_IN_BYTES, "Loop size too large\n" );
        ASSERT_MSG( loop_size > FIXED_FADE_TIME_SAMPLES * 2, "Loop size too small\n" );
        
        enable_loop( loop_start, loop_end );
    }
    else
    {
        int position                           = m_delay_buffer.write_head() - ( play_head_to_write_head_buffer_size() + m_shift_speed );
        m_destination_play_head                = m_delay_buffer.wrap_to_buffer( position );
        m_current_play_head                    = m_destination_play_head;
        m_fade_samples_remaining               = FIXED_FADE_TIME_SAMPLES;
    }
}

void PLAY_HEAD::read_from_play_head( int16_t* dest, int size )
{
    for( int x = 0; x < size; ++x )
    {
        if( m_loop_end >= 0  && !position_inside_section( m_destination_play_head, m_loop_start, m_loop_end ) )
        {
            set_next_loop();
        }
        
        dest[x] = read_sample_with_cross_fade();
    }
    
    if( m_shift_speed > 0 && !crossfade_active() )
    {
        m_loop_start      = m_delay_buffer.wrap_to_buffer( m_loop_start + m_shift_speed );
        m_loop_end        = m_delay_buffer.wrap_to_buffer( m_loop_end + m_shift_speed );
    }
}

void PLAY_HEAD::enable_loop( int start, int end )
{
    ASSERT_MSG( play_forwards(), "Looping only currently supported on playing forwards" );
    
    m_loop_start            = start;
    m_loop_end              = end;
    m_unjittered_loop_start = start;
    
    m_initial_loop_crossfade_complete = false;
    
    // force a new cross fade
    m_destination_play_head           = m_loop_start;
    m_fade_samples_remaining          = FIXED_FADE_TIME_SAMPLES;
}


void PLAY_HEAD::disable_loop()
{
    m_loop_start                      = -1;
    m_loop_end                        = -1;
}

#ifdef DEBUG_OUTPUT
void PLAY_HEAD::debug_output()
{
    DEBUG_TEXT("PLAY_HEAD current:");
    DEBUG_TEXT(m_current_play_head);
    DEBUG_TEXT(" destination:");
    DEBUG_TEXT(m_destination_play_head);
    DEBUG_TEXT(" loop start:");
    DEBUG_TEXT(m_loop_start);
    DEBUG_TEXT(" loop end:");
    DEBUG_TEXT(m_loop_end);
    DEBUG_TEXT(" fade samples:");
    DEBUG_TEXT(m_fade_samples_remaining);
    if( !m_initial_loop_crossfade_complete )
    {
        DEBUG_TEXT(" INITIAL CF");
    }
    DEBUG_TEXT("\n");
}
#endif

/////////////////////////////////////////////////////////////////////

DELAY_BUFFER::DELAY_BUFFER() :
    m_buffer(),
    m_buffer_size_in_samples(0),
    m_sample_size_in_bits(0),
    m_write_head(0),
    m_fade_samples_remaining(0),
	m_freeze_active(false)
{
    set_bit_depth( 16 );
}

int DELAY_BUFFER::position_offset_from_head( int offset ) const
{
    ASSERT_MSG( offset >= 0 && offset < m_buffer_size_in_samples - 1, "DELAY_BUFFER::position_offset_from_head()" );
    
    int position = wrap_to_buffer( m_write_head - offset );
    
    ASSERT_MSG( position >= 0 && position < m_buffer_size_in_samples, "DELAY_BUFFER::position_offset_from_head()" );
    return position;
}

int DELAY_BUFFER::delay_offset_from_ratio( float ratio_of_max_delay ) const
{
    int offset = trunc_to_int( ratio_of_max_delay * ( m_buffer_size_in_samples - AUDIO_BLOCK_SAMPLES ) );
    ASSERT_MSG( offset >= 0 && offset <= m_buffer_size_in_samples - AUDIO_BLOCK_SAMPLES, "DELAY_BUFFER::delay_offset_from_ratio()" );
    return offset;
}

int DELAY_BUFFER::delay_offset_from_time( int time_in_ms ) const
{
    int offset   = convert_time_in_ms_to_samples( time_in_ms );
    
    if( offset > m_buffer_size_in_samples - AUDIO_BLOCK_SAMPLES )
    {
        offset = m_buffer_size_in_samples - AUDIO_BLOCK_SAMPLES;
    }
    
    ASSERT_MSG( offset >= 0 && offset <= m_buffer_size_in_samples - AUDIO_BLOCK_SAMPLES, "DELAY_BUFFER::delay_offset_from_time()" );
    return offset;
}

int DELAY_BUFFER::write_head() const
{
    return m_write_head;
}

int DELAY_BUFFER::wrap_to_buffer( int position ) const
{
    if( position < 0 )
    {
        return m_buffer_size_in_samples + position;
    }
    else if( position >= m_buffer_size_in_samples )
    {
        return position - m_buffer_size_in_samples;
    }
    
    return position;
}

bool DELAY_BUFFER::write_buffer_fading_in() const
{
    return m_fade_samples_remaining > 0;
}

void DELAY_BUFFER::write_sample( int16_t sample, int index )
{
    ASSERT_MSG( index >= 0 && index < m_buffer_size_in_samples, "DELAY_BUFFER::write_sample() writing outside buffer" );
    
    switch( m_sample_size_in_bits )
    {
        case 8:
        {
            int8_t sample8                          = (sample >> 8) & 0x00ff;
            int8_t* sample_buffer                   = reinterpret_cast<int8_t*>(m_buffer);
            sample_buffer[ index ]                  = sample8;
            break;
        }
        case 12:
        {
            int8_t* sample_buffer                   = reinterpret_cast<int8_t*>(m_buffer);
            
            const int offset_index                  = static_cast<int>( index * 1.5f );
            
            ASSERT_MSG( offset_index + 1 < DELAY_BUFFER_SIZE_IN_BYTES, "Buffer overrun" );
            
            if( index & 1 )
            {
                // odd indices
                const uint8_t prev_byte                  = sample_buffer[ offset_index ];
                sample_buffer[ offset_index ]            = ( (sample & 0xf000) >> 12 ) | (prev_byte & 0x00f0);
                sample_buffer[ offset_index + 1 ]        = (( sample & 0x0ff0 ) >> 4);
            }
            else
            {
                // even indices
                sample_buffer[ offset_index ]            = (sample >> 8);
                const uint8_t prev_byte                  = sample_buffer[ offset_index + 1 ];
                sample_buffer[ offset_index + 1 ]        = ( sample & 0x00f0 ) | ( prev_byte & 0x000f );
                
                // read sample asserts if you read at the write head ASSERT_MSG( abs( read_sample( index ) - sample ) < 16, "EVEN 12 bit converison failure" );
            }
            
            break;
        }
        case 16:
        {
            int16_t* sample_buffer                  = reinterpret_cast<int16_t*>(m_buffer);
            sample_buffer[ index ]                  = sample;
            break;
        }
    }
}

int16_t DELAY_BUFFER::read_sample( int index ) const
{
    ASSERT_MSG( index >= 0 && index < m_buffer_size_in_samples, "DELAY_BUFFER::read_sample() writing outside buffer" );
    //ASSERT_MSG( index != m_write_head, "Reading from the write head position, expect a glitch" );
    
    switch( m_sample_size_in_bits )
    {
        case 8:
        {
            const int8_t* sample_buffer    = reinterpret_cast<const int8_t*>(m_buffer);
            const int8_t sample            = sample_buffer[ index ];
            
            int16_t sample16               = sample;
            sample16                       <<= 8;
            
            return sample16;
        }
        case 12:
        {
            const int8_t* sample_buffer    = reinterpret_cast<const int8_t*>(m_buffer);
            
            const int offset_index         = static_cast<int>( index * 1.5f );
            
            if( index & 1 )
            {
                // odd indices
                const uint8_t top           = sample_buffer[offset_index] & 0x000f;
                const uint8_t bottom        = sample_buffer [offset_index + 1 ];
                return ( (top << 12) | (bottom << 4) );
            }
            else
            {
                // even indices
                const uint8_t top           = sample_buffer[offset_index];
                const uint8_t bottom        = sample_buffer [offset_index + 1 ] & 0x00f0;
                return ( (top << 8) | bottom );
            }
            
            break;
        }
        case 16:
        {
            const int16_t* sample_buffer    = reinterpret_cast<const int16_t*>(m_buffer);
            const int16_t sample            = sample_buffer[ index ];
            return sample;
        }
    }
    
    return 0;
}

int16_t DELAY_BUFFER::read_sample_with_speed( float index, float speed ) const
{
    if( speed < 1.0f )
    {
        float nif = index;
        increment_head( nif, speed );
        
        int curr_index = index;
        int next_index = nif;
        
        if( curr_index == next_index )
        {
            // both current and next are in the same sample
            return read_sample( curr_index );
        }
        else
        {
            // crossing 2 samples - calculate how much of each sample to use, then lerp between them
            // use the fractional part - if 0.3 'into' next sample, then we mix 0.3 of next and 0.7 of current
            float int_part;
            float rem             = modff( nif, &int_part );
            const float t         = rem / speed;
            return lerp( read_sample(curr_index), read_sample(next_index), t );
        }
    }
    else
    {
        return read_sample( index );
    }
}

void DELAY_BUFFER::increment_head( int& head ) const
{
    ++head;
    
    if( head >= m_buffer_size_in_samples )
    {
        head = 0;
    }
}

void DELAY_BUFFER::increment_head( float& head, float speed ) const
{
    head                        += speed;
    if( head >= m_buffer_size_in_samples )
    {
        const float rem         = head - m_buffer_size_in_samples;
        head                    = rem;
    }
    else if( head < 0.0f )
    {
        head                    = m_buffer_size_in_samples + head;
    }
    
    ASSERT_MSG( truncf(head) >= 0 && truncf(head) < m_buffer_size_in_samples, "DELAY_BUFFER::increment_head()" );
    
}

void DELAY_BUFFER::write_to_buffer( const int16_t* source, int size )
{
    ASSERT_MSG( m_write_head >= 0 && m_write_head < m_buffer_size_in_samples, "GLITCH_DELAY_EFFECT::write_to_buffer()" );
	
	if( m_freeze_active )
	{
		return;
	}
	
    for( int x = 0; x < size; ++x )
    {
        // fading in the write head
        if( m_fade_samples_remaining > 0 )
        {
            int16_t old_sample       = read_sample( m_write_head );
            int16_t new_sample       = source[x];
            
            const float t            = static_cast<float>(m_fade_samples_remaining) / FIXED_FADE_TIME_SAMPLES; // t=1 at old t=0 at new
            --m_fade_samples_remaining;
            
            int16_t cf_sample         = cross_fade_samples( new_sample, old_sample, t );
            
            write_sample( cf_sample, m_write_head );
        }
        else
        {
            write_sample( source[x], m_write_head );
        }
        
        // increment write head
        increment_head( m_write_head );
    }
}

void DELAY_BUFFER::set_bit_depth( int sample_size_in_bits )
{
    // NOTE - do not print in this function, it is called before Serial is configured
    if( sample_size_in_bits != m_sample_size_in_bits )
    {
        m_sample_size_in_bits       = sample_size_in_bits;
        m_buffer_size_in_samples    = delay_buffer_size_in_samples( m_sample_size_in_bits );
        
        m_write_head                = 0;
        
        memset( m_buffer, 0, sizeof(m_buffer) );
    }
}

bool DELAY_BUFFER::freeze_active() const
{
	return m_freeze_active;
}

void DELAY_BUFFER::set_freeze( bool freeze )
{
	if( m_freeze_active != freeze && m_fade_samples_remaining == 0 )
	{
		if( m_freeze_active )
		{
			// starting to write again, so fade in the new audio over the old
			fade_in_write();
			
			m_freeze_active = false;
		}
		else
		{
			m_freeze_active = true;
		}
	}
}

void DELAY_BUFFER::fade_in_write()
{
    ASSERT_MSG( m_fade_samples_remaining == 0, "DELAY_BUFFER::fade_in_write() trying to start a fade during a fade" );
    m_fade_samples_remaining = FIXED_FADE_TIME_SAMPLES;
}

#ifdef DEBUG_OUTPUT
void DELAY_BUFFER::debug_output()
{
    DEBUG_TEXT("DELAY_BUFFER write head:");
    DEBUG_TEXT(m_write_head);
    DEBUG_TEXT(" fade samples:");
    DEBUG_TEXT(m_fade_samples_remaining);
    DEBUG_TEXT(" bit depth:");
    DEBUG_TEXT(m_sample_size_in_bits);
    DEBUG_TEXT("\n");
}
#endif

/////////////////////////////////////////////////////////////////////

GLITCH_DELAY_EFFECT::GLITCH_DELAY_EFFECT() :
  m_delay_buffer(),
  m_play_heads( { PLAY_HEAD( m_delay_buffer, 0.5f ), PLAY_HEAD( m_delay_buffer, 1.0f ), PLAY_HEAD( m_delay_buffer, 2.0f ), PLAY_HEAD( m_delay_buffer, -1.0f ) } ),
  m_loop_size_ratio(),
	m_jitter_ratio(),
  m_loop_moving(true),
  m_next_sample_size_in_bits(12),
  m_next_loop_moving(true),
  m_next_beat(false),
	m_next_freeze_active(false)
{
	for( int i = 0; i < NUM_PLAY_HEADS; ++ i )
	{
		m_loop_size_ratio[i]	= 0.0f;
		m_jitter_ratio[i]		= 0.0f;
	}
}

void GLITCH_DELAY_EFFECT::process_audio_in_impl( int channel, const int16_t* sample_data, int num_samples )
{
    ASSERT_MSG( channel == 0, "Only mono input supported" );
	
    m_delay_buffer.write_to_buffer( sample_data, num_samples );
}

void GLITCH_DELAY_EFFECT::process_audio_out_impl( int channel, int16_t* sample_data, int num_samples )
{
    ASSERT_MSG( !m_play_heads[channel].position_inside_next_read( m_delay_buffer.write_head(), num_samples ), "Non - reading over write buffer\n" ); // position after write head is OLD DATA
 
	m_play_heads[channel].read_from_play_head( sample_data, num_samples );
}

int GLITCH_DELAY_EFFECT::num_input_channels() const
{
    return 1;
}

int GLITCH_DELAY_EFFECT::num_output_channels() const
{
    return NUM_PLAY_HEADS;
}

void GLITCH_DELAY_EFFECT::update()
{
    static int num_updates(0);
    ++num_updates;
    
    m_delay_buffer.set_bit_depth( m_next_sample_size_in_bits );
	m_delay_buffer.set_freeze( m_next_freeze_active );
	
    m_loop_moving               = m_next_loop_moving;
    
    for( int pi = 0; pi < NUM_PLAY_HEADS; ++pi )
    {
        PLAY_HEAD& play_head = m_play_heads[pi];
        if( m_loop_moving )
        {
            play_head.set_shift_speed( m_jitter_ratio[pi] ); // TODO remove this mode?
        }
        else
        {
            play_head.set_shift_speed( 0.0f );
            play_head.set_jitter( m_jitter_ratio[pi] );
        }
        
        play_head.set_loop_size( m_loop_size_ratio[pi] );
        
        if( m_next_beat && play_head.play_forwards() && !play_head.crossfade_active() ) // let the reverse head play regardless of beats
        {
            play_head.set_next_loop();
            play_head.set_loop_behind_write_head();
        }
        else
        {
            // TODO Move to PLAY_HEAD object
            // check whether the write head is about to run over the read head, in which case cross fade read head to new position
            if( play_head.looping() )
            {
                if( play_head.position_inside_section( m_delay_buffer.write_head(), play_head.buffered_loop_start(), play_head.loop_end() ) )
                {
                    play_head.set_loop_behind_write_head();
                }
            }
            else
            {
                if( play_head.position_inside_next_read( m_delay_buffer.write_head(), AUDIO_BLOCK_SAMPLES * 2 ) )
                {
                    play_head.set_loop_behind_write_head();
                }
            }
        }
    }
    m_next_beat = false;
    
    // read in on channel 0
    process_audio_in( 0 );
    
    // write out all the playheads
    for( int pi = 0; pi < NUM_PLAY_HEADS; ++pi )
    {
        process_audio_out( pi );
    }
}

void GLITCH_DELAY_EFFECT::set_bit_depth( int sample_size_in_bits )
{
    m_next_sample_size_in_bits = sample_size_in_bits;
    //set_bit_depth_impl( sample_size_in_bits );
}

void GLITCH_DELAY_EFFECT::set_loop_moving( bool moving )
{
    m_next_loop_moving = moving;
}

void GLITCH_DELAY_EFFECT::set_loop_size( int play_head, float loop_size )
{
	ASSERT_MSG( play_head < NUM_PLAY_HEADS, "Invalid play head index" );
	m_loop_size_ratio[play_head] = loop_size;
}

void GLITCH_DELAY_EFFECT::set_jitter( int play_head, float jitter )
{
	ASSERT_MSG( play_head < NUM_PLAY_HEADS, "Invalid play head index" );
	m_jitter_ratio[play_head] = jitter;
}

void GLITCH_DELAY_EFFECT::set_beat()
{
    m_next_beat = true;
}

void GLITCH_DELAY_EFFECT::set_freeze_active( bool active )
{
	m_next_freeze_active = active;
}

int GLITCH_DELAY_EFFECT::num_heads() const
{
    return NUM_PLAY_HEADS + 1; // + 1 for write head
}

void GLITCH_DELAY_EFFECT::head_ratio_details( int head, float& loop_start, float& loop_end, float& current_position ) const
{
    auto convert_12_bit_sample_to_ratio = []( int sample_index ) -> float
    {
        const float ratio = ( sample_index * 1.5f ) / DELAY_BUFFER_SIZE_IN_BYTES;
        return ratio;
    };
    
    if( head < NUM_PLAY_HEADS )
    {
        const PLAY_HEAD& play_head = m_play_heads[head];
        
        if( play_head.loop_start() >= 0 )
        {
            loop_start      = convert_12_bit_sample_to_ratio( play_head.loop_start() );
            loop_end        = convert_12_bit_sample_to_ratio( play_head.loop_end() );
        }
        else
        {
            loop_start      = 0;
            loop_end        = 0;
        }
        current_position    = convert_12_bit_sample_to_ratio( play_head.current_position() );
    }
    else if( head == NUM_PLAY_HEADS )
    {
        loop_start          = 0;
        loop_end            = 0;
        current_position    = convert_12_bit_sample_to_ratio( m_delay_buffer.write_head() );
    }
    else
    {
        DEBUG_TEXT( "Invalid head" );
    }
}

