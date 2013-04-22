/* File: generate.c */

/* Purpose: Dungeon generation */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

/* Prfnoff -- reorganized generate.c into generate.c, genroom.c, wild.c */
#include "angband.h"


/*
 * Note that Level generation is *not* an important bottleneck,
 * though it can be annoyingly slow on older machines...  Thus
 * we emphasize "simplicity" and "correctness" over "speed".
 *
 * This entire file is only needed for generating levels.
 * This may allow smart compilers to only load it when needed.
 *
 * Consider the "v_info.txt" file for vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types,
 * where "basic" is normal, "inner" is inside a room, "outer" is the
 * outer wall of a room, and "solid" is the outer wall of the dungeon
 * or any walls that may not be pierced by corridors.  Thus the only
 * wall type that may be pierced by a corridor is the "outer granite"
 * type.  The "basic granite" type yields the "actual" corridors.
 *
 * Note that we use the special "solid" granite wall type to prevent
 * multiple corridors from piercing a wall in two adjacent locations,
 * which would be messy, and we use the special "outer" granite wall
 * to indicate which walls "surround" rooms, and may thus be "pierced"
 * by corridors entering or leaving the room.
 *
 * Note that a tunnel which attempts to leave a room near the "edge"
 * of the dungeon in a direction toward that edge will cause "silly"
 * wall piercings, but will have no permanently incorrect effects,
 * as long as the tunnel can *eventually* exit from another side.
 * And note that the wall may not come back into the room by the
 * hole it left through, so it must bend to the left or right and
 * then optionally re-enter the room (at least 2 grids away).  This
 * is not a problem since every room that is large enough to block
 * the passage of tunnels is also large enough to allow the tunnel
 * to pierce the room itself several times.
 *
 * Note that no two corridors may enter a room through adjacent grids,
 * they must either share an entryway or else use entryways at least
 * two grids apart.  This prevents "large" (or "silly") doorways.
 *
 * To create rooms in the dungeon, we first divide the dungeon up
 * into "blocks" of 11x11 grids each, and require that all rooms
 * occupy a rectangular group of blocks.  As long as each room type
 * reserves a sufficient number of blocks, the room building routines
 * will not need to check bounds.  Note that most of the normal rooms
 * actually only use 23x11 grids, and so reserve 33x11 grids.
 *
 * Note that the use of 11x11 blocks (instead of the old 33x11 blocks)
 * allows more variability in the horizontal placement of rooms, and
 * at the same time has the disadvantage that some rooms (two thirds
 * of the normal rooms) may be "split" by panel boundaries.  This can
 * induce a situation where a player is in a room and part of the room
 * is off the screen.  It may be annoying enough to go back to 33x11
 * blocks to prevent this visual situation.
 *
 * Note that the dungeon generation routines are much different (2.7.5)
 * and perhaps "DUN_ROOMS" should be less than 50.
 *
 * XXX XXX XXX Note that it is possible to create a room which is only
 * connected to itself, because the "tunnel generation" code allows a
 * tunnel to leave a room, wander around, and then re-enter the room.
 *
 * XXX XXX XXX Note that it is possible to create a set of rooms which
 * are only connected to other rooms in that set, since there is nothing
 * explicit in the code to prevent this from happening.  But this is less
 * likely than the "isolated room" problem, because each room attempts to
 * connect to another room, in a giant cycle, thus requiring at least two
 * bizarre occurances to create an isolated section of the dungeon.
 *
 * Note that (2.7.9) monster pits have been split into monster "nests"
 * and monster "pits".  The "nests" have a collection of monsters of a
 * given type strewn randomly around the room (jelly, animal, or undead),
 * while the "pits" have a collection of monsters of a given type placed
 * around the room in an organized manner (orc, troll, giant, dragon, or
 * demon).  Note that both "nests" and "pits" are now "level dependant",
 * and both make 16 "expensive" calls to the "get_mon_num()" function.
 *
 * Note that the cave grid flags changed in a rather drastic manner
 * for Angband 2.8.0 (and 2.7.9+), in particular, dungeon terrain
 * features, such as doors and stairs and traps and rubble and walls,
 * are all handled as a set of 64 possible "terrain features", and
 * not as "fake" objects (440-479) as in pre-2.8.0 versions.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls, or even permanent granite in a different
 * way from granite.  XXX XXX XXX
 */


/*
 * Dungeon generation values
 */
#define DUN_UNUSUAL 194 /* Level/chance of unusual room (was 200) */
#define DUN_DEST    18  /* 1/chance of having a destroyed level */
#define SMALL_LEVEL 3   /* 1/chance of smaller size (3)*/
#define EMPTY_LEVEL 15  /* 1/chance of being 'empty' (15)*/
#define DARK_EMPTY  5   /* 1/chance of arena level NOT being lit (2)*/

/* Number of rooms to attempt (was 50) */
#define DUN_ROOMS_MIN	10
#define DUN_ROOMS_MAX	50

static int dun_rooms;

/*
 * Dungeon tunnel generation values
 */
#define DUN_TUN_RND_MIN	 5 /* Chance of random direction (was 10) */
#define DUN_TUN_RND_MAX	20
#define DUN_TUN_CHG_MIN	20 /* Chance of changing direction (was 30) */
#define DUN_TUN_CHG_MAX	60
#define DUN_TUN_CON_MIN 10 /* Chance of extra tunneling (was 15) */
#define DUN_TUN_CON_MAX	40
#define DUN_TUN_PEN_MIN 30 /* Chance of doors at room entrances (was 25) */
#define DUN_TUN_PEN_MAX 70
#define DUN_TUN_JCT_MIN 60 /* Chance of doors at tunnel junctions (was 90) */
#define DUN_TUN_JCT_MAX 90

static int dun_tun_rnd;
static int dun_tun_chg;
static int dun_tun_con;
static int dun_tun_pen;
static int dun_tun_jct;

/*
 * Dungeon streamer generation values
 */
#define DUN_STR_DEN     5	/* Density of streamers */
#define DUN_STR_RNG     2	/* Width of streamers */
#define DUN_STR_MAG     3	/* Number of magma streamers */
#define DUN_STR_MC     90	/* 1/chance of treasure per magma */
#define DUN_STR_QUA	    2	/* Number of quartz streamers */
#define DUN_STR_QC     40	/* 1/chance of treasure per quartz */
#define DUN_STR_WLW     1	/* Width of lava & water streamers -KMW- */
#define DUN_STR_DWLW    8	/* Density of water & lava streams -KMW- */

#define DUN_MOS_DEN	2	/* Density of moss streamers */
#define DUN_MOS_RNG	10	/* Width of moss streamers */
#define DUN_STR_MOS	2	/* Number of moss streamers */
#define DUN_WAT_DEN	15	/* Density of rivers */
#define DUN_WAT_RNG	3	/* Width of rivers */
#define DUN_STR_WAT	3	/* Max number of rivers */


/*
 * Dungeon treausre allocation values
 */
#define DUN_AMT_ROOM	9	/* Amount of objects for rooms */
#define DUN_AMT_ITEM	3	/* Amount of objects for rooms/corridors */
#define DUN_AMT_GOLD	3	/* Amount of treasure for rooms/corridors */

/*
 * Hack -- Dungeon allocation "places"
 */
#define ALLOC_SET_CORR		1	/* Hallway */
#define ALLOC_SET_ROOM		2	/* Room */
#define ALLOC_SET_BOTH		3	/* Anywhere */

/*
 * Hack -- Dungeon allocation "types"
 */
#define ALLOC_TYP_RUBBLE	1	/* Rubble */
#define ALLOC_TYP_TRAP		3	/* Trap */
#define ALLOC_TYP_GOLD		4	/* Gold */
#define ALLOC_TYP_OBJECT	5	/* Object */


/*
 * Dungeon generation data -- see "cave_gen()"
 */
dun_data *dun;



/*
 * Always picks a correct direction
 */
static void correct_dir(int *rdir, int *cdir, int y1, int x1, int y2, int x2)
{
	/* Extract vertical and horizontal directions */
	*rdir = (y1 == y2) ? 0 : (y1 < y2) ? 1 : -1;
	*cdir = (x1 == x2) ? 0 : (x1 < x2) ? 1 : -1;

	/* Never move diagonally */
	if (*rdir && *cdir)
	{
		if (rand_int(100) < 50)
		{
			*rdir = 0;
		}
		else
		{
			*cdir = 0;
		}
	}
}


/*
 * Pick a random direction
 */
static void rand_dir(int *rdir, int *cdir)
{
	/* Pick a random direction */
	int i = rand_int(4);

	/* Extract the dy/dx components */
	*rdir = ddy_ddd[i];
	*cdir = ddx_ddd[i];
}


/*
 * Returns random co-ordinates for player/monster/object
 */
static bool new_player_spot(void)
{
	int	y, x;
	int max_attempts = 5000;

	/* Place the player */
	while (max_attempts--)
	{
		/* Pick a legal spot */
		y = rand_range(1, cur_hgt - 2);
		x = rand_range(1, cur_wid - 2);

		/* Must be a "naked" floor grid */
		if (!cave_naked_bold(y, x)) continue;

		/* Refuse to start on anti-teleport grids */
		if (cave[y][x].info & (CAVE_ICKY)) continue;

		/* Done */
		break;
	}

	if (max_attempts < 1) /* Should be -1, actually if we failed... */
		return FALSE;


	/* Save the new player grid */
	py = y;
	px = x;

	return TRUE;
}



/*
 * Count the number of walls adjacent to the given grid.
 *
 * Note -- Assumes "in_bounds(y, x)"
 *
 * We count only granite walls and permanent walls.
 */
static int next_to_walls(int y, int x)
{
	int	k = 0;

	if (cave[y+1][x].feat >= FEAT_WALL_EXTRA) k++;
	if (cave[y-1][x].feat >= FEAT_WALL_EXTRA) k++;
	if (cave[y][x+1].feat >= FEAT_WALL_EXTRA) k++;
	if (cave[y][x-1].feat >= FEAT_WALL_EXTRA) k++;

	return (k);
}



/*
 * Convert existing terrain type to rubble
 */
static void place_rubble(int y, int x)
{
	cave_type *c_ptr = &cave[y][x];

	/* Create rubble */
	c_ptr->feat = FEAT_RUBBLE;
}



/*
 * Convert existing terrain type to "up stairs"
 */
static void place_up_stairs(int y, int x)
{
	cave_type *c_ptr = &cave[y][x];

	/* Create up stairs */
	c_ptr->feat = FEAT_LESS;
}


/*
 * Convert existing terrain type to "down stairs"
 */
static void place_down_stairs(int y, int x)
{
	cave_type *c_ptr = &cave[y][x];

	/* Create down stairs */
	c_ptr->feat = FEAT_MORE;
}


/*
 * Place an up/down staircase at given location
 */
void place_random_stairs(int y, int x)
{
	bool up_stairs = TRUE;
	bool down_stairs = TRUE;


	/* Paranoia */
	if (!cave_clean_bold(y, x)) return;

	/* Town */
	if (!dun_level)
		up_stairs = FALSE;

	/* Ironman */
	if (ironman_downward)
		up_stairs = FALSE;

	/* Bottom */
	if (dun_level >= MAX_DEPTH - 1)
		down_stairs = FALSE;

	/* Quest-level */
	if (quest_number(dun_level) && (dun_level > 1))
		down_stairs = FALSE;

	/* We can't place both */
	if (down_stairs && up_stairs)
	{
		/* Choose a staircase randomly */
		if (rand_int(100) < 50)
			up_stairs = FALSE;
		else
			down_stairs = FALSE;
	}

	/* Place the stairs */
	if (up_stairs)
		place_up_stairs(y, x);
	else if (down_stairs)
		place_down_stairs(y, x);
}


/*
 * Place a locked door at the given location
 */
void place_locked_door(int y, int x)
{
	cave_type *c_ptr = &cave[y][x];

	/* Create locked door */
	c_ptr->feat = FEAT_DOOR_HEAD + randint(7);
}


/*
 * Place a secret door at the given location
 */
void place_secret_door(int y, int x)
{
	cave_type *c_ptr = &cave[y][x];

	/* Create secret door */
	c_ptr->feat = FEAT_SECRET;
}


/*
 * Place a random type of door at the given location
 */
static void place_random_door(int y, int x)
{
	int tmp;

	/* Choose an object */
	tmp = rand_int(1000);

	/* Open doors (300/1000) */
	if (tmp < 300)
	{
		/* Create open door */
		cave_set_feat(y, x, FEAT_OPEN);
	}

	/* Broken doors (100/1000) */
	else if (tmp < 400)
	{
		/* Create broken door */
		cave_set_feat(y, x, FEAT_BROKEN);
	}

	/* Secret doors (200/1000) */
	else if (tmp < 600)
	{
		/* Create secret door */
		cave_set_feat(y, x, FEAT_SECRET);
	}

	/* Closed, locked, or stuck doors (400/1000) */
	else place_closed_door(y, x);
}


/*
 * Place a random type of normal door at the given location.
 */
void place_closed_door(int y, int x)
{
	int tmp;

	/* Choose an object */
	tmp = rand_int(400);

	/* Closed doors (300/400) */
	if (tmp < 300)
	{
		/* Create closed door */
		cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
	}

	/* Locked doors (99/400) */
	else if (tmp < 399)
	{
		/* Create locked door */
		cave_set_feat(y, x, FEAT_DOOR_HEAD + randint(7));
	}

	/* Stuck doors (1/400) */
	else
	{
		/* Create jammed door */
		cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + rand_int(8));
	}
}


/*
 * Places some staircases near walls
 */
static void alloc_stairs(int feat, int num, int walls)
{
	int         y, x, i, j, flag;
	cave_type   *c_ptr;


	if (feat == FEAT_LESS)
	{
		/* No up stairs in town or in ironman mode */
		if (ironman_downward || !dun_level) return;
	}
	else if (feat == FEAT_MORE)
	{
		/* No downstairs on quest levels */
		if ((dun_level > 1) && quest_number(dun_level)) return;

		/* No downstairs at the bottom */
		if (dun_level >= MAX_DEPTH - 1) return;
	}

	/* Place "num" stairs */
	for (i = 0; i < num; i++)
	{
		/* Place some stairs */
		for (flag = FALSE; !flag; )
		{
			/* Try several times, then decrease "walls" */
			for (j = 0; !flag && j <= 3000; j++)
			{
				/* Pick a random grid */
				y = rand_int(cur_hgt);
				x = rand_int(cur_wid);

				/* Require "naked" floor grid */
				if (!cave_naked_bold(y, x)) continue;

				/* Require a certain number of adjacent walls */
				if (next_to_walls(y, x) < walls) continue;

				/* Access the grid */
				c_ptr = &cave[y][x];

				/* Clear previous contents, add stairs */
				c_ptr->feat = feat;

				/* All done */
				flag = TRUE;
			}

			/* Require fewer walls */
			if (walls) walls--;
		}
	}
}


/*
 * Allocates some objects (using "place" and "type")
 */
static void alloc_object(int set, int typ, int num)
{
	int y, x, k;
	int dummy = 0;

	/* Place some objects */
	for (k = 0; k < num; k++)
	{
		/* Pick a "legal" spot */
		while (dummy < SAFE_MAX_ATTEMPTS)
		{
			bool room;

			dummy++;

			/* Location */
			y = rand_int(cur_hgt);
			x = rand_int(cur_wid);

			/* Require "naked" floor grid */
			if (!cave_naked_bold(y, x) &&
			    !((y == py) && (x == px))) continue;

			/* Check for "room" */
			room = (cave[y][x].info & CAVE_ROOM) ? TRUE : FALSE;

			/* Require corridor? */
			if ((set == ALLOC_SET_CORR) && room) continue;

			/* Require room? */
			if ((set == ALLOC_SET_ROOM) && !room) continue;

			/* Accept it */
			break;
		}

		if (dummy >= SAFE_MAX_ATTEMPTS)
		{
			if (cheat_room)
			{
				msg_print("Warning! Could not place object!");
			}
			return;
		}


		/* Place something */
		switch (typ)
		{
			case ALLOC_TYP_RUBBLE:
			{
				place_rubble(y, x);
				break;
			}

			case ALLOC_TYP_TRAP:
			{
				place_trap(y, x);
				break;
			}

			case ALLOC_TYP_GOLD:
			{
				place_gold(y, x);
				break;
			}

			case ALLOC_TYP_OBJECT:
			{
				place_object(y, x, FALSE, FALSE);
				break;
			}
		}
	}
}


#if 0
/* Taken from Eric Bock's RAngband */
/*
 * Places moss through dungeon.
 */
static void add_moss(void)
{
	int i, tx, ty;
	int y, x, dir;


	/* Hack -- Choose starting point */
	y = rand_spread(cur_hgt / 2, cur_hgt / 2 - 10);
	x = rand_spread(cur_wid / 2, cur_wid / 2 - 10);

	/* Choose a random compass direction */
	dir = ddd[rand_int(8)];

	/* Place streamer into dungeon */
	while (TRUE)
	{
		/* One grid per density */
		for (i = 0; i < DUN_MOS_DEN; i++)
		{
			int d = DUN_MOS_RNG;

			/* Pick a nearby grid */
			while (1)
			{
				ty = rand_spread(y, d);
				tx = rand_spread(x, d);
				if (!in_bounds(ty, tx)) continue;
				break;
			}

			/* Only convert "outer" and "solid" walls and floors */
			if ((cave[ty][tx].feat != FEAT_WALL_OUTER) &&
				 (cave[ty][tx].feat != FEAT_WALL_SOLID) &&
				 (cave[ty][tx].feat != FEAT_FLOOR))
			{
				continue;
			}

			/* Clear previous contents, add proper moss type */
			if (cave[ty][tx].feat == FEAT_FLOOR)
				cave[ty][tx].feat = FEAT_FLOOR_M;
			else
				cave[ty][tx].feat = FEAT_WALL_M;
		}

		/* Advance the streamer */
		y += ddy[dir];
		x += ddx[dir];

		/* Stop at dungeon edge */
		if (!in_bounds(y, x)) break;
	}
}


/*
 * Places water through dungeon.
 */
static void add_river(int feat1, int feat2)
{
	int tx, ty;
	int y, x, dir, dd, len, wid;


	/* Hack -- Choose starting point */
	y = rand_spread(cur_hgt / 2, cur_hgt / 2 - 10);
	x = rand_spread(cur_wid / 2, cur_wid / 2 - 10);

	/* Choose a random direction */
	dd = rand_int(16);
	dir = cdd[dd / 2];

	wid = randint(DUN_WAT_RNG);

	/* Choose a length */
	len = rand_int(10) + 5;

	/* Place streamer into dungeon */
	while (TRUE)
	{
		for (ty = y - wid - 1; ty <= y + wid + 1; ty++)
		{
			for (tx = x - wid - 1; tx <= x + wid + 1; tx++)
			{
				if (!in_bounds(ty, tx)) continue;

				if (cave[ty][tx].feat == feat1) continue;
				if (cave[ty][tx].feat == feat2) continue;

				if (distance(ty, tx, y, x) > rand_spread(wid, 1)) continue;

				/* Do not convert permanent features */
				if (cave_perma_bold(ty, tx)) continue;

				/*
				 * Clear previous contents, add feature
				 * The border mainly gets feat2, while the center gets feat1 */
				if (distance(ty, tx, y, x) > wid)
					cave[ty][tx].feat = feat2;
				else
					cave[ty][tx].feat = feat1;

				/* Lava terrain glows */
				if ((feat1 == FEAT_DEEP_LAVA) ||
				    (feat1 == FEAT_SHAL_LAVA))
				{
					cave[ty][tx].info |= CAVE_GLOW;
				}

				/* Hack -- don't teleport here */
				cave[ty][tx].info |= CAVE_ICKY;
			}
		}

		/* Advance the streamer */
		if (dd % 2 == 0)
		{
			y += ddy[dir];
			x += ddx[dir];
		}
		/* Acquire the intermediate direction */
		else
		{
			y += ddy[dir] + ddy_cdd[((dd + 1) / 2) % 8];
			x += ddx[dir] + ddx_cdd[((dd + 1) / 2) % 8];
		}

		if (len-- == 0)
		{
			/* Change direction slowly */
			dd = rand_spread(dd, 1) % 16;
			dir = cdd[dd / 2];

			wid = rand_spread(wid, 1);

			if (wid < 0) wid = 0;
			if (wid > DUN_WAT_RNG) wid = DUN_WAT_RNG;

			len = rand_int(10) + 5;
		}

		/* Stop at dungeon edge */
		if (!in_bounds(y, x)) break;
	}
}
#endif /* 0 */


/*
 * Places "streamers" of rock through dungeon
 *
 * Note that their are actually six different terrain features used
 * to represent streamers.  Three each of magma and quartz, one for
 * basic vein, one with hidden gold, and one with known gold.  The
 * hidden gold types are currently unused.
 */
static void build_streamer(int feat, int chance)
{
	int		i, tx, ty;
	int		y, x, dir;
	int dummy = 0;

	cave_type *c_ptr;

	/* Hack -- Choose starting point */
	y = rand_spread(cur_hgt / 2, 10);
	x = rand_spread(cur_wid / 2, 15);

	/* Choose a random compass direction */
	dir = ddd[rand_int(8)];

	/* Place streamer into dungeon */
	while (dummy < SAFE_MAX_ATTEMPTS)
	{
		dummy++;

		/* One grid per density */
		for (i = 0; i < DUN_STR_DEN; i++)
		{
			int d = DUN_STR_RNG;

			/* Pick a nearby grid */
			while (1)
			{
				ty = rand_spread(y, d);
				tx = rand_spread(x, d);
				if (!in_bounds2(ty, tx)) continue;
				break;
			}

			/* Access the grid */
			c_ptr = &cave[ty][tx];

			/* Only convert "granite" walls */
			if (c_ptr->feat < FEAT_WALL_EXTRA) continue;
			if (c_ptr->feat > FEAT_WALL_SOLID) continue;

			/* Clear previous contents, add proper vein type */
			c_ptr->feat = feat;

			/* Hack -- Add some (known) treasure */
			if (rand_int(chance) == 0) c_ptr->feat += 0x04;
		}

		if (dummy >= SAFE_MAX_ATTEMPTS)
		{
			if (cheat_room)
			{
				msg_print("Warning! Could not place streamer!");
			}
			return;
		}


		/* Advance the streamer */
		y += ddy[dir];
		x += ddx[dir];

		/* Quit before leaving the dungeon */
		if (!in_bounds(y, x)) break;
	}
}



/*
 * Place streams of water, lava, & trees -KMW-
 * This routine varies the placement based on dungeon level
 * otherwise is similar to build_streamer
 */
static void build_streamer2(int feat, bool killwall, bool pool)
{
	int i, j, mid, tx, ty;
	int y, x, dir;
	int poolsize;


	/* Hack -- Choose starting point */
	y = rand_spread(cur_hgt / 2, 10);
	x = rand_spread(cur_wid / 2, 15);

	/* Choose a random compass direction */
	dir = ddd[rand_int(8)];

	if (!pool)
	{
		/* Place streamer into dungeon */
		while (TRUE)
		{
			/* One grid per density */
			for (i = 0; i < (DUN_STR_DWLW + 1); i++)
			{
				int d = DUN_STR_WLW;

				/* Pick a nearby grid */
				while (1)
				{
					ty = rand_spread(y, d);
					tx = rand_spread(x, d);
					if (!in_bounds(ty, tx)) continue;
					break;
				}

				/* Only convert non-permanent features */
				if (killwall)
				{
					if (cave[ty][tx].feat >= FEAT_PERM_EXTRA) continue;
					if (cave[ty][tx].feat == FEAT_LESS) continue;
					if (cave[ty][tx].feat == FEAT_MORE) continue;
				}
				else
				{
					if (cave[ty][tx].feat >= FEAT_MAGMA) continue;
					if (cave[ty][tx].feat == FEAT_LESS) continue;
					if (cave[ty][tx].feat == FEAT_MORE) continue;
				}

				/* Clear previous contents, add proper vein type */
				cave[ty][tx].feat = feat;
			}

			/* Advance the streamer */
			y += ddy[dir];
			x += ddx[dir];

			if (randint(20) == 1)
				dir = ddd[rand_int(8)]; /* change direction */

			/* Stop at dungeon edge */
			if (!in_bounds(y, x)) break;
		}
	}
	else
	{
		/* create pool */
		poolsize = 5 + randint(10);
		mid = poolsize / 2;

		/* One grid per density */
		for (i = 0; i < poolsize; i++)
		{
			for (j = 0; j < poolsize; j++)
			{
				tx = x + j;
				ty = y + i;

				if (!in_bounds(ty, tx)) continue;

				if (i < mid)
				{
					if (j < mid)
					{
						if ((i + j + 1) < mid)
							continue;
					}
					else if (j > (mid+ i))
						continue;
				}
				else if (j < mid)
				{
					if (i > (mid + j))
						continue;
				}
				else if ((i + j) > ((mid * 3)-1))
					continue;

				/* Only convert non-permanent features */
				if (killwall)
				{
					if (cave[ty][tx].feat >= FEAT_PERM_EXTRA) continue;
					if (cave[ty][tx].feat == FEAT_LESS) continue;
					if (cave[ty][tx].feat == FEAT_MORE) continue;
				}
				else
				{
					if (cave[ty][tx].feat >= FEAT_MAGMA) continue;
					if (cave[ty][tx].feat == FEAT_LESS) continue;
					if (cave[ty][tx].feat == FEAT_MORE) continue;
				}

				/* Clear previous contents, add proper vein type */
				cave[ty][tx].feat = feat;
			}
		}
	}
}



/*
 * Build a destroyed level
 */
static void destroy_level(void)
{
	int y1, x1, y, x, k, t, n;

	cave_type *c_ptr;

	/* Note destroyed levels */
	if (cheat_room) msg_print("Destroyed Level");

	/* Drop a few epi-centers (usually about two) */
	for (n = 0; n < randint(5); n++)
	{
		/* Pick an epi-center */
		x1 = rand_range(5, cur_wid-1 - 5);
		y1 = rand_range(5, cur_hgt-1 - 5);

		/* Big area of affect */
		for (y = (y1 - 15); y <= (y1 + 15); y++)
		{
			for (x = (x1 - 15); x <= (x1 + 15); x++)
			{
				/* Skip illegal grids */
				if (!in_bounds(y, x)) continue;

				/* Extract the distance */
				k = distance(y1, x1, y, x);

				/* Stay in the circle of death */
				if (k >= 16) continue;

				/* Delete the monster (if any) */
				delete_monster(y, x);

				/* Destroy valid grids */
				if (cave_valid_bold(y, x))
				{
					/* Delete objects */
					delete_object(y, x);

					/* Access the grid */
					c_ptr = &cave[y][x];

					/* Wall (or floor) type */
					t = rand_int(200);

					/* Granite */
					if (t < 20)
					{
						/* Create granite wall */
						c_ptr->feat = FEAT_WALL_EXTRA;
					}

					/* Quartz */
					else if (t < 70)
					{
						/* Create quartz vein */
						c_ptr->feat = FEAT_QUARTZ;
					}

					/* Magma */
					else if (t < 100)
					{
						/* Create magma vein */
						c_ptr->feat = FEAT_MAGMA;
					}

					/* Floor */
					else
					{
						/* Create floor */
						c_ptr->feat = FEAT_FLOOR;
					}

					/* No longer part of a room or vault */
					c_ptr->info &= ~(CAVE_ROOM | CAVE_ICKY);

					/* No longer illuminated or known */
					c_ptr->info &= ~(CAVE_MARK | CAVE_GLOW);
				}
			}
		}
	}
}

/* Removed room-building stuff to genroom.c -- Prfnoff */


/*
 * Constructs a tunnel between two points
 *
 * This function must be called BEFORE any streamers are created,
 * since we use the special "granite wall" sub-types to keep track
 * of legal places for corridors to pierce rooms.
 *
 * We use "door_flag" to prevent excessive construction of doors
 * along overlapping corridors.
 *
 * We queue the tunnel grids to prevent door creation along a corridor
 * which intersects itself.
 *
 * We queue the wall piercing grids to prevent a corridor from leaving
 * a room and then coming back in through the same entrance.
 *
 * We "pierce" grids which are "outer" walls of rooms, and when we
 * do so, we change all adjacent "outer" walls of rooms into "solid"
 * walls so that no two corridors may use adjacent grids for exits.
 *
 * The "solid" wall check prevents corridors from "chopping" the
 * corners of rooms off, as well as "silly" door placement, and
 * "excessively wide" room entrances.
 *
 * Useful "feat" values:
 *   FEAT_WALL_EXTRA -- granite walls
 *   FEAT_WALL_INNER -- inner room walls
 *   FEAT_WALL_OUTER -- outer room walls
 *   FEAT_WALL_SOLID -- solid room walls
 *   FEAT_PERM_EXTRA -- shop walls (perma)
 *   FEAT_PERM_INNER -- inner room walls (perma)
 *   FEAT_PERM_OUTER -- outer room walls (perma)
 *   FEAT_PERM_SOLID -- dungeon border (perma)
 */
static void build_tunnel(int row1, int col1, int row2, int col2)
{
	int i, y, x;
	int tmp_row, tmp_col;
	int row_dir, col_dir;
	int start_row, start_col;
	int main_loop_count = 0;

	bool door_flag = FALSE;

	cave_type *c_ptr;


	/* Reset the arrays */
	dun->tunn_n = 0;
	dun->wall_n = 0;

	/* Save the starting location */
	start_row = row1;
	start_col = col1;

	/* Start out in the correct direction */
	correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

	/* Keep going until done (or bored) */
	while ((row1 != row2) || (col1 != col2))
	{
		/* Mega-Hack -- Paranoia -- prevent infinite loops */
		if (main_loop_count++ > 2000) break;

		/* Allow bends in the tunnel */
		if (rand_int(100) < dun_tun_chg)
		{
			/* Acquire the correct direction */
			correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

			/* Random direction */
			if (rand_int(100) < dun_tun_rnd)
			{
				rand_dir(&row_dir, &col_dir);
			}
		}

		/* Get the next location */
		tmp_row = row1 + row_dir;
		tmp_col = col1 + col_dir;


		/* Extremely Important -- do not leave the dungeon */
		while (!in_bounds(tmp_row, tmp_col))
		{
			/* Acquire the correct direction */
			correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

			/* Random direction */
			if (rand_int(100) < dun_tun_rnd)
			{
				rand_dir(&row_dir, &col_dir);
			}

			/* Get the next location */
			tmp_row = row1 + row_dir;
			tmp_col = col1 + col_dir;
		}


		/* Access the location */
		c_ptr = &cave[tmp_row][tmp_col];


		/* Avoid the edge of the dungeon */
		if (c_ptr->feat == FEAT_PERM_SOLID) continue;

		/* Avoid the edge of vaults */
		if (c_ptr->feat == FEAT_PERM_OUTER) continue;

		/* Avoid "solid" granite walls */
		if (c_ptr->feat == FEAT_WALL_SOLID) continue;

		/* Pierce "outer" walls of rooms */
		if (c_ptr->feat == FEAT_WALL_OUTER)
		{
			/* Acquire the "next" location */
			y = tmp_row + row_dir;
			x = tmp_col + col_dir;

			/* Hack -- Avoid outer/solid permanent walls */
			if (cave[y][x].feat == FEAT_PERM_SOLID) continue;
			if (cave[y][x].feat == FEAT_PERM_OUTER) continue;

			/* Hack -- Avoid outer/solid granite walls */
			if (cave[y][x].feat == FEAT_WALL_OUTER) continue;
			if (cave[y][x].feat == FEAT_WALL_SOLID) continue;

			/* Accept this location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Save the wall location */
			if (dun->wall_n < WALL_MAX)
			{
				dun->wall[dun->wall_n].y = row1;
				dun->wall[dun->wall_n].x = col1;
				dun->wall_n++;
			}

			/* Forbid re-entry near this piercing */
			for (y = row1 - 1; y <= row1 + 1; y++)
			{
				for (x = col1 - 1; x <= col1 + 1; x++)
				{
					/* Convert adjacent "outer" walls as "solid" walls */
					if (cave[y][x].feat == FEAT_WALL_OUTER)
					{
						/* Change the wall to a "solid" wall */
						cave[y][x].feat = FEAT_WALL_SOLID;
					}
				}
			}
		}

		/* Travel quickly through rooms */
		else if (c_ptr->info & (CAVE_ROOM))
		{
			/* Accept the location */
			row1 = tmp_row;
			col1 = tmp_col;
		}

		/* Tunnel through all other walls */
		else if (c_ptr->feat >= FEAT_WALL_EXTRA)
		{
			/* Accept this location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Save the tunnel location */
			if (dun->tunn_n < TUNN_MAX)
			{
				dun->tunn[dun->tunn_n].y = row1;
				dun->tunn[dun->tunn_n].x = col1;
				dun->tunn_n++;
			}

			/* Allow door in next grid */
			door_flag = FALSE;
		}

		/* Handle corridor intersections or overlaps */
		else
		{
			/* Accept the location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Collect legal door locations */
			if (!door_flag)
			{
				/* Save the door location */
				if (dun->door_n < DOOR_MAX)
				{
					dun->door[dun->door_n].y = row1;
					dun->door[dun->door_n].x = col1;
					dun->door_n++;
				}

				/* No door in next grid */
				door_flag = TRUE;
			}

			/* Hack -- allow pre-emptive tunnel termination */
			if (rand_int(100) >= dun_tun_con)
			{
				/* Distance between row1 and start_row */
				tmp_row = row1 - start_row;
				if (tmp_row < 0) tmp_row = (-tmp_row);

				/* Distance between col1 and start_col */
				tmp_col = col1 - start_col;
				if (tmp_col < 0) tmp_col = (-tmp_col);

				/* Terminate the tunnel */
				if ((tmp_row > 10) || (tmp_col > 10)) break;
			}
		}
	}


	/* Turn the tunnel into corridor */
	for (i = 0; i < dun->tunn_n; i++)
	{
		/* Access the grid */
		y = dun->tunn[i].y;
		x = dun->tunn[i].x;

		/* Access the grid */
		c_ptr = &cave[y][x];

		/* Clear previous contents, add a floor */
		c_ptr->feat = FEAT_FLOOR;
	}


	/* Apply the piercings that we found */
	for (i = 0; i < dun->wall_n; i++)
	{
		/* Access the grid */
		y = dun->wall[i].y;
		x = dun->wall[i].x;

		/* Access the grid */
		c_ptr = &cave[y][x];

		/* Clear previous contents, add up floor */
		c_ptr->feat = FEAT_FLOOR;

		/* Occasional doorway */
		if (rand_int(100) < dun_tun_pen)
		{
			/* Place a random door */
			place_random_door(y, x);
		}
	}
}




/*
 * Count the number of "corridor" grids adjacent to the given grid.
 *
 * Note -- Assumes "in_bounds(y1, x1)"
 *
 * XXX XXX This routine currently only counts actual "empty floor"
 * grids which are not in rooms.  We might want to also count stairs,
 * open doors, closed doors, etc.
 */
static int next_to_corr(int y1, int x1)
{
	int i, y, x, k = 0;

	cave_type *c_ptr;

	/* Scan adjacent grids */
	for (i = 0; i < 4; i++)
	{
		/* Extract the location */
		y = y1 + ddy_ddd[i];
		x = x1 + ddx_ddd[i];

		/* Skip non floors */
		if (!cave_floor_bold(y, x)) continue;

		/* Access the grid */
		c_ptr = &cave[y][x];

		/* Skip non "empty floor" grids */
		if (c_ptr->feat != FEAT_FLOOR) continue;

		/* Skip grids inside rooms */
		if (c_ptr->info & (CAVE_ROOM)) continue;

		/* Count these grids */
		k++;
	}

	/* Return the number of corridors */
	return (k);
}


/*
 * Determine if the given location is "between" two walls,
 * and "next to" two corridor spaces.  XXX XXX XXX
 *
 * Assumes "in_bounds(y,x)"
 */
static bool possible_doorway(int y, int x)
{
	/* Count the adjacent corridors */
	if (next_to_corr(y, x) >= 2)
	{
		/* Check Vertical */
		if ((cave[y-1][x].feat >= FEAT_MAGMA) &&
		    (cave[y+1][x].feat >= FEAT_MAGMA))
		{
			return (TRUE);
		}

		/* Check Horizontal */
		if ((cave[y][x-1].feat >= FEAT_MAGMA) &&
		    (cave[y][x+1].feat >= FEAT_MAGMA))
		{
			return (TRUE);
		}
	}

	/* No doorway */
	return (FALSE);
}


/*
 * Places door at y, x position if at least 2 walls found
 */
static void try_door(int y, int x)
{
	/* Paranoia */
	if (!in_bounds(y, x)) return;

	/* Ignore walls */
	if (cave[y][x].feat >= FEAT_MAGMA) return;

	/* Ignore room grids */
	if (cave[y][x].info & (CAVE_ROOM)) return;

	/* Occasional door (if allowed) */
	if ((rand_int(100) < dun_tun_jct) && possible_doorway(y, x))
	{
		/* Place a door */
		place_random_door(y, x);
	}
}


/*
 * Generate a new dungeon level
 *
 * Note that "dun_body" adds about 4000 bytes of memory to the stack.
 */
static bool cave_gen(void)
{
	int i, j, k, y, x, y1, x1;

	int max_vault_ok = 2;

#if 0
	int feat1, feat2;
#endif

	bool destroyed = FALSE;
	bool empty_level = FALSE;

	dun_data dun_body;

	/* Prepare allocation table */
	get_mon_num_prep(get_monster_hook(), NULL);

	/* Global data */
	dun = &dun_body;

	if (!max_panel_rows) max_vault_ok--;
	if (!max_panel_cols) max_vault_ok--;

	/* Randomize the dungeon creation values */
	dun_rooms = rand_range(DUN_ROOMS_MIN, DUN_ROOMS_MAX);
	dun_tun_rnd = rand_range(DUN_TUN_RND_MIN, DUN_TUN_RND_MAX);
	dun_tun_chg = rand_range(DUN_TUN_CHG_MIN, DUN_TUN_CHG_MAX);
	dun_tun_con = rand_range(DUN_TUN_CON_MIN, DUN_TUN_CON_MAX);
	dun_tun_pen = rand_range(DUN_TUN_PEN_MIN, DUN_TUN_PEN_MAX);
	dun_tun_jct = rand_range(DUN_TUN_JCT_MIN, DUN_TUN_JCT_MAX);

	/* Prfnoff added ironman option */
	if (ironman_empty_levels || (empty_levels && (randint(EMPTY_LEVEL) == 1)))
	{
		empty_level = TRUE;
		if (cheat_room)
			msg_print("Arena level.");
	}

	/* Hack -- Start with basic granite */
	for (y = 0; y < cur_hgt; y++)
	{
		for (x = 0; x < cur_wid; x++)
		{
			cave_type *c_ptr = &cave[y][x];

			if (empty_level)
				c_ptr->feat = FEAT_FLOOR;
			else
				/* Create granite wall */
				c_ptr->feat = FEAT_WALL_EXTRA;
		}
	}


	/* Possible "destroyed" level */
	if ((dun_level > 10) && (rand_int(DUN_DEST) == 0) && (small_levels))
		destroyed = TRUE;

	/* Hack -- No destroyed "quest" levels */
	if (quest_number(dun_level)) destroyed = FALSE;

	/* Actual maximum number of rooms on this level */
	dun->row_rooms = cur_hgt / BLOCK_HGT;
	dun->col_rooms = cur_wid / BLOCK_WID;

	/* Initialize the room table */
	for (y = 0; y < dun->row_rooms; y++)
	{
		for (x = 0; x < dun->col_rooms; x++)
		{
			dun->room_map[y][x] = FALSE;
		}
	}


	/* No "crowded" rooms yet */
	dun->crowded = FALSE;


	/* No rooms yet */
	dun->cent_n = 0;

	/* Build some rooms */
	for (i = 0; i < dun_rooms; i++)
	{
		/* Pick a block for the room */
		y = rand_int(dun->row_rooms);
		x = rand_int(dun->col_rooms);

		/* Align dungeon rooms */
		if (dungeon_align)
		{
			/* Slide some rooms right */
			if ((x % 3) == 0) x++;

			/* Slide some rooms left */
			if ((x % 3) == 2) x--;
		}

		/* Destroyed levels are boring */
		if (destroyed)
		{
			/* Attempt a "trivial" room */
			if (room_build(y, x, 1)) continue;

			/* Never mind */
			continue;
		}

		/* Attempt an "unusual" room */ /* Prfnoff */
		if (ironman_rooms || (rand_int(DUN_UNUSUAL) < dun_level))
		{
			/* Roll for room type */ /* Prfnoff */
			k = (ironman_rooms ? 0 : rand_int(100));

			/* Attempt a very unusual room */ /* Prfnoff */
			if (ironman_rooms || (rand_int(DUN_UNUSUAL) < dun_level))
			{
#ifdef FORCE_V_IDX
				if (room_build(y, x, 8)) continue;
#else
				/* Type 8 -- Greater vault (10%) */
				if (k < 10)
				{
					if (max_vault_ok > 1)
					{
						if (room_build(y, x, 8)) continue;
					}
					else
					{
						if (cheat_room) msg_print("Refusing a greater vault.");
					}
				}

				/* Type 7 -- Lesser vault (15%) */
				if (k < 25)
				{
					if (max_vault_ok > 0)
					{
						if (room_build(y, x, 7)) continue;
					}
					else
					{
						if (cheat_room) msg_print("Refusing a lesser vault.");
					}
				}


				/* Type 5 -- Monster nest (15%) */
				if ((k < 40) && room_build(y, x, 5)) continue;

				/* Type 6 -- Monster pit (15%) */
				if ((k < 55) && room_build(y, x, 6)) continue;
#endif

			}

			/* Type 4 -- Large room (20%) */
			if ((k < 20) && room_build(y, x, 4)) continue;

			/* Type 3 -- Cross room (20%) */
			if ((k < 40) && room_build(y, x, 3)) continue;

			/* Type 2 -- Overlapping (20%) */
			if ((k < 60) && room_build(y, x, 2)) continue;

			/* Type 10 -- Labyrinthine (20%)*/
			if ((k < 80) && room_build(y, x, 10)) continue;

			/* Type 9 -- Circular (20%) */
			if ((k < 100) && room_build(y, x, 9)) continue;
		}

		/* Attempt a trivial room */
		if (room_build(y, x, 1)) continue;
	}


	/* Special boundary walls -- Top */
	for (x = 0; x < cur_wid; x++)
	{
		cave_type *c_ptr = &cave[0][x];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}

	/* Special boundary walls -- Bottom */
	for (x = 0; x < cur_wid; x++)
	{
		cave_type *c_ptr = &cave[cur_hgt-1][x];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}

	/* Special boundary walls -- Left */
	for (y = 0; y < cur_hgt; y++)
	{
		cave_type *c_ptr = &cave[y][0];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}

	/* Special boundary walls -- Right */
	for (y = 0; y < cur_hgt; y++)
	{
		cave_type *c_ptr = &cave[y][cur_wid-1];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}


	/* Hack -- Scramble the room order */
	for (i = 0; i < dun->cent_n; i++)
	{
		int pick1 = rand_int(dun->cent_n);
		int pick2 = rand_int(dun->cent_n);
		y1 = dun->cent[pick1].y;
		x1 = dun->cent[pick1].x;
		dun->cent[pick1].y = dun->cent[pick2].y;
		dun->cent[pick1].x = dun->cent[pick2].x;
		dun->cent[pick2].y = y1;
		dun->cent[pick2].x = x1;
	}

	/* Start with no tunnel doors */
	dun->door_n = 0;

	/* Hack -- connect the first room to the last room */
	y = dun->cent[dun->cent_n-1].y;
	x = dun->cent[dun->cent_n-1].x;

	/* Connect all the rooms together */
	for (i = 0; i < dun->cent_n; i++)
	{
		/* Connect the room to the previous room */
		build_tunnel(dun->cent[i].y, dun->cent[i].x, y, x);

		/* Remember the "previous" room */
		y = dun->cent[i].y;
		x = dun->cent[i].x;
	}

	/* Place intersection doors	 */
	for (i = 0; i < dun->door_n; i++)
	{
		/* Extract junction location */
		y = dun->door[i].y;
		x = dun->door[i].x;

		/* Try placing doors */
		try_door(y, x - 1);
		try_door(y, x + 1);
		try_door(y - 1, x);
		try_door(y + 1, x);
	}


	/* Hack -- Add some magma streamers */
	for (i = 0; i < DUN_STR_MAG; i++)
	{
		build_streamer(FEAT_MAGMA, DUN_STR_MC);
	}

	/* Hack -- Add some quartz streamers */
	for (i = 0; i < DUN_STR_QUA; i++)
	{
		build_streamer(FEAT_QUARTZ, DUN_STR_QC);
	}

#if 0
	/* Choose water or lava */
	if (rand_int(MAX_DEPTH) > dun_level)
	{
		feat1 = FEAT_DEEP_WATER;
		feat2 = FEAT_SHAL_WATER;
	}
	else
	{
		feat1 = FEAT_DEEP_LAVA;
		feat2 = FEAT_SHAL_LAVA;
	}

	/* Hack -- Add some rivers */
	while(!rand_int(3)) add_river(feat1, feat2);
#endif /* 0 */

	/* Destroy the level if necessary */
	if (destroyed) destroy_level();

	if (terrain_streams) /* Prfnoff */
	{
		/* Add streamers of trees, water, or lava */
		if ((dun_level == 1) && (randint(20) > 10))
			for (i = 0; i < randint(DUN_STR_QUA); i++)
				build_streamer2(FEAT_TREES, FALSE, TRUE);

		if ((dun_level <= 19) && (randint(20) > 15))
		{
			for (i = 0; i < randint(DUN_STR_QUA - 1); i++)
				build_streamer2(FEAT_SHAL_WATER, FALSE, FALSE);
			if (randint(20) > 15)
			{
				for (i = 0; i < randint(DUN_STR_QUA); i++)
					build_streamer2(FEAT_DEEP_WATER, TRUE, (bool)(randint(10) < 3));
			}
		}
		else if ((dun_level > 19) && (randint(20) > 15))
		{
			for (i = 0; i < randint(DUN_STR_QUA); i++)
				build_streamer2(FEAT_SHAL_LAVA, FALSE, FALSE);
			if (randint(20) > 15)
			{
				for (i = 0; i < randint(DUN_STR_QUA - 1); i++)
					build_streamer2(FEAT_DEEP_LAVA, TRUE, (bool)(randint(10) < 3));
			}
		}
		else if ((randint(20) > 15))
		{
			for (i = 0; i < randint(DUN_STR_QUA - 1); i++)
				build_streamer2(FEAT_SHAL_WATER, FALSE, FALSE);

			if (randint(20) > 15)
			{
				for (i = 0; i < randint(DUN_STR_QUA); i++)
					build_streamer2(FEAT_DEEP_WATER, TRUE, (bool)(randint(10) < 3));
			}
		}
	}

	/* Place 3 or 4 down stairs near some walls */
	alloc_stairs(FEAT_MORE, rand_range(3, 4), 3);

	/* Place 1 or 2 up stairs near some walls */
	alloc_stairs(FEAT_LESS, rand_range(1, 2), 3);


	/* Determine the character location */
	if (!new_player_spot())
		return FALSE;

	/* Handle the quest monster placements */
	for (i = 0; i < max_quests; i++)
	{
		if ((quest[i].status == QUEST_STATUS_TAKEN) &&
		    ((quest[i].type == QUEST_TYPE_KILL_LEVEL) ||
		    (quest[i].type == QUEST_TYPE_RANDOM)) &&
		    (quest[i].level == dun_level) &&
			!(quest[i].flags & QUEST_FLAG_PRESET))
		{
			monster_race *r_ptr = &r_info[quest[i].r_idx];

			/* Hack -- "unique" monsters must be "unique" */
			if ((r_ptr->flags1 & RF1_UNIQUE) &&
			    (r_ptr->cur_num >= r_ptr->max_num))
			{
				/* The unique is already dead */
				quest[i].status = QUEST_STATUS_FINISHED;
			}
			else
			{
				bool group;

				/* Hard quests -> revive all monsters */
				if (ironman_hard_quests)
				{
					quest[i].cur_num = 0;
				}

				for (j = 0; j < (quest[i].max_num - quest[i].cur_num); j++)
				{
					for (k = 0; k < SAFE_MAX_ATTEMPTS; k++)
					{
						/* Find an empty grid */
						while (TRUE)
						{
							y = rand_int(cur_hgt);
							x = rand_int(cur_wid);
							if (!cave_naked_bold(y, x)) continue;
							if (distance(y, x, py, px) < 10) continue;
							else break;
						}

						if (r_ptr->flags1 & RF1_FRIENDS)
							group = FALSE;
						else
							group = TRUE;

						/* Try to place the monster */
						if (place_monster_aux(y, x, quest[i].r_idx, FALSE, group, FALSE, FALSE))
						{
							/* Success */
							break;
						}
						else
						{
							/* Failure - Try again */
							continue;
						}
					}
				}
			}
		}
	}


	/* Basic "amount" */
	k = (dun_level / 3);
	if (k > 10) k = 10;
	if (k < 2) k = 2;

	/* Pick a base number of monsters */
	i = MIN_M_ALLOC_LEVEL;

	/* To make small levels a bit more playable */
	if (cur_hgt < MAX_HGT || cur_wid < MAX_WID)
	{
		int small_tester = i;

		i = (i * cur_hgt) / MAX_HGT;
		i = (i * cur_wid) / MAX_WID;
		i += 1;

		if (i > small_tester) i = small_tester;
		else if (cheat_hear)
		{
			msg_format("Reduced monsters base from %d to %d", small_tester, i);
		}
	}

	i += randint(8);

	/* Put some monsters in the dungeon */
	for (i = i + k; i > 0; i--)
	{
		(void)alloc_monster(0, TRUE);
	}

	/* Place some traps in the dungeon */
	alloc_object(ALLOC_SET_BOTH, ALLOC_TYP_TRAP, randint(k));

	/* Put some rubble in corridors */
	alloc_object(ALLOC_SET_CORR, ALLOC_TYP_RUBBLE, randint(k));

	/* Put some objects in rooms */
	alloc_object(ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, randnor(DUN_AMT_ROOM, 3));

	/* Put some objects/gold in the dungeon */
	alloc_object(ALLOC_SET_BOTH, ALLOC_TYP_OBJECT, randnor(DUN_AMT_ITEM, 3));
	alloc_object(ALLOC_SET_BOTH, ALLOC_TYP_GOLD, randnor(DUN_AMT_GOLD, 3));

	if (empty_level && ((randint(DARK_EMPTY) != 1) || (randint(100) > dun_level)))
	{
		/* Lite the cave */
		for (y = 0; y < cur_hgt; y++)
		{
			for (x = 0; x < cur_wid; x++)
			{
				cave[y][x].info |= (CAVE_GLOW);
			}
		}
	}

	return TRUE;
}


/*
 * Generate a quest level
 */
static void quest_gen(void)
{
	int x, y;


	/* Start with perm walls */
	for (y = 0; y < cur_hgt; y++)
	{
		for (x = 0; x < cur_wid; x++)
		{
			cave[y][x].feat = FEAT_PERM_SOLID;
		}
	}

	/* Set the quest level */
	base_level = quest[p_ptr->inside_quest].level;
	dun_level = base_level;
	object_level = base_level;
	monster_level = base_level;

	/* Prepare allocation table */
	get_mon_num_prep(get_monster_hook(), NULL);

	init_flags = INIT_CREATE_DUNGEON | INIT_ASSIGN;
	process_dungeon_file("q_info.txt", 0, 0, MAX_HGT, MAX_WID);
}


/*
 * Generates a random dungeon level			-RAK-
 *
 * Hack -- regenerate any "overflow" levels
 *
 * Hack -- allow auto-scumming via a gameplay option.
 */
void generate_cave(void)
{
	int tester_1, tester_2;
	int y, x, num;


	/* The dungeon is not ready */
	character_dungeon = FALSE;

	/* Generate */
	for (num = 0; TRUE; num++)
	{
		bool okay = TRUE;

		cptr why = NULL;


		/* XXX XXX XXX XXX */
		o_max = 1;
		m_max = 1;

		/* Start with a blank cave */
		for (y = 0; y < MAX_HGT; y++)
		{
			for (x = 0; x < MAX_WID; x++)
			{
				/* No flags */
				cave[y][x].info = 0;

				/* No features */
				cave[y][x].feat = 0;

				/* No objects */
				cave[y][x].o_idx = 0;

				/* No monsters */
				cave[y][x].m_idx = 0;

				/* No mimic */
				cave[y][x].mimic = 0;

#ifdef MONSTER_FLOW
				/* No flow */
				cave[y][x].cost = 0;
				cave[y][x].when = 0;
#endif /* MONSTER_FLOW */
			}
		}


		/* Mega-Hack -- no player yet */
		px = py = 0;


		/* Mega-Hack -- no panel yet */
		panel_row_min = 0;
		panel_row_max = 0;
		panel_col_min = 0;
		panel_col_max = 0;


		/* Set the base level */
		base_level = dun_level;

		/* Reset the monster generation level */
		monster_level = base_level;

		/* Reset the object generation level */
		object_level = base_level;

		/* Nothing special here yet */
		good_item_flag = FALSE;

		/* Nothing good here yet */
		rating = 0;

#ifdef USE_SCRIPT
		if (!generate_level_callback(dun_level))
#endif /* USE_SCRIPT */
		{
			/* Quest levels -KMW- */
			if (p_ptr->inside_quest)
			{
				quest_gen();
			}

			/* Build the town */
			else if (!dun_level)
			{
				/* Big town */
				cur_hgt = MAX_HGT;
				cur_wid = MAX_WID;

				/* Determine number of panels */
				max_panel_rows = (cur_hgt / SCREEN_HGT) * 2 - 2;
				max_panel_cols = (cur_wid / SCREEN_WID) * 2 - 2;

				/* Assume illegal panel */
				panel_row = max_panel_rows;
				panel_col = max_panel_cols;

				/* Make the wilderness */
				wilderness_gen();
			}

			/* Build a real level */
			else
			{
				if (ironman_small_levels || /* Prfnoff */
					((randint(SMALL_LEVEL) == 1) && small_levels))
				{
					if (cheat_room)
						msg_print("A 'small' dungeon level.");

					do
					{
						tester_1 = randint(MAX_HGT/SCREEN_HGT);
						tester_2 = randint(MAX_WID/SCREEN_WID);
					}
					while ((tester_1 == MAX_HGT/SCREEN_HGT) &&
						   (tester_2 == MAX_WID/SCREEN_WID));

					cur_hgt = tester_1 * SCREEN_HGT;
					cur_wid = tester_2 * SCREEN_WID;

					/* Determine number of panels */
					max_panel_rows = (cur_hgt / SCREEN_HGT) * 2 - 2;
					max_panel_cols = (cur_wid / SCREEN_WID) * 2 - 2;

					/* Assume illegal panel */
					panel_row = max_panel_rows;
					panel_col = max_panel_cols;

					if (cheat_room)
						msg_format("X:%d, Y:%d.", max_panel_cols, max_panel_rows);
				}
				else
				{
					/* Big dungeon */
					cur_hgt = MAX_HGT;
					cur_wid = MAX_WID;

					/* Determine number of panels */
					max_panel_rows = (cur_hgt / SCREEN_HGT) * 2 - 2;
					max_panel_cols = (cur_wid / SCREEN_WID) * 2 - 2;

					/* Assume illegal panel */
					panel_row = max_panel_rows;
					panel_col = max_panel_cols;
				}

				/* Make a dungeon */
				if (!cave_gen())
				{
					why = "could not place player";
					okay = FALSE;
				}
			}
		}

		/* Extract the feeling */
		if (rating > 100) feeling = 2;
		else if (rating > 80) feeling = 3;
		else if (rating > 60) feeling = 4;
		else if (rating > 40) feeling = 5;
		else if (rating > 30) feeling = 6;
		else if (rating > 20) feeling = 7;
		else if (rating > 10) feeling = 8;
		else if (rating > 0) feeling = 9;
		else feeling = 10;

		/* Hack -- Have a special feeling sometimes */
		if (good_item_flag && !p_ptr->preserve) feeling = 1;

		/* It takes 1000 game turns for "feelings" to recharge */
		if ((turn - old_turn) < 1000) feeling = 0;

		/* Hack -- no feeling in the town */
		if (!dun_level) feeling = 0;


		/* Prevent object over-flow */
		if (o_max >= max_o_idx)
		{
			/* Message */
			why = "too many objects";

			/* Message */
			okay = FALSE;
		}

		/* Prevent monster over-flow */
		if (m_max >= max_m_idx)
		{
			/* Message */
			why = "too many monsters";

			/* Message */
			okay = FALSE;
		}

		/* Mega-Hack -- "auto-scum" */
		if ((auto_scum || ironman_autoscum) && (num < 100) && !p_ptr->inside_quest)
		{
			/* Require "goodness" */
			if ((feeling > 9) ||
			    ((dun_level >= 5) && (feeling > 8)) ||
			    ((dun_level >= 10) && (feeling > 7)) ||
			    ((dun_level >= 20) && (feeling > 6)) ||
			    ((dun_level >= 40) && (feeling > 5)))
			{
				/* Give message to cheaters */
				if (cheat_room || cheat_hear ||
				    cheat_peek || cheat_xtra)
				{
					/* Message */
					why = "boring level";
				}

				/* Try again */
				okay = FALSE;
			}
		}

		/* Accept */
		if (okay) break;


		/* Message */
		if (why) msg_format("Generation restarted (%s)", why);

		/* Wipe the objects */
		wipe_o_list();

		/* Wipe the monsters */
		wipe_m_list();
	}


	/* The dungeon is ready */
	character_dungeon = TRUE;

	/* Remember when this level was "created" */
	old_turn = turn;
}
