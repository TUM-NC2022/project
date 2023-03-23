#include <string.h>
#include <stdio.h>

#include <moeprlnc/rlnc.h>
#include <moepcommon/util.h>

struct slot {
	uint16_t	len;
	uint8_t		data[0];
} __attribute__ ((packed));

struct length {
        unsigned int max;
	unsigned int max_data;
        unsigned int coeff;
        unsigned int cc;
};

struct rank {
	int encode;
	int decode;
	int max;
};

struct rlnc_block {
	uint8_t 	*buffer;
	uint8_t 	**slot;

	struct rank	rank;

	struct length 	len;
	size_t  	alignment;

	int     	*pvlist;

	unsigned int 	r_seed;
	struct		moepgf gf;

	int		sent;
	int		encode_start;
};

static inline int
rank(const rlnc_block_t b)
{
	return b->rank.encode + b->rank.decode;
}

static inline uint8_t
get_coefficient(const rlnc_block_t b, int x, int y)
{
	int byte = (y * b->gf.exponent) / 8;
	int pos  = (y * b->gf.exponent) % 8;

	return (b->slot[x][byte] >> pos) & b->gf.mask;
}

static inline void
set_coefficient(rlnc_block_t b, int x, int y, uint8_t c)
{
	int byte = (y * b->gf.exponent) / 8;
	int pos  = (y * b->gf.exponent) % 8;

	b->slot[x][byte] &= ~(b->gf.mask << pos);
	b->slot[x][byte] |= (c & b->gf.mask) << pos;
}

void
print_block(const rlnc_block_t b)
{
	int x,y;

	for (x=0; x<b->rank.max; x++) {
		for (y=0; y<b->rank.max; y++)
			fprintf(stdout, "%02x ", get_coefficient(b,x,y));
		fprintf(stdout, "\n");
	}

	fprintf(stdout, "\n");
}

static int
is_decoded(const rlnc_block_t b, int pv)
{
	int i;

	for (i=0; i<b->rank.max; i++) {
		if (i == pv) {
			if (get_coefficient(b, pv, i) == 0)
				return 0;
			continue;
		}

		if (get_coefficient(b, pv, i))
			return 0;
	}

	return 1;
}

static inline int
find_pivot_position(const rlnc_block_t b, int x)
{
	int i;

	for (i=0; i<b->rank.max; i++) {
		if (get_coefficient(b, x, i))
			return i;
	}

	return -1;
}

rlnc_block_t
rlnc_block_init(int count, size_t dlen, size_t alignment, enum MOEPGF_TYPE gftype)
{
	int i;
	rlnc_block_t b;

	if (NULL == (b = malloc(sizeof(struct rlnc_block)))) {
		LOG(LOG_ERR, "malloc() failed");
		return NULL;
	}

	memset(b, 0, sizeof(*b));

	moepgf_init(&b->gf, gftype, MOEPGF_ALGORITHM_BEST);

	b->len.coeff = count / (8/b->gf.exponent);
	if (count % (8/b->gf.exponent))
		b->len.coeff++;
	b->len.max_data	= dlen;
	b->len.max	= aligned_length(b->len.coeff + sizeof(struct slot)
						+ b->len.max_data, alignment);
	b->rank.max	= count;
	b->alignment	= alignment;

	if (posix_memalign((void *)&b->buffer, alignment,
						(b->len.max)*(count+1))) {
		LOG(LOG_ERR, "posix_memalign() failed");
		return NULL;
	}

	if (NULL == (b->slot = malloc((b->rank.max+1) * sizeof(*b->slot)))) {
		LOG(LOG_ERR, "malloc() failed");
		return NULL;
	}

	for (i=0; i<b->rank.max+1; i++)
		b->slot[i] = &b->buffer[i*b->len.max];

	if (NULL == (b->pvlist = malloc(sizeof(*b->pvlist)*count))) {
		LOG(LOG_ERR, "malloc() failed");
		return NULL;
	}

	(void) rlnc_block_reset(b);

	return b;
}

int
rlnc_block_reset(rlnc_block_t b)
{
	if (!b->buffer)
		return -1;

	memset(b->buffer, 0, b->len.max * (b->rank.max+1));
	memset(b->pvlist, -1, sizeof(*b->pvlist) * b->rank.max);

	b->len.cc	= 0;
	b->rank.encode	= 0;
	b->rank.decode	= 0;

	b->sent = 0;
	b->encode_start = -1;

	return 0;
}

void
rlnc_block_free(rlnc_block_t b)
{
	free(b->buffer);
	free(b->slot);
	free(b->pvlist);
	free(b);
}

ssize_t
rlnc_block_encode(const rlnc_block_t b, uint8_t *dst, size_t maxlen, int flags)
{
	int i, x;
	uint8_t c;
	uint8_t *tmp = b->slot[b->rank.max];

	// Check whether or not an encoded frame can be generated, i.e., if flen
	// is 0, there are currently no frames in this block.
	if (b->len.cc == 0) {
		LOG(LOG_ERR, "block empty, unable to retrieve encoded frame");
		return -1;
	}

	// maxlen must be large enough
	if (maxlen < aligned_length(b->len.cc, b->alignment)) {
		LOG(LOG_ERR, "buffer too small, have %lu, need %lu",
			maxlen, aligned_length(b->len.cc, b->alignment));
		return -1;
	}

	memset(tmp, 0, b->len.max);
	if ((flags & RLNC_STRUCTURED) && b->encode_start > -1 && b->sent < b->rank.encode) {
		b->gf.maddrc(tmp, b->slot[b->sent+b->encode_start],
						1, b->len.cc);
		b->sent++;
	}
	else {
		for (i=0; i<b->rank.max; i++) {
			// Iterate over pivo list. If there are no more pivots
			// available, break.
			x = b->pvlist[i];
			if (x == -1)
				break;

			c = rand_r(&b->r_seed) & b->gf.mask;
			b->gf.maddrc(tmp, b->slot[x], c, b->len.cc);
		}
	}

	memcpy(dst, tmp, b->len.cc);

	return b->len.cc;
}

int
rlnc_block_decode(rlnc_block_t b, const uint8_t *src, size_t len)
{
	int i, pv, pvpos;
	uint8_t inv, c;
	uint8_t *tmp = b->slot[b->rank.max];

	if (len > b->len.max)
		return -1;

	if (rank(b) == b->rank.max)
		return 0;

	// Copy encoded data into the spare slot
	memset(tmp, 0, b->len.max);
	memcpy(tmp, src, len);

	b->len.cc = max_t(size_t, b->len.cc, len);

	// forward substitution
	for (i=0; i<rank(b); i++) {
		pvpos = b->pvlist[i];
		pv = get_coefficient(b, b->rank.max, pvpos);

		if (pv == 0)
			continue;

		b->gf.maddrc(tmp, b->slot[pvpos], pv, b->len.cc);
	}

	pvpos = find_pivot_position(b, b->rank.max);
	// If true, the packet was linear dependent and thus eliminated
	if (pvpos == -1)
		return 0;

	// Make the new pivot element 1
	pv = get_coefficient(b, b->rank.max, pvpos);
	inv = b->gf.inv(pv);
	b->gf.mulrc(tmp, inv, b->len.cc);

	// Backward substitution
	for (i=0; i<rank(b); i++) {
		c = get_coefficient(b, b->pvlist[i], pvpos);
		if (c == 0)
			continue;

		b->gf.maddrc(b->slot[b->pvlist[i]], tmp, c, b->len.cc);
	}

	// Insert new pivot position into pivo list and increment rank
	b->pvlist[rank(b)] = pvpos;
	b->rank.decode++;

	// Swap pointers between spare slot und slot t pivot position
	tmp = b->slot[pvpos];
	b->slot[pvpos] = b->slot[b->rank.max];
	b->slot[b->rank.max] = tmp;

	return 0;
}

int
rlnc_block_add(rlnc_block_t b, int pv, const uint8_t *data, size_t len)
{
	int i;
	struct slot *s;

	if (len > b->len.max_data) {
		LOG(LOG_ERR, "unable to add frame to block: frame too large");
		return -1;
	}

	if (rank(b) == b->rank.max) {
		LOG(LOG_ERR, "unable to add frame to block: rank exceeded");
		return -1;
	}

	for (i=0; i<rank(b); i++) {
		if (b->pvlist[i] == pv) {
			LOG(LOG_ERR, "unable to add frame to block: pv exists");
			return -1;
		}
	}

	b->pvlist[rank(b)] = pv;

	s = (void *)(b->slot[pv] + b->len.coeff);

	memcpy(s->data, data, len);
	s->len = len;
	set_coefficient(b, pv, pv, 1);

	b->len.cc = max_t(size_t, b->len.cc, len+b->len.coeff+sizeof(*s));
	b->rank.encode++;

	if (b->encode_start == -1)
		b->encode_start = pv;

	return 0;
}

ssize_t
rlnc_block_get(rlnc_block_t b, int pv, uint8_t *dst, size_t maxlen)
{
	struct slot *s;
	size_t len;

	// check if frame in row(pv) is decoded
	if (!is_decoded(b, pv))
		return 0;

	len = b->len.cc - b->len.coeff - sizeof(*s);
	if (maxlen < len) {
		LOG(LOG_ERR, "destination buffer too small (buffer has %d B "\
			"but %d B needed)", (int)maxlen, (int)len);
		return -1;
	}

	s = (void *)(b->slot[pv] + b->len.coeff);
	memcpy(dst, s->data, s->len);

	return s->len;
}

int
rlnc_block_rank_encode(const rlnc_block_t b)
{
	return b->rank.encode;
}

int
rlnc_block_rank_decode(const rlnc_block_t b)
{
	return b->rank.decode;
}

ssize_t
rlnc_block_current_frame_len(const rlnc_block_t b)
{
	return b->len.cc;
}

