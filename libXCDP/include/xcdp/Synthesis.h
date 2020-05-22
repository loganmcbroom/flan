#pragma once

#include "xcdp/Audio.h"
#include "xcdp/Function.h"

namespace xcdp {

namespace Synthesis {

/** Generate an Audio from a Func1x1.
 *  \param wave This is evaluated from o to 2pi. This portion is repeated to create the Audio.
 *  \param length The length of the output.
 *  \param freq The frequency of the output.
 *  \param sampleRate The sample rate of the output.
 *  \param oversample Synthesized audio must be generated at a sample rate higher than the desired sample rate to avoid aliasing.
 *      This describes how many samples should be used in the synthesis per sample in the output.
 */
Audio waveform( Func1x1 wave, float length, Func1x1 freq, size_t samplerate = 44100, size_t oversample = 16 );

/** Generate a sine wave Audio.
 *
 *  \param length The length of the output.
 *  \param freq The frequency of the output.
 */
Audio sine( float length, Func1x1 freq );

/** Generate a square wave Audio.
 *
 *  \param length The length of the output.
 *  \param freq The frequency of the output.
 */
Audio square( float length, Func1x1 freq );

/** Generate a saw wave Audio.
 *
 *  \param length The length of the output.
 *  \param freq The frequency of the output.
 */
Audio saw( float length, Func1x1 freq );

/** Generate a triangle wave Audio.
 *
 *  \param length The length of the output.
 *  \param freq The frequency of the output.
 */
Audio triangle( float length, Func1x1 freq );

}
}