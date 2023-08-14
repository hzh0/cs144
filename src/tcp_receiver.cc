#include "tcp_receiver.hh"

#include <limits>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if (message.SYN) {
      zero_point_ = message.seqno;
      syn_ = true;
  }

  if (!syn_) return;

  fin_ = message.FIN;

  auto ceheckpoint = reassembler.first_unassemble_index_;
  auto unwrapnum = message.seqno.unwrap(zero_point_.value_or(Wrap32(0) ), ceheckpoint);
  if (message.SYN) ++unwrapnum;
  reassembler.insert( unwrapnum-1, message.payload, fin_, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
   if (!zero_point_.has_value()) return {nullopt, static_cast<uint16_t>(min(inbound_stream.available_capacity(), static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())))};
   auto index = inbound_stream.bytes_pushed() + zero_point_->get_raw_value() + 1 + inbound_stream.is_closed();
   return {Wrap32(index),static_cast<uint16_t>(min(inbound_stream.available_capacity(), static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())))};
}
