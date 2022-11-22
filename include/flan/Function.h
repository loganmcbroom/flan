#pragma once

#include <functional>
#include <complex>
#include <type_traits>
#include <random>
#include <ctime>
#include <algorithm>
#include <concepts>

#include "flan/defines.h"
#include "flan/Utility/vec2.h"
#include "flan/Utility/Rect.h"

#undef max
#undef min
#undef clamp

namespace flan {

class Graph;

/** This is used as a base for all the vector (math vector) classes used throughout flan.
 *	These are primarily created by passing either lambda functions or constants to Audio or PVOC methods.
 */
template< typename I, typename O >
struct Function
{
	/** Callable constructor. This allows anything that can be called with the correct input and output type to be cast to flan::Function. */
	//template< typename Callable >
	//Function( Callable f_ ) : f( f_ ) {}
	Function( std::function< O ( I ) > f_ ) : f( f_ ) {}

	template<typename T>
	requires std::convertible_to<T, O>
	Function( T t0 		  ) : f( [t0]( I ){ return O( t0 ); } ) {}
	Function(			  ) : f( [  ]( I ){ return O( 0 ); } ) {}

	/** Function application */
	O operator()( I t ) const { return f(t); }

	/** Function composition */
	template< typename P >
	Function<P,O> operator()( const Function<P,I> g ) const { return (Function<I,O>) [*this, g]( I t ){ return operator()( g(t) ); }; }

	/** Function multiplication */
    Function<I,O> operator*( const Function<I,O> r ) const { return (Function<I,O>) [*this, r](I t){ return operator()(t) * r(t); }; }

	/** Function division */
	Function<I,O> operator/( const Function<I,O> r ) const { return (Function<I,O>) [*this, r](I t){ return operator()(t) / r(t); }; }

	/** Function modulus */
	Function<I,O> operator%( const Function<I,O> r ) const 
		{ 
		if constexpr ( std::is_floating_point<O>::value )
			return (Function<I,O>) [*this, r](I t){ return fmod( operator()(t), r(t) ); }; 
		else
			return (Function<I,O>) [*this, r](I t){ return operator()(t) % r(t); }; 
		}

	/** Function addition */
	Function<I,O> operator+( const Function<I,O> r ) const { return (Function<I,O>) [*this, r](I t){ return operator()(t) + r(t); }; }

	/** Function subtraction */
	Function<I,O> operator-( const Function<I,O> r ) const { return (Function<I,O>) [*this, r](I t){ return operator()(t) - r(t); }; }

	/** Function negation */
	Function<I,O> operator-() const { return (Function<I,O>) [*this](I t) { return -operator()(t); }; }

	/** Clamp a function within a function range */
	static Function<I,O> clamp( const Function<I,O> a, const Function<I,O> b, const Function<I,O> c ) 
		{ 
		return (Function<I,O>) [a, b, c]( I in )
			{ 
			return std::clamp( a.operator()( in ), b.operator()( in ), c.operator()( in ) ); 
			}; 
		}

	/** Return the maximum of two functions at each input */
	static Function<I,O> max( const Function<I,O> a, const Function<I,O> b ) 
		{ 
		return (Function<I,O>) [a, b]( I in )
			{ 
			return std::max( a.operator()( in ), b.operator()( in ) ); 
			}; 
		}

	/** Return the minimum of two functions at each input */
	static Function<I,O> min( const Function<I,O> a, const Function<I,O> b ) 
		{ 
		return (Function<I,O>) [a, b]( I in )
			{ 
			return std::min( a.operator()( in ), b.operator()( in ) ); 
			}; 
		}

	static Function<I,O> uniformDistribution( Function<I,O> lowerBound, Function<I,O> upperBound )
		{
		static std::default_random_engine rng( std::time( nullptr ) );
		return (Function<I,O>) [lowerBound, upperBound]( I x ) -> O
			{ 
			return std::uniform_real_distribution<float>( lowerBound( x ), upperBound( x ) )( rng ); 
			};
		}

	static Function<I,O> normalDistribution( Function<I,O> mean, Function<I,O> sigma )
		{
		static std::default_random_engine rng( std::time( nullptr ) );
		return (Function<I,O>) [mean, sigma]( I x ) -> O
			{ 
			const O m = mean( x );
			const O s = sigma( x );
			if( s <= 0 ) return m;
			return std::normal_distribution<float>( m, s )( rng ); 
			};
		}

	std::function< O ( I ) > f;
};

/** Real function of one real variable type. */
struct Func1x1 : public Function<float, float>
{
	template< typename T >
	Func1x1( T f ) : Function( f ) {}
	Func1x1() : Function() {}

	/** Exponentiate as a function */
    Func1x1 exp() const { return [*this]( float t ){ return std::exp( t ); }; }

	/** Create a graph of the function. 
	 * \param view The area of the cartesian plane that should be viewed.
	 * \param domain The interval on which the function should be graphed.
	 * \param width The bmp width.
	 * \param height The bmp height.
	 */
	Graph convertToGraph( Rect view = { -5, -5, 5, 5 }, Interval domain = Interval::R, Pixel width = -1, Pixel height = -1, flan_CANCEL_ARG ) const;

	/** Create and save a graph of the function. 
	 * \param filename The location to save the bmp.
	 * \param view The area of the cartesian plane that should be viewed.
	 * \param domain The interval on which the function should be graphed.
	 * \param width The bmp width.
	 * \param height The bmp height.
	 */
	Func1x1 saveAsBMP( const std::string & filename, Rect view = { -5, -5, 5, 5 }, Interval domain = Interval::R, Pixel width = -1, Pixel height = -1, flan_CANCEL_ARG ) const;

	/** Generate an ADSR envelope. Generated envelopes always range from 0 to 1.
	 *	The Exp parameters dictate how the envelope curves between fixed points. Using an Exp of 1 gives a linear movement.
	 *	Values between 0 and 1 give curves that move rapidly toward their destination before slowing down.
	 *	Values larger than 1 do the opposite. Other values will give unusual, but well defined, behaviour, so no clamping is applied.
	 *
	 * \param a Attack length in time.
	 * \param d Decay length in time.
	 * \param s Sustain length in time.
	 * \param r Release length in time.
	 * \param sLvl Sustain level.
	 * \param aExp Exponent of the attack curve.
	 * \param dExp Exponent of the decay curve.
	 * \param rExp Exponent of the release curve. 
	 */
	static Func1x1 ADSR( float a, float d, float s, float r, float sLvl,
		float aExp = 1, float dExp = 1, float rExp = 1 );

	/** This generates a periodic Function from the input function. 
	 *
	 *	\param wave The function to periodize. This will only be accessed on the domain [0,period).
	 *	\param min Bottom of the generated function.
	 *	\param max Top of the generated function.
	 *	\param period The period of the output.
	 */
	Func1x1 periodize( Func1x1 period = 1.0f );

	///** Generate a function that returns random data with an identity distribution
	// *	using the given bounds.
	// *
	// * \param mean The sample mean
	// * \param sigma The sample standard deviation.
	// */
	//static Func1x1 uniformDistribution( Func1x1 lowerBound, Func1x1 upperBound );

	///** Generate a function that returns random data with a normal distribution 
	// *	with the given mean and standard deviation.
	// *
	// * \param mean The sample mean
	// * \param sigma The sample standard deviation.
	// */
	//static Func1x1 normalDistribution( Func1x1 mean, Func1x1 sigma );

	static Func1x1 sine; 	
	static Func1x1 square; 	
	static Func1x1 saw; 		
	static Func1x1 triangle; 
};

/** Real function of vec2 type. */
struct Func2x1 : public Function<vec2, float>
{
	/** Generic constructor 
	*/
	template<typename T>
	Func2x1( T f ) : Function( f ) {}

	/** Constructor when argument is convertable to std::function< float ( float, float ) >.
	 */
	template<typename T>
	requires std::convertible_to< T, std::function< float ( float, float ) > >
	Func2x1( T f ) : Function( [f]( vec2 v ) { return f( v.x(), v.y() ); } ) {}

	/** Constructor when argument is convertable to std::function< float ( float ) >.
	 *	The additional parameter is ignored.
	 */
	template<typename T>
	requires std::convertible_to< T, std::function< float ( float ) > >
	Func2x1( T f_ ) : Function( [f_]( vec2 v ){ return f_( v.x() ); } ) {}

	/** Default
	 */
	Func2x1() : Function() {}

	/** Helper for calling without converting parameters to vec2 */
	float operator()( float t, float f ) const { return Function::operator()( vec2{ t, f } ); }
	float operator()( vec2 z ) { return Function::operator()( z ); }

	/** Helper for calling Function::uniformDistribution without explicit conversion. */
	static Func2x1 uniformDistribution( Func2x1 m, Func2x1 s ) 
		{ 
		return Function<vec2,float>::uniformDistribution( m, s );
		}

	/** Helper for calling Function::normalDistribution without explicit conversion. */
	static Func2x1 normalDistribution( Func2x1 m, Func2x1 s ) 
		{ 
		return Function<vec2,float>::normalDistribution( m, s );
		}
};


/** vec2 function of vec2 type. */
struct Func2x2 : public Function<vec2, vec2>
{
	/** Generic constructor 
	*/
	template< typename T >
	Func2x2( T f ) : Function( f ) {}

	/** Default
	 */
	Func2x2() : Function() {}

	/** Helper for calling without converting parameters to vec2 */
	vec2 operator()( float x, float y ) const { return Function::operator()( { x, y } ); }
	vec2 operator()( vec2 z ) const { return Function::operator()( z ); }
};

} // End namespace flan
