/*
 * lwip.h
 *
 *  Created on: Jul 19, 2017
 *      Author: admin
 */

#ifndef LWIP_2_0_2_TEST_LINUX_LWIP_H_
#define LWIP_2_0_2_TEST_LINUX_LWIP_H_

#include <lwip/dhcp.h>
#include <lwip/igmp.h>
#include <lwip/inet.h>
#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/tcp.h>
#include <lwip/timeouts.h>
#include <lwip/udp.h>
#include <netif/etharp.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "lwip/arch.h"

#define TCP_SERVER_PRIO TCP_PRIO_MIN
// #define TCP_REMOTE_SERVER_ADDR ((178 << 24) | (145 << 16) | (105 << 8) | (128))
#define TCP_REMOTE_SERVER_ADDR ((12 << 24) | (2 << 16) | (168 << 8) | (192))
#define TCP_REMOTE_SERVER_PORT 8000
#define TCP_LOCAL_SERVER_PORT 8000

#define ECHO_SERVER 1
#define TCP_CLIENT 2
#define CONT_NF 3
#define TEST_ID 1

err_t
net_init(char *ifname);
void
net_quit(void);
pthread_t
start_netif(void);
struct netif *
get_netif(void);
int
lwip_linux_check_port(u16_t port);
int
get_if_address(const char *ifname, uint32_t *ip, uint32_t *mask, uint8_t *mac);

err_t
create_echo_server(void);
err_t
tcp_client(const ip_addr_t *ip_addr, u16_t port);
err_t
cont_nf(void);
#endif /* LWIP_2_0_2_TEST_LINUX_LWIP_H_ */
