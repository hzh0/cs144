#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{
    timer_.cur_rto_ = initial_RTO_ms;
    timer_.init_rto_ = initial_RTO_ms;
    last_ack_isn_ = isn_ + 1;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return in_flight_num_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return timer_.retry_times_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (timer_.outtime_) {
    timer_.outtime();
    return sent_segs_.front().data_;
  }
  if (!pushed_segs_.empty()) {
      if (!timer_.start_) {
          timer_.reset();
      }
      sent_segs_.push_back(pushed_segs_.front());
      next_isn_ = pushed_segs_.front().isn_ + pushed_segs_.front().data_.sequence_length();
      pushed_segs_.pop_front();
      return sent_segs_.back().data_;
  }
  return nullopt;
}

void TCPSender::push( Reader& outbound_stream )
{
  if (fin_) return;
  if (!syn_) {
      syn_ = true;
      fin_ = outbound_stream.is_finished();
      TCPSenderMessage tcpSenderMessage = {isn_, syn_, std::string(""), fin_};
      pushed_segs_.push_back({isn_,tcpSenderMessage});
      in_flight_num_ += tcpSenderMessage.sequence_length();
      return;
  }
  if (windows_size_ == 0 && in_flight_num_ == 0) remain_size_ = 1;
  if (remain_size_ == 0) return;
  auto len = min(outbound_stream.get_len(), static_cast<uint64_t>(remain_size_));
  while(len > TCPConfig::MAX_PAYLOAD_SIZE) {
      auto data = outbound_stream.get_data(TCPConfig::MAX_PAYLOAD_SIZE).value_or("");
      Wrap32 isn{Wrap32::wrap(outbound_stream.bytes_popped() + 1, isn_)};
      outbound_stream.pop(TCPConfig::MAX_PAYLOAD_SIZE);
      TCPSenderMessage senderMessage{isn, false, data, false};
      pushed_segs_.push_back({isn, senderMessage});
      in_flight_num_ += senderMessage.sequence_length();
      remain_size_ -= senderMessage.sequence_length();
      len -= senderMessage.sequence_length();
  }

  auto data = outbound_stream.get_data(len).value_or("");
  Wrap32 isn{Wrap32::wrap(outbound_stream.bytes_popped() + 1, isn_)};
  outbound_stream.pop(len);

  if (outbound_stream.is_finished()) fin_ = true;
  if (data.length() == static_cast<size_t>(remain_size_) && fin_) fin_ = false;
  if (data == "" && !fin_) return;
  TCPSenderMessage senderMessage{isn, false, data, fin_};
  pushed_segs_.push_back({isn, senderMessage});
  remain_size_ -= senderMessage.sequence_length();
  in_flight_num_ += senderMessage.sequence_length();
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return {next_isn_, false, std::string(), false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (!syn_) return;
  if (!msg.ackno.has_value()) return;
  if (msg.ackno.value().get_raw_value() < last_ack_isn_.get_raw_value()) return;
  if (msg.ackno.value().get_raw_value() > next_isn_.get_raw_value()) return;

  if (msg.ackno.value().get_raw_value() != last_ack_isn_.get_raw_value()) {
      timer_.cur_rto_ = initial_RTO_ms_;
      timer_.retry_times_ = 0;
  }

  windows_size_ = msg.window_size;
  remain_size_ = max(0, windows_size_-in_flight_num_);
  last_ack_isn_ = msg.ackno.value();

  while(!sent_segs_.empty()) {
      auto head = sent_segs_.front();
      if (head.isn_.get_raw_value() + head.data_.sequence_length() <= last_ack_isn_.get_raw_value()) {
          in_flight_num_ -= head.data_.sequence_length();
          remain_size_ = max(0, windows_size_-in_flight_num_);
          sent_segs_.pop_front();
          timer_.cur_time_ = 0;
          continue;
      }
      timer_.start_ = true;
      return;
  }
  timer_.start_ = false;
}

void TCPSender::tick( uint64_t ms_since_last_tick )
{
    if (windows_size_ == 0) timer_.cur_rto_ = initial_RTO_ms_;
    timer_.add(ms_since_last_tick);
}
