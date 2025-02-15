/*
 * Packet Capture Implementation
 * 
 * This module handles network packet capture and analysis for VoIP traffic.
 * It uses libpcap for packet capture and implements real-time analysis
 * of SIP and RTP packets to track VoIP calls.
 *
 * Key Components:
 * - Packet capture initialization and configuration
 * - Packet processing and analysis
 * - Call state tracking
 * - Auto-mode call detection
 * - Resource cleanup
 */

#include <pcap.h>
#include "packet_capture.h"
#include "packet_utils.h"
#include "call_session.h"
#include "rtp_utils.h"
#include "debug.h"
#include "sip_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <stdatomic.h>
#include <pwd.h>
#include <sys/stat.h>

// Configuration constants for capture behavior
#define AUTO_MODE_TIMEOUT 300  // 5 minutes maximum wait for call
#define RTP_ACTIVITY_THRESHOLD 10  // Minimum RTP packets to consider stream active
#define STATUS_CHECK_INTERVAL 1  // Check status every second
#define SNAPLEN 65535  // Maximum bytes per packet to capture
#define PCAP_BUFFER_SIZE (32 * 1024 * 1024)  // 32MB buffer size
#define PCAP_TIMEOUT_MS 100  // 100ms timeout for immediate packet processing

// Auto mode states
#define AUTO_MODE_WAITING 0
#define AUTO_MODE_CALL_STARTED 1
#define AUTO_MODE_CALL_ENDED 2

// Global state variables for capture session
pcap_t *global_handle = NULL;
pcap_dumper_t *global_dumper = NULL;
static atomic_int capture_stopping = 0;
int capture_time = 0;  // Global capture time limit
extern int auto_mode;  // Reference the global auto_mode variable
int silent_mode = 0;  // Initialize silent mode to off
static time_t last_status_check = 0;

/*
 * Status Check Implementation
 * 
 * Performs periodic checks on capture status to detect:
 * - Stalled captures
 * - Incomplete call setups
 * - Timeouts in various states
 * 
 * This helps maintain capture reliability and prevents
 * hanging in edge cases.
 */
static void check_capture_status(void) {
    time_t now = time(NULL);
    
    // Only check every STATUS_CHECK_INTERVAL seconds
    if (now - last_status_check < STATUS_CHECK_INTERVAL) {
        return;
    }
    
    last_status_check = now;

    // Check for stalled capture conditions
    if (current_session.total_packets > 0) {
        // Check for prolonged period with no SIP or RTP
        if (current_session.last_sip_seen > 0 && 
            (now - current_session.last_sip_seen) >= AUTO_MODE_TIMEOUT &&
            (now - current_session.last_rtp_seen) >= RTP_TIMEOUT) {
            pcap_breakloop(global_handle);
            return;
        }
        
        // Check for incomplete call setup
        if (current_session.dialog.state == DIALOG_TRYING && 
            (now - current_session.last_sip_seen) >= 60) {  // 1 minute timeout for call setup
            pcap_breakloop(global_handle);
            return;
        }
    }
}

/*
 * Signal Handler Implementation
 * 
 * Handles various signals for graceful shutdown:
 * - SIGINT (interrupt)
 * - SIGTERM (termination)
 * - SIGHUP (hangup)
 * - SIGQUIT (quit)
 * 
 * Ensures proper cleanup of resources on program termination
 */
static void signal_handler(int signo) {
    const char *sig_name = NULL;
    
    switch (signo) {
        case SIGINT:
            sig_name = "interrupt";
            break;
        case SIGTERM:
            sig_name = "termination";
            break;
        case SIGHUP:
            sig_name = "hangup";
            break;
        case SIGQUIT:
            sig_name = "quit";
            break;
        default:
            sig_name = "unknown";
    }
    
    if (!silent_mode) {
        printf("\nReceived %s signal, stopping capture...\n", sig_name);
    }
    
    // Set atomic flag for clean shutdown
    atomic_store(&capture_stopping, 1);
    
    // Break pcap loop if handle exists
    if (global_handle) {
        pcap_breakloop(global_handle);
    }
}

// Timer handler for capture time limit
static void timer_handler(int signo __attribute__((unused))) {
    if (!silent_mode) {
        printf("\nCapture time limit reached (%d seconds), stopping...\n", capture_time);
    }
    DEBUG_PRINT("Time limit reached (%d seconds), stopping capture", capture_time);
    pcap_breakloop(global_handle);
}

/*
 * Resource Cleanup
 * 
 * Handles cleanup of:
 * - PCAP handles
 * - Dump files
 * - Associated resources
 * 
 * Called during normal termination and error conditions
 */
static void cleanup_capture(void) {
    // Cleanup resources
    if (global_dumper) {
        pcap_dump_flush(global_dumper);  // Ensure all packets are written
        pcap_dump_close(global_dumper);
        global_dumper = NULL;
    }
    if (global_handle) {
        pcap_close(global_handle);
        global_handle = NULL;
    }
}

/*
 * Capture Completion Check
 * 
 * Determines if capture should be terminated based on:
 * - Call completion in auto mode
 * - Timeout conditions
 * - Error states
 * 
 * Returns:
 * 1 if capture should stop
 * 0 if capture should continue
 */
static int is_capture_complete(void) {
    time_t now = time(NULL);
    
    // Check time limit first if set
    if (capture_time > 0 && 
        (now - current_session.start_time) >= capture_time) {
        DEBUG_PRINT("Time limit reached: %d seconds", capture_time);
        return 1;
    }
    
    // Check for RTP inactivity timeout first
    if (current_session.last_rtp_seen > 0 && 
        (now - current_session.last_rtp_seen) >= RTP_TIMEOUT) {
        return 1;
    }
    
    // Check if call is terminated via SIP
    if (current_session.dialog.state == DIALOG_TERMINATED) {
        // Only proceed if we've seen a BYE
        if (current_session.last_bye_seen > 0) {
            int grace_remaining = RTP_GRACE_PERIOD - (now - current_session.last_bye_seen);
            
            // Wait for grace period after BYE/200 OK
            if (grace_remaining <= 0) {
                return 1;
            }
        }
    }
    
    // Check for initial timeout (no call started)
    if (current_session.dialog.state == DIALOG_INIT && 
        (now - current_session.start_time) >= AUTO_MODE_TIMEOUT) {
        return 1;
    }
    
    // Check for session timeout in auto mode
    if (auto_mode) {
        time_t now = time(NULL);
        
        // Check for call setup timeout
        if (current_session.last_sip_seen > 0 && 
            current_session.sip_state != SIP_STATE_ESTABLISHED &&
            (now - current_session.last_sip_seen) >= AUTO_MODE_TIMEOUT &&
            (now - current_session.last_rtp_seen) >= AUTO_MODE_TIMEOUT) {
            
            DEBUG_PRINT("Call setup timeout in auto mode");
            return 1;  // Exit capture
        }
        
        // Check for call completion
        if (current_session.sip_state == SIP_STATE_TERMINATED &&
            (now - current_session.last_sip_seen) >= 60) {  // 1 minute timeout for call setup
            
            DEBUG_PRINT("Call completed in auto mode");
            return 1;  // Exit capture
        }
    }

    return 0;
}

/*
 * Packet Processing Callback
 * 
 * Called by libpcap for each captured packet
 * Analyzes packets for:
 * - SIP signaling
 * - RTP media
 * - Related protocols
 * 
 * Updates call state and triggers appropriate actions
 * based on packet content
 */
void process_packet(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    static int error_count = 0;
    const int MAX_ERRORS = 10;

    // Add periodic status check for other conditions
    check_capture_status();

    if (!header || !packet) {
        if (++error_count >= MAX_ERRORS) {
            if (!silent_mode) {
                printf("\nCapture stopping: Too many invalid packets\n");
            }
            pcap_breakloop(global_handle);
        }
        DEBUG_PRINT("Invalid packet or header");
        return;
    }

    // Write packet to pcap file and increment total count first
    pcap_dumper_t *dumper = (pcap_dumper_t *)user;
    if (dumper) {
        pcap_dump((u_char *)dumper, header, packet);
        pcap_dump_flush(dumper);
        current_session.total_packets++;
        if (current_session.total_packets % 100 == 0) {
            DEBUG_PRINT("Processed %u packets total", current_session.total_packets);
        }
    } else {
        if (++error_count >= MAX_ERRORS) {
            if (!silent_mode) {
                printf("\nCapture stopping: Output file error\n");
            }
            pcap_breakloop(global_handle);
        }
        DEBUG_PRINT("Invalid packet dumper");
        return;
    }

    struct ip_header *ip;
    struct udphdr *udp;
    
    if (get_packet_headers(packet, header->len, &ip, &udp) != 0) {
        // Not a UDP/IP packet, skip silently
        return;
    }

    // Calculate payload offset and length
    int ip_header_len = IP_HEADER_LEN(ip->ip_vhl);
    int payload_offset = ip_header_len + sizeof(struct udphdr);
    int total_len = ntohs(ip->ip_len);
    int payload_len = total_len - payload_offset;
    
    if (payload_len <= 0) {
        if (++error_count >= MAX_ERRORS) {
            if (!silent_mode) {
                printf("\nCapture stopping: Too many malformed packets\n");
            }
            pcap_breakloop(global_handle);
        }
        DEBUG_PRINT("Zero or negative payload length: %d", payload_len);
        return;
    }

    const uint8_t *payload = packet + sizeof(struct ether_header) + payload_offset;
    int direction = get_packet_direction(ip);

    // Process SIP packets
    uint16_t sport = ntohs(udp->uh_sport);
    uint16_t dport = ntohs(udp->uh_dport);

    if (sport == 5060 || dport == 5060) {
        // Print first few bytes of payload for debugging
        char preview[32] = {0};
        int preview_len = payload_len > 31 ? 31 : payload_len;
        memcpy(preview, payload, preview_len);
        DEBUG_PRINT("SIP packet found - src=%u dst=%u len=%d preview='%s'", 
                    sport, dport, payload_len, preview);

        process_sip_packet(payload, payload_len, direction);
        DEBUG_PRINT("SIP packet processed - Total: %u, SIP: %u, State: %d", 
                    current_session.total_packets,
                    current_session.sip_packets,
                    current_session.dialog.state);
    }
    
    // Process RTP packets
    if (IS_RTP_PORT(ntohs(udp->uh_dport)) || IS_RTP_PORT(ntohs(udp->uh_sport))) {
        if (is_rtp_packet(payload, payload_len)) {
            process_rtp_packet(header, ip, udp, payload, payload_len, direction);
            current_session.last_rtp_seen = time(NULL);
            error_count = 0;  // Reset error count on successful RTP packet
        }
    }

    // Check for auto-exit condition
    if (auto_mode && is_capture_complete()) {
        DEBUG_PRINT("Auto-exit condition met - State: %d, BYE seen: %ld seconds ago",
                    current_session.dialog.state,
                    time(NULL) - current_session.last_bye_seen);
        DEBUG_PRINT("Final packet count - Total: %u, SIP: %u",
                    current_session.total_packets,
                    current_session.sip_packets);
        pcap_breakloop(global_handle);
    }
}

/*
 * Capture Initialization and Start
 * 
 * Parameters:
 * interface - Network interface to capture from
 * output_file - Optional PCAP file for packet storage
 * 
 * Returns:
 * 0 on success
 * Non-zero on error
 * 
 * Configures and starts packet capture with:
 * - Interface setup
 * - Filter configuration
 * - Callback registration
 * - Output file handling
 */
int start_capture(const char *interface, const char *output_file) {
    if (!interface || !output_file) {
        if (!silent_mode) {
            fprintf(stderr, "Invalid interface or output file\n");
        }
        return PCAP_ERROR;
    }

    // Validate capture time
    if (capture_time < 0) {
        if (!silent_mode) {
            fprintf(stderr, "Invalid capture time: %d (must be >= 0)\n", capture_time);
        }
        return PCAP_ERROR;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    struct bpf_program fp;
    char filter_exp[] = "udp";  // Capture all UDP traffic to ensure we don't miss anything
    bpf_u_int32 net = 0;  // Initialize net to 0

    // Set up signal handlers for graceful shutdown
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGHUP, &sa, NULL) == -1 ||
        sigaction(SIGQUIT, &sa, NULL) == -1) {
        if (!silent_mode) {
            fprintf(stderr, "Could not set up signal handlers: %s\n", strerror(errno));
        }
        return PCAP_ERROR;
    }

    // Set up timer handler if capture time is specified
    if (capture_time > 0) {
        struct sigaction timer_sa;
        memset(&timer_sa, 0, sizeof(timer_sa));
        timer_sa.sa_handler = timer_handler;
        sigemptyset(&timer_sa.sa_mask);
        timer_sa.sa_flags = 0;

        if (sigaction(SIGALRM, &timer_sa, NULL) == -1) {
            if (!silent_mode) {
                fprintf(stderr, "Could not set up timer handler: %s\n", strerror(errno));
            }
            return PCAP_ERROR;
        }
    }

    // Create capture handle
    global_handle = pcap_create(interface, errbuf);
    if (global_handle == NULL) {
        if (!silent_mode) {
            fprintf(stderr, "Couldn't create capture handle for %s: %s\n", interface, errbuf);
        }
        return PCAP_ERROR;
    }

    // Set capture options with error checking
    if (pcap_set_snaplen(global_handle, SNAPLEN) != 0 ||
        pcap_set_promisc(global_handle, 1) != 0 ||
        pcap_set_timeout(global_handle, PCAP_TIMEOUT_MS) != 0 ||
        pcap_set_immediate_mode(global_handle, 1) != 0 ||
        pcap_set_buffer_size(global_handle, PCAP_BUFFER_SIZE) != 0) {
        if (!silent_mode) {
            fprintf(stderr, "Failed to set capture options: %s\n", pcap_geterr(global_handle));
        }
        cleanup_capture();
        return PCAP_ERROR;
    }

    // Show configured buffer size
    if (!silent_mode) {
        printf("Configured capture buffer size: %d bytes\n", PCAP_BUFFER_SIZE);
    }

    // Activate the capture handle
    int status = pcap_activate(global_handle);
    if (status < 0) {
        if (!silent_mode) {
            fprintf(stderr, "Couldn't activate capture: %s\n", pcap_geterr(global_handle));
        }
        cleanup_capture();
        return PCAP_ERROR;
    } else if (status > 0) {
        // Warning, but we can continue
        DEBUG_PRINT("Warning activating capture: %s", pcap_geterr(global_handle));
    }

    // Check if we have permission to capture
    if (pcap_can_set_rfmon(global_handle) == 0) {
        DEBUG_PRINT("Running without monitor mode");
    }

    // Compile and apply the filter
    if (pcap_compile(global_handle, &fp, filter_exp, 0, net) == -1) {
        if (!silent_mode) {
            fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(global_handle));
        }
        cleanup_capture();
        return PCAP_ERROR;
    }

    if (pcap_setfilter(global_handle, &fp) == -1) {
        if (!silent_mode) {
            fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(global_handle));
        }
        pcap_freecode(&fp);
        cleanup_capture();
        return PCAP_ERROR;
    }

    // Open output file for writing packets
    global_dumper = pcap_dump_open(global_handle, output_file);
    if (global_dumper == NULL) {
        if (!silent_mode) {
            fprintf(stderr, "Couldn't open output file %s: %s\n", output_file, pcap_geterr(global_handle));
        }
        pcap_freecode(&fp);
        cleanup_capture();
        return PCAP_ERROR;
    }

    // Add auto-mode timeout
    if (auto_mode) {
        DEBUG_PRINT("Auto mode enabled, waiting for call...");
    }

    // Start the capture loop
    if (!silent_mode) {
        printf("Starting packet capture on interface %s\n", interface);
        if (capture_time > 0) {
            printf("Capture will stop after %d seconds\n", capture_time);
        }
        printf("Press Ctrl+C to stop capture\n");
    }

    // Initialize session start time right before capture starts
    current_session.start_time = time(NULL);
    
    // Set alarm if capture time is specified
    if (capture_time > 0) {
        alarm(capture_time);
    }
    
    // Start capture loop
    int result = pcap_loop(global_handle, -1, process_packet, (u_char *)global_dumper);

    // Cancel alarm if it was set
    if (capture_time > 0) {
        alarm(0);
    }

    // Cleanup
    pcap_freecode(&fp);
    cleanup_capture();

    if (result == -1 && !capture_stopping) {
        if (!silent_mode) {
            fprintf(stderr, "Capture loop failed: %s\n", pcap_geterr(global_handle));
        }
        return PCAP_ERROR;
    }

    return PCAP_SUCCESS;
}