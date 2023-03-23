#ifndef __LINKSTATE_H
#define __LINKSTATE_H

#include <moepcommon/util.h>
#include <moepcommon/list.h>
#include <moepcommon/timeout.h>

#include "global.h"
#include "ralqe.h"

int ls_add(const u8 *ta, const u8 *ra);
int ls_del(const u8 *ta, const u8 *ra);
int ls_update(const u8 *ta, const u8 *ra, int p, int q);
double ls_quality(const u8 *ta, const u8 *ra, int *p, int *q);

#endif
