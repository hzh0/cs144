#include <stdexcept>
#include <iostream>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

uint64_t ByteStream::get_len() const {return len_;}

void Writer::push( string data )
{
    if (data.empty() || closed_) return;
    uint64_t len = data.size();
    uint64_t sum = min(available_capacity(), len);
    for (uint64_t i = 0; i < sum; ++i) stream_.push_back(data[i]);
    len_ += sum;
    cumulative_ += sum;
}

void Writer::close()
{
  closed_ = true;
}

void Writer::set_error()
{
  error_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - len_;
}

uint64_t Writer::bytes_pushed() const
{
  return cumulative_;
}

string_view Reader::peek() const
{
    if (stream_.empty()) return string_view();
    return string_view(&stream_.front(), 1);
}

bool Reader::is_finished() const
{
  return closed_ && stream_.empty();
}

bool Reader::has_error() const
{
  return error_;
}

void Reader::pop( uint64_t len )
{
  uint64_t sum = min(len, len_);
  for (uint64_t i = 0; i < sum; ++i) stream_.pop_front();
  len_ -= sum;
  poped_ += sum;
}

uint64_t Reader::bytes_buffered() const
{
  return len_;
}

uint64_t Reader::bytes_popped() const
{
  return poped_;
}

std::optional<std::string> Reader::get_data(uint64_t len) {
    if (len == 0 || len > len_) return nullopt;
    return std::string{stream_.begin(), stream_.begin() + len};
}
