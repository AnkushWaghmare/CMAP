/*
 * Command Line Interface Styling Utilities
 * 
 * This module provides ANSI escape codes and styling utilities for
 * creating a rich terminal user interface. It includes definitions
 * for colors, text styles, and cursor control.
 *
 * Key Features:
 * - Text color control
 * - Background color control
 * - Text styling (bold, italic, etc.)
 * - Cursor movement
 * - Line clearing
 */

#ifndef CLI_STYLE_H
#define CLI_STYLE_H

#include "call_session.h"

/*
 * ANSI Color Codes
 * Foreground text colors for terminal output
 */
#define ANSI_COLOR_RED     "\x1b[31m"  // Error messages
#define ANSI_COLOR_GREEN   "\x1b[32m"  // Success messages
#define ANSI_COLOR_YELLOW  "\x1b[33m"  // Warnings
#define ANSI_COLOR_BLUE    "\x1b[34m"  // Information
#define ANSI_COLOR_MAGENTA "\x1b[35m"  // Special highlights
#define ANSI_COLOR_CYAN    "\x1b[36m"  // Secondary information
#define ANSI_COLOR_RESET   "\x1b[0m"   // Reset to default

/*
 * Text Style Codes
 * ANSI escape sequences for text formatting
 */
#define ANSI_BOLD          "\x1b[1m"   // Bold text
#define ANSI_DIM           "\x1b[2m"   // Dimmed text
#define ANSI_ITALIC        "\x1b[3m"   // Italic text
#define ANSI_UNDERLINE     "\x1b[4m"   // Underlined text
#define ANSI_BLINK         "\x1b[5m"   // Blinking text
#define ANSI_REVERSE       "\x1b[7m"   // Reversed colors
#define ANSI_HIDDEN        "\x1b[8m"   // Hidden text
#define ANSI_STRIKE        "\x1b[9m"   // Strikethrough text

/*
 * Background Color Codes
 * ANSI escape sequences for background colors
 */
#define ANSI_BG_RED        "\x1b[41m"  // Red background
#define ANSI_BG_GREEN      "\x1b[42m"  // Green background
#define ANSI_BG_YELLOW     "\x1b[43m"  // Yellow background
#define ANSI_BG_BLUE       "\x1b[44m"  // Blue background
#define ANSI_BG_MAGENTA    "\x1b[45m"  // Magenta background
#define ANSI_BG_CYAN       "\x1b[46m"  // Cyan background
#define ANSI_BG_WHITE      "\x1b[47m"  // White background

/*
 * Cursor Control Codes
 * ANSI escape sequences for cursor movement
 */
#define ANSI_CURSOR_UP     "\x1b[1A"   // Move cursor up
#define ANSI_CURSOR_DOWN   "\x1b[1B"   // Move cursor down
#define ANSI_CURSOR_RIGHT  "\x1b[1C"   // Move cursor right
#define ANSI_CURSOR_LEFT   "\x1b[1D"   // Move cursor left
#define ANSI_CLEAR_LINE    "\x1b[2K"   // Clear current line

/*
 * Display Functions
 * Functions for showing progress and statistics
 */
void show_progress(void);      // Show real-time progress
void show_final_stats(void);   // Show final statistics

#endif // CLI_STYLE_H