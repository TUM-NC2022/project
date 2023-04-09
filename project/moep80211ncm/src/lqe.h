#ifndef _LQE_H_
#define _LQE_H_

#include <arpa/inet.h>

#include <moep/system.h>
#include <moep/types.h>
#include <moep/modules/moep80211.h>

#include "session.h"

// PZ - Struct that holds info about the session that is pushed via LQE socket
typedef struct
{
    // From session
    char session[32];
    int count;
    float TX_data;
    float TX_ack;
    float RX_data;
    float RX_ack;
    float RX_EXCESS_DATA;
    float RX_LATE_DATA;
    float RX_LATE_ACK;
    float TX_REDUNDANT;
    float redundancy;
    float uplink;
    int p;
    int q;
    float downlink;
    float qdelay;

    // From radiotap header
    int rate;
    int channelFrequency;
    int channelFlags;
    int signal;
    int noise;
    int lockQuality;
    int TX_attenuation;
    int TX_attenuation_dB;
    int TX_power;
    int antenna;
    int signal_dB;
    int noise_dB;
    int RTS_retries;
    int DATA_retries;
    int MCS_known;
    int MCS_flags;
    int MCS_MCS;

    // Remote address + Master/Slave
    int remoteAddress;
    int masterOrSlave; // -1 neither, 0 slave, 1 master
} lqe_info_data;

struct thread_data
{
    int arg1;
    char arg2[20];
};

typedef struct
{
    int socket;
    struct in_addr peer_address;
} lqe_connection_test_data;

// PZ
void lqe_push_data(session_t s, struct moep80211_radiotap *rt, int socket);
void start_connection_test(lqe_connection_test_data lqe_connection_test_data);
void *connection_test_thread(void *arg);

#endif