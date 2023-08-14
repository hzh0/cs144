#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

#define TIMEOUT (30*1000)
#define REQUESTTIME (5*1000)

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame ethernetFrame;
  ethernetFrame.header.src = ethernet_address_;
  auto targetIp = next_hop.ipv4_numeric();
  auto it = ip_ethAddr_time_.find(targetIp);

  if (it != ip_ethAddr_time_.end()) {
    if (cur_time_ - it->second.second <= TIMEOUT) {
      ethernetFrame.header.type = EthernetHeader::TYPE_IPv4;
      ethernetFrame.header.dst = it->second.first;
      ethernetFrame.payload = serialize(dgram);
      pushed_.push_back(ethernetFrame);
      return;
    }
  }

  ARPMessage arpMessage;
  arpMessage.opcode = ARPMessage::OPCODE_REQUEST;
  arpMessage.sender_ethernet_address = ethernet_address_;
  arpMessage.sender_ip_address = ip_address_.ipv4_numeric();
  arpMessage.target_ip_address = targetIp;

  ethernetFrame.header.type = EthernetHeader::TYPE_ARP;
  ethernetFrame.header.dst = ETHERNET_BROADCAST;
  ethernetFrame.header.src = ethernet_address_;
  ethernetFrame.payload = serialize(arpMessage);
  if (ip_requesttime_.find(targetIp) == ip_requesttime_.end() || cur_time_ - ip_requesttime_[targetIp] > REQUESTTIME) {
      ip_requesttime_[targetIp] = cur_time_;
      pushed_.push_back(ethernetFrame);
  }
  queued_[targetIp].push_back(dgram);
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
    EthernetHeader ethernetHeader = frame.header;
    EthernetAddress dst = ethernetHeader.dst;
    uint16_t type = ethernetHeader.type;
    if (dst != ETHERNET_BROADCAST && dst != ethernet_address_) return nullopt;
    if (type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram internetDatagram;
        if (parse(internetDatagram, frame.payload)) {
            return internetDatagram;
        }
        else {
            return nullopt;
        }
    }
    else if (type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpMessage;
        if (parse(arpMessage, frame.payload)) {
            uint32_t ip = arpMessage.sender_ip_address;
            ip_ethAddr_time_[ip].first = arpMessage.sender_ethernet_address;
            ip_ethAddr_time_[ip].second = cur_time_;
            for (auto &i : queued_[ip]) {
                send_datagram(i, Address::from_ipv4_numeric(ip));
            }
            std::vector<InternetDatagram>().swap(queued_[ip]);

            if (arpMessage.opcode == ARPMessage::OPCODE_REQUEST) {
                if (arpMessage.target_ip_address != ip_address_.ipv4_numeric()) return nullopt;
                ARPMessage retArpMsg;
                retArpMsg.opcode = ARPMessage::OPCODE_REPLY;
                retArpMsg.sender_ethernet_address = ethernet_address_;
                retArpMsg.sender_ip_address = ip_address_.ipv4_numeric();
                retArpMsg.target_ethernet_address = arpMessage.sender_ethernet_address;
                retArpMsg.target_ip_address = ip;
                InternetDatagram internetDatagram;
                internetDatagram.payload = serialize(retArpMsg);

                EthernetFrame ethernetFrame;
                ethernetFrame.header.type = EthernetHeader::TYPE_ARP;
                ethernetFrame.header.dst = arpMessage.sender_ethernet_address;
                ethernetFrame.header.src = ethernet_address_;
                ethernetFrame.payload = serialize(retArpMsg);
                pushed_.push_back(ethernetFrame);
            }
        }
    }
    return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  cur_time_ += ms_since_last_tick;
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (!pushed_.empty()) {
      auto head = pushed_.front();
      pushed_.pop_front();
      return head;
  }
  return nullopt;
}
