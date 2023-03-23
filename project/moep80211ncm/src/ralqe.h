#ifndef RALQE_H
#define RALQE_H

typedef struct ralqe_link *ralqe_link_t;


ralqe_link_t
ralqe_link_new();

void
ralqe_link_del(ralqe_link_t l);

void
ralqe_init(ralqe_link_t l, int p, int q);

void
ralqe_update(ralqe_link_t l, int *p, int *q);

double
ralqe_redundancy(ralqe_link_t l, double thresh);

#endif /* RALQE_H */
