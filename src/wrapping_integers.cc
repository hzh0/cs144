#include "wrapping_integers.hh"

#include <cmath>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
    return Wrap32((n+zero_point.raw_value_) % (uint64_t{1}<<32));
}
//813068917  0
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
    //raw_value_ -> cloest checkpoint
    uint64_t ret = raw_value_;
    if (ret < zero_point.raw_value_) ret += uint64_t{1}<<32;
    ret -= zero_point.raw_value_;
    if (ret >= checkpoint) return ret;
    uint64_t t = (checkpoint-ret) / (uint64_t{1}<<32);
    uint64_t low = ret + t*(uint64_t{1}<<32), h = ret + (t+1)* (uint64_t{1}<<32);
    return checkpoint -low < h-checkpoint ? low : h;
}
