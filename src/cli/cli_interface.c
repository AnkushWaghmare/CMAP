/*
 * Command Line Interface Implementation
 * 
 * This module implements the command-line interface for the Call Monitor
 * and Analyzer Program (CMAP). It provides functions for displaying network
 * interfaces, progress updates, and call statistics.
 *
 * Key Features:
 * - Network interface listing
 * - Real-time progress display
 * - Call statistics reporting
 * - Formatted output with ANSI colors
 */

#include "cli_style.h"
#include "cli_interface.h"
#include "packet_capture.h"
#include "call_session.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>

// Global configuration flag for statistics display
int stats_display_enabled = 1;

/*
 * List Network Interfaces
 * 
 * Displays available network interfaces with their:
 * - Interface name
 * - Interface type (Ethernet, Loopback, Point-to-Point)
 * - IP address
 * - Interface status
 * 
 * Uses getifaddrs() to enumerate interfaces and formats
 * output in a table format with appropriate spacing.
 */
void list_interfaces(void) {
    struct ifaddrs *ifaddr, *ifa;
    char addr[INET6_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "Failed to get interfaces\n");
        return;
    }

    printf("\nAvailable Network Interfaces:\n");
    printf("%-12s %-15s %-20s %s\n", "Interface", "Type", "Address", "Status");
    printf("%-12s %-15s %-20s %s\n", "---------", "----", "-------", "------");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        int family = ifa->ifa_addr->sa_family;
        
        if (family == AF_INET || family == AF_INET6) {
            void *addr_ptr;
            
            // Get appropriate address pointer based on family
            if (family == AF_INET) {
                addr_ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            } else {
                addr_ptr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            }
            
            inet_ntop(family, addr_ptr, addr, sizeof(addr));
            
            printf("%-12s %-15s %-20s %s\n",
                   ifa->ifa_name,
                   (ifa->ifa_flags & IFF_LOOPBACK) ? "Loopback" :
                   (ifa->ifa_flags & IFF_POINTOPOINT) ? "Point-to-Point" : "Ethernet",
                   addr,
                   (ifa->ifa_flags & IFF_UP) ? "UP" : "DOWN");
        }
    }

    printf("\n");
    freeifaddrs(ifaddr);
}

/*
 * Show Progress
 * 
 * Displays real-time progress of packet capture and analysis:
 * - Current call state
 * - Packet statistics
 * - RTP stream information
 * - Call quality metrics
 * 
 * Uses ANSI escape codes for cursor control and colors
 * to create an updating display.
 */
void show_progress(void) {
    if (!is_call_active(&current_session)) {
        if (!silent_mode && stats_display_enabled) {
            printf("No active call\n");
        }
        return;
    }
    
    // Get call statistics
    uint32_t total_packets, sip_packets;
    uint32_t duration;
    get_session_stats(&current_session, &total_packets, &sip_packets, &duration);
    
    // Get quality metrics
    double avg_jitter;
    uint32_t lost_packets, out_of_order;
    get_call_quality_stats(&current_session, &avg_jitter, &lost_packets, &out_of_order);
    
    // Print call information
    if (!silent_mode && stats_display_enabled) {
        printf("\nCall Statistics:\n");
        printf("  Duration: %u seconds\n", duration);
        printf("  Total Packets: %u\n", total_packets);
        printf("  SIP Packets: %u\n", sip_packets);
        printf("  Average Jitter: %.2f ms\n", avg_jitter);
        printf("  Lost Packets: %u\n", lost_packets);
        printf("  Out of Order: %u\n", out_of_order);
        
        // Print stream information
        printf("\nActive Streams:\n");
        for (int i = 0; i < MAX_RTP_STREAMS; i++) {
            const struct rtp_stream *stream = &current_session.streams[i];
            if (stream->active) {
                double jitter;
                uint32_t lost, ooo;
                get_stream_metrics(stream, &jitter, &lost, &ooo);
                
                printf("  Stream %d:\n", i+1);
                printf("    SSRC: 0x%08x\n", stream->ssrc);
                printf("    Payload Type: %d\n", stream->payload_type);
                printf("    Direction: %s\n", stream->direction == DIRECTION_INCOMING ? "Incoming" : "Outgoing");
                printf("    Source: %s:%d\n", stream->src_ip, stream->src_port);
                printf("    Destination: %s:%d\n", stream->dst_ip, stream->dst_port);
                printf("    Packets: %u\n", stream->packets_received);
                printf("    Jitter: %.2f ms\n", jitter);
                printf("    Lost: %u\n", lost);
                printf("    Out of Order: %u\n", ooo);
                
                // Print codec info if available
                for (int j = 0; j < MAX_RTP_STREAMS; j++) {
                    struct rtp_stream_info *info = current_session.stream_info[j];
                    if (info && info->payload_type == stream->payload_type) {
                        printf("    Codec: %s\n", info->codec);
                        printf("    Sample Rate: %d Hz\n", info->sample_rate);
                        if (info->fmtp[0]) {
                            printf("    Parameters: %s\n", info->fmtp);
                        }
                        break;
                    }
                }
            }
        }
    }
}

/*
 * Show Final Statistics
 * 
 * Displays final call statistics after capture completion:
 * - Call duration
 * - Total packets captured
 * - Stream statistics
 * - Quality metrics
 * 
 * Formats output with appropriate colors and spacing
 * for readability.
 */
void show_final_stats(void) {
    // Get final statistics
    uint32_t total_packets, sip_packets;
    uint32_t duration;
    get_session_stats(&current_session, &total_packets, &sip_packets, &duration);
    
    // Get quality metrics
    double avg_jitter;
    uint32_t lost_packets, out_of_order;
    get_call_quality_stats(&current_session, &avg_jitter, &lost_packets, &out_of_order);
    
    // Print final statistics only if not in silent mode and stats display is enabled
    if (!silent_mode && stats_display_enabled) {
        printf("\nFinal Call Statistics:\n");
        printf("------------------------\n");
        printf("Duration: %u seconds\n", duration);
        printf("Total Packets: %u\n", total_packets);
        printf("SIP Packets: %u\n", sip_packets);
        printf("Average Jitter: %.2f ms\n", avg_jitter);
        printf("Lost Packets: %u\n", lost_packets);
        printf("Out of Order: %u\n", out_of_order);
        
        // Print per-stream statistics
        printf("\nStream Statistics:\n");
        printf("-----------------\n");
        for (int i = 0; i < MAX_RTP_STREAMS; i++) {
            const struct rtp_stream *stream = &current_session.streams[i];
            if (stream->active) {
                double jitter;
                uint32_t lost, ooo;
                get_stream_metrics(stream, &jitter, &lost, &ooo);
                
                printf("\nStream %d:\n", i+1);
                printf("  SSRC: 0x%08x\n", stream->ssrc);
                printf("  Direction: %s\n", stream->direction == DIRECTION_INCOMING ? "Incoming" : "Outgoing");
                printf("  Packets Received: %u\n", stream->packets_received);
                printf("  Jitter: %.2f ms\n", jitter);
                printf("  Lost Packets: %u\n", lost);
                printf("  Out of Order: %u\n", ooo);
                
                // Print codec info if available
                for (int j = 0; j < MAX_RTP_STREAMS; j++) {
                    struct rtp_stream_info *info = current_session.stream_info[j];
                    if (info && info->payload_type == stream->payload_type) {
                        printf("  Codec: %s\n", info->codec);
                        printf("  Sample Rate: %d Hz\n", info->sample_rate);
                        if (info->fmtp[0]) {
                            printf("  Parameters: %s\n", info->fmtp);
                        }
                        break;
                    }
                }
            }
        }
        printf("\n");
    }
}

/*
 * Get Final Statistics
 * 
 * Formats final call statistics into a provided buffer:
 * - Formats timestamps
 * - Calculates durations
 * - Aggregates stream statistics
 * 
 * Parameters:
 * - session: Source call session
 * - buffer: Output buffer
 * - buffer_size: Size of output buffer
 */
void get_final_stats(struct call_session *session, char *buffer, size_t buffer_size) {
    // Get final statistics
    uint32_t total_packets, sip_packets;
    uint32_t duration;
    get_session_stats(session, &total_packets, &sip_packets, &duration);
    
    // Get quality metrics
    double avg_jitter;
    uint32_t lost_packets, out_of_order;
    get_call_quality_stats(session, &avg_jitter, &lost_packets, &out_of_order);
    
    // Format statistics into buffer
    int written = snprintf(buffer, buffer_size,
        "Final Call Statistics:\n"
        "------------------------\n"
        "Duration: %u seconds\n"
        "Total Packets: %u\n"
        "SIP Packets: %u\n"
        "Average Jitter: %.2f ms\n"
        "Lost Packets: %u\n"
        "Out of Order: %u\n\n"
        "Stream Statistics:\n"
        "-----------------\n",
        duration, total_packets, sip_packets, avg_jitter, lost_packets, out_of_order);

    size_t remaining = buffer_size - written;
    char *current = buffer + written;
    
    // Add per-stream statistics
    for (int i = 0; i < MAX_RTP_STREAMS && remaining > 0; i++) {
        const struct rtp_stream *stream = &session->streams[i];
        if (stream->active) {
            double jitter;
            uint32_t lost, ooo;
            get_stream_metrics(stream, &jitter, &lost, &ooo);
            
            int stream_written = snprintf(current, remaining,
                "\nStream %d:\n"
                "  SSRC: 0x%08x\n"
                "  Direction: %s\n"
                "  Packets Received: %u\n"
                "  Jitter: %.2f ms\n"
                "  Lost Packets: %u\n"
                "  Out of Order: %u\n",
                i+1, stream->ssrc,
                stream->direction == DIRECTION_INCOMING ? "Incoming" : "Outgoing",
                stream->packets_received, jitter, lost, ooo);
                
            if (stream_written > 0) {
                remaining -= stream_written;
                current += stream_written;
            }
            
            // Add codec info if available
            for (int j = 0; j < MAX_RTP_STREAMS && remaining > 0; j++) {
                struct rtp_stream_info *info = session->stream_info[j];
                if (info && info->payload_type == stream->payload_type) {
                    int codec_written = snprintf(current, remaining,
                        "  Codec: %s\n"
                        "  Sample Rate: %d Hz\n",
                        info->codec, info->sample_rate);
                    
                    if (codec_written > 0) {
                        remaining -= codec_written;
                        current += codec_written;
                    }
                    
                    if (info->fmtp[0] && remaining > 0) {
                        int fmtp_written = snprintf(current, remaining,
                            "  Parameters: %s\n",
                            info->fmtp);
                            
                        if (fmtp_written > 0) {
                            remaining -= fmtp_written;
                            current += fmtp_written;
                        }
                    }
                    break;
                }
            }
        }
    }
    
    if (remaining > 0) {
        snprintf(current, remaining, "\n");
    }
}
