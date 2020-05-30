/* TODO:

Compatibility:
	check saving and loading on a big endian system

Additions:
	wavetable synth
	additive synth
	more filters
	add Func1x1 spline generator

Improvements:
	optimize cpu audio/pvoc conversions
	sort PVOC data by frequency, see if output improves
	add interpolation to gpu methods with interpolator sampling
	allow big widths in Audio::graph
	Add \mainpage with examples somewhere
	Add window function parameter in pvoc analysis, PVOCBuffer, and pvoc file io

Fixes:
	stretch 0 frame artifact
*/

#include <iostream>

#include <algorithm>

#include "xcdp/Audio.h"
#include "xcdp/PVOC.h"
#include "xcdp/Function.h"
#include "xcdp/Synthesis.h"

using namespace xcdp;

void play( const Audio & toPlay );
void graph( const std::string & f );

void main()
	{
	Func2x1 bar( []( float t, float f ){ return t * f; } );

	std::cout << bar( 2, 3 );
	//std::cout << s( 2, 3 );
	//Audio meow( "Audio/meow.wav" );
	//meow.convertToMono().save( "Audio/monoMeow.wav" );

	//PVOC meow( "Audio/meow.pvx" );
	//play( meow.convertToAudio().setVolume( .9 ) );
	}


#include <Windows.h>
#include <Mmsystem.h>
void play( const Audio & toPlay )
	{
	if( !toPlay.save( "TempFileSave.wav" ) )
        return;
	std::cout << "Playing sound ... \n";
	if( ! PlaySound("TempFileSave.wav", nullptr, SND_FILENAME) )
		std::cout << "Error playing sound\n";
	}

void graph( const std::string & f )
	{
	system( (std::string("start ") + f).c_str() );
	}