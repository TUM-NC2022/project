#ifndef __GENERATION_H_
#define __GENERATION_H_

#include <sys/types.h>

#include <moeprlnc/rlnc.h>
#include <moepgf/moepgf.h>
#include <moepcommon/list.h>

#define EGENNOMEM	-1	// packet too large
#define EGENLOCKED	-2	// tried to add packet but generation is locked
#define EGENINVAL	-3	// invalid function for generation type
#define EGENNOMORE	-4	// no more packets available 
				// (everything has been returned)
#define EGENFAIL	-5	// general error has occured

enum generation_taskno {
	GEN_TASK_RTX = 0,
	GEN_TASK_ACK,
	GEN_TASK_CNT,
};

struct ncm_hdr_coded;

struct generation;
typedef struct generation * generation_t;

struct session;
typedef struct session * session_t;

/* Defines the generation type, which is essential to decide which endpoint uses
   which pivot elements: Generation master shall be the node with the
   lexicographic smaller MAC address. Given a generation of N packets, the
   master uses the first N/2 slots while the slave uses the second N/2 slots.
   A forwarder is an intermediate node and does not generate source packets.
   Forwarders simply call generation_decoder_add_pdu() for every incoming
   packet, which is guaranteed to succeed. Encoded packets can be generated by
   calling generation_encoder_get_pdu(). */
enum GENERATION_TYPE {
	MASTER	= 0,
	SLAVE	= 1,
	FORWARD	= 2,
};

struct generation_packet_counter {
	int data;
	int redundant;
	int excess;
	int ack;
};

struct generation_flowstate {
	int	sdim;	// source dimension
	int	ddim;	// destination dimension
	int	lock;	// lock bit
};

struct generation_state {
	struct generation_flowstate fms;	// flow master -> slave
	struct generation_flowstate fsm;	// flow master <- slave
	struct generation_flowstate *local;	// points to fms (fsm)
	struct generation_flowstate *remote;	// points to fsm (fms)

	struct generation_packet_counter tx;
	struct generation_packet_counter rx;
};


struct generation_feedback {
	struct {
	       uint8_t ms:1;
	       uint8_t sm:1;
	       uint8_t unused:6;
	} __attribute__ ((packed)) lock;
	struct {
	       uint8_t ms;
	       uint8_t sm;
	} __attribute__ ((packed)) ddim;
	struct {
	       uint8_t ms;
	       uint8_t sm;
	} __attribute__ ((packed)) sdim;
} __attribute__ ((packed));

/* Initializes and returns a new generation. */
generation_t	generation_init(session_t s, struct list_head *glhead,
				enum GENERATION_TYPE gentype, 
				enum MOEPGF_TYPE gftype, int packet_count, 
				size_t packet_size, int sequence_number);
/* Frees a generation. */
void		generation_list_destroy(struct list_head *gl);

/* Resets a generation without (de)allocating memory. Resets everything to
   safe/initial values. */
int		generation_reset(generation_t g, uint16_t seq);

/* Returns an encoded PDU (encoded packet including generation header). Return
   value is positive on success, 0 if no packet is available, and -1 on error.
 */
ssize_t		generation_encoder_get(const generation_t g,
					void *buffer, size_t maxlen);

/* Returns a generation header filled with the current generation state. */
ssize_t generation_feedback(generation_t g, struct generation_feedback *fb,
		size_t maxlen);


/* Returns 1 if the local flow (originating at this node) is locked, and 0
   otherwise. */
int		generation_local_flow_locked(const generation_t g);

/* Returns 1 if the remote flow (originating at the remote node) is locked, and
   0 otherwise. */
int		generation_remote_flow_locked(const generation_t g);

/* Returns 1 if both flows are locked, and 0 otherwise. */
int		generation_is_locked(const generation_t g);

/* Returns 1 if the local flow is decoded, i.e., the remote node was able to
   decode everything it has received so far, and 0 otherwise. */
int		generation_local_flow_decoded(const generation_t g);

/* Returns 1 if the remote flow is decoded, i.e., the local node was able to
   decode everything it has received so far, and 0 otherwise. */
int		generation_remote_flow_decoded(const generation_t g);

/* Returns 1 if both flows are decoded, and 0 otherwise. */
int		generation_is_decoded(const generation_t g);

/* Returns 1 if both flows are decoded and locked, i.e., this generation can be
   deleted. */
int		generation_is_complete(const generation_t g);

/* Returns 1 if all packets available in the decoder have been returned and 0
 * otherwise. If the decoder contains packets that cannot be decoded yet, 1 is
 * returned. */
int		generation_is_returned(const generation_t g);

/* Returns the remaining space in the encoder of this generation. */
int		generation_encoder_space(const generation_t g);

int		generation_encoder_dimension(const generation_t g);
int		generation_decoder_dimension(const generation_t g);



int		generation_assume_complete(generation_t g);

/* Prints the current state of a generation:
 * local.sdim | local.ddim | local.lock || remote.sdim | remote.ddim | remote.lock
 *
 * sdim = source dimension (at the sender)
 * ddim = destination dimension (at the receiver)
 * lock = 0 (not locked) or 1 (locked)
 * local  = flow that is originating locally
 * remote = flow that is originating at the remote side
 *
 * Example: A sends 1 packet to B, then we have:
 * A: 1 | 0 | 0 || 0 | 0 | 0
 * B: 0 | 0 | 0 || 1 | 1 | 0
 * The first line tells that A has generated a source packet but does not yet
 * know whether or not B has received the packet, i.e., local.ddim remains 0.
 * The second line tells that B has received the packet (remote.ddim = 1) and
 * that it knows about the subspace dimension at A (remote.sdim = 1). Since both
 * values match, B can conclude that the remote flow is currently in decoded
 * state.
 * Now assume that B replies with a packet containing an acknowledgement and
 * data. After A has received that packet, we have:
 * A: 1 | 1 | 0 || 1 | 1 | 0
 * B: 1 | 0 | 0 || 1 | 1 | 0
 * The first line should be obvious. The second line lacks a 1-entry for
 * local.ddim. This is because B has not yet received an acknowledgement. */
void		generation_debug_print_state(const generation_t g);

void		generation_add(struct list_head *head, generation_t g);


generation_t	generation_encoder_add(struct list_head *head, void *buffer,
		size_t len);

ssize_t		generation_decoder_get(struct list_head *gl, void *buffer,
								size_t maxlen);

generation_t	generation_decoder_add(struct list_head *gl,
					const void *payload, size_t len,
					const struct ncm_hdr_coded *hdr);

generation_t
generation_get(const struct list_head *gl, int idx);

int generation_window_size(const struct list_head *gl);

void generation_reschedule_rtx_timeout(generation_t g, unsigned int timeout);
void generation_disable_rtx_timeout(generation_t g);
void generation_reschedule_ack_timeout(generation_t g);
void generation_disable_ack_timeout(generation_t g);
session_t generation_get_session(generation_t g);

int generation_local_flow_missing(const generation_t g);

int generation_idx(const generation_t g);

int generation_window_size(const struct list_head *gl);
int generation_index(const generation_t g);

int generation_remaining_space(const struct list_head *gl);

int generation_lseq(const struct list_head *gl);
int generation_seq(const generation_t g);
void generation_add_tx(generation_t g);

#endif // __GENERATION_H_
