#ifndef _NCM_H_
#define _NCM_H_

#include <moep/system.h>
#include <moep/types.h>
#include <moep/ieee80211_addr.h>
#include <moep/ieee80211_frametypes.h>

#include "frametypes.h"

// PZ
struct lqe_socket
{
	int client_fd;
	int port;
};

typedef struct lqe_socket lqe_socket;

int ncm_frame_type(moep_frame_t frame);

/*
 * Returns the mac address of the tap interface.
 */
u8 *ncm_get_local_hwaddr();

/*
 * Functions to transmit frames on radio and tap interfaces. The supplied frames
 * are copied, i.e., memory must be deallocated by the caller.
 */
int rad_tx(struct moep_frame *f);
int tap_tx(struct moep_frame *f);

int rad_tx_ready();
int tap_tx_ready();

moep_frame_t create_rad_frame();

static inline int
fd_ready(int fd)
{
	fd_set fds;
	struct timeval t;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	t.tv_sec = 0;
	t.tv_usec = 0;

	return select(fd + 1, &fds, NULL, NULL, &t);
}

#endif // _NCM_H_
