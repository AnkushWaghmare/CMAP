/*
 * Network Packet Capture Module
 * 
 * This module provides functionality for capturing and processing network
 * packets, specifically focusing on RTP traffic for VoIP monitoring.
 * It uses libpcap for packet capture and implements packet processing logic.
 *
 * Key Features:
 * - Network interface capture
 * - Packet processing and analysis
 * - RTP stream tracking
 * - Automatic mode support
 */

#ifndef PACKET_CAPTURE_H
#define PACKET_CAPTURE_H

#include <pcap.h>            // libpcap packet capture library
#include "rtp_types.h"       // RTP type definitions
#include "rtp_defs.h"        // RTP protocol definitions

/*
 * Return Codes
 * Standard return values for packet capture operations
 */
#define PCAP_SUCCESS 0       // Operation completed successfully
#define PCAP_ERROR -1       // Operation failed

/*
 * RTP Stream Timing Parameters
 * Timeouts and grace periods for RTP stream management
 */
#define RTP_GRACE_PERIOD 5   // Grace period in seconds for last RTP packets
#define RTP_TIMEOUT 30       // Timeout in seconds to mark stream as inactive

/*
 * Operation Mode Flags
 * Global flags controlling capture behavior
 */
extern int auto_mode;        // Automatic capture mode
extern int silent_mode;      // Suppress output mode
extern int capture_time;     // Time limit in seconds for capture

/*
 * Start Packet Capture
 * 
 * Initializes packet capture on specified network interface
 * 
 * Parameters:
 * - interface: Network interface name
 * - output_file: File to save capture data
 * 
 * Returns:
 * PCAP_SUCCESS on success, PCAP_ERROR on failure
 */
int start_capture(const char *interface, const char *output_file);

/*
 * Stop Packet Capture
 * 
 * Terminates active packet capture and cleans up resources
 */
void stop_capture(void);

/*
 * Process Captured Packet
 * 
 * Callback function for processing each captured packet
 * 
 * Parameters:
 * - user: User-provided data (unused)
 * - header: Packet metadata (timestamp, length)
 * - packet: Raw packet data
 */
void process_packet(u_char *user, const struct pcap_pkthdr *header, 
                   const u_char *packet);

#endif // PACKET_CAPTURE_H