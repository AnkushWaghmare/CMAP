/*
 * Command Line Interface (CLI) Module
 * 
 * This module provides the user interface components for the Call Monitor
 * and Analyzer tool. It handles display formatting, progress updates,
 * and statistics presentation.
 *
 * Key Features:
 * - Network interface listing
 * - Real-time progress display
 * - Call statistics presentation
 * - Formatted output handling
 */

#ifndef CLI_INTERFACE_H
#define CLI_INTERFACE_H

#include "call_session.h"

/*
 * Global control flag for statistics display
 * When enabled, shows real-time statistics updates
 */
extern int stats_display_enabled;

/*
 * List Available Network Interfaces
 * 
 * Displays all network interfaces available for capture
 * Shows interface name, description, and status
 */
void list_interfaces(void);

/*
 * Show Current Progress
 * 
 * Displays real-time capture statistics including:
 * - Packets captured
 * - Call state
 * - Stream information
 * - Quality metrics
 */
void show_progress(void);

/*
 * Show Final Statistics
 * 
 * Displays comprehensive capture summary including:
 * - Total duration
 * - Packet counts
 * - Call quality metrics
 * - Stream statistics
 */
void show_final_stats(void);

/*
 * Get Final Statistics
 * 
 * Retrieves capture statistics in string format
 * 
 * Parameters:
 * - session: Call session to get stats from
 * - buffer: Output buffer for formatted stats
 * - buffer_size: Size of output buffer
 */
void get_final_stats(struct call_session *session, char *buffer, size_t buffer_size);

#endif // CLI_INTERFACE_H