#include "rand.hpp"
#include <cassert>

// George Marsaglia's Multiply With Carry (MWC) algorithm
// http://www.bobwheeler.com/statistics/Password/MarsagliaPost.txt
// http://www.codeproject.com/KB/recipes/SimpleRNG.aspx

rand_t::rand_t(uint64_t seed): m_z(seed>>32), m_w(seed) {}

rand_t::rand_t(): m_z(high_precision_time()>>32), m_w(high_precision_time()) {}

uint32_t rand_t::rand() {
    m_z = 36969 * (m_z & 0xffff) + (m_z >> 16);
    m_w = 18000 * (m_w & 0xffff) + (m_w >> 16);
    return ((m_z << 16) + m_w); // >> 1;
}

float rand_t::randf() {
    // 0 <= u < 2^32
    // The magic number below is 1/(2^32 + 2).
    // The result is strictly between 0 and 1.
    const double f = (rand() + 1.0) * 2.328306435454494e-10;
    assert(f>0.0f);
    assert(f<1.0f);
    return f;
}

#ifdef __WIN32
	#include <windows.h>
	uint64_t high_precision_time() {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		static __int64 base;
		static double freq;
		static bool inited = false;
		if(!inited) {
			base = now.QuadPart; 
			LARGE_INTEGER li;
			QueryPerformanceFrequency(&li);
			freq = (double)li.QuadPart/1000000000;
			std::cout << "FREQ "<<li.QuadPart<<","<<freq<<std::endl;
			inited = true;
		}
		return (now.QuadPart-base)/freq;	
	}
#elif defined(__native_client__)
	#include <sys/time.h>
	uint64_t high_precision_time() {
		static uint64_t base = 0;
		struct timeval tv;
		gettimeofday(&tv,NULL);
		if(!base)
			base = tv.tv_sec;
		return (tv.tv_sec-base)*1000000000+(tv.tv_usec*1000);
	}
#else	
	#include <time.h>
	uint64_t high_precision_time() {
		static uint64_t base = 0;
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC,&ts);
		if(!base)
			base = ts.tv_sec;
		return (ts.tv_sec-base)*1000000000+ts.tv_nsec;
	}
#endif
