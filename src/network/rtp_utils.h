/*
 * RTP (Real-time Transport Protocol) Utilities
 * 
 * This module provides functions for RTP packet analysis and stream tracking.
 * It handles RTP packet validation, parsing, and statistics collection for
 * VoIP media streams.
 *
 * Key Features:
 * - RTP packet validation and parsing
 * - Stream identification and tracking
 * - Packet statistics and metrics
 * - Jitter and loss calculation
 */

#ifndef RTP_UTILS_H
#define RTP_UTILS_H

#include <stdint.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <pcap.h>
#include "rtp_types.h"
#include "packet_utils.h"
#include "call_session.h"
#include "nat64_utils.h"
#include "audio_quality.h"

// RTP Packet Validation Functions
/*
 * Validates RTP payload type against known codecs
 * Returns: 1 if valid, 0 if invalid
 */
int is_valid_rtp_payload_type(uint8_t pt);

/*
 * Checks if packet size is valid for given payload type
 * Returns: 1 if valid, 0 if invalid
 */
int is_valid_rtp_packet_size(int payload_len, uint8_t pt);

/*
 * Validates if a packet is a valid RTP packet
 * Returns: 1 if valid RTP, 0 if not
 */
int is_rtp_packet(const uint8_t *payload, int payload_len);

// RTP Header Field Extraction
/*
 * Extract RTP sequence number from packet
 * Returns: 16-bit sequence number
 */
uint16_t get_sequence_number(const uint8_t *rtp_packet);

/*
 * Extract RTP timestamp from packet
 * Returns: 32-bit timestamp
 */
uint32_t get_timestamp(const uint8_t *rtp_packet);

/*
 * Extract RTP SSRC identifier from packet
 * Returns: 32-bit SSRC
 */
uint32_t get_ssrc(const uint8_t *rtp_packet);

/*
 * Extract RTP payload type from packet
 * Returns: 7-bit payload type
 */
uint8_t get_payload_type(const uint8_t *rtp_packet);

// RTP Stream Processing
/*
 * Process an RTP packet and update session state
 * Parameters:
 * - header: PCAP packet header
 * - ip: IP header
 * - udp: UDP header
 * - payload: RTP packet data
 * - payload_len: Length of RTP data
 * - direction: Packet direction (0=incoming, 1=outgoing)
 */
void process_rtp_packet(const struct pcap_pkthdr *header,
                       const struct ip_header *ip,
                       const struct udphdr *udp,
                       const uint8_t *payload,
                       int payload_len,
                       int direction);

/*
 * Find existing stream or create new one
 * Parameters:
 * - session: Current call session
 * - src_ip/port: Source address
 * - dst_ip/port: Destination address
 * - ssrc: RTP SSRC identifier
 * - payload_type: RTP payload type
 * - direction: Stream direction
 * Returns: Pointer to stream structure
 */
struct rtp_stream* find_or_create_stream(struct call_session *session,
                                       const char *src_ip, uint16_t src_port,
                                       const char *dst_ip, uint16_t dst_port,
                                       uint32_t ssrc, uint8_t payload_type,
                                       int direction);

/*
 * Update stream statistics with new packet
 * Parameters:
 * - stream: RTP stream to update
 * - seq: Packet sequence number
 * - timestamp: RTP timestamp
 * - packet_time: Packet arrival time
 */
void update_stream_stats(struct rtp_stream *stream,
                        uint16_t seq, uint32_t timestamp,
                        uint32_t packet_time);

/*
 * Check if stream matches given endpoints
 * Parameters:
 * - stream: Stream to check
 * - src_ip/port: Source address
 * - dst_ip/port: Destination address
 * Returns: 1 if matching, 0 if not
 */
int is_matching_stream(struct rtp_stream *stream,
                      const char *src_ip, uint16_t src_port,
                      const char *dst_ip, uint16_t dst_port);

// Audio Quality Functions
/*
 * Get next audio packet for playout
 * Returns: Number of bytes copied, or -1 on error
 */
int get_next_audio_packet(struct rtp_stream *stream, uint8_t *buffer, int max_size);

/*
 * Clean up RTP stream resources
 */
void cleanup_rtp_stream(struct rtp_stream *stream);

#endif // RTP_UTILS_H