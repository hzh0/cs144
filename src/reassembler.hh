#pragma once

#include "byte_stream.hh"

#include <string>
#include <map>

class Reassembler
{
public:

    Reassembler(uint64_t capacity = 0);

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;



    class Segment{
    public:
        uint64_t start_ = 0;
        uint64_t len_ = 0;
        std::deque<char> data_{};
    };

    uint64_t capacity_ = 0;
    uint64_t first_unpopped_index_ = 0;
    uint64_t first_unassemble_index_ = 0;
    uint64_t first_unacceptable_index_ = 0;
    bool last_ = false;
    std::map<std::pair<uint64_t, uint64_t>,Segment> start_seg_map_{};
};
