#include "Audio.h"

using namespace flan; 
using namespace std::ranges;

#include <ranges>

#include "flan/Utility/iota_iter.h"
#include "flan/DSPUtility.h"
#include "flan/FFTHelper.h"

//================================================================================================================
// Helper Functions
//================================================================================================================

// If sample rates all match, return empty vec, else resample everything to highest sr
std::vector<Audio> Audio::match_sample_rates_or_return_null( const std::vector<const Audio *> & ins )
	{
	if( ins.empty() ) return std::vector<Audio>();

	const FrameRate max_sr = (*std::max_element( ins.begin(), ins.end(), 
		[]( const Audio * a, const Audio * b ){ return a->get_sample_rate() < b->get_sample_rate(); } ))->get_sample_rate();

	bool all_match = true;
	for( const Audio * in : ins ) 
		if( in->get_sample_rate() != max_sr )
			all_match = false;
	if( all_match == true ) return std::vector<Audio>();

	std::vector<Audio> out;
	for( const auto & in : ins )
		out.emplace_back( in->resample( max_sr ) );

	return out;
	}

static bool do_channel_counts_match( const std::vector<Audio> & ins )
	{
	// Check if all channel counts match the first one
	if( ins.empty() ) return true;
	const Channel numchannels = ins[0].get_num_channels();
	for( auto & in : ins ) 
		if( in.get_num_channels() != numchannels )
			return false;
	return true;
	}

static Channel get_max_num_channels( const std::vector<const Audio *> & ins )
	{
	const auto maxNumChannelsIter = std::max_element( ins.begin(), ins.end(), []( const Audio * a, const Audio * b )
		{ 
		return a->get_num_channels() < b->get_num_channels();
		} );

	return (*maxNumChannelsIter)->get_num_channels();
	}

static Frame get_max_num_frames( const std::vector<Audio> & ins )
	{
	return std::max_element( ins.begin(), ins.end(), []( const Audio & a, const Audio & b )
		{ 
		return a.get_num_frames() < b.get_num_frames();
		} )->get_num_frames();
	}

template<typename T>
static std::vector<const T *> get_pointers( const std::vector<T> & ts )
	{
	std::vector<const T *> ptrs( ts.size() );
	std::transform( ts.begin(), ts.end(), ptrs.begin(), []( const T & t ){ return &t; } );
	return std::move( ptrs );
	}

//================================================================================================================
// Methods
//================================================================================================================

Audio Audio::mix( 
	std::vector<const Audio *> ins_unmatched, 
	std::vector<Second> start_times, 
	std::vector<Amplitude> amplitudes 
	)
	{
	// Input validation
	if( ins_unmatched.empty() ) return Audio::create_null();

	const std::vector<Audio> ins_resampled_container = match_sample_rates_or_return_null( ins_unmatched );
	std::vector<const Audio *> ins = ins_resampled_container.empty() ? ins_unmatched : get_pointers( ins_resampled_container );

	// If start_times or amplitudes ore too small, fill with 0 and 1
	for( auto _ : iota_view( start_times.size(), ins.size() ) ) start_times.push_back( 0 );
	for( auto _ : iota_view( amplitudes.size(),  ins.size() ) ) amplitudes .push_back( 1 );

	// Convert start times to frames
	std::vector<Frame> start_frames;
	transform( start_times, std::back_inserter( start_frames ), [&]( Second t ) -> Frame { 
		return ins[0]->time_to_frame( t ); 
		} );	

	// Output setup
	auto format = ins[0]->get_format();
	// Get the maximum number of channels among inputs
	format.num_channels = (*max_element( ins, less(), []( const Audio * p ) { return p->get_num_channels(); } ) )->get_num_channels();
	// Get the maximum of output sizes required by each input
	format.num_frames = 0;
	for( const Index i : iota_view( 0u, ins.size() ) ) {
		const Frame frames_needed_for_i = ins[i]->get_num_frames() + start_frames[i];
		format.num_frames = std::max( format.num_frames, frames_needed_for_i );
		}
	Audio out( format );
	out.clear_buffer();

	// For each input, write to output
	for( Index in = 0; in < ins.size(); ++in )
		{
		const Audio & me = *ins[in];
		for( Channel channel = 0; channel < me.get_num_channels(); ++channel )
			std::for_each( FLAN_PAR_UNSEQ iota_iter( 0 ), iota_iter( me.get_num_frames() ), [&]( Frame local_frame )
				{
				const Frame global_frame = local_frame + start_frames[in];
				if( 0 <= global_frame && global_frame < format.num_frames )
					out.get_sample( channel, global_frame ) += me.get_sample( channel, local_frame ) * amplitudes[in];
				} );
		}

	return out;
	}

Audio Audio::mix( 
	const std::vector<Audio> & ins, 
	std::vector<Second> start_times, 
	std::vector<Amplitude> amplitudes
	)
	{
	return mix( get_pointers( ins ), start_times, amplitudes );
	}

Audio Audio::mix_variable_gain( 
	std::vector<const Audio *> ins_unmatched, 
	std::vector<Second> start_times, 
	const std::vector<const Function<Second, Amplitude> *> & amplitudes 
	)
	{
	
	// Input validation
	if( ins_unmatched.empty() ) return Audio::create_null();

	std::vector<Audio> ins_resampled_container = match_sample_rates_or_return_null( ins_unmatched );
	std::vector<const Audio *> ins = ins_resampled_container.empty() ? ins_unmatched : get_pointers( ins_resampled_container );

	// If start_times is too small fill with 0
	for( auto _ : iota_view( start_times.size(), ins.size() ) )
		start_times.push_back( 0 );

	// Convert start times to frames
	std::vector<Frame> start_frames;
	transform( start_times, std::back_inserter( start_frames ), [&]( Second t ) -> Frame { 
		return ins[0]->time_to_frame( t ); 
		} );

	// Sample each function, if there aren't enough use constant 1
	// We only sample the gains when they are needed, but because these functions are relative to global time 0, we need to be 
	// careful later when accessing them
	std::vector<std::vector<Amplitude>> amplitude_samples;
	const Function<Second, Amplitude> level_func = 1;
	std::transform( iota_iter( 0 ), iota_iter( ins.size() ), std::back_inserter( amplitude_samples ), [&]( Index i ) {
		const Function<Second, Amplitude> * f = i < amplitudes.size() ? amplitudes[i] : &level_func;
		return f->sample( start_frames[i], start_frames[i] + ins[i]->get_num_frames(), ins[i]->frame_to_time( 1 ) );
		} );

	// Output setup
	auto format = ins[0]->get_format();
	// Get the maximum number of channels among inputs
	format.num_channels = (*max_element( ins, less(), []( const Audio * p ) { return p->get_num_channels(); } ) )->get_num_channels();
	// Get the maximum of output sizes required by each input
	format.num_frames = 0;
	for( const Index i : iota_view( 0u, ins.size() ) ) {
		const Frame frames_needed_for_i = ins[i]->get_num_frames() + start_frames[i];
		format.num_frames = std::max( format.num_frames, frames_needed_for_i );
		}
	Audio out( format );
	out.clear_buffer();

	// For each input, write to output
	for( Index in = 0; in < ins.size(); ++in )
		{
		const Audio & me = *ins[in];
		for( Channel channel = 0; channel < me.get_num_channels(); ++channel )
			std::for_each( FLAN_PAR_UNSEQ iota_iter( 0 ), iota_iter( me.get_num_frames() ), [&]( Frame local_frame )
				{
				const Frame global_frame = local_frame + start_frames[in];
				if( 0 <= global_frame && global_frame < format.num_frames ) {
					const float gain = amplitude_samples[in][local_frame];
					out.get_sample( channel, global_frame ) += me.get_sample( channel, local_frame ) * gain;
					}
				} );
		}

	return out;
	}

Audio Audio::mix_variable_gain( 
	const std::vector<Audio> & ins, 
	const std::vector<Second> & start_times, 
	const std::vector<Function<Second, Amplitude>> & amplitudes 
	)
	{
	return mix_variable_gain( get_pointers( ins ), start_times, get_pointers( amplitudes ) );
	}

Audio& Audio::mix_in_place( 
	const Audio & other, 
	Second start_time, 
	const Function<Second, Amplitude> & other_amplitude 
	)
	{
	const Audio resampled =	get_sample_rate() == other.get_sample_rate() ? Audio::create_null() : other.resample( get_sample_rate() );
	const Audio * sr_correct_source = get_sample_rate() == other.get_sample_rate() ? &other : &resampled;

	const Channel num_channels = std::min( get_num_channels(), sr_correct_source->get_num_channels() );

	auto other_amplitude_sampled = sr_correct_source->sample_function_over_domain( other_amplitude );

	for( Channel channel = 0; channel < num_channels; ++channel )
		for( Frame other_frame = 0; other_frame < sr_correct_source->get_num_frames(); ++other_frame )
			{
			const Frame this_frame = time_to_frame( start_time ) + other_frame;
			if( this_frame < 0 ) continue;
			if( get_num_frames() <= this_frame ) break;
			get_sample( channel, this_frame ) += sr_correct_source->get_sample( channel, other_frame ) * other_amplitude_sampled[other_frame];
			}
	return *this;
	}

Audio Audio::join( 
	const std::vector<const Audio *> & ins, 
	Second offset 
	)
	{
	
	if( ins.empty() ) return Audio::create_null();

	// Get input Audio lengths
	std::vector<Second> jumps( { 0 } );
	transform( ins, std::back_inserter( jumps ), []( const Audio * a ){ return a->get_length(); } );

	// Sum jumps to get mix positions
	std::vector<Second> start_times;
	std::partial_sum( jumps.begin(), jumps.end() - 1, std::back_inserter( start_times ), [&]( Second a, Second b ){ 
		return a + b + offset; 
		} );

	return mix( ins, start_times, {} );
	}

Audio Audio::join( 
	const std::vector<Audio> & ins, 
	Second offset 
	)
	{
	return join( get_pointers( ins ), offset );
	}

Audio Audio::select( 
	const std::vector<const Audio *> & ins, 
	const Function<Second, float> & selection, 
	const std::vector<Second> & start_times 
	)
	{
	// Generate balances from selection
	std::vector<Function<Second, Amplitude>> balances;
	for( Index i = 0; i < ins.size(); ++i )
		{
		balances.emplace_back( Function<Second, Amplitude>( [&selection, i]( Second t )
			{
			const float distance = std::abs( selection( t ) - i );
			if( distance >= 1 ) return 0.0f;
			else return std::sqrt( 1.0f - distance );
			}, selection.get_execution_policy() ) );
		}
		
	return mix_variable_gain( ins, start_times, get_pointers( balances ) );
	}

Audio Audio::select( 
	const std::vector<Audio> & ins, 
	const Function<Second, float> & selection, 
	std::vector<Second> start_times 
	)
	{
	return select( get_pointers( ins ), selection, start_times );
	}

// Audio Audio::convolve( const Function<Second, std::vector<float>> & ir ) const
// 	{
// 	if( is_null() ) return Audio::create_null();

// 	auto irSamples = ir.sample( 0, get_num_frames(), frame_to_time( 1 ) );
// 	const size_t maxIrSize = max_element( irSamples, less(), []( const std::vector<float> & v ){ return v.size(); } )->size();

// 	auto format = get_format();
// 	format.num_frames = get_num_frames() + maxIrSize; // This may be more frames than needed if the ir size changes
// 	Audio out( format );

// 	for( Channel channel = 0; channel < out.get_num_channels(); ++channel )
// 		std::for_each( std::execution::par_unseq, iota_iter( 0 ), iota_iter( out.get_num_frames() ), [&]( Frame out_frame )
// 			{
// 			const auto ir_c = irSamples[out_frame];

// 			Sample convolution_sum = 0;
// 			for( Frame ir_frame = 0; ir_frame < ir_c.size(); ++ir_frame )
// 				{
// 				const Frame in_frame = out_frame + 1 - ir_frame;
// 				if( 0 <= in_frame && in_frame < get_num_frames() )
// 					convolution_sum += ir_c[ir_frame] * get_sample( channel, in_frame );
// 				}

// 			out.set_sample( channel, out_frame, convolution_sum );
// 			} );

// 	return out;
// 	}

Audio Audio::convolve( 
	const Audio & ir,
	bool normalize
	) const
	{
	if( is_null() ) return Audio::create_null();
	if( ir.is_null() ) return Audio::create_null();

	const Audio resampled =	get_sample_rate() == ir.get_sample_rate() ? Audio::create_null() : ir.resample( get_sample_rate() );
	const Audio * sr_correct_ir = get_sample_rate() == ir.get_sample_rate() ? &ir : &resampled;

	Audio::Format format;
	format.num_channels = get_num_channels();
	format.num_frames = get_num_frames() + sr_correct_ir->get_num_frames();
	format.sample_rate = get_sample_rate();
	Audio out( format );

	const Frame dft_size = 2 * power_of_2_container( std::max( get_num_frames(), sr_correct_ir->get_num_frames() ) );

	FFTHelper fft( dft_size, true, true, false );

	for( Channel channel = 0; channel < get_num_channels(); ++channel )
		{
		// Get fft of this
		std::fill( fft.real_begin(), fft.real_end(), 0 );
		for( Frame frame = 0; frame < get_num_frames(); ++frame )
			fft.get_real_buffer()[frame] = get_sample( channel, frame ) / std::sqrt( dft_size );
		fft.r2c_execute();
		std::vector<std::complex<float>> this_ffted( fft.complex_buffer_size() );
		std::copy( fft.complex_begin(), fft.complex_end(), this_ffted.begin() );

		// Get fft of ir, but leave it in the fft buffer
		std::fill( fft.real_begin(), fft.real_end(), 0 );
		for( Frame frame = 0; frame < sr_correct_ir->get_num_frames(); ++frame )
			fft.get_real_buffer()[frame] = sr_correct_ir->get_sample( channel % sr_correct_ir->get_num_channels(), frame ) / std::sqrt( dft_size );
		fft.r2c_execute();

		// Multiply spectrums
		for( Bin bin = 0; bin < fft.complex_buffer_size(); ++bin )
			fft.get_complex_buffer()[bin] *= this_ffted[bin];

		// Inverse fft and recover the convolution
		fft.c2r_execute();
		for( Frame frame = 0; frame < out.get_num_frames(); ++frame )
			out.get_sample( channel, frame ) = fft.get_real_buffer()[frame];
		}

	if( normalize )
		{
		const Sample max_sample_mag = out.get_max_sample_magnitude();
		out.modify_volume_in_place( 1.0f / max_sample_mag ); 
		}

	return out;
	}