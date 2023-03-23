#ifndef _SESSION_H_
#define _SESSION_H_

#include <moep/system.h>
#include <moep/types.h>
#include <moep/modules/moep80211.h>

#include <moepgf/moepgf.h>

#include <jsm.h>
#include "params.h"

/**
 * Global statistics of this sesssion:
 * @rx.data: number of (coded) data packets received.
 * @rx.ack: number of explicit acks reveived.
 * @rx.late_data: number of (coded) data packets reveived for past generation.
 * @rx.late_ack: number of explicit acks reveied for past generation.
 * @rx.excess_data: FIXME still valid?
 * tx.data: number of (coded) data packes sent.
 * tx.ack: number of explicit acknowledgements sent.
 * tx.redundant: number of redundant (coded) data packets sent, not account for
 * random linear dependencies.
 * count: number of passed generations.
 */
struct session_state
{
    struct
    {
        int data;
        int ack;
        int late_data;
        int late_ack;
        int excess_data;
    } rx;
    struct
    {
        int data;
        int ack;
        int redundant;
    } tx;
    int count;
};

struct session_tasks
{
    timeout_t destroy;
};

struct session
{
    struct list_head list;
    struct params_session params;
    struct jsm80211_module *jsm_module;

    // FIXME we do not want unnamed unions
    union
    {
        u8 sid[2 * IEEE80211_ALEN];
        struct
        {
            u8 master[IEEE80211_ALEN];
            u8 slave[IEEE80211_ALEN];
        } hwaddr;
    };
    enum GENERATION_TYPE gentype;

    struct session_state state;
    struct session_tasks task;

    struct list_head gl;
};
typedef struct session *session_t;

session_t
session_register(const struct params_session *params, const struct params_jsm *jsm, const u8 *sid);

session_t session_find(const u8 *sid);

int session_sid(u8 *sid, const u8 *hwaddr1, const u8 *hwaddr2);

void session_decoder_add(session_t s, moep_frame_t f);

int session_encoder_add(session_t s, moep_frame_t f);

int tx_decoded_frame(struct session *s);
int tx_encoded_frame(struct session *s, generation_t g);
int tx_ack_frame(struct session *s, generation_t g);

void session_commit_state(struct session *s, const struct generation_state *state);

void session_log_state();

void session_cleanup();

double session_redundancy(session_t s);

int session_remaining_space(const session_t s);

int session_min_remaining_space();

double session_ul_quality(session_t s);

u8 *session_find_remote_address(struct session *s);

#endif
