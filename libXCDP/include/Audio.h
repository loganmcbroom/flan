#pragma once

#include <vector>
#include <string>
#include <functional>

#include "AudioBuffer.h"

namespace xcdp {

class PVOC;
struct RealFunc;

class Audio : public AudioBuffer
{
public:
	typedef const std::vector<Audio> & Vec;
	typedef std::function< Audio ( const Audio &, size_t ) > Mod;
	
	Audio() : AudioBuffer() {} //Only called to error out
	Audio( const AudioBuffer::Format & other ) : AudioBuffer( other ) {}
	Audio( const std::string & filename ) : AudioBuffer( filename ) {}

	//======================================================
	//	Conversions
	//======================================================

	PVOC convertToPVOC( const size_t frameSize = 2048, const size_t overlaps = 16 ) const;

	Audio convertToMidSide() const;
	Audio convertToLeftRight() const;

	//===========================================================================================
	//	Procs
	//===========================================================================================

	Audio monoToStereo() const;
	Audio modifyVolume( RealFunc volumeLevel ) const;
	Audio setVolume( RealFunc level ) const;
	Audio waveshape( RealFunc shaper ) const;
	Audio pan( RealFunc panAmount ) const;
	Audio widen( RealFunc widenAmount ) const;
	Audio iterate( size_t n, Audio::Mod mod = nullptr, bool fbIterate = false ) const;
	Audio reverse() const;
	Audio cut( double startTime, double endTime ) const;
	Audio repitch( RealFunc factor ) const;
	Audio convolve( const std::vector<double> & ) const;
	Audio delay( double delayTime, size_t numDelays, double decayAmount = .5, Audio::Mod mod = nullptr, bool fbIterate = true ) const;
	Audio fades( double fadeTime = .05 ) const;
	//Audio freeze( double freezeTime, double freezeLength, double delay, double rand = 0, Audio::Mod mod = nullptr ) const;
	// drunk walk / scramble
	// extend loop
	// extend repititions?

	//========================================================
	// Multi-In Procs
	//========================================================

	static Audio mix( Audio::Vec ins, 
		std::vector< RealFunc > balances = std::vector< RealFunc >(),
		std::vector< double > startTimes = std::vector<double>() );
	static Audio join( Audio::Vec ins );
};

} // End namespace xcdp
