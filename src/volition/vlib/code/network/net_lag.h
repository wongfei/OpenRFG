// Original: volition/vlib/code/network/net_lag.h
// Implementation: volition/vlib/code/network/net_lag.cpp
//
// Simulated network lag and loss for testing (wraps recvfrom/select); lag_buf is a delayed packet. Placement of
// lag_buf inferred.
#pragma once

#include <winsock2.h>
#include "volition/vlib/code/time/timestamp.h"

struct fd_set;
class mempool;
struct sockaddr;
struct timeval;

// size 0x4D8
struct lag_buf
{
    lag_buf(); // 0x0058AAB0

    unsigned char data[1202]; // 0x00
    int data_len; // 0x4B4
    unsigned int socket; // 0x4B8
    timestamp stamp; // 0x4BC
    sockaddr_in ip_addr; // 0x4C0
    lag_buf* prev; // 0x4D0
    lag_buf* next; // 0x4D4
};

void net_lag_init(mempool* pool); // 0x005AC7F0 (:124)
void net_lag_close(); // 0x0058AAC0 (:175)
int net_lag_select(unsigned int s, int nfds, fd_set* readfds, fd_set* writefds, fd_set* except_fds, const timeval* timeout); // 0x0058AAE0 (:194)
int net_lag_recvfrom(unsigned int s, char* buf, int len, int flags, sockaddr* from, int* fromlen); // 0x005AC810 (:288)
int net_lag_get_random_lag(); // 0x005AC930 (:323)
int net_lag_should_be_lost(); // 0x005ACA40 (:369)
lag_buf* net_lag_get_free(); // 0x0058AAF0 (:402)
void net_lag_put_free(lag_buf* buf); // 0x0058AB70 (:427)
void net_lagloss_dcf(); // 0x005BCB20 (:440)
void unused_dcf_mp_lag(); // 0x005BCC70 (:465)
void unused_dcf_mp_lag_min(); // 0x005BCCE0 (:495)
void unused_dcf_mp_lag_max(); // 0x005BCD30 (:520)
void unused_dcf_mp_loss(); // 0x005BCD80 (:545)
void unused_dcf_mp_loss_min(); // 0x005BCE40 (:579)
void unused_dcf_mp_loss_max(); // 0x005BCEB0 (:607)
void unused_dcf_mp_laginfo(); // 0x005BCF20 (:635)
void unused_dcf_mp_lag_streak(); // 0x005BCF30 (:646)
void unused_dcf_mp_lag_bad(); // 0x005BCF70 (:664)
void unused_dcf_mp_lag_tcr(); // 0x005BD000 (:688)
void unused_dcf_mp_lag_med(); // 0x005BD080 (:712)
void unused_dcf_mp_lag_good(); // 0x005BD110 (:736)
void unused_dcf_mp_lag_none(); // 0x005BD1A0 (:760)
