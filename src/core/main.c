/*
 * Call Monitor and Analyzer (cmap) - Main Program
 * 
 * This is the main entry point for the Call Monitor and Analyzer tool.
 * The program captures and analyzes VoIP (Voice over IP) traffic,
 * specifically SIP signaling and RTP media streams.
 *
 * Key Features:
 * - Real-time packet capture from network interfaces
 * - SIP signaling analysis
 * - RTP media stream tracking
 * - PCAP file output support
 * - Auto-mode for automated call capture
 * - Debug and silent operation modes
 */

#include "packet_capture.h"
#include "cli_interface.h"
#include "cli_style.h"
#include "debug.h"
#include "call_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>

// Function to get desktop path
static char* get_desktop_path(void) {
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    static char desktop_path[PATH_MAX];
    snprintf(desktop_path, PATH_MAX, "%s/Desktop", homedir);
    return desktop_path;
}

// Global variables for program configuration
int debug_mode = 0;  // Global debug mode flag
int auto_mode = 0;   // Global auto mode flag
extern int silent_mode;  // Reference to silent mode flag
extern int stats_display_enabled;  // Reference to stats display enabled flag
static char *interface = NULL;
static char *output_file = NULL;
extern int capture_time;  // Global capture time limit in seconds

// Global session
struct call_session current_session;

/*
 * Signal Handler for Clean Program Termination
 * Handles SIGINT (Ctrl+C) by cleaning up resources and exiting gracefully
 */
static void handle_signal(int signo) {
    if (signo == SIGINT) {
        printf("\nReceived interrupt signal, stopping capture...\n");
        cleanup_call_session(&current_session);
        if (output_file && strchr(output_file, '/') == NULL) {
            free(output_file);
        }
        exit(0);
    }
}

// Signal handler setup
static struct sigaction sa = {
    .sa_handler = handle_signal,
    .sa_flags = 0
};

/*
 * Command Line Options Configuration
 * Supports both short (-x) and long (--option) format arguments
 * for flexible program control
 */
static struct option long_options[] = {
    {"interface", required_argument, 0, 'i'},
    {"output",    required_argument, 0, 'O'},
    {"time",      required_argument, 0, 't'},
    {"auto",      no_argument,       0, 'a'},
    {"debug",     no_argument,       0, 'd'},
    {"list",      no_argument,       0, 'l'},
    {"help",      no_argument,       0, 'h'},
    {"version",   no_argument,       0, 'v'},
    {"silent",    no_argument,       0, 's'},
    {0, 0, 0, 0}
};

/*
 * Display program version information including:
 * - Program name and version
 * - Platform details
 * - Build timestamp
 */
void print_version(void) {
    printf("%sCall Monitor and Analyzer (cmap) v1.0.0%s\n", 
           ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
    printf("Platform: macOS ARM64\n");
    printf("Compiled: %s %s\n", __DATE__, __TIME__);
}

/*
 * Print usage instructions and available command line options
 * Provides a comprehensive help message for program usage
 */
void print_usage(void) {
    printf("Usage: cmap [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -i, --interface <if>  Interface to capture from\n");
    printf("  -O, --output <file>   Output file (pcap format)\n");
    printf("  -t, --time <seconds>  Stop after specified time\n");
    printf("  -a, --auto           Auto mode - stop when call ends\n");
    printf("  -d, --debug          Enable debug output\n");
    printf("  -l, --list           List available interfaces\n");
    printf("  -s, --silent         Suppress all output\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -v, --version        Show version information\n");
}

/*
 * Main Program Entry Point
 * 
 * Command line arguments:
 * -i, --interface: Network interface to capture from
 * -O, --output: Output file in PCAP format
 * -t, --time: Capture duration in seconds
 * -a, --auto: Auto mode - stop when call ends
 * -d, --debug: Enable debug output
 * -l, --list: List available interfaces
 * -s, --silent: Suppress all output
 * -h, --help: Show help message
 * -v, --version: Show version information
 *
 * Returns:
 * 0 on successful execution
 * Non-zero on error
 */
int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "i:O:t:adhlvs", 
                            long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                interface = optarg;
                break;
            case 'O': {
                if (strchr(optarg, '/') == NULL) {
                    // Only filename provided, prepend desktop path
                    char *desktop = get_desktop_path();
                    char *full_path = malloc(PATH_MAX);
                    snprintf(full_path, PATH_MAX, "%s/%s", desktop, optarg);
                    output_file = full_path;
                } else {
                    output_file = optarg;
                }
                break;
            }
            case 't':
                capture_time = atoi(optarg);
                break;
            case 'a':
                auto_mode = 1;
                break;
            case 'd':
                debug_mode = 1;
                break;
            case 's':
                silent_mode = 1;
                break;
            case 'h':
                print_usage();
                return 0;
            case 'v':
                print_version();
                return 0;
            case 'l':
                list_interfaces();
                return 0;
            default:
                print_usage();
                return 1;
        }
    }

    if (!interface || !output_file) {
        print_usage();
        return 1;
    }

    // Initialize signal handler
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // Initialize call session
    memset(&current_session, 0, sizeof(current_session));
    current_session.start_time = time(NULL);

    if (debug_mode) {
        DEBUG_PRINT("Starting capture with options:");
        DEBUG_PRINT("  Interface: %s", interface);
        DEBUG_PRINT("  Output: %s", output_file);
        DEBUG_PRINT("  Auto mode: %s", auto_mode ? "enabled" : "disabled");
        DEBUG_PRINT("  Capture time: %d seconds", capture_time);
    }

    // Start packet capture
    int result = start_capture(interface, output_file);
    if (result != PCAP_SUCCESS) {
        return result;
    }

    cleanup_call_session(&current_session);
    if (output_file && strchr(output_file, '/') == NULL) {
        free(output_file);
    }
    return 0;
}