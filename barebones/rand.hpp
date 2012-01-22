#ifndef __RAND_HPP__
#define __RAND_HPP__

#include <inttypes.h>

uint64_t high_precision_time();

class rand_t {
public:
	rand_t(); // use system time(!)
	rand_t(uint64_t seed);
	uint32_t rand(); // 0 .. 2 billion
	inline uint32_t rand(uint32_t max) { return rand()%max; }
	inline uint32_t rand(uint32_t min,uint32_t max) { return min+rand(max-min); }
	float randf(); // > 0 && < 1
	inline float randf(float max) { return randf()*max; } // >= 0 && < max
	inline float randf(float min,float max) { return min+randf(max-min); } // >= min && < max
private:
	uint32_t m_z, m_w;
};

#endif//__RAND_HPP__