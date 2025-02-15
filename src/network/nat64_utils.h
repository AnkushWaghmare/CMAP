/*
 * NAT64 (Network Address Translation IPv6 to IPv4) Utilities
 * 
 * This module provides functionality for handling NAT64 address translation
 * in VoIP traffic monitoring. It helps identify and process addresses that
 * are part of NAT64 translation.
 *
 * Key Features:
 * - NAT64 address detection
 * - IPv6 to IPv4 translation support
 * - Address validation
 */

#ifndef NAT64_UTILS_H
#define NAT64_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * Check if Address is NAT64
 * 
 * Determines if an IP address is part of NAT64 translation
 * 
 * Parameters:
 * - addr: IP address string to check
 * 
 * Returns:
 * 1 if address is NAT64
 * 0 if not NAT64
 */
int is_nat64_address(const char *addr);

/*
 * Extract IPv4 Address from NAT64 Address
 * 
 * Extracts the embedded IPv4 address from a NAT64 IPv6 address.
 * NAT64 addresses contain an embedded IPv4 address in the last 32 bits.
 * 
 * Parameters:
 * - nat64_addr: NAT64 IPv6 address string
 * - ipv4_buf: Buffer to store extracted IPv4 address
 * - buf_size: Size of ipv4_buf
 * 
 * Returns:
 * - 1 on success
 * - 0 on failure or invalid input
 */
int extract_ipv4_from_nat64(const char *nat64_addr, char *ipv4_buf, size_t buf_size);

#endif // NAT64_UTILS_H