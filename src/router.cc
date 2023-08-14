#include "router.hh"

#include <iostream>
#include <limits>
#include <cmath>

using namespace std;

uint32_t calculateRouteNum(uint32_t ip_num, uint8_t prefix_length) {
    uint32_t route_num = 0;
    for (int i = 0; i < prefix_length; ++i) {
        if (ip_num & 0x80000000) route_num += pow(2, 32-i-1);
        ip_num <<= 1;
    }
    return route_num;
}

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num )
{
  route_table_[prefix_length].push_back({calculateRouteNum(route_prefix, prefix_length), next_hop, interface_num});
}

void Router::route() {
    for (auto &i : interfaces_) {
        std::optional<IPv4Datagram> data{std::nullopt};
        while ((data = i.maybe_receive()) != std::nullopt) {
            auto ipv4data = data.value();
            if (ipv4data.header.ttl <= 1) continue;
            ipv4data.header.ttl--;
            auto targetIp = ipv4data.header.dst;
            for (auto it = route_table_.rbegin(); it != route_table_.rend(); ++it) {
                auto ipnum = calculateRouteNum(targetIp, it->first);
                for (auto &j: it->second) {
                    if (ipnum == j.route_num_) {
                        ipv4data.header.compute_checksum();
                        Address address = Address::from_ipv4_numeric(targetIp);
                        if (j.next_hop_.has_value()) address = j.next_hop_.value();
                        interface(j.interface_num_).send_datagram(ipv4data, address);
                        goto end;
                    }
                }
            }
            end:
            continue;
        }
    }
}
