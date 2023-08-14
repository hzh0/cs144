#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPSender
{

  class Timer{
  public:
      uint64_t init_rto_{0};
      uint64_t cur_rto_{0};
      uint64_t cur_time_{0};
      uint64_t retry_times_{0};

      bool start_{false};
      bool outtime_{false};

      void restart() {
          cur_time_ = 0;
          outtime_ = false;
          start_ = true;
      }

      void outtime() {
          restart();
      }

      void reset() {
          restart();
          cur_rto_ = init_rto_;
          retry_times_ = 0;
      }

      void add(uint64_t ms) {
          if (start_) {
              cur_time_ += ms;
              while(cur_time_ >= cur_rto_) {
                  outtime_ = true;
                  cur_time_ -= cur_rto_;
                  cur_rto_ *= 2;
                  ++retry_times_;
              }
          }
      }
  };

  class Segment {
  public:
      Wrap32 isn_{0};
      TCPSenderMessage data_{};
  };

  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  int32_t windows_size_{1};
  int32_t remain_size_{0};
  bool syn_{false};
  bool fin_{false};
  Timer timer_{};

  int32_t in_flight_num_{0};

  Wrap32 last_ack_isn_{0};
  Wrap32 next_isn_{0};

  std::deque<Segment> pushed_segs_{};
  std::deque<Segment> sent_segs_{};


public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
