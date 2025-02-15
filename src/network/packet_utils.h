/*
 * Network Packet Utilities
 * 
 * This module provides utilities for parsing and analyzing network packets,
 * particularly focusing on IP and UDP headers used in VoIP traffic.
 * It includes structures and functions for packet header manipulation.
 *
 * Key Features:
 * - IP header parsing
 * - UDP header extraction
 * - Traffic direction detection
 * - Header field access macros
 */

#ifndef PACKET_UTILS_H
#define PACKET_UTILS_H

#include <stdint.h>
#include <netinet/ip.h>      // IP header definitions
#include <netinet/ip6.h>     // IPv6 header definitions
#include <netinet/udp.h>     // UDP header definitions
#include "rtp_defs.h"
#include "rtp_types.h"
#include "nat64_utils.h"

/*
 * IPv4 Header Structure
 * 
 * Defines the structure of an IPv4 packet header:
 * - ip_vhl: Version (4 bits) and header length (4 bits)
 * - ip_tos: Type of service/DSCP+ECN
 * - ip_len: Total packet length
 * - ip_id: Identification field
 * - ip_off: Fragment offset and flags
 * - ip_ttl: Time to live
 * - ip_p: Protocol (TCP, UDP, etc.)
 * - ip_sum: Header checksum
 * - saddr: Source IP address
 * - daddr: Destination IP address
 */
struct ip_header {
    uint8_t  ip_vhl;          // Version and header length
    uint8_t  ip_tos;          // Type of service
    uint16_t ip_len;          // Total length
    uint16_t ip_id;           // Identification
    uint16_t ip_off;          // Fragment offset
    uint8_t  ip_ttl;          // Time to live
    uint8_t  ip_p;            // Protocol
    uint16_t ip_sum;          // Checksum
    struct in_addr saddr;     // Source address
    struct in_addr daddr;     // Destination address
} __attribute__((packed));

/*
 * IP Header Field Access Macros
 * 
 * IP_VERSION: Extract IP version (4 bits)
 * IP_HEADER_LEN: Calculate header length in bytes
 */
#define IP_VERSION(vhl)     (((vhl) >> 4) & 0x0f)  // Upper 4 bits
#define IP_HEADER_LEN(vhl) (((vhl) & 0x0f) * 4)    // Lower 4 bits * 4

/*
 * Extract Packet Headers
 * 
 * Parses a raw packet buffer to extract IP and UDP headers
 * 
 * Parameters:
 * - packet: Raw packet data
 * - len: Packet length
 * - ip: Pointer to store IP header
 * - udp: Pointer to store UDP header
 * 
 * Returns:
 * 0 on success, non-zero on error
 */
int get_packet_headers(const uint8_t *packet, int len, 
                      struct ip_header **ip, struct udphdr **udp);

/*
 * Determine Packet Direction
 * 
 * Analyzes IP header to determine traffic direction
 * 
 * Parameters:
 * - ip: Pointer to IP header
 * 
 * Returns:
 * DIRECTION_* value indicating packet direction
 */
int get_packet_direction(const struct ip_header *ip);

#endif // PACKET_UTILS_H