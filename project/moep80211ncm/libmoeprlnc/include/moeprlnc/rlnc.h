#ifndef __RLNC_H_
#define __RLNC_H_

#include <stdint.h>
#include <stdlib.h>

//#include "global.h"
#include <moepgf/moepgf.h>

#define RLNC_STRUCTURED 0x1

/* Forward declaration for typedef */
struct rlnc_block;

/* Nobody should look inside an rlnc block */
typedef struct rlnc_block * rlnc_block_t;

/* Functions to init, free, and reset (zero-out memory, do not touch paramters
 * such as packet count, and to not deallocate/reallocate memory) */
rlnc_block_t	rlnc_block_init(int count, size_t dlen, size_t alignment,
						enum MOEPGF_TYPE gftype);
void		rlnc_block_free(rlnc_block_t b);
int		rlnc_block_reset(rlnc_block_t b);

/* Functions to add source frames, add/decode encoded frames, encode frames, and
 * get (return) decoded frames if available. */
int 	rlnc_block_add(rlnc_block_t b, int pv, const uint8_t *data, size_t len);
int 	rlnc_block_decode(rlnc_block_t b, const uint8_t *src, size_t len);
ssize_t	rlnc_block_encode(const rlnc_block_t b, uint8_t *dst, size_t maxlen, int flags);
ssize_t	rlnc_block_get(rlnc_block_t b, int pv, uint8_t *dst, size_t maxlen);

/* Temporary helper functions that may become static in the future. */
void 	print_block(const rlnc_block_t b);

/* Interface to query stats from an rlnc block. */
int	rlnc_block_rank_encode(const rlnc_block_t b);
int	rlnc_block_rank_decode(const rlnc_block_t b);
ssize_t	rlnc_block_current_frame_len(const rlnc_block_t b);

#endif
