/*
 * File: z-type.c
 * Purpose: Helper classes for the display of typed data
 *
 * Copyright (c) 2007 Angband Developers
 * Copyright (c) 2012 MAngband and PWMAngband Developers
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "angband.h"


type_union null2u()
{
    type_union r;
    r.t = T_END;
    return r;
}


#define TYPE_FUN(v2u, tv, T, v) \
type_union v2u(T v) \
{ \
    type_union r; \
    r.u.v = v; \
    r.t = tv; \
    return r; \
}


TYPE_FUN(i2u, T_INTEGER, int, i)
TYPE_FUN(c2u, T_CHAR, char, c)
TYPE_FUN(f2u, T_FLOAT, float, f)
TYPE_FUN(s2u, T_STRING, const char *, s)


/*
 * Utility functions to work with point_sets
 */
struct point_set *point_set_new(int initial_size)
{
    struct point_set *ps = mem_alloc(sizeof(struct point_set));

    ps->n = 0;
    ps->allocated = initial_size;
    ps->pts = mem_zalloc(sizeof(*(ps->pts)) * ps->allocated);
    return ps;
}


void point_set_dispose(struct point_set *ps)
{
    mem_free(ps->pts);
    mem_free(ps);
}


/*
 * Add the point to the given point set, making more space if there is
 * no more space left.
 */
void add_to_point_set(struct point_set *ps, int Ind, int y, int x)
{
    ps->pts[ps->n].Ind = Ind;
    ps->pts[ps->n].x = x;
    ps->pts[ps->n].y = y;
    ps->n++;
    if (ps->n >= ps->allocated)
    {
        ps->allocated *= 2;
        ps->pts = mem_realloc(ps->pts, sizeof(*(ps->pts)) * ps->allocated);
    }
}


int point_set_size(struct point_set *ps)
{
    return ps->n;
}
