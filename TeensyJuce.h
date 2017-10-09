//
//  TeensyJuce.h
//  TeensyJuce
//
//  Created by Scott Pitkethly on 23/09/2017.
//
//

#ifndef TeensyJuce_h
#define TeensyJuce_h

#include <stdint.h>
#include "CompileSwitches.h"

#ifdef TARGET_TEENSY

#include <Audio.h>

class TEENSY_AUDIO_STREAM_WRAPPER : public AudioStream
{
    audio_block_t*                  m_input_queue_array[1];
    
protected:
    
    // these are the only functions that require bespoke Teensy code
    bool                            process_audio_in( int channel )
    {        
        audio_block_t* read_block        = receiveReadOnly();
        
        if( read_block != nullptr )
        {
            process_audio_in_impl( channel, read_block->data, AUDIO_BLOCK_SAMPLES );
            release( read_block );
            
            return true;
        }
        
        return false;
    }
    
    bool                            process_audio_out( int channel )
    {      
        audio_block_t* write_block = allocate();
        
        if( write_block != nullptr )
        {
            process_audio_out_impl( channel, write_block->data, AUDIO_BLOCK_SAMPLES );
            
            transmit( write_block, channel );
            
            release( write_block );
            
            return true;
        }
        
        return false;
    }
    
    // add audio processing code in these 2 functions
    virtual void                    process_audio_in_impl( int channel, const int16_t* sample_data, int num_samples ) = 0;
    virtual void                    process_audio_out_impl( int channel, int16_t* sample_data, int num_samples ) = 0;
    
public:
    
    TEENSY_AUDIO_STREAM_WRAPPER() :
        AudioStream( 1, m_input_queue_array ),
        m_input_queue_array()
    {
        
    }
    
    virtual ~TEENSY_AUDIO_STREAM_WRAPPER()      {;}
    
    virtual int                     num_input_channels() const = 0;
    virtual int                     num_output_channels() const = 0;
};

#endif // TARGET_TEENSY

#ifdef TARGET_JUCE

#include <vector>
#include "../JuceLibraryCode/JuceHeader.h"

class TEENSY_AUDIO_STREAM_WRAPPER
{
    int                             m_num_input_channels;
    int                             m_num_output_channels;
    
protected:
 
    // store the 16-bit in/out buffers
    typedef std::vector< int16_t >  SAMPLE_BUFFER;
    std::vector< SAMPLE_BUFFER >    m_channel_buffers;
    
    // these are the only functions that require bespoke JUCE code
    bool                            process_audio_in( int channel );
    bool                            process_audio_out( int channel );
    
    // add audio processing code in these 2 functions
    virtual void                    process_audio_in_impl( int channel, const int16_t* sample_data, int num_samples ) = 0;
    virtual void                    process_audio_out_impl( int channel, int16_t* sample_data, int num_samples ) = 0;
    
public:
    
    TEENSY_AUDIO_STREAM_WRAPPER();
    virtual ~TEENSY_AUDIO_STREAM_WRAPPER();
    
    
    void                            pre_process_audio( const AudioSampleBuffer& audio_in, int num_input_channels, int num_output_channels );
    void                            post_process_audio( AudioSampleBuffer& audio_out );
    
    virtual int                     num_input_channels() const = 0;
    virtual int                     num_output_channels() const = 0;
    
    virtual void                    update() = 0;
};

#endif // TARGET_JUCE

#endif /* TeensyJuce_h */
