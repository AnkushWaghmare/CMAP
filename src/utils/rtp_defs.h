/*
 * RTP (Real-time Transport Protocol) Definitions
 * 
 * This header defines constants and structures for RTP protocol handling.
 * Includes port ranges, header structures, and protocol-specific constants
 * used throughout the VoIP monitoring system.
 *
 * Key Features:
 * - RTP port range definitions
 * - Protocol header structures
 * - Network constants
 * - NAT64 configuration
 */

#ifndef RTP_DEFS_H
#define RTP_DEFS_H

#include <stdint.h>

/*
 * RTP Port Range Definitions
 * Defines valid port ranges for RTP traffic:
 * - Standard RTP ports (1024-65535)
 * - WebRTC specific ports (10000-60000)
 */
#define RTP_MIN_PORT 1024
#define RTP_MAX_PORT 65535
#define WEBRTC_MIN_PORT 10000
#define WEBRTC_MAX_PORT 60000

/*
 * Port Range Validation Macro
 * Checks if a given port number falls within valid RTP range
 */
#define IS_RTP_PORT(port) ((port) >= RTP_MIN_PORT && (port) <= RTP_MAX_PORT)

/*
 * RTP Header Structure
 * Defines the standard RTP packet header format:
 * - version: RTP protocol version (2 bits)
 * - padding: Padding flag (1 bit)
 * - extension: Extension flag (1 bit)
 * - csrc_count: CSRC count (4 bits)
 * - marker: Marker bit (1 bit)
 * - payload_type: Payload type (7 bits)
 * - sequence_number: Packet sequence number (16 bits)
 * - timestamp: Sampling timestamp (32 bits)
 * - ssrc: Synchronization source (32 bits)
 */
struct rtp_header {
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t extension:1;
    uint8_t csrc_count:4;
    uint8_t marker:1;
    uint8_t payload_type:7;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
} __attribute__((packed));

/*
 * Common Port Range Definitions
 * Standard port ranges for various services:
 * - Common RTP ports (16384-32767)
 * - Ephemeral ports (49152-65535)
 */
#define COMMON_RTP_MIN_PORT 16384
#define COMMON_RTP_MAX_PORT 32767
#define EPHEMERAL_MIN_PORT 49152
#define EPHEMERAL_MAX_PORT 65535

/*
 * Protocol-Specific Port Numbers
 * Standard ports for VoIP protocols
 */
#define SIP_PORT 5060

/*
 * RTP sequence number constants
 */
#define MAX_DROPOUT    3000    // Maximum allowed sequence number gap
#define MAX_MISORDER   100     // Maximum allowed out of order packets
#define MIN_SEQUENTIAL 2       // Minimum sequential packets for valid source
#define RTP_SEQ_MOD    65536   // 2^16 (sequence number wrap value)

/*
 * RTP Header Size and Payload Types
 */
#define RTP_HEADER_SIZE     12     // Standard RTP header size
#define RTP_VERSION         2      // RTP version we support

// Standard payload types
#define PT_PCMU            0      // G.711 µ-law
#define PT_PCMA            8      // G.711 A-law
#define PT_G722            9      // G.722
#define PT_CN             13      // Comfort Noise
#define PT_DTMF          101      // DTMF Events (RFC 2833/4733)

// Sampling rates
#define RATE_8KHZ        8000    // 8kHz sampling (PCMU/PCMA)
#define RATE_16KHZ      16000    // 16kHz sampling (G.722)
#define RATE_32KHZ      32000    // 32kHz sampling
#define RATE_48KHZ      48000    // 48kHz sampling

/*
 * NAT64 Configuration
 * Standard NAT64 prefix for IPv6-IPv4 translation
 */
#define NAT64_PREFIX "64:ff9b::"

#endif // RTP_DEFS_H