/*
 * Debug and Logging Implementation
 * 
 * This module implements debugging and logging functionality for the
 * Call Monitor and Analyzer Program (CMAP). It provides control flags
 * and functions for managing debug output.
 *
 * Key Features:
 * - Debug mode control
 * - Selective message filtering
 * - Packet count tracking
 * - Color-coded output
 */

#include "debug.h"

/*
 * Global Debug Configuration
 * Controls the overall debug output behavior
 */
extern int debug_mode;       // Global debug mode flag

/*
 * Message Filtering
 * Controls which types of messages are displayed
 */
int rtp_msg_only = 0;       // When true, show only RTP-related messages

/*
 * Packet Count Display
 * Controls display of packet count statistics
 */
int show_packet_count = 0;   // When true, show running packet counts