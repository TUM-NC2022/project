#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <moep/system.h>
#include <moep/types.h>
#include <moep/radiotap.h>
#include <moep/modules/moep80211.h>

#include <moepcommon/timeout.h>

#include "frametypes.h"
#include "ncm.h"
#include "session.h"
#include "qdelay.h"
#include "neighbor.h"
#include "lqe.h"

// Pushes statistics of the current link via the LQE socket
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void lqe_push_data(session_t s, struct moep80211_radiotap *rt, int socket)
{
#pragma GCC diagnostic pop
    u8 *remoteAddress;
    int ret1, ret2;

    int p, q;
    double uplink, downlink;

    lqe_info_data session_info;

    remoteAddress = session_find_remote_address(s);

    // LOG(LOG_INFO, "%u", *remoteAddress);
    session_info.remoteAddress = *remoteAddress;

    ret1 = memcmp(s->hwaddr.master, ncm_get_local_hwaddr(), IEEE80211_ALEN);
    ret2 = memcmp(s->hwaddr.slave, ncm_get_local_hwaddr(), IEEE80211_ALEN);

    if (ret1 == 0 && ret2 == 0)
    {
        session_info.masterOrSlave = -1;
        // LOG(LOG_INFO, "session_push_data: Node neither Master nor Slave");
    }
    else if (ret1 == 0)
    {
        session_info.masterOrSlave = 0;
        // LOG(LOG_INFO, "session_push_data: Node is Slave");
    }
    else
    {
        session_info.masterOrSlave = 1;
        // LOG(LOG_INFO, "session_push_data: Node is Master");
    }

    uplink = nb_ul_quality(session_find_remote_address(s), &p, &q),
    downlink = nb_dl_quality(session_find_remote_address(s), NULL, NULL),

    // Fill the session_info struct with data
        snprintf(session_info.session, sizeof(session_info.session), "%s:%s", ether_ntoa((const struct ether_addr *)s), ether_ntoa((const struct ether_addr *)s + IEEE80211_ALEN));
    session_info.count = s->state.count;
    session_info.TX_data = (float)((double)s->state.tx.data / (double)s->state.count);
    session_info.TX_ack = (float)((double)s->state.tx.ack / (double)s->state.count);
    session_info.RX_data = (float)((double)s->state.rx.data / (double)s->state.count);
    session_info.RX_ack = (float)((double)s->state.rx.ack / (double)s->state.count);
    session_info.RX_EXCESS_DATA = (float)((double)s->state.rx.excess_data / (double)s->state.count);
    session_info.RX_LATE_DATA = (float)((double)s->state.rx.late_data / (double)s->state.count);
    session_info.RX_LATE_ACK = (float)((double)s->state.tx.redundant / (double)s->state.count);
    session_info.TX_REDUNDANT = (float)((double)s->state.tx.redundant / (double)s->state.count);
    session_info.redundancy = (float)session_redundancy(s);
    session_info.uplink = (float)uplink;
    session_info.p = p;
    session_info.q = q;
    session_info.downlink = (float)downlink;
    session_info.qdelay = (float)qdelay_get();

    // Radiotap stuff
    if (rt != NULL)
    {
        session_info.rate = rt->rate;
        session_info.channelFrequency = rt->channel.frequency;
        session_info.channelFlags = rt->channel.flags;
        session_info.signal = rt->signal;
        session_info.noise = rt->noise;
        session_info.lockQuality = rt->lock_quality;
        session_info.TX_attenuation = rt->tx_attenuation;
        session_info.TX_attenuation_dB = rt->tx_attenuation_dB;
        session_info.TX_power = rt->tx_power;
        session_info.antenna = rt->antenna;
        session_info.signal_dB = rt->signal_dB;
        session_info.noise_dB = rt->noise_dB;
        session_info.RTS_retries = rt->rts_retries;
        session_info.DATA_retries = rt->data_retries;
        session_info.MCS_known = rt->mcs.known;
        session_info.MCS_flags = rt->mcs.flags;
        session_info.MCS_MCS = rt->mcs.mcs;
    }

    // Copy in order to make sure that data didn't change while sending via socket
    lqe_info_data session_info_sending = session_info;

    // Send the product data to the client
    if (socket != -1)
    {
        LOG(LOG_INFO, "session_push_data: Pushing data via Socket");

        if (send(socket, &session_info_sending, sizeof(lqe_info_data), 0) < 0)
        {
            LOG(LOG_INFO, "session_push_data: Sending data failed!");
            // exit(EXIT_FAILURE);
        }
    }
    else
    {
        LOG(LOG_INFO, "session_push_data: Socket not open!");
    }
}

// Starts the thread that receives link quality estimations from the socket connection
void start_connection_test(struct connection_test_data connection_test_data)
{
    pthread_t tid;
    int rc;

    // TODO: Pass the peer_address to the thread
    rc = pthread_create(&tid, NULL, connection_test_thread, (void *)&connection_test_data);
    if (rc)
    {
        LOG(LOG_ERR, "Return code from pthread_create() is %d\n", rc);
        exit(EXIT_FAILURE);
    }

    // Detach the thread
    pthread_detach(tid);
}

// Runs in a seperate thread
// Pings a specified peer address and prints the received link quality estimations
void *connection_test_thread(void *arg)
{
    sleep(3); // Wait for the connection to be established between both nodes

    struct connection_test_data *data = (struct connection_test_data *)arg;
    FILE *fp;
    char *peer_address_string = inet_ntoa(data->peer_address);
    char ping_cmd[100];
    sprintf(ping_cmd, "ping -c 5 -i 1 -s 1200 %s &", peer_address_string); // 5 times, 1 second interval, 1200 bytes payload
    LOG(LOG_INFO, "connection_test_thread: %s", ping_cmd);
    char buffer[128];

    // Execute the ping command
    system(ping_cmd);

    // Wait for incoming data from LQE socket and print it (should be exacly 5 data points)
    int values[5];
    for (int i = 0; i < 5; i++)
    {
        int bytes_read = recv(data->socket, &values[i], sizeof(values[i]), 0);
        if (bytes_read < 0)
        {
            perror("recv failed");
            exit(1);
        }
        if (bytes_read == 0)
        {
            printf("Socket closed\n");
            exit(0);
        }
        values[i] = ntohl(values[i]);
        LOG(LOG_INFO, "Received: %d", values[i]);
    }

    pthread_exit(NULL); 
}