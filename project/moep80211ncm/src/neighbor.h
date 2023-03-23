#ifndef __NEIGHBOR_H
#define __NEIGHBOR_H

#include <moepcommon/util.h>
#include <moepcommon/list.h>
#include <moepcommon/timeout.h>

#include "global.h"
#include "ralqe.h"

extern float ralqe_theta;


int nb_exists(const u8 *hwaddr);
int nb_add(const u8 *hwaddr);
int nb_del(const u8 *hwaddr);
int nb_update_seq(const u8 *hwaddr, u16 seq);
int nb_update_ul(const u8 *hwaddr, int p, int q);
double nb_ul_redundancy(const u8 *hwaddr);
double nb_ul_quality(const u8 *hwaddr, int *p, int *q);
double nb_dl_quality(const u8 *hwaddr, int *p, int *q);
typedef void (*nb_dl_filler_t)(void *data, int i, u8 *hwaddr, int p, int q);
int nb_fill_dl(nb_dl_filler_t filler, void *data);

#endif
