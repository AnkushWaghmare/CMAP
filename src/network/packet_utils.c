/*
 * Network Packet Utilities Implementation
 * 
 * This module implements packet parsing and analysis functions for the
 * Call Monitor and Analyzer Program (CMAP). It handles Ethernet, IP,
 * and UDP packet headers with a focus on VoIP traffic.
 *
 * Key Features:
 * - Ethernet frame parsing
 * - IPv4 header validation
 * - UDP header extraction
 * - Traffic direction detection
 */

#include "packet_utils.h"
#include "debug.h"
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>  // For ETHERTYPE_IP
#include "rtp_defs.h"

/*
 * Minimum RTP Packet Size
 * 12 bytes for header + minimum 1 byte payload
 */
#define MIN_RTP_SIZE 13

/*
 * Extract Packet Headers
 * 
 * Parses an Ethernet frame to extract IP and UDP headers.
 * Performs validation at each layer:
 * 1. Ethernet frame validation
 * 2. IPv4 header checks
 * 3. UDP header extraction
 * 
 * Parameters:
 * - packet: Raw packet data
 * - len: Packet length
 * - ip: Pointer to store IP header
 * - udp: Pointer to store UDP header
 * 
 * Returns:
 * 0 on success, -1 on error or non-matching packet
 */
int get_packet_headers(const u_char *packet, int len, struct ip_header **ip, struct udphdr **udp) {
    // Validate input parameters
    if (!packet || !ip || !udp || len < (int)sizeof(struct ether_header)) {
        DEBUG_PRINT("Invalid parameters or packet too short");
        return -1;
    }

    // Parse Ethernet header
    const struct ether_header *eth = (const struct ether_header *)packet;
    // Check for IPv4 packet (0x0800) - silently ignore IPv6 (0x86dd)
    if (ntohs(eth->ether_type) != ETHERTYPE_IP) {
        return -1;  // Silently skip non-IPv4 packets
    }

    // Extract and validate IP header
    *ip = (struct ip_header *)(packet + sizeof(struct ether_header));
    int ip_header_len = IP_HEADER_LEN((*ip)->ip_vhl);

    // Validate IP version and header length
    if (IP_VERSION((*ip)->ip_vhl) != 4 || ip_header_len < 20) {
        DEBUG_PRINT("Invalid IP version: %d, header len: %d", 
                    IP_VERSION((*ip)->ip_vhl),
                    ip_header_len);
        return -1;
    }

    // Check packet length against IP header
    if (len < (int)(sizeof(struct ether_header) + ip_header_len)) {
        DEBUG_PRINT("Packet too short for IP header");
        return -1;
    }

    // Verify UDP protocol
    if ((*ip)->ip_p != IPPROTO_UDP) {
        return -1;  // Silently skip non-UDP packets
    }

    // Extract UDP header
    *udp = (struct udphdr *)((u_char *)*ip + ip_header_len);

    // Validate UDP header
    if (len < (int)(sizeof(struct ether_header) + ip_header_len + sizeof(struct udphdr))) {
        DEBUG_PRINT("Packet too short for UDP header");
        return -1;
    }

    return 0;
}

/*
 * Determine Packet Direction
 * 
 * Analyzes source and destination IP addresses to determine
 * traffic direction. Considers:
 * - Local network ranges
 * - NAT64 translations
 * - Special purpose addresses
 * 
 * Parameters:
 * - ip: Pointer to IP header
 * 
 * Returns:
 * DIRECTION_* value indicating packet flow
 */
int get_packet_direction(const struct ip_header *ip) {
    char src_ip[INET6_ADDRSTRLEN], dst_ip[INET6_ADDRSTRLEN];
    
    if (IP_VERSION(ip->ip_vhl) == 6) {
        const struct ip6_hdr *ip6 = (const struct ip6_hdr *)ip;
        inet_ntop(AF_INET6, &ip6->ip6_src, src_ip, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET6, &ip6->ip6_dst, dst_ip, INET6_ADDRSTRLEN);
    } else {
        inet_ntop(AF_INET, &ip->saddr, src_ip, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET, &ip->daddr, dst_ip, INET6_ADDRSTRLEN);
    }
    
    // Check for NAT64 addresses
    int src_is_nat64 = is_nat64_address(src_ip);
    int dst_is_nat64 = is_nat64_address(dst_ip);
    
    if (src_is_nat64 && !dst_is_nat64) {
        return DIRECTION_INCOMING;
    }
    if (!src_is_nat64 && dst_is_nat64) {
        return DIRECTION_OUTGOING;
    }
    
    return DIRECTION_UNKNOWN;
} 