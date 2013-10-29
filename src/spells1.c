/* File: spells1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"

#include "script.h"

/*
 * Helper function -- return a "nearby" race for polymorphing
 *
 * Note that this function is one of the more "dangerous" ones...
 */
s16b poly_r_idx(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	int i, r, lev1, lev2;
	int frlev = r_ptr->level;
	if (frlev > 40) frlev -= (frlev - 38) / 3; /* lower minimum level */

	/* Hack -- Uniques never polymorph */
	if (r_ptr->flags1 & (RF1_UNIQUE)) return (r_idx);

	/* Allowable range of "levels" for resulting monster */
	lev1 = frlev - ((randint(20)/randint(9))+1);
	lev2 = r_ptr->level + ((randint(20)/randint(9))+1);

	/* Pick a (possibly new) non-unique race */
	for (i = 0; i < 1000; i++)
	{
		/* Pick a new race, using a level calculation */
		r = get_mon_num(((p_ptr->depth + r_ptr->level) / 2) + (2 + badluck/3), FALSE);

		/* Handle failure */
		if (!r) break;

		/* Obtain race */
		r_ptr = &r_info[r];

		/* Ignore unique monsters */
		if (r_ptr->flags1 & (RF1_UNIQUE)) continue;

		/* Ignore monsters with incompatible levels */
		if ((r_ptr->level < lev1) || (r_ptr->level > lev2)) continue;

		/* Use that index */
		r_idx = r;

		/* Done */
		break;
	}

	/* Result */
	return (r_idx);
}


/*
 * Teleport monster, normally up to "dis" grids away.
 *
 * Attempt to move the monster at least "dis/2" grids away.
 *
 * But allow variation to prevent infinite loops.
 *
 * mode == 0  - normal mode (monster casted the spell)
 * mode == 1  - PC casted the spell (less likely to teleport closer close to the PC)
 *           (this is also sometimes used when the monster is afraid)
 * mode == 2  - monster is blinking from out of view of the player, so
 *           blink toward the player.
 * mode == 3  - monster cast the spell while afraid (try to teleport further away)
 */
void teleport_away(int m_idx, int dis, int mode)
{
	int ny, nx, oy, ox, d, i, min, ooy, oox;

	bool look = TRUE;
	bool warppull = FALSE;

	/* for special modes */
    bool trysuccess = FALSE;
	int tryfurther = 0;

	monster_type *m_ptr = &mon_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	/* get monster's escape spell flags */
	u32b f6 = r_ptr->flags6;

	/* Paranoia */
	if (!m_ptr->r_idx) return;

	/* monster is no longer holding the PC */
	if ((p_ptr->timed[TMD_BEAR_HOLD]) && (m_idx == p_ptr->held_m_idx) &&
		((mode) || (randint(100) < 50)))
	{
		p_ptr->held_m_idx = 0;
		clear_timed(TMD_BEAR_HOLD);
	}
	/* monster takes the player with it (only when mode == 0) */
	else if ((p_ptr->timed[TMD_BEAR_HOLD]) && (m_idx == p_ptr->held_m_idx))
	{
		warppull = TRUE;
	}

	/* Save the old location (twice) */
	oy = m_ptr->fy;
	ox = m_ptr->fx;
	ooy = m_ptr->fy;
	oox = m_ptr->fx;
	
	/* if player cast the spell try to teleport the monster further away */
    if (mode == 1)
    {
        tryfurther = 50 + p_ptr->lev + (goodluck/2);
        if (p_ptr->telecontrol) tryfurther += 8 + ((1 + p_ptr->learnedcontrol) * 2);
    }
    /* monster cast the spell */
    if (mode > 1)
    {
        tryfurther = 25 + (r_ptr->level/3);
		/* having a lot of teleportation spells makes it better at controlling it */
		if (r_ptr->flags4 & (RF6_TPORT)) tryfurther += 15;
		if (r_ptr->flags4 & (RF6_TELE_TO)) tryfurther += 15;
		if (r_ptr->flags4 & (RF6_TELE_AWAY)) tryfurther += 15;
		if (r_ptr->flags2 & (RF2_SMART)) tryfurther += 15;
		/* tengus are masters of teleportation */
		if (m_ptr->r_idx == 176) tryfurther = 105;
    }
    
    if (randint(105) < tryfurther) trysuccess = TRUE;
    
    /* teleport further away from the PC */
    if ((trysuccess) && (mode == 1))
    {
	   oy = p_ptr->py;
	   ox = p_ptr->px;
	   dis += m_ptr->cdis / 2;
    }

	/* Minimum distance */
	min = dis / 2;

	/* Look until done */
	while (look)
	{
		/* Verify max distance */
		if (dis > 200) dis = 200;

		/* Try several locations */
		for (i = 0; i < 500; i++)
		{
			/* Pick a (possibly illegal) location */
			while (1)
			{
				ny = rand_spread(oy, dis);
				nx = rand_spread(ox, dis);
				d = distance(oy, ox, ny, nx);
				if ((d >= min) && (d <= dis)) break;
			}

			/* mode 2: monster is trying to teleport towards the player */
			/* so don't accept locations which are further from the PC */
			if ((mode == 2) && (trysuccess) && 
               (distance(p_ptr->py, p_ptr->px, ny, nx) > m_ptr->cdis))
			   continue;
			/* mode 3: monster is afriad */
			/* so don't accept locations which are closer to the PC */
            if ((mode == 3) && (trysuccess) && 
               (distance(p_ptr->py, p_ptr->px, ny, nx) < m_ptr->cdis))
			   continue;

			/* Ignore illegal locations */
			if (!in_bounds_fully(ny, nx)) continue;

			/* Require "empty" floor space (now allows rubble) */
			/* if (!cave_empty_bold(ny, nx)) continue; */
			if (!cave_can_occupy_bold(ny, nx)) continue;

			/* Hack -- no teleport onto glyph of warding */
			if (cave_feat[ny][nx] == FEAT_GLYPH) continue;

			/* No teleporting into vaults and such */
			/* if (cave_info[ny][nx] & (CAVE_ICKY)) continue; */

			/* This grid looks good */
			look = FALSE;

			/* Stop looking */
			break;
		}

		/* Increase the maximum distance */
		if (dis > 90) dis += (dis * 3) / 2;
		else dis = dis * 2;

		/* Decrease the minimum distance */
		min = min / 2;
	}

	/* Monsters can hide again whenever they teleport out of sight of the PC */
	if ((!player_can_see_bold(ny, nx)) && (!warppull))
	{
		/* go back into hiding */
	    if (m_ptr->monseen > 3) m_ptr->monseen -= randint(2);
	    else if (m_ptr->monseen > 2) m_ptr->monseen -= 1;
		else if ((m_ptr->monseen) && (rand_int(100) < r_ptr->stealth*10))
		  m_ptr->monseen -= 1;
		  
		/* mimmics can disguise themselves again */
		if ((r_ptr->flags1 & (RF1_CHAR_MULTI)) && (strchr("!,-_~=?#%:.$", r_ptr->d_char)))
		{
	        if (strchr("!", r_ptr->d_char)) /* potion mimmics */
			{
		        /* disguised = index of object it is disguised as */
				m_ptr->disguised = lookup_kind(75, 16 + rand_int(30));
			}
			else if (strchr(",", r_ptr->d_char)) /* mushroom mimmic (currently none) */
			{
				m_ptr->disguised = lookup_kind(80, 0 + rand_int(19));
			}
			else if (strchr("-", r_ptr->d_char)) /* wand mimmics */
			{
				m_ptr->disguised = lookup_kind(65, 0 + rand_int(25));
			}
			else if (strchr("_", r_ptr->d_char)) /* staff mimmics */
			{
				m_ptr->disguised = lookup_kind(55, 0 + rand_int(33));
			}
			else if (strchr("~", r_ptr->d_char)) /* chest mimmics */
			{
				m_ptr->disguised = lookup_kind(7, 1 + rand_int(5));
			}
			else if (strchr("$", r_ptr->d_char)) /* coin mimmics */
			{
				if (p_ptr->depth < 25) m_ptr->disguised = lookup_kind(100, 1 + rand_int(15));
				else m_ptr->disguised = lookup_kind(100, 6 + rand_int(14));
			}
			else if (strchr("=", r_ptr->d_char)) /* ring mimmics */
			{
				int dk = 6 + rand_int(22);
				if (dk > 12) dk += 3; /* no 13-15 svals  */
				m_ptr->disguised = lookup_kind(45, dk);
			}
			else if (strchr("?", r_ptr->d_char)) /* scroll mimmics */
			{
				int dk = 7 + rand_int(36);
				/* no scroll sval for these numbers (ugly) */
				if ((dk == 19) || (dk == 23) || (dk == 26) || (dk == 31))
					dk = 7 + rand_int(12);
        	    m_ptr->disguised = lookup_kind(70, dk);
			}
			/* disguised as a wall or floor ('#', '.', ':' or '%') */
        	else m_ptr->disguised = 1;
		}
	}

	/* Sound */
	sound(MSG_TPOTHER);

	/* Swap the monsters (this calls update_mon() ) */
	monster_swap(ooy, oox, ny, nx);

	/* monster took the player with it */
	if (warppull)
	{
		teleport_player_to(ny, nx);
	}
}


/*
 * Teleport the player to a location up to "dis" grids away.
 *
 * If no such spaces are readily available, the distance may increase.
 * Try very hard to move the player at least a quarter that distance.
 */
void teleport_player(int dis)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int d, i, min, y, x;

	bool look = TRUE;


	/* Initialize */
	y = py;
	x = px;

	/* Minimum distance */
	min = dis / 2;

	/* minimum distance shouldn't be too high */
	if (min > 50) min = min - (min-49)/2;
	if (min > 60) min = min - (min-59)/2;
    if (min > 74) min = 74;

	/* Look until done */
	while (look)
	{
		/* Verify max distance */
		if (dis > 200) dis = 200;

		/* Try several locations */
		for (i = 0; i < 500; i++)
		{
			/* Pick a (possibly illegal) location */
			while (1)
			{
				y = rand_spread(py, dis);
				x = rand_spread(px, dis);
				d = distance(py, px, y, x);
				if ((d >= min) && (d <= dis)) break;
			}

			/* Ignore illegal locations */
			if (!in_bounds_fully(y, x)) continue;

			/* Require "can occupy" floor space */
			if (!cave_can_occupy_bold(y, x)) continue;
			
			/* make sure you don't teleport onto a trap door */
			if ((cave_feat[y][x] == FEAT_INVIS) || 
				(cave_feat[y][x] == FEAT_TRAP_HEAD + 0x00)) continue;

			/* No teleporting into vaults and such */
			if (cave_info[y][x] & (CAVE_ICKY)) continue;

			/* This grid looks good */
			look = FALSE;

			/* Stop looking */
			break;
		}

		/* Increase the maximum distance */
		dis = dis * 2;

		/* Decrease the minimum distance */
		min = min / 2;
	}

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player */
	monster_swap(py, px, y, x);
	
	disturb(0, 0);

	/* Handle stuff XXX XXX XXX */
	handle_stuff();
}



/*
 * Teleport player to a grid near the given location
 *
 * This function is slightly obsessive about correctness.
 * This function allows teleporting into vaults (except with teleport control)
 *
 * Used by tele_to monster spell, nexus and teleport control.
 */
void teleport_player_to(int ny, int nx)
{
	int py = p_ptr->py;
	int px = p_ptr->px;

	int y, x;

	int dis = 0, ctr = 0;

	/* Initialize */
	y = py;
	x = px;
	
	/* teleport control cannot teleport into vaults */
	if ((spellswitch == 13) && (cave_info[ny][nx] & (CAVE_ICKY)))
	{
       return; /* FAIL */
    }

	/* Find a usable location */
	while (1)
	{
		/* Pick a nearby legal location */
		while (1)
		{
			y = rand_spread(ny, dis);
			x = rand_spread(nx, dis);
			if (in_bounds_fully(y, x)) break;
		}

		/* Accept empty floor grids */
		if (cave_can_occupy_bold(y, x)) break;

		/* (forbid traps but not objects) */
		if ((cave_feat[y][x] >= FEAT_TRAP_HEAD) && (cave_feat[y][x] <= FEAT_TRAP_TAIL))
			break;

		/* Occasionally advance the distance */
		if (++ctr > (4 * dis * dis + 4 * dis + 1))
		{
			ctr = 0;
			dis++;
		}
	}

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player */
	monster_swap(py, px, y, x);

	/* Handle stuff XXX XXX XXX */
	handle_stuff();
}



/*
 * Teleport the player one level up or down (random when legal)
 */
void teleport_player_level(void)
{
	if (adult_ironman)
	{
		msg_print("Nothing happens.");
		return;
	}


	if (!p_ptr->depth)
	{
		message(MSG_TPLEVEL, 0, "You sink through the floor.");

		/* New depth */
		p_ptr->depth++;

		/* Leaving */
		p_ptr->leaving = TRUE;
	}

	else if (is_quest(p_ptr->depth) || (p_ptr->depth >= MAX_DEPTH-1))
	{
		message(MSG_TPLEVEL, 0, "You rise up through the ceiling.");

		/* New depth */
		p_ptr->depth--;

		/* Leaving */
		p_ptr->leaving = TRUE;
	}

	else if (rand_int(100) < 50)
	{
		message(MSG_TPLEVEL, 0, "You rise up through the ceiling.");

		/* New depth */
		p_ptr->depth--;

		/* Leaving */
		p_ptr->leaving = TRUE;
	}

	else
	{
		message(MSG_TPLEVEL, 0, "You sink through the floor.");

		/* New depth */
		p_ptr->depth++;

		/* Leaving */
		p_ptr->leaving = TRUE;
	}
}

/*
 * Teleport the player 1-2 levels past their deepest level so far
 */
void deep_descent(void)
{
	   if ((p_ptr->depth >= MAX_DEPTH-2) || (is_quest(p_ptr->depth)))
	   {
		   msg_print("Nothing happens.");
		   return;
       }
       
       message(MSG_TPLEVEL, 0, "You sink through the floor.");

	   /* New depth */
	   if ((adult_ironman) || (p_ptr->depth > 96)) p_ptr->depth = p_ptr->max_depth + 1;
	   else p_ptr->depth = p_ptr->max_depth + randint(2);

	   /* Leaving */
	   p_ptr->leaving = TRUE;
}


/*
 * Attempt controlled teleportation
 * high resistcrtl is usually caused by a monster
 */
bool control_tport(int resistcrtl, int enforcedist)
{
     int opy, opx, controlchance, dir;
	 bool worked = TRUE;
 
	 /* paranoia */
     if (!p_ptr->telecontrol) return FALSE;

     /* negative timed modifiers */
     if (p_ptr->timed[TMD_CURSE]) resistcrtl += 25;
     if ((p_ptr->timed[TMD_FRENZY]) || (p_ptr->timed[TMD_CONFUSED])) resistcrtl += 100;

     /* save old location */
     opy = p_ptr->py;
	 opx = p_ptr->px;

     /* magic device skill minus class 10/level improvement (1/3) */
     controlchance = (p_ptr->skills[SKILL_DEV] - (cp_ptr->x_dev * p_ptr->lev / 10)) / 3;

     /* cap magic device factor */
     /* learning telecontrol must be risky or else it'd be overpowered */
     if (controlchance > 50) controlchance = 50;
     
     /* positive timed modifiers */
     if ((p_ptr->timed[TMD_BRAIL]) || (p_ptr->timed[TMD_SKILLFUL])) controlchance += 20;
     if (p_ptr->timed[TMD_BLESSED]) controlchance += 10;

     /* luck factor */
     controlchance += (goodluck+1)/2 - (badluck+1)/2;

     /* learned skill from experience with teleport control */
     if (p_ptr->learnedcontrol) controlchance += p_ptr->learnedcontrol;
     
     /* minimum chance */
     if ((controlchance < 18 + p_ptr->learnedcontrol) && (controlchance < 30))
        controlchance = 18 + p_ptr->learnedcontrol;

     /* attempt to gain control */
     if (randint(150 + resistcrtl) > controlchance) return FALSE;

     /* spellswitch 13 prevents using old target in get_aim_dir() */
     /* and prevents teleporting into vaults in teleport_player_to() */
     spellswitch = 13;
            
     /* prompt and get destination */
     msg_print(format("Where do you want to teleport to? (Max distance %d) -more-", enforcedist));
     if (inkey() == ESCAPE) /* do nothing (just a -more-) */;

     if (get_aim_dir(&dir))
     {
         /* enforce blinking distance */
         if (enforcedist < distance(p_ptr->py, p_ptr->px, p_ptr->target_row, p_ptr->target_col))
         {
            if (enforcedist < 30) msg_print("Too far away to blink to!");
            else msg_print("Too far away!");
            spellswitch = 0;
            return FALSE;
         }
         
         /* Teleport to the target */
		 teleport_player_to(p_ptr->target_row, p_ptr->target_col);
     }

     /** check for success **/
     /* teleport control worked */
     if ((p_ptr->py == p_ptr->target_row) && (p_ptr->px == p_ptr->target_col))
     {
         bool learn = FALSE;
         /* gain skill (usually) (prevent scumming for skill on L0-2) */
         if ((rand_int(100) < 85 + goodluck - badluck) && (p_ptr->depth > 2))
         {
            if (p_ptr->learnedcontrol < 50) learn = TRUE;
            else if ((p_ptr->learnedcontrol < 75) &&
                    (rand_int(100) < 25 - (p_ptr->learnedcontrol/4))) learn = TRUE;
         }
         /* first bit of skill is easy to get */
         else if (p_ptr->learnedcontrol < 2) learn = TRUE;

         if (learn)
         {
             p_ptr->learnedcontrol += 1;

 		     /* message for testing purposes only */
	         msg_print("You feel slightly more in control.");
         }

		 /* Handle objects (later) */
		 p_ptr->notice |= (PN_PICKUP);
     }
     /* if teleport control failed, teleport randomly (elsewhere) */
	 else
     {
         /* small chance to gain skill anyway */
         /* (about 1/11 chance with max luck, 1/100 with no luck) */
         if (rand_int(200) < ((goodluck * 3) + 9) / 4)
         {
            if ((p_ptr->learnedcontrol < 50) && (p_ptr->depth > 2)) 
               p_ptr->learnedcontrol += 1;
         }

         worked = FALSE;
     }

     /* reset target and spellswitch (would usually not want */
	 /*  to use the same target as a teleport-to target) */
	 p_ptr->target_set = 0;
     spellswitch = 0;
     
     return worked;
}


/*
 * Return a color to use for the bolt/ball spells
 */
static byte spell_color(int type)
{
	/* Analyze */
	switch (type)
	{
		case GF_MISSILE:	return (TERM_VIOLET);
		case GF_ACID:		return (TERM_SLATE);
		case GF_ELEC:		return (TERM_L_GREEN);
		case GF_FIRE:		return (TERM_RED);
		case GF_ZOMBIE_FIRE: return (TERM_L_RED);
		case GF_COLD:		return (TERM_WHITE);
		case GF_POIS:		return (TERM_GREEN);
		case GF_HOLY_ORB:	return (TERM_WHITE);
		case GF_MANA:		return (TERM_L_DARK);
		case GF_ARROW:		return (TERM_WHITE);
		case GF_THROW:      return (TERM_UMBER);
		case GF_AMNESIA:    return (TERM_L_UMBER);
		case GF_WATER:		return (TERM_SLATE);
		case GF_NETHER:		return (TERM_L_GREEN);
		case GF_CHAOS:		return (TERM_VIOLET);
		case GF_DISENCHANT:	return (TERM_VIOLET);
		case GF_NEXUS:		return (TERM_L_RED);
		case GF_CONFUSION:	return (TERM_L_UMBER);
		case GF_SOUND:		return (TERM_YELLOW);
		case GF_SHARD:		return (TERM_UMBER);
		case GF_FORCE:		return (TERM_L_DARK);
		case GF_INERTIA:	return (TERM_L_WHITE);
		case GF_GRAVITY:	return (TERM_L_WHITE);
		case GF_TIME:		return (TERM_L_BLUE);
		case GF_LITE_WEAK:	return (TERM_ORANGE);
		case GF_LITE:		return (TERM_ORANGE);
		case GF_DARK_WEAK:	return (TERM_L_DARK);
		case GF_DARK:		return (TERM_L_DARK);
		case GF_BOULDER:    return (TERM_SLATE);
		case GF_PLASMA:		return (TERM_RED);
		case GF_METEOR:		return (TERM_RED);
		case GF_ICE:		return (TERM_WHITE);
		case GF_SILENCE_S:  return (TERM_ORANGE);
		case GF_SILVER_BLT: return (TERM_L_WHITE); /* silver */
	}

	/* Standard "color" */
	return (TERM_WHITE);
}



/*
 * Find the attr/char pair to use for a spell effect
 *
 * It is moving (or has moved) from (x,y) to (nx,ny).
 *
 * If the distance is not "one", we (may) return "*".
 */
static u16b bolt_pict(int y, int x, int ny, int nx, int typ)
{
	int base;

	byte k;

	byte a;
	char c;

	if (!(use_graphics && (arg_graphics == GRAPHICS_DAVID_GERVAIS)))
	{
		/* No motion (*) */
		if ((ny == y) && (nx == x)) base = 0x30;

		/* Vertical (|) */
		else if (nx == x) base = 0x40;

		/* Horizontal (-) */
		else if (ny == y) base = 0x50;

		/* Diagonal (/) */
		else if ((ny-y) == (x-nx)) base = 0x60;

		/* Diagonal (\) */
		else if ((ny-y) == (nx-x)) base = 0x70;

		/* Weird (*) */
		else base = 0x30;

		/* Basic spell color */
		k = spell_color(typ);

		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k];
	}
	else
	{
		int add;

		/* No motion (*) */
		if ((ny == y) && (nx == x)) {base = 0x00; add = 0;}

		/* Vertical (|) */
		else if (nx == x) {base = 0x40; add = 0;}

		/* Horizontal (-) */
		else if (ny == y) {base = 0x40; add = 1;}

		/* Diagonal (/) */
		else if ((ny-y) == (x-nx)) {base = 0x40; add = 2;}

		/* Diagonal (\) */
		else if ((ny-y) == (nx-x)) {base = 0x40; add = 3;}

		/* Weird (*) */
		else {base = 0x00; add = 0;}

		if (typ >= 0x40) k = 0;
		else k = typ;

		/* Obtain attr/char */
		a = misc_to_attr[base+k];
		c = misc_to_char[base+k] + add;
	}

	/* Create pict */
	return (PICT(a,c));
}




/*
 * Decreases players hit points and sets death flag if necessary
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 *
 * New disturbnow parameter allows you to rest without being disturbed
 * while poisoned or cut as long as you are above your HP warning mark.
 */
void take_hit_reallynow(int dam, cptr kb_str, bool disturbnow)
{
	int old_chp = p_ptr->chp;
	int warning = (p_ptr->mhp * op_ptr->hitpoint_warn / 10);
	
	/* minimal HP warning */
	if (warning < p_ptr->mhp / 50 + 2) warning = (p_ptr->mhp / 50) + 2;

	/* Paranoia */
	if (p_ptr->is_dead) return;


	/* Disturb */
	if (disturbnow) disturb(1, 0);

	/* Mega-Hack -- Apply "invulnerability" */
	/* if (p_ptr->timed[TMD_INVULN] && (dam < 9000)) return; */

	/* Hurt the player */
	p_ptr->chp -= dam;

	/* Display the hitpoints */
	p_ptr->redraw |= (PR_HP);

	/* Window stuff */
	p_ptr->window |= (PW_PLAYER_0 | PW_PLAYER_1);

	/* Dead player */
	if (p_ptr->chp < 0)
	{
		/* Hack -- Note death */
		message(MSG_DEATH, 0, "You die.");
		message_flush();

		/* Note cause of death */
		my_strcpy(p_ptr->died_from, kb_str, sizeof(p_ptr->died_from));

		/* No longer a winner */
		/* If the PC killed Morgoth, death should not keep him from being a winner. */
		/* p_ptr->total_winner = FALSE; */

		/* Note death */
		p_ptr->is_dead = TRUE;

		/* Leaving */
		p_ptr->leaving = TRUE;

#ifdef yes_c_history
        if (1) /* always true, just wanted to keep the declarations within this #ifdef */
        {
		   /* Write a note */
			time_t ct = time((time_t*)0);
			char long_day[25];
			char buf[120];

 		  	/* Get time */
 		  	(void)strftime(long_day, 25, "%m/%d/%Y at %I:%M %p", localtime(&ct));

 		  	/* Add note */
		  	fprintf(notes_file, "============================================================\n");

			/*killed by */
 		  	sprintf(buf, "Killed by %s.", p_ptr->died_from);

			/* Write message */
            do_cmd_note(buf,  p_ptr->depth, FALSE);

			/* date and time*/
			sprintf(buf, "Killed on %s.", long_day);

			/* Write message */
            do_cmd_note(buf,  p_ptr->depth, FALSE);

			fprintf(notes_file, "============================================================\n");
        }
#endif

		/* Dead */
		return;
	}

	/* Hitpoint warning */
	if (p_ptr->chp < warning)
	{
		/* always disturb when below low HP warning mark */
        disturb(1, 0);

        /* Hack -- bell on first notice */
		if (old_chp > warning)
		{
			bell("Low hitpoint warning!");
		}

		/* Message */
		message(MSG_HITPOINT_WARN, 0, "*** LOW HITPOINT WARNING! ***");
		message_flush();
	}
}


/*
 * Call the real function with an added parameter
 * always disturbs (normal usage)
 */
void take_hit(int dam, cptr kb_str)
{
     take_hit_reallynow(dam, kb_str, TRUE);
}



/*
 * Does a given class of objects (usually) hate acid?
 * Note that acid can either melt or corrode something.
 */
static bool hates_acid(const object_type *o_ptr)
{
	/* Analyze the type */
	switch (o_ptr->tval)
	{
		/* Wearable items */
		case TV_ARROW:
		case TV_BOLT:
		case TV_BOW:
		case TV_SWORD:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_HELM:
		case TV_CROWN:
		case TV_SHIELD:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		case TV_HARD_ARMOR:
		case TV_DRAG_ARMOR:
		{
			return (TRUE);
		}

		/* Staffs/Scrolls are wood/paper */
		/* black magic books resist fire but burn up in acid */
		case TV_STAFF:
		case TV_SCROLL:
		case TV_DARK_BOOK:
		{
			return (TRUE);
		}

		/* Ouch */
		case TV_CHEST:
		{
			return (TRUE);
		}

		/* Junk is useless */
		case TV_SKELETON:
		case TV_BOTTLE:
		case TV_JUNK:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Does a given object (usually) hate electricity? (stench)
 */
static bool hates_elec(const object_type *o_ptr)
{
	switch (o_ptr->tval)
	{
		case TV_RING:
		case TV_WAND:
		case TV_ROD:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Does a given object (usually) hate fire?
 * Hafted/Polearm weapons have wooden shafts.
 * Arrows/Bows are mostly wooden.
 */
static bool hates_fire(const object_type *o_ptr)
{
	/* Analyze the type */
	switch (o_ptr->tval)
	{
		/* Wearable */
		case TV_LITE:
		case TV_ARROW:
		case TV_BOW:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_SOFT_ARMOR:
		{
			return (TRUE);
		}

		/* Books */
		case TV_MAGIC_BOOK:
		case TV_PRAYER_BOOK:
		case TV_NEWM_BOOK:
		case TV_LUCK_BOOK:
		case TV_CHEM_BOOK:
		{
			return (TRUE);
		}

		/* Chests */
		case TV_CHEST:
		{
			return (TRUE);
		}

		/* Staffs/Scrolls burn */
		case TV_STAFF:
		case TV_SCROLL:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}


/*
 * Does a given object (usually) hate cold?
 */
static bool hates_cold(const object_type *o_ptr)
{
	switch (o_ptr->tval)
	{
		case TV_POTION:
		case TV_FLASK:
		case TV_BOTTLE:
		{
			return (TRUE);
		}
	}

	return (FALSE);
}









/*
 * Melt something
 */
static int set_acid_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3, f4;
	if (!hates_acid(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3, &f4);
	if (f3 & (TR3_IGNORE_ACID)) return (FALSE);
	return (TRUE);
}


/*
 * Electrical damage
 */
static int set_elec_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3, f4;
	if (!hates_elec(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3, &f4);
	if (f3 & (TR3_IGNORE_ELEC)) return (FALSE);
	return (TRUE);
}


/*
 * Burn something
 */
static int set_fire_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3, f4;
	if (!hates_fire(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3, &f4);
	if (f3 & (TR3_IGNORE_FIRE)) return (FALSE);
	return (TRUE);
}


/*
 * Freeze things
 */
static int set_cold_destroy(const object_type *o_ptr)
{
	u32b f1, f2, f3, f4;
	if (!hates_cold(o_ptr)) return (FALSE);
	object_flags(o_ptr, &f1, &f2, &f3, &f4);
	if (f3 & (TR3_IGNORE_COLD)) return (FALSE);
	return (TRUE);
}




/*
 * This seems like a pretty standard "typedef"
 */
typedef int (*inven_func)(const object_type *);

/*
 * Destroys a type of item on a given percent chance
 * Note that missiles are no longer necessarily all destroyed
 *
 * Returns number of items destroyed.
 */
static int inven_damage(inven_func typ, int perc)
{
	int i, j, k, amt;

	object_type *o_ptr;

	char o_name[80];
	
	bool damage;

	/* Count the casualties */
	k = 0;

	/* Scan through the slots backwards */
	for (i = 0; i < INVEN_TOTAL; i++)
	{
		o_ptr = &inventory[i];

		/* Skip non-objects */
		if (!o_ptr->k_idx) continue;

		/* Damage only inventory with quiverguard spell */
		if (p_ptr->timed[TMD_QUIVERGUARD])
		{
			if (i >= INVEN_PACK) continue;
		}
		/* Otherwise damage inventory and quiver */
		else if ((i >= INVEN_PACK) && !IS_QUIVER_SLOT(i)) continue;

		/* Hack -- for now, skip artifacts */
		if (artifact_p(o_ptr)) continue;

		/* Give this item slot a shot at death */
		if ((*typ)(o_ptr))
		{
            /* Scale the destruction chance up */
			int chance = perc * 100;
			/* stuff has been damaged, so less likely to damage more */
			if (k) chance -= (k+1);

			damage = FALSE;

			/* Analyze the type to see if we just damage it */
			switch (o_ptr->tval)
			{
				/* Weapons */
				case TV_BOW:
				case TV_SWORD:
				case TV_HAFTED:
				case TV_POLEARM:
				case TV_DIGGING:
				{
					/* Chance to damage it */
					if (rand_int(10000) < perc)
					{
						/* Damage the item */
						o_ptr->to_h--;
						o_ptr->to_d--;

						/* Damaged! */
						damage = TRUE;
					}
					else continue;

					break;
				}

				/* Wearable items */
				case TV_HELM:
				case TV_CROWN:
				case TV_SHIELD:
				case TV_BOOTS:
				case TV_GLOVES:
				case TV_CLOAK:
				case TV_SOFT_ARMOR:
				case TV_HARD_ARMOR:
				case TV_DRAG_ARMOR:
				{
					/* Chance to damage it */
					if (rand_int(10000) < perc)
					{
						/* Damage the item */
						o_ptr->to_a--;

						/* Damaged! */
						damage = TRUE;
					}
					else continue;

					break;
				}
				
				/* Rods are tough */
				case TV_ROD:
				{
					chance = (chance / 4);
					
					break;
				}

				/* elec doesn't destroy things as often */
				case TV_WAND:
				case TV_RING:
				{
					chance = ((chance * 4) / 5);
					break;
				}
			}

			/* Damage instead of destroy */
			if (damage)
			{
				/* Calculate bonuses */
				p_ptr->update |= (PU_BONUS);

				/* Window stuff */
				p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_PLAYER_1);

				/* Casualty count */
				amt = o_ptr->number;
			}

			/* Count the casualties */
			else for (amt = j = 0; j < o_ptr->number; ++j)
			{
				if (rand_int(10000) < chance) amt++;
			}

			/* Some casualities */
			if (amt)
			{
				/* Get a description */
				object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 3);

				/* Message */
				message_format(MSG_DESTROY, 0, "%sour %s (%c) %s %s!",
				           ((o_ptr->number > 1) ?
				            ((amt == o_ptr->number) ? "All of y" :
				             (amt > 1 ? "Some of y" : "One of y")) : "Y"),
				           o_name, index_to_label(i),
				           ((amt > 1) ? "were" : "was"),
					   (damage ? "damaged" : "destroyed"));

				/* Damage already done? */
				if (damage) continue;

				/* Hack -- If rods, wands, or staves are destroyed, the total
				 * maximum timeout or charges of the stack needs to be reduced,
				 * unless all the items are being destroyed. -LM-
				 */
				if (((o_ptr->tval == TV_WAND) ||
				     (o_ptr->tval == TV_STAFF) ||
				     (o_ptr->tval == TV_ROD)) &&
				    (amt < o_ptr->number))
				{
					o_ptr->pval -= o_ptr->pval * amt / o_ptr->number;
				}

				/* Destroy "amt" items */
				inven_item_increase(i, -amt);
				inven_item_optimize(i);

				/* Count the casualties */
				k += amt;
			}
		}
	}

	/* Return the casualty count */
	return (k);
}




/*
 * Acid has hit the player, attempt to affect some armor.
 *
 * Note that the "base armor" of an object never changes.
 *
 * If any armor is damaged (or resists), the player takes less damage.
 */
static int minus_ac(void)
{
	object_type *o_ptr = NULL;

	u32b f1, f2, f3, f4;

	char o_name[80];


	/* Pick a (possibly empty) inventory slot */
	switch (randint(6))
	{
		case 1: o_ptr = &inventory[INVEN_BODY]; break;
		case 2: o_ptr = &inventory[INVEN_ARM]; break;
		case 3: o_ptr = &inventory[INVEN_OUTER]; break;
		case 4: o_ptr = &inventory[INVEN_HANDS]; break;
		case 5: o_ptr = &inventory[INVEN_HEAD]; break;
		case 6: o_ptr = &inventory[INVEN_FEET]; break;
	}

	/* Nothing to damage */
	if (!o_ptr->k_idx) return (FALSE);

	/* No damage left to be done */
	if (o_ptr->ac + o_ptr->to_a <= 0) return (FALSE);


	/* Describe */
	object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);

	/* Extract the flags */
	object_flags(o_ptr, &f1, &f2, &f3, &f4);

	/* Object resists */
	if (f3 & (TR3_IGNORE_ACID))
	{
		msg_format("Your %s is unaffected!", o_name);

		return (TRUE);
	}

	/* Message */
	msg_format("Your %s is damaged!", o_name);

	/* Damage the item */
	o_ptr->to_a--;

	/* Calculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Window stuff */
	p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_PLAYER_1);

	/* Item was damaged */
	return (TRUE);
}


/*
 * Hurt the PC with Acid
 */
void acid_dam(int dam, cptr kb_str)
{
	int inv = (dam < 40) ? 1 : (dam < 80) ? 2 : 3;

	/* Total Immunity */
	if (p_ptr->immune_acid || (dam <= 0)) return;

	/* Resist the damage */
	if (p_ptr->resist_acid) dam = (dam + 2) / 3;
	if (p_ptr->timed[TMD_OPP_ACID]) dam = (dam + 2) / 3;

	/* double resist protects inventory a little better */
	if ((p_ptr->resist_acid) && (p_ptr->timed[TMD_OPP_ACID])) inv -= 1;

	/* If any armor gets hit, defend the player */
	if ((badluck > 2) && (minus_ac())) dam = (dam * 3) / 4;
	else if (minus_ac()) dam = (dam * 2) / 3;

	/* Take damage */
	take_hit(dam, kb_str);

    /* Inventory damage */
	if ((inv > 0) && (!p_ptr->timed[TMD_ACID_BLOCK])) inven_damage(set_acid_destroy, inv);
}


/*
 * Hurt the player with electricity
 */
void elec_dam(int dam, cptr kb_str)
{
	int inv = (dam < 40) ? 1 : (dam < 80) ? 2 : 3;

	/* Total immunity */
	if (p_ptr->immune_elec || (dam <= 0)) return;

	/* Resist the damage */
	if (p_ptr->timed[TMD_OPP_ELEC]) dam = (dam + 2) / 3;
	if (p_ptr->resist_elec) dam = (dam + 2) / 3;

	/* double resist protects inventory a little better */
	if ((p_ptr->resist_elec) && (p_ptr->timed[TMD_OPP_ELEC])) inv -= 1;

	/* Take damage */
	take_hit(dam, kb_str);

	/* Inventory damage */
	if (inv > 0) inven_damage(set_elec_destroy, inv);
}




/*
 * Hurt the player with Fire
 */
void fire_dam(int dam, cptr kb_str)
{
	int inv = (dam < 40) ? 1 : (dam < 80) ? 2 : 3;

	/* Totally immune */
	if (p_ptr->immune_fire || (dam <= 0)) return;

	/* Resist the damage */
	if (p_ptr->resist_fire) dam = (dam + 2) / 3;
	if (p_ptr->timed[TMD_OPP_FIRE]) dam = (dam + 2) / 3;

	/* double resist protects inventory a little better */
	if ((p_ptr->resist_fire) && (p_ptr->timed[TMD_OPP_FIRE])) inv -= 1;

	/* Take damage */
	take_hit(dam, kb_str);

	/* Inventory damage */
	if (inv > 0) inven_damage(set_fire_destroy, inv);
}


/*
 * Hurt the player with Cold
 */
void cold_dam(int dam, cptr kb_str)
{
	int inv = (dam < 40) ? 1 : (dam < 80) ? 2 : 3;

	/* Total immunity */
	if (p_ptr->immune_cold || (dam <= 0)) return;

	/* Resist the damage */
	if (p_ptr->resist_cold) dam = (dam + 2) / 3;
	if (p_ptr->timed[TMD_OPP_COLD]) dam = (dam + 2) / 3;

	/* double resist protects inventory a little better */
	if ((p_ptr->resist_cold) && (p_ptr->timed[TMD_OPP_COLD])) inv -= 1;

	/* Take damage */
	take_hit(dam, kb_str);

	/* Inventory damage */
	if (inv > 0) inven_damage(set_cold_destroy, inv);
}


/*
 * Increase a stat by one randomized level
 *
 * Most code will "restore" a stat before calling this function,
 * in particular, stat potions will always restore the stat and
 * then increase the fully restored value.
 *
 * The amount parameter is 0 for usual use of this function.
 */
bool inc_stat(int stat, int amount)
{
	int value, gain, again, leftover = 0;
	bool done = FALSE;

	/* Then augment the current/max stat */
	value = p_ptr->stat_cur[stat];

	/* inhibit / fail */
	if (value + (amount*5) > 18+99) return (FALSE);

	/* Cannot go above 18/100 */
	if (value < 18+100)
	{
		if (value < 18)
		{
			/* increase by set amount */
			if ((amount) && (value + amount <= 18))
			{
				value += amount;
				again = amount;
				done = TRUE;
			}
			else if (amount)
			{
				gain = 18 - value;
				leftover = amount - gain;
				value = 18;
				again = gain;
				done = FALSE;
			}
			/* Gain one (sometimes two) points */
			else /* no set amount (usual) */
			{
				gain = ((rand_int(100) < 85 - (goodluck+1)/2) ? 1 : 2);
				value += gain;
				done = TRUE;
			}
		}

		/* Gain 1/6 to 1/3 of distance to 18/100 */
		/* (unless an amount is specified) */
		if ((value >= 18) && (!done) && (value < 18+98))
		{
			/* increase by set amount */
			if (amount)
			{
				if (leftover)
				{
					gain = leftover * 5;
					again += gain;
				}
				else
				{
					gain = amount * 5;
					again = gain;
				}
				value += gain;
			}
			else
			{
				/* Approximate gain value */
				gain = (((18+100) - value) / 2 + 3) / 2;

				/* Paranoia */
				if (gain < 1) gain = 1;

				/* Apply the bonus */
				value += randint(gain) + gain / 2;
			}

			/* Maximal value */
			if (value > 18+99) value = 18 + 99;
			done = TRUE;
		}

		/* Gain one point at a time (never if set amount) */
		if ((value >= 18+98) && (!done))
		{
			value++;
		}

		/* Save the new value */
		p_ptr->stat_cur[stat] = value;

		/* Bring up the maximum too */
		if (amount)
		{
			p_ptr->stat_max[stat] += again;
		}
		else if (value > p_ptr->stat_max[stat])
		{
			p_ptr->stat_max[stat] = value;
		}

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to gain */
	return (FALSE);
}

/*
 * decrease a stat by a set amount
 * (always permanent)
 */
bool dec_set_stat(int stat, int amount)
{
	int cur, max, adec = 0;
	int leftover = 0;
	bool res = FALSE;

	/* Get the current value */
	cur = p_ptr->stat_cur[stat];
	max = p_ptr->stat_max[stat];

	if (cur - amount < 3) return (FALSE);

	if (cur <= 18)
	{
		cur -= amount;
	}
	else
	{
		if (cur - (amount*5) < 18)
		{
			while (cur - (amount*5) < 18)
			{
				amount -= 1;
				leftover += 1;
			}
		}
		adec = (amount * 5) + leftover;
		cur -= adec;
	}

	/* Prevent illegal values */
	if (cur < 3) cur = 3;

	/* Something happened */
	if (cur != p_ptr->stat_cur[stat]) res = TRUE;

	/* Damage "max" value */
	/* (always by the same amount that the current is damaged) */
	if (adec) max -= adec;
	else max -= amount;

	/* Prevent illegal values */
	if (max < 3) max = 3;

	/* Something happened */
	if (max != p_ptr->stat_max[stat]) res = TRUE;

	/* Apply changes */
	if (res)
	{
		/* Actually set the stat to its new value. */
		p_ptr->stat_cur[stat] = cur;
		p_ptr->stat_max[stat] = max;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);
	}

	/* Done */
	return (res);
}


/*
 * Decreases a stat by an amount indended to vary from 0 to 100 percent.
 *
 * Note that "permanent" means that the *given* amount is permanent,
 * not that the new value becomes permanent.  This may not work exactly
 * as expected, due to weirdness" in the algorithm, but in general,
 * if your stat is already drained, the "max" value will not drop all
 * the way down to the "cur" value.
 */
bool dec_stat(int stat, int amount, bool permanent)
{
	int cur, max, loss, same, res = FALSE;


	/* Get the current value */
	cur = p_ptr->stat_cur[stat];
	max = p_ptr->stat_max[stat];

	/* Note when the values are identical */
	same = (cur == max);

	/* Damage current value */
	if (cur > 3)
	{
		/* Handle low values */
		if (cur <= 18)
		{
			if (amount > 90) cur--;
			if (amount > 50) cur--;
			if (amount > 20) cur--;
			cur--;
		}

		/* Handle high values */
		else
		{
			/* Hack -- Decrement by a random amount between one-quarter */
			/* and one-half of the stat bonus times the percentage, with a */
			/* minimum damage of half the percentage. -CWS */
			loss = (((cur-18) / 2 + 1) / 2 + 1);

			/* Paranoia */
			if (loss < 1) loss = 1;

			/* Randomize the loss */
			loss = ((randint(loss) + loss) * amount) / 100;

			/* Maximal loss */
			if (loss < amount/2) loss = amount/2;

			/* Lose some points */
			cur = cur - loss;

			/* Hack -- Only reduce stat to 17 sometimes */
			if (cur < 18) cur = (amount <= 20) ? 18 : 17;
		}

		/* Prevent illegal values */
		if (cur < 3) cur = 3;

		/* Something happened */
		if (cur != p_ptr->stat_cur[stat]) res = TRUE;
	}

	/* Damage "max" value */
	if (permanent && (max > 3))
	{
		/* Handle "low" values */
		if (max <= 18)
		{
			if (amount > 90) max--;
			if (amount > 50) max--;
			if (amount > 20) max--;
			max--;
		}

		/* Handle "high" values */
		else
		{
			/* Hack -- Decrement by a random amount between one-quarter */
			/* and one-half of the stat bonus times the percentage, with a */
			/* minimum damage of half the percentage. -CWS */
			loss = (((max-18) / 2 + 1) / 2 + 1);
			if (loss < 1) loss = 1;
			loss = ((randint(loss) + loss) * amount) / 100;
			if (loss < amount/2) loss = amount/2;

			/* Lose some points */
			max = max - loss;

			/* Hack -- Only reduce stat to 17 sometimes */
			if (max < 18) max = (amount <= 20) ? 18 : 17;
		}

		/* Hack -- keep it clean */
		if (same || (max < cur)) max = cur;

		/* Something happened */
		if (max != p_ptr->stat_max[stat]) res = TRUE;
	}

	/* Apply changes */
	if (res)
	{
		/* Actually set the stat to its new value. */
		p_ptr->stat_cur[stat] = cur;
		p_ptr->stat_max[stat] = max;

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);
	}

	/* Done */
	return (res);
}


/*
 * Restore a stat.  Return TRUE only if this actually makes a difference.
 */
bool res_stat(int stat)
{
	/* Restore if needed */
	if (p_ptr->stat_cur[stat] != p_ptr->stat_max[stat])
	{
		/* Restore */
		p_ptr->stat_cur[stat] = p_ptr->stat_max[stat];

		/* Recalculate bonuses */
		p_ptr->update |= (PU_BONUS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to restore */
	return (FALSE);
}




/*
 * Apply disenchantment to the player's stuff
 *
 * This function is also called from the "melee" code.
 *
 * The "mode" is currently unused.
 *
 * Return "TRUE" if the player notices anything.
 */
bool apply_disenchant(int mode)
{
	int t = 0;

	object_type *o_ptr;

	char o_name[80];


	/* Unused parameter */
	(void)mode;

	/* Pick a random slot */
	switch (randint(8))
	{
		case 1: t = INVEN_WIELD; break;
		case 2: t = INVEN_BOW; break;
		case 3: t = INVEN_BODY; break;
		case 4: t = INVEN_OUTER; break;
		case 5: t = INVEN_ARM; break;
		case 6: t = INVEN_HEAD; break;
		case 7: t = INVEN_HANDS; break;
		case 8: t = INVEN_FEET; break;
	}

	/* Get the item */
	o_ptr = &inventory[t];

	/* No item, nothing happens */
	if (!o_ptr->k_idx) return (FALSE);


	/* Nothing to disenchant */
	if ((o_ptr->to_h <= 0) && (o_ptr->to_d <= 0) && (o_ptr->to_a <= 0))
	{
		/* Nothing to notice */
		return (FALSE);
	}


	/* Describe the object */
	object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);


	/* Artifacts have 60% chance to resist */
	if (artifact_p(o_ptr) && (rand_int(100) < 60))
	{
		/* Message */
		msg_format("Your %s (%c) resist%s disenchantment!",
		           o_name, index_to_label(t),
		           ((o_ptr->number != 1) ? "" : "s"));

		/* Notice */
		return (TRUE);
	}


	/* Disenchant tohit */
	if (o_ptr->to_h > 0) o_ptr->to_h--;
	if ((o_ptr->to_h > 5) && (rand_int(100) < 20)) o_ptr->to_h--;

	/* Disenchant todam */
	if (o_ptr->to_d > 0) o_ptr->to_d--;
	if ((o_ptr->to_d > 5) && (rand_int(100) < 20)) o_ptr->to_d--;

	/* Disenchant toac */
	if (o_ptr->to_a > 0) o_ptr->to_a--;
	if ((o_ptr->to_a > 5) && (rand_int(100) < 20)) o_ptr->to_a--;

	/* Message */
	msg_format("Your %s (%c) %s disenchanted!",
	           o_name, index_to_label(t),
	           ((o_ptr->number != 1) ? "were" : "was"));

	/* Recalculate bonuses */
	p_ptr->update |= (PU_BONUS);

	/* Window stuff */
	p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_PLAYER_1);

	/* Notice */
	return (TRUE);
}


/*
 * Apply Nexus
 */
static void apply_nexus(const monster_type *m_ptr)
{
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	int max1, cur1, max2, cur2, ii, jj;
	int randcase = randint(7);

	switch (randcase)
	{
		case 1: case 2: case 3:
		{
			bool controlled = FALSE;
			/* controlled teleport (much harder when caused by a monster) */
		    if (p_ptr->telecontrol)
		    {
                int resistcrtl;
                if (r_ptr->level > 35) resistcrtl = (r_ptr->level * 3) / 2;
                else resistcrtl = 52;
                if (control_tport(resistcrtl, 130)) controlled = TRUE;
            }

            if (!controlled) teleport_player((65 * randcase) + 1);
			break;
		}

		case 4: case 5:
		{
			teleport_player_to(m_ptr->fy, m_ptr->fx);
			break;
		}

		case 6:
		{
			if (rand_int(100) < p_ptr->skills[SKILL_SAV])
			{
				msg_print("You resist the effects!");
				break;
			}

			/* Teleport Level */
			teleport_player_level();
			break;
		}

		case 7:
		{
			if (rand_int(100) < p_ptr->skills[SKILL_SAV])
			{
				msg_print("You resist the effects!");
				break;
			}

			msg_print("Your body starts to scramble...");

			/* Pick a pair of stats */
			ii = rand_int(A_MAX);
			for (jj = ii; jj == ii; jj = rand_int(A_MAX)); /* loop */

			max1 = p_ptr->stat_max[ii];
			cur1 = p_ptr->stat_cur[ii];
			max2 = p_ptr->stat_max[jj];
			cur2 = p_ptr->stat_cur[jj];

			p_ptr->stat_max[ii] = max2;
			p_ptr->stat_cur[ii] = cur2;
			p_ptr->stat_max[jj] = max1;
			p_ptr->stat_cur[jj] = cur1;

			p_ptr->update |= (PU_BONUS);

			break;
		}
	}
}




/*
 * Mega-Hack -- track "affected" monsters (see "project()" comments)
 */
static int project_m_n;
static int project_m_x;
static int project_m_y;



/*
 * We are called from "project()" to "damage" terrain features
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * We return "TRUE" if the effect of the projection is "obvious".
 *
 * Hack -- We also "see" grids which are "memorized".
 *
 * Perhaps we should affect doors and/or walls.
 */
static bool project_f(int who, int r, int y, int x, int dam, int typ)
{
	bool obvious = FALSE;
	int tx, ty;
	bool isobj;

	/* Unused parameters */
	(void)who;
	(void)r;

#if 0 /* unused */
	/* Reduce damage by distance */
	dam = (dam + r) / (r + 1);
#endif /* 0 */       

	/* Analyze the type */
	switch (typ)
	{
		/* Ignore most effects */
		case GF_ELEC:
		case GF_COLD:
		case GF_METEOR:
		case GF_ICE:
		case GF_SHARD:
		case GF_FORCE:
		case GF_SOUND:
		case GF_MANA:
        case GF_AMNESIA:
		case GF_HOLY_ORB:
		{
			break;
		}

		/* chance to dry up water */
		case GF_FIRE:
		case GF_PLASMA:
		{
			if ((cave_feat[y][x] == FEAT_WATER) && (randint(dam) > 50))
			{
				if (player_has_los_bold(y, x))
				{
					msg_print("The water evaporates.");
					obvious = TRUE;
				}
				cave_set_feat(y, x, FEAT_FLOOR);
			}
			break;
		}

		/* occationally leaves puddles */
		case GF_WATER:
		{
			/* when spellswitch 9 is used, it's not real water */
			if (spellswitch != 9)
			{
				int puddlechance = 1600;
				/* only floors and pits can become puddles */
				if (cave_feat[y][x] == FEAT_FLOOR) puddlechance = 90;
				/* pits become puddles much easier */
				if (cave_feat[y][x] == FEAT_OPEN_PIT) puddlechance = 60;

				if (randint(dam) > puddlechance)
				{
					cave_set_feat(y, x, FEAT_WATER);
					if (player_has_los_bold(y, x)) obvious = TRUE;
				}
			}
			break;
		}

		/* (rarely) creates pits or melts rubble */
		case GF_ACID:
		{
			int meltchance = 2000;
			/* may damage the dungeon floor */
			if (cave_feat[y][x] == FEAT_FLOOR) meltchance = 205;
			/* may destroy rubble */
			if (cave_feat[y][x] == FEAT_RUBBLE) meltchance = 150;
			if (randint(dam) > meltchance)
			{
				if (cave_feat[y][x] == FEAT_FLOOR)
				{
					if (player_has_los_bold(y, x))
					{
						msg_print("The acid eats away the dungeon floor!");
						obvious = TRUE;
					}
					cave_set_feat(y, x, FEAT_OPEN_PIT);
				}
				else if (cave_feat[y][x] == FEAT_RUBBLE)
				{
					if (player_has_los_bold(y, x))
					{
						msg_print("The acid melts the rubble!");
						obvious = TRUE;
					}
					cave_set_feat(y, x, FEAT_FLOOR);
				}
			}
			break;
		}

		/* Destroy Traps (and Locks) */
		case GF_KILL_TRAP:
		{
			/* Reveal secret doors */
			if (cave_feat[y][x] == FEAT_SECRET)
			{
				place_closed_door(y, x);

				/* Check line of sight */
				if (player_has_los_bold(y, x))
				{
					obvious = TRUE;
				}
			}

			/* Destroy traps */
			if ((cave_feat[y][x] == FEAT_INVIS) ||
			    ((cave_feat[y][x] >= FEAT_TRAP_HEAD) &&
			     (cave_feat[y][x] <= FEAT_TRAP_TAIL)))
			{
				/* Check line of sight */
				if (player_has_los_bold(y, x))
				{
					msg_print("There is a bright flash of light!");
					obvious = TRUE;
				}

				/* Forget the trap */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the trap */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			/* Locked doors are unlocked */
			else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01) &&
			          (cave_feat[y][x] <= FEAT_DOOR_HEAD + 0x07))
			{
				/* Unlock the door */
				cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);

				/* Check line of sound */
				if (player_has_los_bold(y, x))
				{
					msg_print("Click!");
					obvious = TRUE;
				}
			}
			/* dam==10 means unjam jammed doors also */
			else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08) && (dam == 10))
			{
				/* Unjam the door */
				cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);

				/* Check line of sound */
				if (player_has_los_bold(y, x))
				{
					obvious = TRUE;
				}
			}

			break;
		}

		/* Destroy Doors (and traps) */
		case GF_KILL_DOOR:
		{
			/* Destroy all doors and traps */
			if ((cave_feat[y][x] == FEAT_OPEN) ||
			    (cave_feat[y][x] == FEAT_BROKEN) ||
			    (cave_feat[y][x] == FEAT_INVIS) ||
			    ((cave_feat[y][x] >= FEAT_TRAP_HEAD) &&
			     (cave_feat[y][x] <= FEAT_TRAP_TAIL)) ||
			    ((cave_feat[y][x] >= FEAT_DOOR_HEAD) &&
			     (cave_feat[y][x] <= FEAT_DOOR_TAIL)))
			{
				/* Check line of sight */
				if (player_has_los_bold(y, x))
				{
					/* Message */
					msg_print("There is a bright flash of light!");
					obvious = TRUE;

					/* Visibility change */
					if ((cave_feat[y][x] >= FEAT_DOOR_HEAD) &&
					    (cave_feat[y][x] <= FEAT_DOOR_TAIL))
					{
						/* Update the visuals */
						p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
					}
				}

				/* Forget the door */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the feature */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			break;
		}

		/* Destroy walls (and doors) */
		case GF_KILL_WALL:
		{
			s16b this_o_idx, next_o_idx = 0;
			/* Non-walls (etc) */
			if (cave_floor_bold(y, x)) break;

			/* Permanent walls */
			if (cave_feat[y][x] >= FEAT_PERM_EXTRA) break;

			/* Granite */
			if (cave_feat[y][x] >= FEAT_WALL_EXTRA)
			{
				/* Message */
				if (cave_info[y][x] & (CAVE_MARK))
				{
					msg_print("The wall turns into mud!");
					obvious = TRUE;
				}

				/* Forget the wall */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the wall */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			/* Quartz / Magma with treasure */
			else if (cave_feat[y][x] >= FEAT_MAGMA_H)
			{
				/* Message */
				if (cave_info[y][x] & (CAVE_MARK))
				{
					msg_print("The vein turns into mud!");
					msg_print("You have found something!");
					obvious = TRUE;
				}

				/* Forget the wall */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the wall */
				cave_set_feat(y, x, FEAT_FLOOR);

				/* Place some gold */
				place_gold(y, x);
			}

			/* Quartz / Magma */
			else if (cave_feat[y][x] >= FEAT_MAGMA)
			{
				/* Message */
				if (cave_info[y][x] & (CAVE_MARK))
				{
					msg_print("The vein turns into mud!");
					obvious = TRUE;
				}

				/* Forget the wall */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the wall */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			/* Rubble */
			else if (cave_feat[y][x] == FEAT_RUBBLE)
			{
				/* Message */
				if (cave_info[y][x] & (CAVE_MARK))
				{
					msg_print("The rubble turns into mud!");
					obvious = TRUE;
				}

				/* Forget the wall */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the rubble */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			/* Destroy doors (and secret doors) */
			else /* if (cave_feat[y][x] >= FEAT_DOOR_HEAD) */
			{
				/* Hack -- special message */
				if (cave_info[y][x] & (CAVE_MARK))
				{
					msg_print("The door turns into mud!");
					obvious = TRUE;
				}

				/* Forget the wall */
				cave_info[y][x] &= ~(CAVE_MARK);

				/* Destroy the feature */
				cave_set_feat(y, x, FEAT_FLOOR);
			}

			/* check for unburied object */
			if (cave_o_idx[y][x])
			{
				bool unbury = FALSE;
				for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
				{
					/* get object */
					object_type *o_ptr = &o_list[this_o_idx];
					/* Get the next object */
					next_o_idx = o_ptr->next_o_idx;
					/* Skip non-objects */
					if (!o_ptr->k_idx) continue;
					/* unhide it */
					o_ptr->hidden = 0;
					/* don't give message more than once if more than one item was uncovered */
					if (!squelch_hide_item(o_ptr)) unbury = TRUE;
				}
				/* Found something */
				if ((unbury) && (player_can_see_bold(y, x)))
				{
					msg_print("There was something buried there!");
					obvious = TRUE;
					lite_spot(y, x);
				}
			}

			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			/* Fully update the flow */
			p_ptr->update |= (PU_FORGET_FLOW | PU_UPDATE_FLOW);

			break;
		}

		/* Make doors */
		case GF_MAKE_DOOR:
		{
            bool isdoor = FALSE;
			if ((cave_feat[y][x] == FEAT_OPEN) ||
			     (cave_feat[y][x] == FEAT_BROKEN))
				 isdoor = TRUE;
			if ((cave_feat[y][x] >= FEAT_DOOR_HEAD) &&
			     (cave_feat[y][x] <= FEAT_DOOR_TAIL))
				 isdoor = TRUE;
			/* rubble doesn't count as a wall */
			if (cave_feat[y][x] == FEAT_RUBBLE) /* */;
			/* already a wall there */
			else if ((!cave_floor_bold(y, x)) && (!isdoor)) break;
			
			/* magic damage number to modify effect (like Wizlock) */
			/* in fact we could easily combine GF_WIZLOCK and GF_MAKE_DOOR */
			if (dam == 10) 
			{
				object_type *o_ptr;

				/* move monsters who are in the way */
				/* (do it here because project_m comes after project_f) */
				if (cave_m_idx[y][x] > 0)
				{
					teleport_away(cave_m_idx[y][x], 5, 1);
				}

				/* Create closed door (maximum jamming) */
				cave_set_feat(y, x, FEAT_DOOR_TAIL);

				/* Shift any objects to further away */
				for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
				{
					drop_near(o_ptr, 0, y, x);
					/* move, don't duplicate */
					delete_object_idx(cave_o_idx[y][x]);
				}
			}
			else
			{
				/* Require a "naked" floor grid */
				if (!cave_naked_bold(y, x)) break;

				/* Create closed door */
				cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
			}

			/* Observe */
			if (cave_info[y][x] & (CAVE_MARK)) obvious = TRUE;

			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			break;
		}

		/* Wizard Lock */
		case GF_WIZLOCK:
		{
			isobj = FALSE;
			
			/* Observe */
			if (cave_info[y][x] & (CAVE_MARK)) obvious = TRUE;

			/* If target is an open space, create a jammed door */
            if ((cave_feat[y][x] == FEAT_FLOOR) && (cave_naked_bold(y, x)))
            {
				if (obvious) msg_print("A door magically appears and jams itself.");

				/* Hack - maximum jamming. */
			    cave_set_feat(y, x, FEAT_DOOR_TAIL);
            }

			/* can't create a door here (do not forbid objects) */
			if ((!cave_empty_bold(y, x)) && (cave_floor_bold(y, x)))
			{
				if (obvious) msg_print("A doorknob appears and falls to the floor");
				break;
            }

			if (!cave_naked_bold(y, x)) isobj = TRUE;
            
            /* If door is open, close it */
            if ((cave_feat[y][x] == FEAT_OPEN) ||
			     (cave_feat[y][x] == FEAT_BROKEN))
            {
				if ((cave_feat[y][x] == FEAT_OPEN) && (obvious)) msg_print("The door closes and jams itself.");
				if ((cave_feat[y][x] == FEAT_BROKEN) && (obvious)) msg_print("The door fixes and jams itself.");

				/* Hack - maximum jamming. */
			    cave_set_feat(y, x, FEAT_DOOR_TAIL);
            }
			/* Require any door. */
			else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD) &&
			     (cave_feat[y][x] <= FEAT_DOOR_TAIL))
			{
				/* Message. */
				if (obvious) msg_print("You cast a binding spell on the door.");

				/* Hack - maximum jamming. */
			    cave_set_feat(y, x, FEAT_DOOR_TAIL);
			}
			else
            {
				if (obvious) msg_print("A door magically appears and jams itself.");

				/* Hack - maximum jamming. */
			    cave_set_feat(y, x, FEAT_DOOR_TAIL);
            }

			if (isobj)
			{
				object_type *o_ptr;

				/* Shift any objects to further away */
				for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
				{
					drop_near(o_ptr, 0, y, x);
					/* move, don't duplicate */
					delete_object_idx(cave_o_idx[y][x]);
				}
			}

			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			break;
		}

		/* Boulders sometimes leave rubble */
		case GF_BOULDER:
		{
			bool debris = FALSE;
			/* dam == 0 is meant to create rubble */
			if (dam == 0) debris = TRUE;
			else if ((dam < 20) && (randint(100) < 12)) debris = TRUE;
			else if (dam < 45)
            {
                 if (randint(100) < 18) debris = TRUE;
            }
			else if (dam < 75)
            {
                 if (randint(100) < 27) debris = TRUE;
            }
			else
            {
                 if (randint(100) < 40) debris = TRUE;
            }
			
			/* no rubble */
            if (!debris) break;
            
            tx = x;
            ty = y;
            /* Require a "naked" floor grid */
			if (!cave_naked_bold(y, x))
            {
                int spoty, spotx;
                if (!get_nearby(y, x, &spoty, &spotx, 1)) break;
                
                ty = spoty;
                tx = spotx;
            }

			/* */
			if (cave_feat[y][x] == FEAT_OPEN_PIT)
			{
				msg_print("The boulder fills the pit.");
				cave_set_feat(y, x, FEAT_FLOOR);
			}
			else
			{
				/* Create rubble */
			    cave_set_feat(y, x, FEAT_RUBBLE);
				/* Create big rocks */
				big_rocks(y, x);
			}

			/* Observe */
			if (cave_info[y][x] & (CAVE_MARK)) obvious = TRUE;

			/* Update the visuals */
			p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			break;
		}

		/* Make traps */
		case GF_MAKE_TRAP:
		{
			/* Require floor grid */
			if (!(cave_feat[y][x] == FEAT_FLOOR)) break;

			/* Require a "naked" floor grid (no more: now may create */
            /* traps on spaces with objects or monsters, but doesn't always) */
			if ((!cave_naked_bold(y, x)) && (randint(100) < 22+goodluck)) break;
			

			/* Place a trap */
			place_trap(y, x);

			break;
		}

		/* Lite up the grid */
		case GF_LITE_WEAK:
		case GF_LITE:
		{
            /* temporary light, don't actually light the grid */
			/* just show it for a turn */
			if (spellswitch == 9)
			{
				monster_type *m_ptr;
				monster_race *r_ptr;
                if (player_has_los_bold(y, x))
				{
					if (cave_m_idx[y][x] > 0)
					{
						m_ptr = &mon_list[cave_m_idx[y][x]];
						r_ptr = &r_info[m_ptr->r_idx];
						if (!m_ptr->ml)
						{
							repair_mflag_show = TRUE;
							m_ptr->mflag |= (MFLAG_SHOW);
						}
					}
					lite_spot(y, x);
				}
			}
			else
			{
			    /* Turn on the light */
			    cave_info[y][x] |= (CAVE_GLOW);
            }

			/* Grid is in line of sight */
			if (player_has_los_bold(y, x))
			{
				if (!p_ptr->timed[TMD_BLIND])
				{
					/* Observe */
					obvious = TRUE;
				}

				/* Fully update the visuals */
				p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
			}
			break;
		}

		/* Darken the grid */
		case GF_DARK_WEAK:
		case GF_DARK:
		{
			/* Turn off the light */
			cave_info[y][x] &= ~(CAVE_GLOW);

			/* Hack -- Forget "boring" grids */
			if (cave_feat[y][x] <= FEAT_INVIS)
			{
				/* Forget */
				cave_info[y][x] &= ~(CAVE_MARK);
			}

			/* Grid is in line of sight */
			if (player_has_los_bold(y, x))
			{
				/* Observe */
				obvious = TRUE;

				/* Fully update the visuals */
				p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
			}

			/* All done */
			break;
		}
	}

	/* Return "Anything seen?" */
	return (obvious);
}



/*
 * We are called from "project()" to "damage" objects
 *
 * We are called both for "beam" effects and "ball" effects.
 *
 * Perhaps we should only SOMETIMES damage things on the ground.
 *
 * The "r" parameter is the "distance from ground zero".
 *
 * Note that we determine if the player can "see" anything that happens
 * by taking into account: blindness, line-of-sight, and illumination.
 *
 * Hack -- We also "see" objects which are "memorized".
 *
 * We return "TRUE" if the effect of the projection is "obvious".
 */
static bool project_o(int who, int r, int y, int x, int dam, int typ, bool spread)
{
	s16b this_o_idx, next_o_idx = 0;

	bool destroy, dieodd;
	bool obvious = FALSE;

	u32b f1, f2, f3, f4;

	char o_name[80];


	/* Unused parameters */
	(void)who;
	(void)r;

#if 0 /* unused */
	/* Reduce damage by distance */
	dam = (dam + r) / (r + 1);
#endif /* 0 */


	/* Scan all objects in the grid */
	for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		bool is_art = FALSE;
		bool ignore = FALSE;
		bool plural = FALSE;
		bool do_kill = FALSE;

		cptr note_kill = NULL;

		/* Get the object */
		o_ptr = &o_list[this_o_idx];

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3, &f4);

		/* Get the "plural"-ness */
		if (o_ptr->number > 1) plural = TRUE;

		/* Check for artifact */
		if (artifact_p(o_ptr)) is_art = TRUE;

		/* Analyze the type */
		switch (typ)
		{
			/* Acid -- Lots of things */
			case GF_ACID:
			{
				if (hates_acid(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " melt!" : " melts!");
					if (f3 & (TR3_IGNORE_ACID)) ignore = TRUE;
				}
				break;
			}

			/* Elec -- Rings, Wands, and rods */
			case GF_ELEC:
			{
				if (hates_elec(o_ptr))
				{
                    if (randint(100) < 90)
                    {
					   do_kill = TRUE;
					   note_kill = (plural ? " are destroyed!" : " is destroyed!");
					   if (f3 & (TR3_IGNORE_ELEC)) ignore = TRUE;
                    }
				}
				break;
			}

			/* Fire -- Flammable objects */
			case GF_FIRE:
			{
				if (hates_fire(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " burn up!" : " burns up!");
					if (f3 & (TR3_IGNORE_FIRE)) ignore = TRUE;
				}
				break;
			}
			case GF_ZOMBIE_FIRE:
			{
				if ((hates_fire(o_ptr)) && (randint(100) < 55))
				{
					do_kill = TRUE;
					note_kill = (plural ? " burn up!" : " burns up!");
					if (f3 & (TR3_IGNORE_FIRE)) ignore = TRUE;
				}
				break;
			}

			/* Cold -- potions and flasks */
			case GF_COLD:
			{
				if (hates_cold(o_ptr))
				{
					note_kill = (plural ? " shatter!" : " shatters!");
					do_kill = TRUE;
					if (f3 & (TR3_IGNORE_COLD)) ignore = TRUE;
				}
				break;
			}

			/* Plasma: destroys objects that hate fire and maybe acid */
			case GF_PLASMA:
			{
				if (hates_fire(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " burn up!" : " burns up!");
					if (f3 & (TR3_IGNORE_FIRE)) ignore = TRUE;
				}
				if ((hates_acid(o_ptr)) && (randint(100) < 33))
				{
					do_kill = TRUE;
					note_kill = (plural ? " melt!" : " melts!");
					if (f3 & (TR3_IGNORE_ACID)) ignore = TRUE;
				}
				break;
			}

			/* Fire + Cold */
			case GF_METEOR:
			{
				if (hates_fire(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " burn up!" : " burns up!");
					if (f3 & (TR3_IGNORE_FIRE)) ignore = TRUE;
				}
				if (hates_cold(o_ptr))
				{
					ignore = FALSE;
					do_kill = TRUE;
					note_kill = (plural ? " shatter!" : " shatters!");
					if (f3 & (TR3_IGNORE_COLD)) ignore = TRUE;
				}
				break;
			}

			case GF_SHARD:
			{
				/* prevent wierd message with grenades */
                if ((o_ptr->tval == TV_FLASK) && 
					(f3 & (TR3_IGNORE_COLD))) break;
				/* fall through */
			}

			/* Hack -- break potions and such */
			case GF_ICE:
            case GF_FORCE:
			case GF_SOUND:
			{
                if (hates_cold(o_ptr))
				{
					note_kill = (plural ? " shatter!" : " shatters!");
					do_kill = TRUE;
				}
				break;
			}

			/* Mana -- destroys everything */
			case GF_MANA:
			{
				do_kill = TRUE;
				note_kill = (plural ? " are destroyed!" : " is destroyed!");
				break;
			}

			/* Holy Orb -- destroys cursed non-artifacts */
			case GF_HOLY_ORB:
			{
				if (cursed_p(o_ptr))
				{
					do_kill = TRUE;
					note_kill = (plural ? " are destroyed!" : " is destroyed!");
				}
				break;
			}

			/* Unlock chests */
			case GF_KILL_TRAP:
			case GF_KILL_DOOR:
			{
				/* Chests are noticed only if trapped or locked */
				if (o_ptr->tval == TV_CHEST)
				{
					/* Disarm/Unlock traps */
					if (o_ptr->pval > 0)
					{
						/* Disarm or Unlock */
						o_ptr->pval = (0 - o_ptr->pval);

						/* Identify */
						object_known(o_ptr);

						/* Notice */
						if (o_ptr->marked)
						{
							msg_print("Click!");
							obvious = TRUE;
						}
					}
				}

				break;
			}
		}
		
		/* Attempt to destroy the object */
		/* shouldn't always destroy, but HOLY_ORB should always destroy cursed stuff */
		destroy = FALSE;
		dieodd = 75 - ((goodluck+1)/2) + (dam/20);
		/* spread effects damage objects much less often */
		if (spread) dieodd = dieodd/4;

		if ((randint(100) < dieodd) || ((cursed_p(o_ptr)) && (typ == GF_HOLY_ORB)))
		{
		   if (do_kill) destroy = TRUE;
        }

		if (destroy)
		{
			/* Effect "observed" */
			if (o_ptr->marked)
			{
				obvious = TRUE;
				object_desc(o_name, sizeof(o_name), o_ptr, FALSE, 0);
			}

			/* Artifacts, and other objects, get to resist */
			if (is_art || ignore)
			{
				/* Observe the resist */
				if (o_ptr->marked)
				{
					msg_format("The %s %s unaffected!",
					           o_name, (plural ? "are" : "is"));
#ifdef EFG
					/* EFGchange remove need for identify wrto preserving artifacts */
					if ((do_kill) && (o_ptr->name1))
					{
						/* ??? message */
						object_known(o_ptr);
					}
#endif
				}
			}

			/* Kill it */
			else
			{
				/* Describe if needed */
				/* (only give message if player can see object) */
				if ((o_ptr->marked && note_kill) && (!squelch_hide_item(o_ptr)))
				{
					message_format(MSG_DESTROY, 0, "The %s%s", o_name, note_kill);
				}

				/* Delete the object */
				delete_object_idx(this_o_idx);

				/* Redraw */
				lite_spot(y, x);
			}
		}
	}

	/* Return "Anything seen?" */
	return (obvious);
}



/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball causing damage to a monster.
 *
 * This routine takes a "source monster" (by index) which is mostly used to
 * determine if the player is causing the damage, and a "radius" (see below),
 * which is used to decrease the power of explosions with distance, and a
 * location, via integers which are modified by certain types of attacks
 * (polymorph and teleport being the obvious ones), a default damage, which
 * is modified as needed based on various properties, and finally a "damage
 * type" (see below).
 *
 * Note that this routine can handle "no damage" attacks (like teleport) by
 * taking a "zero" damage, and can even take "parameters" to attacks (like
 * confuse) by accepting a "damage", using it to calculate the effect, and
 * then setting the damage to zero.  Note that the "damage" parameter is
 * divided by the radius, so monsters not at the "epicenter" will not take
 * as much damage (or whatever)...
 *
 * Note that "polymorph" is dangerous, since a failure in "place_monster()"'
 * may result in a dereference of an invalid pointer.  XXX XXX XXX
 *
 * Various messages are produced, and damage is applied.
 *
 * Just "casting" a substance (i.e. plasma) does not make you immune, you must
 * actually be "made" of that substance, or "breathe" big balls of it.
 *
 * We assume that "Plasma" monsters, and "Plasma" breathers, are immune
 * to plasma.
 *
 * We assume "Nether" is an evil, necromantic force, so it doesn't hurt undead,
 * and hurts evil less.  If can breath nether, then it resists it as well.
 *
 * Damage reductions use the following formulas:
 *   Note that "dam = dam * 6 / (randint(6) + 6);"
 *     gives avg damage of .655, ranging from .858 to .500
 *   Note that "dam = dam * 5 / (randint(6) + 6);"
 *     gives avg damage of .544, ranging from .714 to .417
 *   Note that "dam = dam * 4 / (randint(6) + 6);"
 *     gives avg damage of .444, ranging from .556 to .333
 *   Note that "dam = dam * 3 / (randint(6) + 6);"
 *     gives avg damage of .327, ranging from .427 to .250
 *   Note that "dam = dam * 2 / (randint(6) + 6);"
 *     gives something simple.
 *
 * In this function, "result" messages are postponed until the end, where
 * the "note" string is appended to the monster name, if not NULL.  So,
 * to make a spell have "no effect" just set "note" to NULL.  You should
 * also set "notice" to FALSE, or the player will learn what the spell does.
 *
 * We attempt to return "TRUE" if the player saw anything "useful" happen.
 */
static bool project_m(int who, int r, int y, int x, int dam, int typ, int spread)
{
	int tmp;
	int erlev, rlev, upnt;
	bool has_summon, healedundead;

	monster_type *m_ptr;
	monster_race *r_ptr;
	monster_lore *l_ptr;

	cptr name;

	/* Is the monster "seen"? */
	bool seen = FALSE;

	/* Were the effects "obvious" (if seen)? */
	bool obvious = FALSE;

	/* Were the effects "irrelevant"? */
	bool skipped = FALSE;

	/* For nexus telelevel effect on monsters */
	bool do_delete = FALSE;

	/* Polymorph setting (true or false) */
	int do_poly = 0;

	/* Teleport setting (max distance) */
	int do_dist = 0;

	/* Confusion setting (amount to confuse) */
	int do_conf = 0;

	/* Stunning setting (amount to stun) */
	int do_stun = 0;

	/* Sleep amount (amount to sleep) */
	int do_sleep = 0;

	/* Fear amount (amount to fear) */
	int do_fear = 0;

	/* Silence Summons (monster timed effect) */
	int do_silence = 0;


	/* Hold the monster name */
	char m_name[80];

	/* Assume no note */
	cptr note = NULL;

	/* Assume a default death */
	cptr note_dies = " dies.";


	/* Walls protect monsters (not anymore) DJXXX */
	/* if (!cave_floor_bold(y,x)) return (FALSE); */

	/* No monster here */
	if (!(cave_m_idx[y][x] > 0)) return (FALSE);

	/* Never affect projector */
	if (cave_m_idx[y][x] == who) return (FALSE);

	/* Obtain monster info */
	m_ptr = &mon_list[cave_m_idx[y][x]];
	r_ptr = &r_info[m_ptr->r_idx];
	l_ptr = &l_list[m_ptr->r_idx];
	name = (r_name + r_ptr->name);
	if (m_ptr->ml) seen = TRUE;

	/* if you can see its disguise, then the effects are obvious when it's damaged */
	if ((player_can_see_bold(m_ptr->fy, m_ptr->fx)) && (m_ptr->disguised)) seen = TRUE;

	/* spread effects use wide radius ball effect centered on the PC */
	if (spread) /* spread = total radius + 1 */
	{
		if (r > 2)
		{
			dam -= (dam/spread) * (r-2);
		}
	}
	else
	{
		/* Reduce damage by distance (radius of ball spells) */
		dam = (dam + r) / (r + 1);
	}


	/* Get the monster name (BEFORE polymorphing) */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);


	/* Some monsters get "destroyed" */
	if ((r_ptr->flags3 & (RF3_DEMON)) || (r_ptr->flags3 & (RF3_UNDEAD)) ||
	    (r_ptr->flags2 & (RF2_STUPID)) || (r_ptr->flags7 & (RF7_NONMONSTER)) ||
	    (strchr("gpruvzO.#", r_ptr->d_char)))
	{
		/* Special note at death */
		note_dies = " is destroyed.";
	}


	/* Analyze the damage type */
	switch (typ)
	{
		/* Magic Missile -- pure damage */
		case GF_MISSILE:
		{
			if (seen) obvious = TRUE;
			break;
		}

		/* Acid */
		case GF_ACID:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_IM_ACID))
			{
				note = " resists a lot.";
				dam /= 9;
				if (seen) l_ptr->flags3 |= (RF3_IM_ACID);
			}
			/* acid does less damage in water */
			else if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 4) / 5;
			}
			break;
		}

		/* Electricity */
		case GF_ELEC:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_IM_ELEC))
			{
				note = " resists a lot.";
				dam /= 9;
				if (seen) l_ptr->flags3 |= (RF3_IM_ELEC);
			}
			/* lightning does more damage in water */
			else if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 4) / 3;
			}
			break;
		}

		/* Fire damage */
		case GF_FIRE:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_IM_FIRE))
			{
				note = " resists a lot.";
				dam /= 9;
				if (seen) l_ptr->flags3 |= (RF3_IM_FIRE);
				break;
			}
			/* fire does less damage in water (and can't do extra damage) */
			if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 3) / 4;
			}
			else if ((r_ptr->flags3 & (RF3_HURT_FIRE)) && 
				(randint(100) < 50 + (goodluck+1)/2))
			{
				note = " is badly burned.";
				dam += dam/4;
				if (seen) l_ptr->flags3 |= (RF3_HURT_FIRE);
			}
			break;
		}

		/* Cold */
		case GF_COLD:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_IM_COLD))
			{
				note = " resists a lot.";
				dam /= 9;
				if (seen) l_ptr->flags3 |= (RF3_IM_COLD);
				break;
			}
			/* frost does more damage in water */
			if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 5) / 4;
			}
			if ((r_ptr->flags3 & (RF3_HURT_COLD)) && 
				(randint(100) < 50 + (goodluck+1)/2))
			{
				note = " is frostbitten.";
				dam += dam/4;
				if (seen) l_ptr->flags3 |= (RF3_HURT_COLD);
			}
			break;
		}

		/* Poison */
		case GF_POIS:
		{
            /* chance of sleep if called from Noxious Fumes spell */
            if ((randint(100) < 29) && (spellswitch == 22))
            {
			   /* Attempt a saving throw */
			   if ((r_ptr->flags1 & (RF1_UNIQUE)) ||
			       (r_ptr->flags3 & (RF3_IM_POIS)) ||
			       (r_ptr->level > randint((dam - 9) < 1 ? 1 : (dam - 9)) + 10))
			   {
				   if (randint(999) == 1) dam-= 1; /* (skip) */
			   }
			   else
			   {
				   /* Go to sleep (much) later */
				   note = " passes out from the toxic fumes!";
				   if (r_ptr->flags3 & (RF3_NO_SLEEP))
                   {
                      dam = dam + randint(dam/2);
                      if (randint(100) < 67) do_sleep = 160 + randint(260);
                   }   
                   else
                   {
   				      dam = (dam * 3) / 2;
				      do_sleep = 500;
                   }
			   }
            }

			if (seen) obvious = TRUE;

			if (r_ptr->flags3 & (RF3_IM_POIS))
			{
				note = " resists a lot.";
				dam /= 9;
				if (seen) l_ptr->flags3 |= (RF3_IM_POIS);
			}
			/* poison has different effect on light fairies */
            else if ((r_ptr->d_char == 'y') && (r_ptr->flags3 & (RF3_HURT_DARK)))
            {
            	do_conf = (6 + randint(15) + r) / (r + 1);
            	dam /= 2;
            }
            
			break;
		}

		/* Holy Orb -- hurts Evil */
		case GF_HOLY_ORB:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (m_ptr->evil)
			{
				dam *= 2;
				note = " is hit hard.";
				if (seen)
                {
					  if (r_ptr->flags3 & (RF3_EVIL)) l_ptr->flags3 |= (RF3_EVIL);
					  else if (r_ptr->flags2 & (RF2_S_EVIL2)) l_ptr->flags2 |= (RF2_S_EVIL2);
					  else if (r_ptr->flags2 & (RF2_S_EVIL1)) l_ptr->flags2 |= (RF2_S_EVIL1);
                }
			}
			break;
		}

		/* zombie flame */
		case GF_ZOMBIE_FIRE:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_UNDEAD))
			{
				if (seen) l_ptr->flags3 |= (RF3_UNDEAD);
			}
			else if (r_ptr->flags3 & (RF3_SILVER))
			{
				if (seen) l_ptr->flags3 |= (RF3_SILVER);
				dam = (dam * 4) / 5;
			}
			else if ((r_ptr->flags3 & (RF3_IM_FIRE)) || (r_ptr->flags3 & (RF3_RES_PLAS)))
			{
				note = " is unaffected.";
				dam = 0;
				if (seen) l_ptr->flags3 |= (RF3_IM_FIRE);
			}
			else if (r_ptr->flags3 & (RF3_NON_LIVING))
			{
				if (seen) l_ptr->flags3 |= (RF3_NON_LIVING);
				dam = (dam * 3) / 4;
			}
			/* resistance is the default */
			else
			{
				note = " resists.";
				dam /= 4;
			}
			break;
		}

		case GF_BRSLIME:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if ((strchr("GWjmvw", r_ptr->d_char)) || (r_ptr->flags4 & (RF4_BR_SLIME)) ||
				(r_ptr->flags2 & (RF2_PASS_WALL)))
			{
				note = " resists.";
				dam /= 4;
			}
			break;
		}

		/* Arrow -- no defense XXX */
		case GF_ARROW:
        case GF_THROW:
        case GF_BOULDER:
		{
			if (seen) obvious = TRUE;
			break;
		}

		/* Plasma */
		case GF_PLASMA:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & RF3_RES_PLAS)
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				if (seen) l_ptr->flags3 |= RF3_RES_PLAS;
			}
			break;
		}

		/* Nether -- see above */
		case GF_NETHER:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_UNDEAD))
			{
				note = " is immune.";
				dam = 0;
				if (seen) l_ptr->flags3 |= (RF3_UNDEAD);
			}
			else if ((r_ptr->flags4 & (RF4_BR_NETH)) || (r_ptr->flags3 & (RF3_RES_NETH)))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				if ((seen) && (r_ptr->flags3 & (RF3_RES_NETH))) 
					l_ptr->flags3 |= (RF3_RES_NETH);
			}
#if grepseresistnether
			else if (r_ptr->flags3 & (RF3_SILVER))
			{
				dam = (dam * 3) / 4;
				note = " resists somewhat.";
				if (seen) l_ptr->flags3 |= (RF3_SILVER);
			}
#endif
			/* only inherently evil monsters resist nether */
			/* not races which are sometimes evil and sometimes not */
			else if (r_ptr->flags3 & (RF3_EVIL)) 
			{
				dam = (dam * 7) / 8;
				note = " resists slightly.";
				if (seen) l_ptr->flags3 |= (RF3_EVIL);
			}
			break;
		}

		/* Water damage */
		case GF_WATER:
		{
			if (seen) obvious = TRUE;
			/* Nature realm stun monster spell (dam=plev, r is always 1) */
			if ((spellswitch == 9) && (cp_ptr->spell_book == TV_NEWM_BOOK))
			{
				do_stun = (dam / 6) + randint(dam / 6);
				/* not much actual damage */
				dam = do_stun + randint(dam / 4);
				break;
			}
			/* spellswitch 9 is stunning for camera flash spell */
			else if (spellswitch == 9)
            {
               do_stun = (dam+2 + randint(dam+5) + r) / (r + 1);

               /* I know do_stun already stuns less if already stunned */
               /* but it should be much less for camera flash */
               if ((m_ptr->stunned) && (do_stun > 3)) 
				   do_stun = do_stun / 2;
            }
			else do_stun = (10 + randint(15) + r) / (r + 1);
			if (r_ptr->flags3 & RF3_IM_WATER)
			{
				if (spellswitch != 9)
                {
                   note = " is immune.";
				   dam = 0;
				   do_stun = (1 + randint(4) - r);
				   if (do_stun < 0) do_stun = 0;
                   if (seen) l_ptr->flags3 |= RF3_IM_WATER;
                }
				else if (cp_ptr->spell_book == TV_NEWM_BOOK)
				{
					if (do_stun >= 4) do_stun -= do_stun / 4;
				}
			}
			else if ((r_ptr->flags7 & (RF7_HATE_WATER)) && (spellswitch != 9))
			{
				dam = (dam * 5) / 4;
				note = " sizzles in the water.";
				note_dies = " is extinguished in the water!";

				/* famous line for the Wicked Witch of the West */
				if (m_ptr->r_idx == 311)
				{
					note = " cries, 'I'm melting!'";
					note_dies = " cries, 'I'm melting!' and soon becomes a puddle.";
				}
			}
			break;
		}

		/* Chaos -- Chaos breathers resist */
		case GF_CHAOS:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (dam > randint(26)) do_poly = TRUE;
			do_conf = (5 + randint(11) + r) / (r + 1);
			if (r_ptr->flags4 & (RF4_BR_CHAO))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				do_poly = FALSE;
			}
			break;
		}

		/* Shards -- Shard breathers resist */
		case GF_SHARD:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (r_ptr->flags4 & (RF4_BR_SHAR))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
			}
			else if (r_ptr->flags3 & (RF3_NON_LIVING))
			{
				note = " doesn't notice.";
				dam *= 5; dam /= (randint(4)+6);
			}
			break;
		}

		/* Sound -- Sound breathers resist */
		case GF_SOUND:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (!(r_ptr->flags3 & RF3_NO_STUN)) do_stun = (10 + randint(15) + r) / (r + 1);
			/* spellswitch 9 for burst of light spell */
			if ((r_ptr->flags4 & (RF4_BR_SOUN)) && (spellswitch != 9))
			{
				note = " resists.";
				dam *= 2; dam /= (randint(6)+6);
				do_stun = ((do_stun * 2) / 3);
			}
			break;
		}

		/* Confusion */
		case GF_CONFUSION:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			do_conf = (10 + randint(15) + r) / (r + 1);
			if (r_ptr->flags4 & (RF4_BR_CONF))
			{
				note = " resists.";
				dam *= 2; dam /= (randint(6)+6);
			}
			else if (r_ptr->flags3 & (RF3_NO_CONF))
			{
				note = " resists somewhat.";
				dam /= 2;
				do_conf = 0;
			}
			break;
		}

		/* Disenchantment */
		case GF_DISENCHANT:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & RF3_RES_DISE)
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				if (seen) l_ptr->flags3 |= RF3_RES_DISE;
			}
			break;
		}

		/* Nexus - now has appropriate effect on monsters */
		case GF_NEXUS:
		{
			int fdam = dam;
			if (fdam > 36) fdam = 36 + ((fdam - 36)/4);
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & RF3_RES_NEXUS)
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				if (seen) l_ptr->flags3 |= RF3_RES_NEXUS;
			}
			else if ((r_ptr->level < fdam/2) && (randint(100) < 5))
			{
				/* simulate telelevel nexus effect (rarely) */
				note = " dissapears!";
				do_delete = TRUE;
			}
			else if ((r_ptr->level < fdam/2) || (randint(100) < (44 - r_ptr->level/2)))
			{
				do_dist = 6 + randint(6);
			}
			break;
		}

		/* Force */
		case GF_FORCE:
		{	
            if (seen) obvious = TRUE;
			if (!r_ptr->flags3 & RF3_NO_STUN) do_stun = (randint(15) + r) / (r + 1);
			if (r_ptr->flags4 & (RF4_BR_WALL))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				do_stun = do_stun / 3;
			}
			/* living walls completely resist BR_WALL */
			if (strchr("#", r_ptr->d_char))
			{
				note = " resists completely.";
				dam = 0;
				do_stun = 0;
			}
			break;
		}
		
		/* Fear */
		case GF_BRFEAR:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (who > 0) /* (projector is a monster) */
			{
				monster_type *whom_ptr;
				whom_ptr = &mon_list[who];
				/* monsters are never scared of other monsters of the same race */
				if (whom_ptr->r_idx == m_ptr->r_idx)
				{
					dam = 0;
					break;
				}
			}
			if ((r_ptr->flags3 & (RF3_NO_FEAR)) || (r_ptr->flags1 & (RF1_UNIQUE)))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+3);
			}
			else
			{
                do_fear = (randint(6) + 3);
            }
			break;
		}

		/* Inertia -- breathers resist */
		case GF_INERTIA:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (r_ptr->flags4 & (RF4_BR_INER))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
			}
			break;
		}

		/* Time -- breathers resist */
		case GF_TIME:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags4 & (RF4_BR_TIME))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
			}
			break;
		}

		/* Gravity -- breathers resist */
		case GF_GRAVITY:
		{
			if (seen) obvious = TRUE;

			/* Higher level monsters can resist the teleportation better */
            if ((spellswitch == 30) && (randint(150) > r_ptr->level))
            {
				do_dist = (p_ptr->lev - 10) + randint(15);
            }
			else if (randint(127) > r_ptr->level)
			{
				do_dist = 11;
            }

            if (spellswitch == 30) /* nether warping */
            {
			   if (r_ptr->flags3 & (RF3_UNDEAD))
			   {
				  dam = 0;
				  do_dist = 0;
				  if (seen) l_ptr->flags3 |= (RF3_UNDEAD);
			   }
			   else if (r_ptr->flags4 & (RF4_BR_NETH))
			   {
				    dam = (dam * 3) / 4;
				    do_dist = (do_dist/3);
			   }
			   else if (r_ptr->flags3 & (RF3_SILVER))
			   {
				    dam = (dam * 3) / 4;
				    do_dist = (do_dist/3);
				    if (seen) l_ptr->flags3 |= (RF3_SILVER);
			   }
            }   
			else if (r_ptr->flags4 & (RF4_BR_GRAV))
			{
				note = " resists.";
				dam *= 3; dam /= (randint(6)+6);
				do_dist = 0;
			}
			break;
		}

		/* Pure damage */
		case GF_MANA:
		{
			if (seen) obvious = TRUE;
			break;
		}

		/* Meteor -- powerful magic missile */
		case GF_METEOR:
		{
			if (seen) obvious = TRUE;
			break;
		}

		/* Ice -- Cold + Cuts + Stun */
		case GF_ICE:
		{
			if (seen) obvious = TRUE;
			do_stun = (randint(15) + 1) / (r + 1);
			if (r_ptr->flags3 & (RF3_IM_COLD))
			{
				note = " resists a lot.";
				dam /= 9;
				/* resists stun completely if immune to cold and water */
				if (r_ptr->flags3 & RF3_IM_WATER)
				{
					do_stun = 0;
					if (seen) l_ptr->flags3 |= (RF3_IM_WATER);
				}
				else
				{
					do_stun /= 2;
				}
				if (seen) l_ptr->flags3 |= (RF3_IM_COLD);
			}
			else if (r_ptr->flags3 & RF3_IM_WATER)
			{
				do_stun /= 3;
				if (seen) l_ptr->flags3 |= (RF3_IM_WATER);
			}
			break;
		}


		/* Silver Bullet (damages demons, dark fairies, and werebeasts) */
		/* but heals silver monsters */
		case GF_SILVER_BLT:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			/* Only affect certain monster types */
			if (r_ptr->flags3 & (RF3_HURT_SILV))
			{
				/* Learn about type */
				if (seen)
                {
                    l_ptr->flags3 |= (RF3_HURT_SILV);
                    if (r_ptr->flags3 & (RF3_DEMON)) l_ptr->flags3 |= (RF3_DEMON);
                }

				/* Obvious */
				if (seen) obvious = TRUE;
				
				/* vulnerable */
                dam = (dam * 5) / 4;
			}
            /* nothing should have both SILVER hand HURT_SILV */
			else if (r_ptr->flags3 & (RF3_SILVER))
			{
				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_SILVER);

				/* Obvious */
				if (seen) obvious = TRUE;
				
				/* heals silver monsters (possibly even above its maxHPS) */
				if (dam > m_ptr->maxhp) dam = m_ptr->maxhp;
				if (dam > 90) dam = 90;
				m_ptr->hp += dam/2 + randint(dam/2);
				dam = 0;
				if (m_ptr->hp > m_ptr->maxhp) note = " seems more powerful.";
				else note = " seems refreshed.";
            }
			else if (r_ptr->flags3 & (RF3_DEMON))
			{
				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_DEMON);

				/* Obvious */
				if (seen) obvious = TRUE;
				
				/* normal damage */
            }
			/* Others (almost) ignore */
			else
			{
				/* Obvious */
				if (seen) obvious = TRUE;
				
				note = " is not vulnerable to silver.";
                dam = dam / 10 + 1;
			}			
			break;
		}


		/* dispel / damage to unnatural */
		case GF_DISP_UNN:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			
			if ((!(r_ptr->flags3 & (RF3_SILVER))) &&
			    (!(r_ptr->flags3 & (RF3_NON_LIVING))))
			{

				note = " is unaffected!";
				obvious = FALSE;
				dam = 0;
			}
			if (r_ptr->flags3 & (RF3_SILVER))
			{
				if (seen) l_ptr->flags3 |= (RF3_SILVER);
			}

			break;
		}


		/* Drain Life */
		case GF_OLD_DRAIN:
		{
			if (seen) obvious = TRUE;
			if ((r_ptr->flags3 & (RF3_NON_LIVING)) || (r_ptr->flags3 & (RF3_UNDEAD)))
			{
				if (r_ptr->flags3 & (RF3_UNDEAD))
				{
					if (seen) l_ptr->flags3 |= (RF3_UNDEAD);
				}
				if (r_ptr->flags3 & (RF3_NON_LIVING))
				{
					if (seen) l_ptr->flags3 |= (RF3_NON_LIVING);
				}

				note = " is unaffected!";
				obvious = FALSE;
				dam = 0;
			}

			/* dispel life does not affect silver monsters */
			if ((spellswitch == 27) && (r_ptr->flags3 & (RF3_SILVER)))
			{
				if (seen) l_ptr->flags3 |= (RF3_SILVER);

				note = " is unaffected!";
				obvious = FALSE;
				dam = 0;
			}
			
			if (spellswitch == 8) /* (annihilation, base damage 250) */
			{
                if (dam < m_ptr->maxhp/10)
                {
                    int diff = m_ptr->maxhp/10 - dam;
                    dam += (diff/2) + randint((diff+1)/2);
                    if (dam > 750) dam = 750;
                }
            }

			break;
		}

		/* Polymorph monster (Use "dam" as "power") */
		case GF_OLD_POLY:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;

			/* Attempt to polymorph (see below) */
			do_poly = TRUE;

			/* Powerful monsters can resist */
			if ((r_ptr->flags1 & (RF1_UNIQUE)) ||
			    (r_ptr->level > randint(dam) + 10))
			{
				note = " is unaffected!";
				do_poly = FALSE;
				obvious = FALSE;
			}

			/* No "real" damage */
			dam = 0;

			break;
		}


		/* Clone monsters (Ignore "dam") */
		case GF_OLD_CLONE:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;

			/* Heal fully */
			m_ptr->hp = m_ptr->maxhp;

			/* Speed up (why??) */
			if ((m_ptr->mspeed < 150) && (badluck)) m_ptr->mspeed += 10;
			else if (randint(100) > goodluck*2) m_ptr->mspeed += 1 + randint(5);

			/* Attempt to clone. */
			if (multiply_monster(cave_m_idx[y][x]))
			{
				note = " spawns!";
			}

			/* No "real" damage */
			dam = 0;

			break;
		}


		/* Heal Monster (use "dam" as amount of healing) */
		case GF_OLD_HEAL:
		{
			/* statues are unaffected (but trees may be healed) */
			if ((r_ptr->flags7 & (RF7_NONMONSTER)) &&
				(r_ptr->flags3 & (RF3_NON_LIVING))) { dam = 0; break; }

			/* hurts undead */
			if (r_ptr->flags3 & (RF3_UNDEAD))
			{
				/* If this destroys it, this */
                /* makes it extremely unlikely to come back to life */
                healedundead = TRUE;
                break;
			}

			if (seen) obvious = TRUE;

			/* Wake up */
			if ((p_ptr->nice) && (!m_ptr->evil) &&
               (goodluck) && (randint(100) < 50) &&
		       ((r_ptr->flags3 & (RF3_HURT_DARK)) ||
		       (r_ptr->flags3 & (RF3_ANIMAL))))
		    {
               /* don't wake up */
            }   
			else if ((goodluck < 14) && (!(r_ptr->sleep == 255)))
            {
                m_ptr->csleep = 0;
                m_ptr->roaming = 0;
            }

			/* Heal */
			m_ptr->hp += dam;

			/* Message */
			if (m_ptr->hp < m_ptr->maxhp) note = " looks healthier.";

			/* No overflow */
			if (m_ptr->hp >= m_ptr->maxhp)
			{
				m_ptr->hp = m_ptr->maxhp;
				/* alternate message */
				note = " looks fully healthy.";
			}

			/* Redraw (later) if needed */
			if (p_ptr->health_who == cave_m_idx[y][x]) p_ptr->redraw |= (PR_HEALTH);

			/* Cancel fear */
			if ((m_ptr->monfear) && (randint(100) < 40 + badluck - goodluck))
			{
				char m_poss[80];
				/* Cancel fear */
				m_ptr->monfear = 0;

				/* Message */
				monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);
				if (!(r_ptr->flags7 & (RF7_NONMONSTER)))
					msg_format("%^s recovers %s courage.", m_name, m_poss);
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Speed Monster (Ignore "dam") */
		case GF_OLD_SPEED:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;

			/* Speed up */
			if (m_ptr->mspeed < 150) m_ptr->mspeed += 10;
			note = " starts moving faster.";

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Slow Monster (Use "dam" as "power") */
		case GF_OLD_SLOW:
		{
			int savemod;
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

            if (dam < 12) dam = 12;
			if (seen) obvious = TRUE;
			/* more likely to work on hasted monsters */
			if (m_ptr->mspeed > r_ptr->speed + 5) dam += 30;
			/* and less likely to work on monsters which are already slowed */
			if ((m_ptr->mspeed < r_ptr->speed - 5) && (m_ptr->mspeed <= 110)) 
				dam = dam/2;
				
			/* to prevent crashes from randint(-1) */
            savemod = dam - 10;
			if (savemod < 1) savemod = 1;

			/* Powerful monsters can resist */
			if ((r_ptr->flags1 & (RF1_UNIQUE)) && (10 + randint(savemod) < r_ptr->level*2))
			{
				note = " resists the spell!";
				obvious = FALSE;
			}
			else if (r_ptr->level > randint(savemod) + 10)
			{
				note = " resists the spell!";
				obvious = FALSE;
			}

			/* Normal monsters slow down */
			else
			{
				if (m_ptr->mspeed > 60)
				{
					m_ptr->mspeed -= 10;
					note = " starts moving slower.";
				}
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Sleep (Use "dam" as "power") */
		case GF_OLD_SLEEP:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			erlev = r_ptr->level + 1;
			if ((r_ptr->flags1 & (RF1_UNIQUE)) && (r_ptr->level > 70)) erlev += 50;
			else if ((r_ptr->flags1 & (RF1_UNIQUE)) && (r_ptr->level < 22)) erlev += 9;
			else if (r_ptr->flags1 & (RF1_UNIQUE)) erlev += 9 + randint((erlev/2) - 9);
			/* monster is already asleep */
			if ((m_ptr->csleep) && (!m_ptr->roaming)) erlev -= 1;
			/* make it not quite as strong as it was in 1.0.99 */
			else if (randint(100) < 35) erlev += rand_int(10);

			if (seen) obvious = TRUE;

            /* Hold monsters ignores NO_SLEEP flag */
            if (spellswitch == 29)
			{
				if (erlev > randint((dam - 10) < 1 ? 1 : (dam - 10)) + 10)
				{
					/* No obvious effect */
					note = " is unaffected!";
					obvious = FALSE;
				}
				/* Uniques can be affected by hold monsters */
				else if ((r_ptr->flags1 & (RF1_UNIQUE)) &&
				   (erlev > randint((dam/2 - 10) < 1 ? 1 : (dam/2 - 10)) + 10))
				{
					/* No obvious effect */
					note = " is unaffected!";
					obvious = FALSE;
			    }
			}
			else if (r_ptr->flags3 & (RF3_NO_SLEEP))
			{
				/* Memorize a flag */
				if (seen) l_ptr->flags3 |= (RF3_NO_SLEEP);

				/* No obvious effect */
				note = " is unaffected!";
				obvious = FALSE;
			}
			/* Attempt a saving throw */
			else if (erlev > randint((dam - 10) < 1 ? 1 : (dam - 10)) + 10)
			{
				/* No message if monster was already asleep */
				if ((m_ptr->csleep) && (!m_ptr->roaming)) /* no message */;
				/* No obvious effect */
				else note = " resists the spell!";
				obvious = FALSE;
			}
			else
			{
				/* No message if monster was already asleep */
				if ((m_ptr->csleep) && (!m_ptr->roaming))
				{
					/* no message (effect isn't obvious) */
					obvious = FALSE;
				}
				else
				{
					/* Go to sleep (much) later */
					note = " falls asleep!";
				}
				do_sleep = 500;
								
				/* monster is sleeping, not roaming */
				m_ptr->roaming = 0;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Confusion (Use "dam" as "power") */
		case GF_OLD_CONF:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			erlev = r_ptr->level + 1;
			if ((r_ptr->flags1 & (RF1_UNIQUE)) && (r_ptr->level < 22))  erlev += 9;
			else if (r_ptr->flags1 & (RF1_UNIQUE)) erlev += 9 + randint((erlev/2) - 9);
			/* make it not quite as strong as it was in 1.0.99 */
			if (randint(100) < 34) erlev += rand_int(10);
			
			if (seen) obvious = TRUE;

			/* Get confused later */
			do_conf = damroll(3, (dam / 2)) + 1;

			if (r_ptr->flags3 & (RF3_NO_CONF))
			{
				/* Memorize a flag */
				if (seen) l_ptr->flags3 |= (RF3_NO_CONF);

				/* Resist */
				do_conf = 0;

				/* No obvious effect */
				note = " is unaffected!";
				obvious = FALSE;
			}
			/* Attempt a saving throw */
			else if (erlev > randint((dam - 10) < 1 ? 1 : (dam - 10)) + 10)
			{
				/* Resist */
				do_conf = 0;

				/* No obvious effect */
				note = " resists the spell!";
				obvious = FALSE;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}

		/* Amnesia (Use "dam" as "power") */
		case GF_AMNESIA:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			erlev = r_ptr->level + 1;
			if (seen) obvious = TRUE;
			/* should work on uniques but not Sauron or Morgoth */
			/* effective monster level: */
			if ((r_ptr->flags1 & (RF1_UNIQUE)) && (r_ptr->level > 80)) erlev = erlev * 2;
			else if ((r_ptr->flags1 & (RF1_UNIQUE)) && (r_ptr->level < 22))  erlev += 9;
			else if (r_ptr->flags1 & (RF1_UNIQUE)) erlev += 9 + randint((erlev/2) - 9);
			/* make it not quite as strong as it was in 1.0.99 */
			if (randint(100) < 34) erlev += rand_int(10);
			
			/* less likely to affect monsters when cast by a monster */
            if (who > 0)
            {
	            monster_type *whom_ptr = &mon_list[who];
	            monster_race *whor_ptr = &r_info[whom_ptr->r_idx];
	            /* scale damage down */
                if (dam > 100 + whor_ptr->level) dam = 100 + randint(whor_ptr->level);
                dam = dam / 2;
                if (dam > 51) dam -= (dam - 50)/2;
            }

			if ((r_ptr->flags3 & (RF3_UNDEAD)) || (r_ptr->flags3 & (RF3_NON_LIVING)))
			{
				/* Memorize a flag */
				if (r_ptr->flags3 & (RF3_NON_LIVING))
				{
					if (seen) l_ptr->flags3 |= (RF3_NON_LIVING);
				}
				if (r_ptr->flags3 & (RF3_UNDEAD))
				{
					if (seen) l_ptr->flags3 |= (RF3_UNDEAD);
				}

				/* No obvious effect */
				note = " is unaffected!";
				obvious = FALSE;
			}
			/* Attempt a saving throw */
			/* damage should be slightly more than plev * 2 when cast by the player */
			else if (erlev > randint((dam - 10) < 1 ? 1 : (dam - 10)) + 20)
			{
				/* No obvious effect */
				note = " resists the spell!";
				obvious = FALSE;
			}
			else
			{
				/* Forget the player (much) later */
				note = " forgets what it was doing.";
				do_sleep = 400 + randint(r_ptr->sleep * 5);
				if (do_sleep > 2000) do_sleep = 2000;

				/* monster is roaming not sleeping (unless it was already asleep) */
				if (!m_ptr->csleep) m_ptr->roaming = 1;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Silence Summons (Use "dam" for duration) */
		case GF_SILENCE_S:
		{
			/* Extract the racial spell flags */
	        u32b f4, f5, f6;
	        f4 = r_ptr->flags4;
	        f5 = r_ptr->flags5;
	        f6 = r_ptr->flags6;

			has_summon = ((f4 & (RF4_SUMMON_MASK)) ||
                          (f5 & (RF5_SUMMON_MASK)) ||
		                  (f6 & (RF6_SUMMON_MASK)));

            /* Silence summons also stops healing spells */
            if ((r_ptr->flags6 & (RF6_HEAL)) || (r_ptr->flags5 & (RF5_DRAIN_MANA)))
               has_summon = TRUE;
               
            /* can work against shriekers if you're lucky */
            if ((goodluck > 4) && (r_ptr->flags4 & (RF4_SHRIEK))) has_summon = TRUE;
		                      
		    /* irrevelent */
            if (!has_summon)
            {
               note = " is unaffected!";
			   dam = 0;
			   break;
            }
            
            if (seen) obvious = TRUE;
			rlev = ((r_ptr->level >= 1) ? r_ptr->level : 1);

			/* become silenced later */
			/* usually "dam" = plev from silence summons spell */
			do_silence = dam/4 + randint((dam*3)/4);
			
			/* monster's saving throw */
            upnt = rlev/7 + randint(15 - goodluck/3);
            if (r_ptr->flags2 & (RF2_SMART)) upnt += 1 + randint(rlev/9);
            if (r_ptr->flags2 & (RF2_STUPID)) upnt -= rlev / 8;
            if (r_ptr->flags1 & (RF1_UNIQUE)) upnt += 15;
            if (m_ptr->silence) upnt += rlev/5;

            /* monsters native to dL>=50 should have some chance to resist */
            /* (1/5 chance of (+3 + d7) for dL50 monsters)*/
            if (randint(100) < 25) upnt += (rlev+14)/15 + randint((rlev+6)/7);
			
			/* monster saves */
			if (dam < upnt)
			{
				/* Memorize a flag */
				if ((r_ptr->flags2 & (RF2_SMART)) && (seen))
					l_ptr->flags2 |= (RF2_SMART);

				/* Resist */
				do_silence = 0;

				/* No obvious effect */
				note = " resists the spell!";
				obvious = FALSE;
			}
			/* partial resist */
			else if ((dam < upnt + 3) &&
             ((r_ptr->flags2 & (RF2_SMART)) || (r_ptr->flags1 & (RF1_UNIQUE))))
            {
				/* Memorize a flag */
				if ((r_ptr->flags2 & (RF2_SMART)) && (seen))
					l_ptr->flags2 |= (RF2_SMART);

               do_silence = (do_silence * 2) / 3;
			   note = " resists somewhat.";

			   /* separate message */
			   msg_format("%^s%s", m_name, note);
            }

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Lite, but only hurts susceptible creatures */
		case GF_LITE_WEAK:
		{
			/* Hurt by light */
			if (r_ptr->flags3 & (RF3_HURT_LITE))
			{
				/* Obvious effect */
				if (seen) obvious = TRUE;

				/* Memorize the effects */
				if (seen) l_ptr->flags3 |= (RF3_HURT_LITE);

				/* Special effect */
				note = " cringes from the light!";
				note_dies = " shrivels away in the light!";
			}

			/* Normally no damage */
			else
			{
				/* No damage */
				dam = 0;
			}
			
			/* magical light makes it hard to hide */
			if (m_ptr->monseen < 2) m_ptr->monseen = 10;
			else if (m_ptr->ml == FALSE) m_ptr->monseen += 2;
			else m_ptr->monseen += 1;
			
            /* update visual */
	        /* p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS); */
	        update_mon(cave_m_idx[y][x], 0);

			break;
		}
		
		
		/* Dark, but only hurts susceptible creatures */
		case GF_DARK_WEAK:
		{
			/* Hurt by dark */
			if (r_ptr->flags3 & (RF3_HURT_DARK))
			{
				/* Obvious effect */
				if (seen) obvious = TRUE;

				/* Memorize the effects */
				if (seen) l_ptr->flags3 |= (RF3_HURT_DARK);

				/* Special effect */
				note = " shudders in the dark!";
				note_dies = " dies from lack of light!";
			}

			/* Normally no damage */
			else
			{
				/* No damage */
				dam = 0;
			}

			break;
		}


		/* Lite -- opposite of Dark */
		case GF_LITE:
		{
			if (seen) obvious = TRUE;
			if (r_ptr->flags3 & (RF3_HURT_DARK))
			{
				note = " is unaffected.";
				dam = 0;
			}
			else if (r_ptr->flags4 & (RF4_BR_LITE))
			{
				note = " resists.";
				dam *= 2; dam /= (randint(6)+6);
			}
			else if (r_ptr->flags3 & (RF3_HURT_LITE))
			{
				if (seen) l_ptr->flags3 |= (RF3_HURT_LITE);
				note = " cringes from the light!";
				note_dies = " shrivels away in the light!";
				dam *= 2;
			}
			break;
		}


		/* Dark -- opposite of Lite */
		case GF_DARK:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			if (seen) obvious = TRUE;
			if (r_ptr->flags4 & (RF4_BR_DARK))
			{
				note = " resists.";
				dam *= 2; dam /= (randint(6)+6);
			}
			else if ((r_ptr->flags3 & (RF3_HURT_LITE)) || (r_ptr->flags3 & (RF3_RES_NETH)))
			{
				note = " resists a little.";
                dam = (dam * 4) / 5;
            }
			else if ((r_ptr->flags3 & (RF3_UNDEAD)) || (r_ptr->flags3 & (RF3_DEMON)))
			{
				note = " resists slightly.";
				if ((seen) && (r_ptr->flags3 & (RF3_DEMON))) l_ptr->flags3 |= (RF3_DEMON);
				if ((seen) && (r_ptr->flags3 & (RF3_UNDEAD))) l_ptr->flags3 |= (RF3_UNDEAD);
                dam = (dam * 7) / 8;
            }
			if (r_ptr->flags3 & (RF3_HURT_DARK))
			{
                dam *= 2;
				note = " trembles in the darkness.";
				note_dies = " dies from lack of light!";
            }
			break;
		}


		/* Stone to Mud */
		case GF_KILL_WALL:
		{
			/* Hurt by rock remover */
			if (r_ptr->flags3 & (RF3_HURT_ROCK))
			{
				/* Notice effect */
				if (seen) obvious = TRUE;

				/* Memorize the effects */
				if (seen) l_ptr->flags3 |= (RF3_HURT_ROCK);

				/* Cute little message */
				if (!(r_ptr->flags7 & (RF7_NONMONSTER)))
					note = " loses some skin!";
				note_dies = " dissolves!";
			}

			/* Usually, ignore the effects */
			else
			{
				/* No damage */
				dam = 0;
			}

			break;
		}


		/* Teleport undead (Use "dam" as "power") */
		/* now used for banish unnatural */
		case GF_AWAY_UNDEAD:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			/* Only affect undead */
			if ((r_ptr->flags3 & (RF3_NON_LIVING)) ||
			   (r_ptr->flags3 & (RF3_SILVER)))
			{
				if (seen) obvious = TRUE;
				if (seen) l_ptr->flags3 |= (RF3_SILVER);
				do_dist = dam;
			}

			/* Others ignore */
			else
			{
				/* Irrelevant */
				skipped = TRUE;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Teleport evil (Use "dam" as "power") */
		case GF_AWAY_EVIL:
		{
			/* Only affect evil */
			if (m_ptr->evil)
			{
				if (seen)
                {
                    obvious = TRUE;
					if (r_ptr->flags3 & (RF3_EVIL)) l_ptr->flags3 |= (RF3_EVIL);
					else if (r_ptr->flags2 & (RF2_S_EVIL2)) l_ptr->flags2 |= (RF2_S_EVIL2);
					else if (r_ptr->flags2 & (RF2_S_EVIL1)) l_ptr->flags2 |= (RF2_S_EVIL1);
                }
				do_dist = dam;
			}

			/* Others ignore */
			else
			{
				/* Irrelevant */
				skipped = TRUE;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Teleport monster (Use "dam" as "power") */
		case GF_AWAY_ALL:
		{
			/* NONMONSTERS are unaffected (cannot teleother a tree) */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			/* Obvious */
			if (seen) obvious = TRUE;

			/* those that resist nexus have a (small) chance to resist) */
			if (r_ptr->flags3 & RF3_RES_NEXUS)
			{
				int resist = (r_ptr->level/4) + 4 + ((badluck+1)/2);
				if (randint(100) < resist)
				{
					note = " resists the teleportation!";
					dam = 0;
					break;
				}
			}

			/* Prepare to teleport */
			do_dist = dam;

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Turn undead (Use "dam" as "power") */
		case GF_TURN_UNDEAD:
		{
			/* Only affect undead */
			if (r_ptr->flags3 & (RF3_UNDEAD))
			{
				/* effective monster level */
				int rlev = r_ptr->level + 1;
				if (r_ptr->flags2 & (RF2_STUPID)) rlev = ((rlev-1) * 3) / 4;

				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_UNDEAD);

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Apply some fear */
				do_fear = damroll(4, (dam / 2));

				/* Attempt a saving throw */
				if (r_ptr->level > randint((dam - 15) < 1 ? 1 : (dam - 15)) + 15)
				{
					/* No obvious effect */
					note = " is unaffected!";
					obvious = FALSE;
					do_fear = 0;
				}
			}

			/* Others ignore */
			else
			{
				/* Irrelevant */
				skipped = TRUE;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}


		/* Turn monster (Use "dam" as "power") */
		case GF_TURN_ALL:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			bool fearless = FALSE;
			erlev = r_ptr->level + 1;
			if (r_ptr->flags3 & (RF3_NO_FEAR))
			{
				fearless = TRUE;
				erlev += 5;
			}
			/* Uniques no longer automatically immune */
			if (r_ptr->flags1 & (RF1_UNIQUE)) erlev += 20;

			/* bypass NO_FEAR */
			if (dam == 999)
			{
				fearless = FALSE;
				dam = 50 + randint(20);

				/* Apply some fear */
				do_fear = damroll(3, 24) + 1;
			}
			else if (randint(100) < (dam/10) + goodluck)
			{
				/* Apply some fear (before damage boost) */
				do_fear = damroll(3, (dam / 2)) + 1;

				dam += randint(10 + ((goodluck+1)/2));
			}
			else
			{
				/* Apply some fear */
				do_fear = damroll(3, (dam / 2)) + 1;
			}

			/* Obvious */
			if (seen) obvious = TRUE;

			/* Attempt a saving throw */
			if ((fearless) || (erlev > randint((dam - 10) < 1 ? 1 : (dam - 10)) + 10))
			{
				/* No obvious effect */
				note = " is unaffected!";
				obvious = FALSE;
				do_fear = 0;
			}

			/* No "real" damage */
			dam = 0;
			break;
		}

        /* Bug Spray */
        case GF_BUG_SPRAY:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

          if (spellswitch == 16) /* disinfectant*/
          {
			/* Only affect certain monsters */
			if ((r_ptr->d_char == 'j') || (r_ptr->d_char == 'm') ||
               (r_ptr->d_char == ',') || (r_ptr->d_char == 'w') ||
               (r_ptr->d_char == 'R') || (r_ptr->d_char == 'S'))
			{
				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " squelches.";
				note_dies = " disentegrates!";
				
				if ((r_ptr->d_char == 'R') || (r_ptr->d_char == 'S'))
				{
                   dam = (dam / 3);
			       note = " cringes.";
				   note_dies = " dies!";
                }
			}

		    /* Others ignore */
		    else
		    {
			    /* Irrelevant */
			    skipped = TRUE;

			    /* No damage */
			    dam = 0;
            }
          }
          else
          {

			/* Only affect bugs */
			if (r_ptr->flags3 & (RF3_BUG))
			{
				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_BUG);

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " cringes.";
				note_dies = " dies!";
			}

			/* also affect rodents, but not as much */
			else if (r_ptr->d_char == 'r')
			{
                dam = (dam / 2);

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " cringes.";
				note_dies = " dies!";
			}

		    /* Others ignore */
		    else
		    {
			    /* Irrelevant */
			    skipped = TRUE;

			    /* No damage */
			    dam = 0;
            }
          }

		  break;
		}

		/* Dispel silver */
		case GF_DISP_SILVER:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			/* Only affect silver */
			if (r_ptr->flags3 & (RF3_SILVER))
			{
				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_SILVER);

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " cringes.";
				note_dies = " dissolves!";
			}

			/* Others ignore */
			else
			{
				/* Irrelevant */
				skipped = TRUE;

				/* No damage */
				dam = 0;
			}

			break;
		}


		/* Dispel undead */
		case GF_DISP_UNDEAD:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

            /* spellswitch 23: dispel demons */
			if ((r_ptr->flags3 & (RF3_DEMON)) && (spellswitch == 23))
			{
				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_DEMON);

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " shudders.";
				note_dies = " dissolves!";
            }
			/* Normally only affect undead */
			else if ((r_ptr->flags3 & (RF3_UNDEAD)) && (spellswitch != 23))
			{
				/* Learn about type */
				if (seen) l_ptr->flags3 |= (RF3_UNDEAD);

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " shudders.";
				note_dies = " dissolves!";
			}
			/* Others ignore */
			else
			{
				/* Irrelevant */
				skipped = TRUE;

				/* No damage */
				dam = 0;
			}

			break;
		}


		/* Dispel evil */
		case GF_DISP_EVIL:
		{
			/* Only affect evil */
			if (m_ptr->evil)
			{
				/* Learn about type */
				if (seen)
                {
					if (r_ptr->flags3 & (RF3_EVIL)) l_ptr->flags3 |= (RF3_EVIL);
					else if (r_ptr->flags2 & (RF2_S_EVIL2)) l_ptr->flags2 |= (RF2_S_EVIL2);
					else if (r_ptr->flags2 & (RF2_S_EVIL1)) l_ptr->flags2 |= (RF2_S_EVIL1);
                }

				/* Obvious */
				if (seen) obvious = TRUE;

				/* Message */
				note = " shudders.";
				note_dies = " dissolves!";
			}

			/* Others ignore */
			else
			{
				/* Irrelevant */
				skipped = TRUE;

				/* No damage */
				dam = 0;
			}

			break;
		}


		/* Dispel monster */
		case GF_DISP_ALL:
		{
			/* NONMONSTERS are unaffected */
			if (r_ptr->flags7 & (RF7_NONMONSTER)) { dam = 0; break; }

			/* Obvious */
			if (seen) obvious = TRUE;

			/* Message */
			note = " shudders.";
			note_dies = " dissolves!";
			
			if ((spellswitch == 10) &&
			   ((r_ptr->d_char == 'g') || (r_ptr->d_char == 'v') || 
			   (r_ptr->d_char == 'X') || (r_ptr->d_char == '%')))
			{
			   note = " is unaffected.";
			   dam = 0;
            }

			break;
		}


		/* Default */
		default:
		{
			/* Irrelevant */
			skipped = TRUE;

			/* No damage */
			dam = 0;

			break;
		}
	}


	/* Absolutely no effect */
	if (skipped) return (FALSE);


	/* "Unique" monsters cannot be polymorphed or teleleveled */
	if (r_ptr->flags1 & (RF1_UNIQUE))
	{
		do_poly = FALSE;
		if (do_delete)
		{
			note = " resists the effect.";
			do_delete = FALSE;
		}
	}


	/* Unique monsters can only be killed by the player */
	if (r_ptr->flags1 & (RF1_UNIQUE))
	{
		/* Uniques may only be killed by the player */
		if ((who > 0) && (dam > m_ptr->hp)) dam = m_ptr->hp;
	}
	
	/* saving throw for polymorph ..because */
	/* "if (randint(95) > r_ptr->level)" is a really ugly way to do a saving throw */
	if (do_poly)
	{
		/* monster level shouldn't have as big an effect */
        int savel = r_ptr->level/2 + 25;
		/* a little less likely to work if caster is another monster (like with BR_CHAO) */
		if (who > 0) savel += 8;
		
		/* leaders don't polymorph as easily */
		if ((r_ptr->flags1 & (RF1_ESCORT)) || (r_ptr->flags2 & (RF2_ESCORT1))) savel += 8;
        
		/* simulate a will save */
		if (r_ptr->flags2 & (RF2_STUPID)) savel -= 6;
		if (r_ptr->flags2 & (RF2_POWERFUL)) savel += 8;
		if (r_ptr->flags2 & (RF2_SMART)) savel += 4;
		
		/* some monster types tend to be easy to polymorph */
		if ((r_ptr->flags3 & (RF3_ORC)) || (r_ptr->flags3 & (RF3_TROLL)) ||
		   (r_ptr->flags3 & (RF3_ANIMAL)) || (r_ptr->flags3 & (RF3_BUG)))
		    savel -= 8;
		else if (strchr("ORjw,", r_ptr->d_char)) savel -= 8;
		else if (strchr("HNhknx", r_ptr->d_char)) savel -= 4;
		
		/* others tend to be harder */
        if (r_ptr->flags3 & (RF3_HURT_DARK)) savel += 8;
        else if (r_ptr->flags3 & (RF3_HURT_SILV)) savel += 8;
		else if (strchr("EKLUViy", r_ptr->d_char)) savel += 4;
        if (r_ptr->flags7 & (RF7_THEME_ONLY)) savel += 4;
		
		/* do saving throw */
        if (randint(96) < savel) do_poly = FALSE;
	}

	/* Check for death */
	if (dam > m_ptr->hp)
	{
		/* Extract method of death */
		note = note_dies;
	}

	/* Mega-Hack -- Handle "polymorph" -- monsters got a saving throw */
	else if (do_poly)
	{
		int oldhp, oldmax, oldmin, oldlev;

		/* Default -- assume no polymorph */
		note = " is unaffected!";

		/* Pick a new monster race */
		tmp = poly_r_idx(m_ptr->r_idx);

		/* Handle polymorph */
		if (tmp != m_ptr->r_idx)
		{
			/* Obvious */
			if (seen) obvious = TRUE;

			/* save old hps */
			oldhp = m_ptr->hp;
			oldmax = m_ptr->maxhp;
			oldmin = r_ptr->hdice;
			oldlev = r_ptr->level;

			/* Monster polymorphs */
			note = " changes!";

			/* Turn off the damage */
			dam = 0;

			/* "Kill" the "old" monster */
			delete_monster_idx(cave_m_idx[y][x], FALSE);

			/* Create a new monster (no groups) */
			(void)place_monster_aux(y, x, tmp, FALSE, FALSE);

			/* Hack -- Assume success XXX XXX XXX */

			/* Hack -- Get new monster */
			m_ptr = &mon_list[cave_m_idx[y][x]];

			/* Hack -- Get new race */
			r_ptr = &r_info[m_ptr->r_idx];

			/* scale hps (don't always heal with polymorphing) */
			if ((m_ptr->hp > oldmax) && (r_ptr->flags1 & (RF1_FORCE_MAXHP)))
			{
				m_ptr->hp = (m_ptr->hp + oldmax) / 2;
				/* m_ptr->maxhp = m_ptr->hp; */
			}
			else if ((m_ptr->hp > oldmax) && (r_ptr->hdice > oldmax))
			{
				m_ptr->hp = r_ptr->hdice;
				m_ptr->maxhp = r_ptr->hdice;
			}
			else if (m_ptr->hp > oldhp + 1)
			{
				m_ptr->hp = oldhp + 1;
				if (m_ptr->maxhp > (oldmax * 10)) m_ptr->maxhp = oldmax * 10;
			}
			else if ((m_ptr->hp < oldhp) && (m_ptr->hp < oldmin))
			{
				m_ptr->hp = (m_ptr->hp + oldmin) / 2;
			}
			else if ((m_ptr->hp < oldhp) && (m_ptr->hp < (r_ptr->hdice*r_ptr->hside)))
			{
				m_ptr->hp = ((m_ptr->hp*2) + oldhp) / 3;
			}

			/* new race is weaker but more maxhps, scale up a little */
			if ((m_ptr->maxhp > oldmax) && (r_ptr->level < oldlev)) 
			{
				if (m_ptr->hp < m_ptr->maxhp - (oldlev - r_ptr->level))
					m_ptr->hp += randint(oldlev - r_ptr->level);
			}
			
			/* correct new max hps */
			if (m_ptr->maxhp < m_ptr->hp) m_ptr->maxhp = m_ptr->hp;
		}
	}

	/* Handle "teleport" */
	else if (do_dist)
	{
		/* Obvious */
		if (seen) obvious = TRUE;

		/* Message */
		note = " disappears!";

		/* Teleport */
		if (who > 0) teleport_away(cave_m_idx[y][x], do_dist, 0);
		else teleport_away(cave_m_idx[y][x], do_dist, 1);

		/* Hack -- get new location */
		y = m_ptr->fy;
		x = m_ptr->fx;
	}

	/* Sound and Impact breathers never stun */
	else if (do_stun &&
	         !(r_ptr->flags4 & (RF4_BR_SOUN)) &&
	         !(r_ptr->flags3 & (RF3_NO_STUN)) &&
	         !(r_ptr->flags4 & (RF4_BR_WALL)))
	{
		/* Obvious */
		if (seen) obvious = TRUE;

		/* Get stunned */
		if (m_ptr->stunned)
		{
			note = " is more dazed.";
			tmp = m_ptr->stunned + (do_stun / 2);
		}
		else
		{
			note = " is dazed.";
			tmp = do_stun;
		}

		/* Apply stun */
		m_ptr->stunned = (tmp < 200) ? tmp : 200;
	}

	/* Confusion and Chaos breathers (and sleepers) never confuse */
	/* (light fairies' confusion from poison bypasses NO_CONF) */
	else if ((do_conf &&
	         !(r_ptr->flags3 & (RF3_NO_CONF)) &&
	         !(r_ptr->flags4 & (RF4_BR_CONF)) &&
	         !(r_ptr->flags4 & (RF4_BR_CHAO))) ||
			 (do_conf && (r_ptr->d_char == 'y')))
	{
		/* Obvious */
		if (seen) obvious = TRUE;

		/* Already partially confused */
		if (m_ptr->confused)
		{
			note = " looks more confused.";
			tmp = m_ptr->confused + (do_conf / 2);
		}

		/* Was not confused */
		else
		{
			note = " looks confused.";
			tmp = do_conf;
		}

		/* Apply confusion */
		m_ptr->confused = (tmp < 200) ? tmp : 200;
	}

    /* silence summons */
    if (do_silence)
    {
		/* Already silenced */
		if (m_ptr->silence)
		{
			note = " groans with frustration.";
            tmp = m_ptr->silence + (do_silence / 3) + 1;
		}
		/* Was not silenced */
		else
		{
			note = " is silenced.";
			tmp = do_silence;
		}
		m_ptr->silence = (tmp < 200) ? tmp : 200;
    }

	/* Fear */
	if (do_fear)
	{
		/* Increase fear */
		tmp = m_ptr->monfear + do_fear;

		/* Set fear */
		m_ptr->monfear = (tmp < 200) ? tmp : 200;
	}


	/* If another monster did the damage, hurt the monster by hand */
	if (who > 0)
	{
		/* Redraw (later) if needed */
		if (p_ptr->health_who == cave_m_idx[y][x]) p_ptr->redraw |= (PR_HEALTH);

		/* non-aggressive monsters just start roaming */
		if ((!do_sleep) && (typ != GF_OLD_HEAL) && (r_ptr->sleep == 255))
		{
			if (!m_ptr->roaming) m_ptr->roaming = 1;
		}
		/* Wake the monster up */
		else if ((!do_sleep) && (typ != GF_OLD_HEAL))
		{
		    m_ptr->csleep = 0;
		    m_ptr->roaming = 0;
		}

		/* Hurt the monster */
		m_ptr->hp -= dam;

		/* Dead monster */
		if (m_ptr->hp < 0)
		{
			/* Generate treasure, etc */
			monster_death(cave_m_idx[y][x]);

			/* Delete the monster */
			delete_monster_idx(cave_m_idx[y][x], TRUE);

			/* Give detailed messages if destroyed */
			if (r_ptr->flags7 & (RF7_NONMONSTER))
				msg_format("%^s is destroyed", m_name);
			else if (note) msg_format("%^s%s", m_name, note);
		}

		/* Damaged monster */
		else
		{
			/* Give detailed messages if visible or destroyed */
			if ((note) && (seen) &&
				((!(r_ptr->flags7 & (RF7_NONMONSTER))) || (do_dist)))
				msg_format("%^s%s", m_name, note);

			/* Hack -- Pain message */
			else if ((dam > 0) && (!(r_ptr->flags7 & (RF7_NONMONSTER))))
				message_pain(cave_m_idx[y][x], dam);

			/* Hack -- handle sleep */
			if ((do_sleep) && (!m_ptr->csleep))
            {
                m_ptr->csleep = do_sleep;
            }
            else m_ptr->csleep += do_sleep/2;
		}
	}

	/* If the player did it, give him experience, check fear */
	else
	{
		bool fear = FALSE;
		
		/* if you hit a charmed animal, the sphere of charm dissapears */
		if (m_ptr->charmed)
		{
			(void)clear_timed(TMD_SPHERE_CHARM);
		}

		/* If an undead is destroyed by a healing spell, then it is */
		/* extremely unlikely to come back to life. */
		if ((m_ptr->hp - dam < 0) && (healedundead) &&
			(r_ptr->flags2 & (RF2_RETURNS)))
			m_ptr->ninelives += 10;

		/* Hurt the monster, check for fear and death */
		if (mon_take_hit(cave_m_idx[y][x], dam, &fear, note_dies))
		{
			/* Dead monster */
		}

		/* Damaged monster */
		else
		{
			/* Give detailed messages if visible or destroyed */
			if ((note) && (seen) && (!(r_ptr->flags7 & (RF7_NONMONSTER))))
				msg_format("%^s%s", m_name, note);

			/* Hack -- Pain message */
			else if (dam > 0) message_pain(cave_m_idx[y][x], dam);

			/* end truce if you did significant damage */
			if ((m_ptr->truce) &&
				((dam >= 20) || ((dam > 0) && 
				(randint(100) < 15 + badluck))))
			{
				m_ptr->truce = 0;
				msg_format("%^s cancells your truce.", m_name);
			}

			/* Take note */
			if ((fear || do_fear) && (m_ptr->ml) &&
				(!(r_ptr->flags7 & (RF7_NONMONSTER))))
			{
				/* Message */
				message_format(MSG_FLEE, m_ptr->r_idx,
				               "%^s flees in terror!", m_name);
			}

			/* Hack -- handle sleep */
			if ((do_sleep) && (!m_ptr->csleep))
            {
                m_ptr->csleep = do_sleep;
            }
            else m_ptr->csleep += do_sleep/2;
		}
	}


	/* Verify this code XXX XXX XXX */


	/* Nexus telelevel effect on monsters */
	if ((do_delete) && (m_ptr->hp > 0))
	{
		/* (Delete the monster) */
		delete_monster_idx(cave_m_idx[y][x], FALSE);
	}
	else
	{
		/* Update the monster */
		update_mon(cave_m_idx[y][x], 0);
	}

	/* Redraw the monster grid */
	lite_spot(y, x);


	/* Update monster recall window */
	if (p_ptr->monster_race_idx == m_ptr->r_idx)
	{
		/* Window stuff */
		p_ptr->window |= (PW_MONSTER);
	}


	/* Track it */
	project_m_n++;
	project_m_x = x;
	project_m_y = y;


	/* Return "Anything seen?" */
	return (obvious);
}




/*
 * Helper function for "project()" below.
 *
 * Handle a beam/bolt/ball causing damage to the player.
 *
 * This routine takes a "source monster" (by index), a "distance", a default
 * "damage", and a "damage type".  See "project_m()" above.
 *
 * If "rad" is non-zero, then the blast was centered elsewhere, and the damage
 * is reduced (see "project_m()" above).  This can happen if a monster breathes
 * at the player and hits a wall instead.
 *
 * We return "TRUE" if any "obvious" effects were observed.
 *
 * Actually, for historical reasons, we just assume that the effects were
 * obvious.  XXX XXX XXX
 */
static bool project_p(int who, int r, int y, int x, int dam, int typ, int spread, bool breath)
{
	int die, k = 0;
	monster_race *r_ptr;

	/* Hack -- assume obvious */
	bool obvious = TRUE;

	/* Player blind-ness */
	bool blind = (p_ptr->timed[TMD_BLIND] ? TRUE : FALSE);

	/* Source monster */
	monster_type *m_ptr;

	/* Monster name (for attacks) */
	char m_name[80];

	/* Monster name (for damage) */
	char killer[80];

	/* Hack -- messages */
	cptr act = NULL;


	/* No player here */
	if (!(cave_m_idx[y][x] < 0)) return (FALSE);

	/* Never affect projector */
	if (cave_m_idx[y][x] == who) return (FALSE);



	/* Get the source monster */
	m_ptr = &mon_list[who];
	r_ptr = &r_info[m_ptr->r_idx];

	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), m_ptr, 0);

	/* Get the monster's real name */
	monster_desc(killer, sizeof(killer), m_ptr, 0x88);

	/* Limit maximum damage XXX XXX XXX */
	if (dam > 1600) dam = 1600;

	/* spread effects use wide radius ball effect centered on the caster */
	if (spread) /* spread = total radius + 1 */
	{
		if (r > 2)
		{
			dam -= (dam/spread) * (r-2);
		}
	}
	else
	{
		/* Reduce damage by radius of ball spell */
		/* usually no effect because monster targets the player */
		dam = (dam + r) / (r + 1);
	}

	/* mark breath() as a breath weapon rather than a monster ball spell */
	if (breath)
	{
       /* Reduce damage by distance (slightly) for monster breath weapons */
	   int bdist = m_ptr->cdis;
	   int reduction = 0;
	   if (bdist > 7) reduction = ((dam-4) * (bdist - 7)) / bdist;
	   else if (bdist == 7) reduction = dam / 12;

	   /* don't reduce damage too much */
	   /* (maximum reduction is slightly more than 1/3 of the damage) */
	   if (reduction > (dam * 7) / 20) reduction = ((dam * 7) / 20) + 1;

	   dam -= reduction;
    }


	/* Analyze the damage */
	switch (typ)
	{
		/* Standard damage -- hurts inventory too */
		/* breath damage */
		case GF_ACID:
		{
			if (blind) msg_print("You are hit by acid!");
			acid_dam(dam, killer);
			/* acid does less damage in water */
			if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 4) / 5;
			}
			break;
		}

		/* Standard damage -- hurts inventory too */
		case GF_FIRE:
		{
			if (blind) msg_print("You are hit by fire!");
			fire_dam(dam, killer);
			/* fire does less damage in water */
			if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 3) / 4;
			}
			break;
		}

		/* Standard damage -- hurts inventory too */
		case GF_COLD:
		{
			if (blind) msg_print("You are hit by cold!");
			/* frost does more damage in water */
			if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 5) / 4;
			}
			cold_dam(dam, killer);
			break;
		}

		/* Standard damage -- hurts inventory too */
		case GF_ELEC:
		{
			if (blind) msg_print("You are hit by lightning!");
			/* lightning does more damage in water */
			if (cave_feat[y][x] == FEAT_WATER)
			{
				dam = (dam * 4) / 3;
			}
			elec_dam(dam, killer);
			break;
		}

		/* Standard damage -- also poisons player */
		case GF_POIS:
		{
			if (blind) msg_print("You are hit by poison!");
			if (p_ptr->resist_pois) dam = (dam + 2) / 3;
			else if (p_ptr->weakresist_pois) dam = (dam + 2) / 2;
			if (p_ptr->timed[TMD_OPP_POIS]) dam = (dam + 2) / 3;
			take_hit(dam, killer);
			if (!(p_ptr->resist_pois || p_ptr->timed[TMD_OPP_POIS]))
			{
                (void)inc_timed(TMD_POISONED, rand_int(dam) + 10);
			}
			break;
		}

		/* Standard damage */
		case GF_MISSILE:
		{
			if (blind) msg_print("You are hit by something!");
			take_hit(dam, killer);
			break;
		}

		/* Holy Orb -- Player only takes partial damage */
		case GF_HOLY_ORB:
		{
			if (blind) msg_print("You are hit by something!");
			dam /= 2;
			take_hit(dam, killer);
			break;
		}

		/* Arrow -- no dodging XXX */
		case GF_ARROW:
		{
			if (blind) msg_print("You are hit by something sharp!");
			take_hit(dam, killer);
			break;
		}
				
		/* Throwing junk -- shorter range */
		case GF_THROW:
		{
			if (blind) msg_print("You are hit by a piece of junk!");
			take_hit(dam, killer);
			range = 4;
			break;
		}

		/* Plasma -- slight resist only */
		case GF_PLASMA:
		{
			int rplas = 0;
			if (blind) msg_print("You are hit by something!");

			/* stuff which provides slight plasma resistance */
			if (p_ptr->resist_acid) rplas += 1;
			if (p_ptr->timed[TMD_OPP_ACID]) rplas += 1;
			if (p_ptr->resist_fire) rplas += 1;
			if (p_ptr->timed[TMD_OPP_FIRE]) rplas += 1;
			dam = (dam * (12-rplas)) / 12;

			take_hit(dam, killer);
			if (!p_ptr->resist_sound)
			{
				int k = (randint((dam > 40) ? 35 : (dam * 3 / 4 + 5)));
				(void)inc_timed(TMD_STUN, k);
			}
			break;
		}

		/* Nether -- drain experience */
		case GF_NETHER:
		{
			if (blind) msg_print("You are hit by something strange!");
			if (p_ptr->resist_nethr)
			{
				/* corruption weakens and may nullify resistance to nether */
                if (randint(75) < p_ptr->corrupt*2)
				{
				   int corp = (p_ptr->corrupt + 2) / 4;
				   if (corp < 1) corp = 1;
				   if (corp > 6) corp = 6;
				   dam *= 6; dam /= (randint(7 - corp) + 5);
				   if (!blind && !p_ptr->resist_blind && (corp > 2)) (void)inc_timed(TMD_BLIND, randint(4) + 1);
				}
				/* else dam *= 6; dam /= (randint(6) + 6); */
				else dam *= 6; dam /= (randint(4) + 7);
			}
			else
			{
				if (p_ptr->hold_life && (rand_int(100) < 75))
				{
					msg_print("You keep hold of your life force!");
				}
				else
				{
					s32b d = 200 + (p_ptr->exp / 100) * MON_DRAIN_LIFE;
					/* corruption makes you vulnerable to nether */
					if (randint(75) < p_ptr->corrupt*2)
					{
						dam += dam / 12;
					}

					if (p_ptr->hold_life)
					{
						msg_print("You feel your life slipping away!");
						lose_exp(d / 10);
					}
					else
					{
						msg_print("You feel your life draining away!");
						lose_exp(d);
					}
				}
			}
			take_hit(dam, killer);
			break;
		}

		/* Water -- stun/confuse */
		case GF_WATER:
		{
			if (blind) msg_print("You are hit by something!");
			if (!p_ptr->resist_sound)
			{
				(void)inc_timed(TMD_STUN, randint(40));
			}
			if (!p_ptr->resist_confu)
			{
				(void)inc_timed(TMD_CONFUSED, randint(5) + 5);
			}
			take_hit(dam, killer);
			break;
		}

		/* Chaos -- many effects */
		case GF_CHAOS:
		{
			bool extrabad = FALSE;
            if (blind) msg_print("You are hit by something strange!");
			if ((p_ptr->resist_chaos) && (p_ptr->timed[TMD_TSIGHT]))
			{
				dam *= 6; dam /= (randint(3) + 8);
			}
			else if (p_ptr->resist_chaos)
			{
				dam *= 6; dam /= (randint(4) + 7);
			}
			else if (p_ptr->timed[TMD_TSIGHT])
			{
				dam *= 6; dam /= (randint(2) + 6);
			}
			if (!p_ptr->resist_confu && !p_ptr->resist_chaos)
			{
				(void)inc_timed(TMD_CONFUSED, rand_int(20) + 10);
				extrabad = TRUE;
			}
			if (!p_ptr->resist_nethr && !p_ptr->resist_chaos)
			{
				if (p_ptr->hold_life && (rand_int(100) < 75))
				{
					msg_print("You keep hold of your life force!");
				}
				else
				{
					s32b d = 5000 + (p_ptr->exp / 100) * MON_DRAIN_LIFE;

					if (p_ptr->hold_life)
					{
						msg_print("You feel your life slipping away!");
						lose_exp(d / 10);
					}
					else
					{
						msg_print("You feel your life draining away!");
						lose_exp(d);
						extrabad = TRUE;
					}
				}
			}
			if ((!p_ptr->resist_chaos) && (!p_ptr->timed[TMD_TSIGHT]) &&
				((rand_int(100) < (badluck+10) * 5) || (!extrabad)))
			{
				(void)inc_timed(TMD_IMAGE, randint(10));
				extrabad = TRUE;
			}
			if ((!p_ptr->resist_nethr && !p_ptr->resist_charm) &&
				((rand_int(100) < (badluck+10) * 2) || (!extrabad)))
			{
				(void)inc_timed(TMD_FRENZY, randint(10));
			}
			take_hit(dam, killer);
			break;
		}

		/* Shards -- mostly cutting */
		case GF_SHARD:
		{
			if (blind) msg_print("You are hit by something sharp!");
			if (p_ptr->resist_shard)
			{
				/* dam *= 6; dam /= (randint(6) + 6); */
				dam *= 6; dam /= (randint(4) + 7);
			}
			else
			{
				(void)inc_timed(TMD_CUT, dam);
			}
			take_hit(dam, killer);
			break;
		}

		/* Sound -- mostly stunning */
		case GF_SOUND:
		{
			if (blind) msg_print("You are hit by sound!");
			if (p_ptr->resist_sound)
			{
				/* dam *= 5; dam /= (randint(6) + 6); */
				/* shouldn't be more effective than others for damage */
				/* because Rsound is mainly Rstun */
				dam *= 6; dam /= (randint(4) + 7);
			}
			else
			{
				int k = (randint((dam > 90) ? 35 : (dam / 3 + 5)));
				(void)inc_timed(TMD_STUN, k);
			}
			take_hit(dam, killer);
			break;
		}

		/* Pure confusion */
		case GF_CONFUSION:
		{
			if (blind) msg_print("You are hit by something!");
			if (p_ptr->resist_confu)
			{
				/* dam *= 5; dam /= (randint(6) + 6); */
				dam *= 5; dam /= (randint(3) + 7);
			}
			if (!p_ptr->resist_confu)
			{
				(void)inc_timed(TMD_CONFUSED, randint(20) + 10);
			}
			take_hit(dam, killer);
			break;
		}

		/* Disenchantment -- see above */
		case GF_DISENCHANT:
		{
			if (blind) msg_print("You are hit by something strange!");
			if (p_ptr->resist_disen)
			{
				/* dam *= 6; dam /= (randint(6) + 6); */
				dam *= 6; dam /= (randint(4) + 7);
			}
			else
			{
				(void)apply_disenchant(0);
			}
			take_hit(dam, killer);
			break;
		}

		/* Nexus -- see above */
		case GF_NEXUS:
		{
			if (blind) msg_print("You are hit by something strange!");
			if (p_ptr->resist_nexus)
			{
				/* dam *= 6; dam /= (randint(6) + 6); */
				dam *= 6; dam /= (randint(4) + 7);
			}
			else
			{
				apply_nexus(m_ptr);
			}
			take_hit(dam, killer);
			break;
		}

		/* Force -- mostly stun */
		case GF_FORCE:
		{
			int k = (randint((dam > 45) ? 20 : (dam / 3 + 5)));
			if (blind) msg_print("You are hit by something!");
			if (!p_ptr->resist_sound)
			{
				(void)inc_timed(TMD_STUN, 5 + k);
			}
			else
			{
				(void)inc_timed(TMD_STUN, rand_int(k/3 + 2));
			}
			take_hit(dam, killer);
			break;
		}

		/* breathe slime */
		case GF_BRSLIME:
		{
			int k = (randint((dam > 45) ? 20 : (dam / 3 + 5)));
			int reflex_save = adj_dex_dis[p_ptr->stat_ind[A_DEX]] * 2;
			int donestuff = 0;
			if (blind) msg_print("You are hit by something!");
			/* create slime blob or stunning or slowing (or none of the above) */
			if (rand_int(100) < 15 + badluck - goodluck/2)
			{
				if (rand_int(100) < p_ptr->depth - 30 + goodluck) do_call_help(201);
				else do_call_help(112);
				donestuff += 10;
			}
			if (p_ptr->resist_slime) donestuff += 18;
			if (rand_int(100) < 33 + badluck - goodluck/2 - donestuff - reflex_save)
			{
				(void)inc_timed(TMD_STUN, k);
				if (p_ptr->resist_slime) donestuff += 40;
				else donestuff += 10;
			}
			if (rand_int(100) < 33 + badluck - goodluck/2 - donestuff - reflex_save)
			{
				(void)inc_timed(TMD_SLOW, k/2);
			}
			if (p_ptr->resist_slime) dam -= dam / 9;
			take_hit(dam, killer);
			/* sliming (usually) */
			if ((rand_int(100) < 75 + badluck - goodluck/2 - reflex_save) &&
				(!p_ptr->resist_slime))
			{
				p_ptr->slime += 1;
				msg_print("you are slimed!");
			}
			break;
		}
		
		/* breathe fear */
		case GF_BRFEAR:
		{
			if (blind) msg_print("You hear a terrifying noise!");
			die = randint(100);
            if (r_ptr->flags2 & (RF2_POWERFUL)) die -= 15;
            if (p_ptr->timed[TMD_FRENZY]) die += 5;
            if (p_ptr->resist_charm) die += 10;
			if (!p_ptr->resist_fear)
            {
               if (die < 95)
               {
				       int k = (randint((dam > 40) ? 20 : (dam / 2)));
                       (void)inc_timed(TMD_AFRAID, k + 10);
               }
               else
               {
                       msg_print("You resist the effects!");
               }
            }
            else
            {
				dam *= 4; dam /= (randint(5) + 6);
                if (die < 35)
                {
                    inc_timed(TMD_AFRAID, rand_int(5) + 1);
                    msg_print("You partially resist the effects!");
                }
                else msg_print("You resist the effects!");
            }
			take_hit(dam, killer);
			break;
		}
		
		/* Amnesia */
		case GF_AMNESIA:
		{
			int mindsave;
			if (blind) msg_print("Huh? What? Where am I?");
			mindsave = p_ptr->skills[SKILL_SAV] - (badluck/3) + (goodluck/4);
            if (r_ptr->flags2 & (RF2_POWERFUL)) mindsave -= 15;
			if (p_ptr->timed[TMD_CLEAR_MIND]) mindsave += 10;
			if ((p_ptr->resist_silver) && (r_ptr->flags3 & (RF3_SILVER))) mindsave += 50;
			else if ((p_ptr->resist_silver) && (p_ptr->resist_charm)) mindsave += 25;
            else if (p_ptr->resist_silver) mindsave += 15;
            else if (p_ptr->resist_charm) mindsave += 5;
			if (p_ptr->timed[TMD_CHARM]) mindsave -= 10;
			if (randint(110) < mindsave)
			{
                msg_print("You resist the effects!");
                dam = dam / 2;
            }
			else
			{
				int k = (randint((dam > 36) ? 27 : (dam * 3 / 4)));
                (void)inc_timed(TMD_AMNESIA, k + 9);
				/* re-added forgetting the map which had been removed (smaller chance than FORGET spell) */
				if (randint(mindsave*2) < 33 + ((badluck+1)/2)) wiz_dark();
            }
			/* silver magic (sometimes) */
			if ((r_ptr->flags3 & (RF3_SILVER)) && (!p_ptr->resist_silver) &&
				(randint(60 + goodluck - badluck) < 5))
			{
				p_ptr->silver += 1;
				msg_print("you feel silver magic corrupting your mind!");
			}

			take_hit(dam, killer);
			break;
		}

		/* Inertia -- slowness */
		case GF_INERTIA:
		{
			if (blind) msg_print("You are hit by something strange!");
			if (p_ptr->timed[TMD_SUST_SPEED])
			{
				msg_print("You feel like nothing can slow you down!");
				dam -= dam / 6;
			}
			else 
			{
				(void)inc_timed(TMD_SLOW, rand_int(4) + 4);
			}
			take_hit(dam, killer);
			break;
		}

		/* Lite -- blinding */
		case GF_LITE:
		{
			if (blind) msg_print("You are hit by something!");
			if ((p_ptr->timed[TMD_BECOME_LICH]) || (p_ptr->corrupt > 40))
			{
                if (p_ptr->resist_lite) /* very slight resistance */
                {
				   dam *= 7; dam /= (randint(6) + 6);
				   if ((!blind) && (!p_ptr->resist_blind)) 
						(void)inc_timed(TMD_BLIND, randint(3));
                }
                else
                {
				   dam = (dam * 3) / 2;
				   if ((!blind) && (!p_ptr->resist_blind)) 
						(void)inc_timed(TMD_BLIND, randint(5) + 4);
				   else (void)inc_timed(TMD_BLIND, randint(3));
                }
			}
			else if (p_ptr->resist_lite)
			{
				if (randint(50) < p_ptr->corrupt-2)
                {
                   dam *= 6; dam /= (randint(6) + 6);
                   if (!blind && !p_ptr->resist_blind) (void)inc_timed(TMD_BLIND, randint(5) + 1);
                }
                else dam *= 4; dam /= (randint(6) + 6);
			}
			else if (!blind && !p_ptr->resist_blind)
			{
				(void)inc_timed(TMD_BLIND, randint(5) + 2);
			}
			take_hit(dam, killer);
			break;
		}

		/* Dark -- blinding */
		case GF_DARK:
		{
			if (blind) msg_print("You are hit by something!");
			if ((p_ptr->resist_dark) || (randint(90) < p_ptr->corrupt))
			{
				dam *= 4; dam /= (randint(6) + 6);
			}
			else if (!blind && !p_ptr->resist_blind)
			{
				(void)inc_timed(TMD_BLIND, randint(5) + 2);
			}
			take_hit(dam, killer);
			break;
		}
				
		/* Boulder (sometimes leaves rubble) */
		case GF_BOULDER:
		{
			if (blind) msg_print("You are hit by a boulder!");
			take_hit(dam, killer);
			break;
		}

		/* Time -- bolt fewer effects XXX */
		case GF_TIME:
		{
			if (blind) msg_print("You are hit by something strange!");

			switch (randint(10))
			{
				case 1: case 2: case 3: case 4: case 5:
				{
					msg_print("You feel life has clocked back.");
					lose_exp(100 + (p_ptr->exp / 100) * MON_DRAIN_LIFE);
					break;
				}

				case 6: case 7: case 8: case 9:
				{
					switch (randint(6))
					{
						case 1: k = A_STR; act = "strong"; break;
						case 2: k = A_INT; act = "bright"; break;
						case 3: k = A_WIS; act = "wise"; break;
						case 4: k = A_DEX; act = "agile"; break;
						case 5: k = A_CON; act = "hale"; break;
						case 6: k = A_CHR; act = "beautiful"; break;
					}

					msg_format("You're not as %s as you used to be...", act);

					p_ptr->stat_cur[k] = (p_ptr->stat_cur[k] * 4) / 5;
					if (p_ptr->stat_cur[k] < 3) p_ptr->stat_cur[k] = 3;
					p_ptr->update |= (PU_BONUS);
					break;
				}

				case 10:
				{
					msg_print("You're not as powerful as you used to be...");

					for (k = 0; k < A_MAX; k++)
					{
						p_ptr->stat_cur[k] = (p_ptr->stat_cur[k] * 4) / 5;
						if (p_ptr->stat_cur[k] < 3) p_ptr->stat_cur[k] = 3;
					}
					p_ptr->update |= (PU_BONUS);
					break;
				}
			}
			take_hit(dam, killer);
			break;
		}

		/* Gravity -- stun plus slowness plus teleport */
		case GF_GRAVITY:
		{
			bool res_slow = FALSE;
			/* (unnessesary message) */
            /* if (blind) msg_print("You are hit by something strange!"); */
			msg_print("Gravity warps around you.");

			/* Higher level players can resist the teleportation better */
			if (randint(127) > p_ptr->lev) teleport_player(5);

			/* feather fall gives partial resist */
			if (p_ptr->ffall)
			{
				dam -= dam / 9;
				if (rand_int(100) < 20) res_slow = TRUE;
			}
			if (p_ptr->timed[TMD_SUST_SPEED]) res_slow = TRUE;

			if (!res_slow) (void)inc_timed(TMD_SLOW, rand_int(4) + 4);
			if (!p_ptr->resist_sound)
			{
				int k = (randint((dam > 90) ? 35 : (dam / 3 + 5)));
				(void)inc_timed(TMD_STUN, k);
			}
			take_hit(dam, killer);
			break;
		}

		/* Pure damage */
		case GF_MANA:
		{
			if (blind) msg_print("You are hit by something!");
			take_hit(dam, killer);
			break;
		}

		/* Pure damage */
		case GF_METEOR:
		{
			if (blind) msg_print("You are hit by something!");
			take_hit(dam, killer);
			break;
		}

		/* Ice -- cold plus stun plus cuts */
		case GF_ICE:
		{
			if (blind) msg_print("You are hit by something sharp!");
			cold_dam(dam, killer);
			if (!p_ptr->resist_shard)
			{
				(void)inc_timed(TMD_CUT, damroll(5, 8));
			}
			if (!p_ptr->resist_sound)
			{
				(void)inc_timed(TMD_STUN, randint(15));
			}
			break;
		}


		/* Default */
		default:
		{
			/* No damage */
			dam = 0;

			break;
		}
	}
	
    /* notice changes of slime and silver poison levels */
    p_ptr->redraw |= (PR_SILVER);
    p_ptr->redraw |= (PR_SLIME);

	/* Disturb */
	disturb(1, 0);

	/* Return "Anything seen?" */
	return (obvious);
}





/*
 * Generic "beam"/"bolt"/"ball" projection routine.
 *
 * Input:
 *   who: Index of "source" monster (negative for "player")
 *   rad: Radius of explosion (0 = beam/bolt, 1 to 9 = ball)
 *   y,x: Target location (or location to travel "towards")
 *   dam: Base damage roll to apply to affected monsters (or player)
 *   typ: Type of damage to apply to monsters (and objects)
 *   flg: Extra bit flags (see PROJECT_xxxx in "defines.h")
 *
 * Return:
 *   TRUE if any "effects" of the projection were observed, else FALSE
 *
 * Allows a monster (or player) to project a beam/bolt/ball of a given kind
 * towards a given location (optionally passing over the heads of interposing
 * monsters), and have it do a given amount of damage to the monsters (and
 * optionally objects) within the given radius of the final location.
 *
 * A "bolt" travels from source to target and affects only the target grid.
 * A "beam" travels from source to target, affecting all grids passed through.
 * A "ball" travels from source to the target, exploding at the target, and
 *   affecting everything within the given radius of the target location.
 *
 * Traditionally, a "bolt" does not affect anything on the ground, and does
 * not pass over the heads of interposing monsters, much like a traditional
 * missile, and will "stop" abruptly at the "target" even if no monster is
 * positioned there, while a "ball", on the other hand, passes over the heads
 * of monsters between the source and target, and affects everything except
 * the source monster which lies within the final radius, while a "beam"
 * affects every monster between the source and target, except for the casting
 * monster (or player), and rarely affects things on the ground.
 *
 * Two special flags allow us to use this function in special ways, the
 * "PROJECT_HIDE" flag allows us to perform "invisible" projections, while
 * the "PROJECT_JUMP" flag allows us to affect a specific grid, without
 * actually projecting from the source monster (or player).
 *
 * The player will only get "experience" for monsters killed by himself
 * Unique monsters can only be destroyed by attacks from the player
 *
 * Only 256 grids can be affected per projection, limiting the effective
 * "radius" of standard ball attacks to nine units (diameter nineteen).
 *
 * One can project in a given "direction" by combining PROJECT_THRU with small
 * offsets to the initial location (see "line_spell()"), or by calculating
 * "virtual targets" far away from the player.
 *
 * One can also use PROJECT_THRU to send a beam/bolt along an angled path,
 * continuing until it actually hits somethings (useful for "stone to mud").
 *
 * Bolts and Beams explode INSIDE walls, so that they can destroy doors.
 *
 * Balls must explode BEFORE hitting walls, or they would affect monsters
 * on both sides of a wall.  Some bug reports indicate that this is still
 * happening in 2.7.8 for Windows, though it appears to be impossible.
 *
 * We "pre-calculate" the blast area only in part for efficiency.
 * More importantly, this lets us do "explosions" from the "inside" out.
 * This results in a more logical distribution of "blast" treasure.
 * It also produces a better (in my opinion) animation of the explosion.
 * It could be (but is not) used to have the treasure dropped by monsters
 * in the middle of the explosion fall "outwards", and then be damaged by
 * the blast as it spreads outwards towards the treasure drop location.
 *
 * Walls and doors are included in the blast area, so that they can be
 * "burned" or "melted" in later versions.
 *
 * This algorithm is intended to maximize simplicity, not necessarily
 * efficiency, since this function is not a bottleneck in the code.
 *
 * We apply the blast effect from ground zero outwards, in several passes,
 * first affecting features, then objects, then monsters, then the player.
 * This allows walls to be removed before checking the object or monster
 * in the wall, and protects objects which are dropped by monsters killed
 * in the blast, and allows the player to see all affects before he is
 * killed or teleported away.  The semantics of this method are open to
 * various interpretations, but they seem to work well in practice.
 *
 * We process the blast area from ground-zero outwards to allow for better
 * distribution of treasure dropped by monsters, and because it provides a
 * pleasing visual effect at low cost.
 *
 * Note that the damage done by "ball" explosions decreases with distance
 * from the center of the ball.
 * This decrease is rapid, grids at radius "dist" take "1/dist" damage.
 *
 * Notice the "napalm" effect of "beam" weapons.  First they "project" to
 * the target, and then the damage "flows" along this beam of destruction.
 * The damage at every grid is the same as at the "center" of a "ball"
 * explosion, since the "beam" grids are treated as if they ARE at the
 * center of a "ball" explosion.
 *
 * Currently, specifying "beam" plus "ball" means that locations which are
 * covered by the initial "beam", and also covered by the final "ball", except
 * for the final grid (the epicenter of the ball), will be "hit twice", once
 * by the initial beam, and once by the exploding ball.  For the grid right
 * next to the epicenter, this results in 150% damage being done.  The center
 * does not have this problem, for the same reason the final grid in a "beam"
 * plus "bolt" does not -- it is explicitly removed.  Simply removing "beam"
 * grids which are covered by the "ball" will NOT work, as then they will
 * receive LESS damage than they should.  Do not combine "beam" with "ball".
 *
 * The array "gy[],gx[]" with current size "grids" is used to hold the
 * collected locations of all grids in the "blast area" plus "beam path".
 *
 * Note the rather complex usage of the "gm[]" array.  First, gm[0] is always
 * zero.  Second, for N>1, gm[N] is always the index (in gy[],gx[]) of the
 * first blast grid (see above) with radius "N" from the blast center.  Note
 * that only the first gm[1] grids in the blast area thus take full damage.
 * Also, note that gm[rad+1] is always equal to "grids", which is the total
 * number of blast grids.
 *
 * Note that once the projection is complete, (y2,x2) holds the final location
 * of bolts/beams, and the "epicenter" of balls.
 *
 * Note also that "rad" specifies the "inclusive" radius of projection blast,
 * so that a "rad" of "one" actually covers 5 or 9 grids, depending on the
 * implementation of the "distance" function.  Also, a bolt can be properly
 * viewed as a "ball" with a "rad" of "zero".
 *
 * Note that if no "target" is reached before the beam/bolt/ball travels the
 * maximum distance allowed (MAX_RANGE), no "blast" will be induced.  This
 * may be relevant even for bolts, since they have a "1x1" mini-blast.
 *
 * Note that for consistency, we "pretend" that the bolt actually takes "time"
 * to move from point A to point B, even if the player cannot see part of the
 * projection path.  Note that in general, the player will *always* see part
 * of the path, since it either starts at the player or ends on the player.
 * *DJA: HELPER monsters have spells which can go straight from monster to
 * monster.
 *
 * Hack -- we assume that every "projection" is "self-illuminating".
 *
 * Hack -- when only a single monster is affected, we automatically track
 * (and recall) that monster, unless "PROJECT_JUMP" is used.
 *
 * Note that all projections now "explode" at their final destination, even
 * if they were being projected at a more distant destination.  This means
 * that "ball" spells will *always* explode.
 *
 * Note that we must call "handle_stuff()" after affecting terrain features
 * in the blast radius, in case the "illumination" of the grid was changed,
 * and "update_view()" and "update_monsters()" need to be called.
 */
bool project(int who, int rad, int y, int x, int dam, int typ, int flg, int pflg)
{
	int py = p_ptr->py;
	int px = p_ptr->px;
	bool hurtmon;

	int i, t, dist;

	int y1, x1;
	int y2, x2;

	int msec = op_ptr->delay_factor * op_ptr->delay_factor;

	/* Assume the player sees nothing */
	bool notice = FALSE;

	/* Assume the player has seen nothing */
	bool visual = FALSE;

	/* Assume the player has seen no blast grids */
	bool drawn = FALSE;

	/* Is the player blind? */
	bool blind = (p_ptr->timed[TMD_BLIND] ? TRUE : FALSE);

	/* Number of grids in the "path" */
	int path_n = 0;

	/* Actual grids in the "path" */
	u16b path_g[512];

	/* Number of grids in the "blast area" (including the "beam" path) */
	int grids = 0;

	/* Coordinates of the affected grids */
	byte gx[256], gy[256];

	/* Encoded "radius" info (see above) */
	byte gm[16];


	/* Hack -- Jump to target */
	if (flg & (PROJECT_JUMP))
	{
		x1 = x;
		y1 = y;

		/* Clear the flag */
		flg &= ~(PROJECT_JUMP);
	}

	/* Start at player */
	else if (who < 0)
	{
		x1 = px;
		y1 = py;
	}

	/* Start at monster */
	else if (who > 0)
	{
		x1 = mon_list[who].fx;
		y1 = mon_list[who].fy;
	}

	/* Oops */
	else
	{
		x1 = x;
		y1 = y;
	}


	/* Default "destination" */
	y2 = y;
	x2 = x;


	/* Hack -- verify stuff */
	if (flg & (PROJECT_THRU))
	{
		if ((x1 == x2) && (y1 == y2))
		{
			flg &= ~(PROJECT_THRU);
		}
	}


	/* Hack -- Assume there will be no blast (max radius 16) */
	for (dist = 0; dist < 16; dist++) gm[dist] = 0;


	/* Initial grid */
	y = y1;
	x = x1;

	/* Collect beam grids */
	if (flg & (PROJECT_BEAM))
	{
		gy[grids] = y;
		gx[grids] = x;
		grids++;
	}


	/* Calculate the projection path */
    /* (allow for shorter ranged spells) */
	if (range) path_n = project_path(path_g, range, y1, x1, y2, x2, flg);
	else path_n = project_path(path_g, MAX_RANGE, y1, x1, y2, x2, flg);


	/* Hack -- Handle stuff */
	handle_stuff();

	/* Project along the path */
	for (i = 0; i < path_n; ++i)
	{
		int oy = y;
		int ox = x;

		int ny = GRID_Y(path_g[i]);
		int nx = GRID_X(path_g[i]);

		/* Hack -- Balls explode before reaching walls */
		/* DJA: if a monster in a wall is targetted by a ball spell */
		/* it will hit the monster, but the radius of the ball will */
		/* be reduced to 0 (effectively a bolt) to prevent affecting */
		/* grids on the other side of the wall. DJXXX */
		if (!cave_floor_bold(ny, nx) && (rad > 0) && (cave_m_idx[ny][nx] > 0))
		{
			rad = 0; /* turn the ball into a bolt */
		}
		else if (!cave_floor_bold(ny, nx) && (rad > 0))
		{
			break;
		}

		/* Advance */
		y = ny;
		x = nx;

		/* Collect beam grids */
		if (flg & (PROJECT_BEAM))
		{
			gy[grids] = y;
			gx[grids] = x;
			grids++;
		}

		/* Only do visuals if requested */
		if (!blind && !(flg & (PROJECT_HIDE)))
		{
			/* Only do visuals if the player can "see" the bolt */
			if (player_has_los_bold(y, x))
			{
				u16b p;

				byte a;
				char c;

				/* Obtain the bolt pict */
				p = bolt_pict(oy, ox, y, x, typ);

				/* Extract attr/char */
				a = PICT_A(p);
				c = PICT_C(p);

				/* Visual effects */
				print_rel(c, a, y, x);
				move_cursor_relative(y, x);

				Term_fresh();
				if (p_ptr->window) window_stuff();

				Term_xtra(TERM_XTRA_DELAY, msec);

				lite_spot(y, x);

				Term_fresh();
				if (p_ptr->window) window_stuff();

				/* Display "beam" grids */
				if (flg & (PROJECT_BEAM))
				{
					/* Obtain the explosion pict */
					p = bolt_pict(y, x, y, x, typ);

					/* Extract attr/char */
					a = PICT_A(p);
					c = PICT_C(p);

					/* Visual effects */
					print_rel(c, a, y, x);
				}

				/* Hack -- Activate delay */
				visual = TRUE;
			}

			/* Hack -- delay anyway for consistency */
			else if (visual)
			{
				/* Delay for consistency */
				Term_xtra(TERM_XTRA_DELAY, msec);
			}
		}
	}


	/* Save the "blast epicenter" */
	y2 = y;
	x2 = x;

	/* Start the "explosion" */
	gm[0] = 0;

	/* Hack -- make sure beams get to "explode" */
	gm[1] = grids;

	/* Explode */

	/* Hack -- remove final beam grid */
	if (flg & (PROJECT_BEAM))
	{
		grids--;
	}

	/* Determine the blast area, work from the inside out */
	for (dist = 0; dist <= rad; dist++)
	{
		/* Scan the maximal blast area of radius "dist" */
		for (y = y2 - dist; y <= y2 + dist; y++)
		{
			for (x = x2 - dist; x <= x2 + dist; x++)
			{
				/* Ignore "illegal" locations */
				if (!in_bounds(y, x)) continue;

				/* Enforce a "circular" explosion */
				if (distance(y2, x2, y, x) != dist) continue;

				/* Ball explosions are stopped by walls */
				if (!los(y2, x2, y, x)) continue;
				
				/* Save this grid */
				gy[grids] = y;
				gx[grids] = x;
				grids++;
			}
		}

		/* Encode some more "radius" info */
		gm[dist+1] = grids;
	}


	/* Speed -- ignore "non-explosions" */
	if (!grids) 
    {
                /* reset range and spellswitch before exiting function */
                range = 0;
                spellswitch = 0;
                return (FALSE);
    }


	/* Display the "blast area" if requested */
	if (!blind && !(flg & (PROJECT_HIDE)))
	{
		/* Then do the "blast", from inside out */
		for (t = 0; t <= rad; t++)
		{
			/* Dump everything with this radius */
			for (i = gm[t]; i < gm[t+1]; i++)
			{
				/* Extract the location */
				y = gy[i];
				x = gx[i];

				/* Only do visuals if the player can "see" the blast */
				if (player_has_los_bold(y, x))
				{
					u16b p;

					byte a;
					char c;

					drawn = TRUE;

					/* Obtain the explosion pict */
					p = bolt_pict(y, x, y, x, typ);

					/* Extract attr/char */
					a = PICT_A(p);
					c = PICT_C(p);

					/* Visual effects -- Display */
					print_rel(c, a, y, x);
				}
			}

			/* Hack -- center the cursor */
			move_cursor_relative(y2, x2);

			/* Flush each "radius" separately */
			Term_fresh();

			/* Flush */
			if (p_ptr->window) window_stuff();

			/* Delay (efficiently) */
			if (visual || drawn)
			{
				Term_xtra(TERM_XTRA_DELAY, msec);
			}
		}

		/* Flush the erasing */
		if (drawn)
		{
			/* Erase the explosion drawn above */
			for (i = 0; i < grids; i++)
			{
				/* Extract the location */
				y = gy[i];
				x = gx[i];

				/* Hack -- Erase if needed */
				if (player_has_los_bold(y, x))
				{
					lite_spot(y, x);
				}
			}

			/* Hack -- center the cursor */
			move_cursor_relative(y2, x2);

			/* Flush the explosion */
			Term_fresh();

			/* Flush */
			if (p_ptr->window) window_stuff();
		}
	}


	/* Check features */
	if (flg & (PROJECT_GRID))
	{
		/* Start with "dist" of zero */
		dist = 0;

		/* Scan for features */
		for (i = 0; i < grids; i++)
		{
			/* Hack -- Notice new "dist" values */
			if (gm[dist+1] == i) dist++;

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Affect the feature in that grid */
			if (project_f(who, dist, y, x, dam, typ)) notice = TRUE;
		}
	}


	/* Update stuff if needed */
	if (p_ptr->update) update_stuff();


	/* Check objects */
	if (flg & (PROJECT_ITEM))
	{
		/* Start with "dist" of zero */
		dist = 0;

		/* Scan for objects */
		for (i = 0; i < grids; i++)
		{
			/* Hack -- Notice new "dist" values */
			if (gm[dist+1] == i) dist++;

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

            /* Hack: weak breath (range 5) doesn't destroy objects */
            if (!((range == 5) && (pflg & (PROJO_BRETH))))
            {
    			bool spread = FALSE;
				/* spread effects destroy object less often  */
				if (pflg & (PROJO_SPRED)) spread = TRUE;
				/* Affect the object in the grid */
			   if (project_o(who, dist, y, x, dam, typ, spread)) notice = TRUE;
            }
		}
	}

    /* earthquake (11 is player activated (doesn't allow big damage to player)) */
    if (spellswitch == 11)
    {
            int quakerad = 0;
            if (rad == 0) quakerad = 5;
            else if (rad == 1) quakerad = 6;
            else if (rad == 2) quakerad = 7;
            else if (rad > 4) quakerad = 10;
            else quakerad = 8;
            if (range == 21) quakerad = rad + 1;
			if (spellswitch == 11) earthquake(y2, x2, quakerad, 0, 0, TRUE);
			else /* 12 */ earthquake(y2, x2, quakerad, 0, 2, FALSE);
    }

	/* Check monsters */
	if (flg & (PROJECT_KILL))
	{
		/* Mega-Hack */
		project_m_n = 0;
		project_m_x = 0;
		project_m_y = 0;

		/* Start with "dist" of zero */
		dist = 0;
		
		hurtmon = FALSE;

		/* Scan for monsters */
		for (i = 0; i < grids; i++)
		{
			int spread = 0;
			/* damage decreases slower with wide radius for spread effects */
			if (pflg & (PROJO_SPRED)) spread = rad + 1;
			
			/* Hack -- Notice new "dist" values */
			if (gm[dist+1] == i) dist++;

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Affect the monster in the grid */
			if (project_m(who, dist, y, x, dam, typ, spread))
            {
                notice = TRUE;
                hurtmon = TRUE;
            }

	        /* beam of destruction: radius-1 earthquake for each grid */
	        if (spellswitch == 26)
            {
               earthquake(y, x, 1, 0, 0, TRUE);
            }
		}
		
		/* exp drain kicks in whenever the PC uses magic that hurts a monster */
		if ((who < 0) && (p_ptr->exp_drain) && (hurtmon)) rxp_drain(33);

		/* Player affected one monster (without "jumping") */
		if ((who < 0) && (project_m_n == 1) && !(flg & (PROJECT_JUMP)))
		{
			/* Location */
			x = project_m_x;
			y = project_m_y;

			/* Track if possible */
			if (cave_m_idx[y][x] > 0)
			{
				monster_type *m_ptr = &mon_list[cave_m_idx[y][x]];

				/* Hack -- auto-recall */
				if (m_ptr->ml) monster_race_track(m_ptr->r_idx);

				/* Hack - auto-track */
				if (m_ptr->ml) health_track(cave_m_idx[y][x]);
			}
		}
	}


	/* Check player */
	if (flg & (PROJECT_KILL))
	{
		/* Start with "dist" of zero */
		dist = 0;

		/* Scan for player */
		for (i = 0; i < grids; i++)
		{
			bool breath = FALSE;
			/* currently there are no monster-cast spread effects, but I may add some */
			int spread = 0;
			/* damage decreases slower with wide radius for spread effects */
			if (pflg & (PROJO_SPRED)) spread = rad + 1;
			/* mark as breath weapon (as opposed to a ball spell) */
			if (pflg & (PROJO_BRETH)) breath = TRUE;
			
			/* Hack -- Notice new "dist" values */
			if (gm[dist+1] == i) dist++;

			/* Get the grid location */
			y = gy[i];
			x = gx[i];

			/* Affect the player */
			if (project_p(who, dist, y, x, dam, typ, spread, breath))
			{
				notice = TRUE;

				/* Only affect the player once */
				break;
			}
		}
	}

    /* reset range and spellswitch */
	range = 0;
	spellswitch = 0;

	/* Return "something was noticed" */
	return (notice);
}


/*
 *  Get a random location (Straight from the teleportation code)
 * mode == 1 is used to find a clear place to put rubble for GF_BOULDER
 * mode == 2 is used to find spots to make a puddle
 * mode == 3 is used to place demons summoned by TMD_WITCH
 * mode == 4 used for placing escorts as an alternative to scatter()
 * mode == 5 used for bringing RETURNS monsters back to life.
 * mode == 6 used for placing trees
 */
bool get_nearby(int dy, int dx, int *toy, int *tox, int mode)
{ 
    int dis, min, tries;
	int i, x, y, d;
	bool look = TRUE;

	dis = 2;
	if (mode == 3) dis = 19 + randint(1 + ((goodluck+1)/2));
	if (mode == 4) dis = 4;
	if ((mode == 5) || (mode == 6)) dis = 3;

	/* Initialize */
	y = dy;
	x = dx;

	/* try a random adjacent space first when making a puddle */
	if (mode == 2)
	{
		bool okay = TRUE;
		switch (randint(4))
		{
			case 1: y -= 1; break;
			case 2: y += 1; break;
			case 3: x -= 1; break;
			case 4: x += 1; break;
		}
		/* Ignore illegal locations */
		if (!in_bounds_fully(y, x)) okay = FALSE;

		/* is it okay? */
		if (okay)
		{
			/* allow monsters & objects to be in a puddle */
			if (!cave_floor_bold(y, x)) okay = FALSE;
			/* don't flood the stairs (but can flood traps) */
			if ((cave_feat[y][x] == FEAT_LESS) || (cave_feat[y][x] == FEAT_MORE))
			    okay = FALSE;
		}
		if (okay) /* (still) */
		{
			/* found a destination (no need to look any further) */
			(*toy) = y;
			(*tox) = x;
			return TRUE;
		}
	}

	/* Minimum distance */
	min = dis / 2;
	if (dis < 6) min = 1;

 	  /* Look until done */
	  while (look)
	  {
		/* Verify max distance */
		if (dis > 200) dis = 200;
		
		if (dis < 5) tries = 200;
		else tries = 300;

		/* Try several locations */
		for (i = 0; i < tries; i++)
		{
			/* Pick a (possibly illegal) location */
			while (1)
			{
				y = rand_spread(dy, dis);
				x = rand_spread(dx, dis);
				d = distance(dy, dx, y, x);
				if ((d >= min) && (d <= dis)) break;
			}

			/* Ignore illegal locations */
			if (!in_bounds_fully(y, x)) continue;

			/* Require floor space */
			if (mode == 2)
			{
				/* allow monsters & objects to be in a puddle */
				if (!cave_floor_bold(y, x)) continue;
    			/* don't flood the stairs (but can flood traps) */
				if (((cave_feat[y][x] == FEAT_LESS) || (cave_feat[y][x] == FEAT_MORE))) 
					continue;
			}
			else if ((mode == 3) || (mode == 4) || (mode == 5))
			{
				/* finding a place for a monster */
                if (!cave_can_occupy_bold(y, x)) continue;
            }
			/* Require empty floor space */
			else
			{
				if (!cave_naked_bold(y, x)) continue;
			}
            
			/* NONMONSTERs shouldn't get in the way */
			if (mode == 6)
			{
				int mtspaces = next_to_floor(y, x);
				if (mtspaces < 3) continue;
				/* between two walls means it's probably in the way */
				if (possible_doorway(y, x, TRUE)) continue;
			}

			/* This grid looks good */
			look = FALSE;

			/* Stop looking */
			break;
		}

		/* Increase the maximum distance */
		if ((dis < 50) && (mode == 3)) dis *= 2;
		else if (mode == 4) return FALSE;
		else if ((mode == 5) && (dis < 25)) dis += 3;
		else if ((dis < 5) && (randint(100) < 75)) dis += 2;
		else if ((dis < 10) && (mode == 5)) dis += 2;
		else /* fail */ return FALSE;

		/* Decrease the minimum distance */
		if (min > 1) min = min / 2;
      }

	  /* found a destination */
      (*toy) = y;
      (*tox) = x;
      return TRUE;
}
