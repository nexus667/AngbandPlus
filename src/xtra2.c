/* File: xtra2.c */

/* Purpose: effects of various "objects" */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#include "angband.h"

#define REWARD_CHANCE 10


/*
 * Advance experience levels and print experience
 */
void check_experience(void)
{
	int		i;
	bool level_reward = FALSE;
	bool level_mutation = FALSE;


	/* Note current level */
	i = p_ptr->lev;

	/* Hack -- lower limit */
	if (p_ptr->exp < 0) p_ptr->exp = 0;

	/* Hack -- lower limit */
	if (p_ptr->max_exp < 0) p_ptr->max_exp = 0;

	/* Hack -- upper limit */
	if (p_ptr->exp > PY_MAX_EXP) p_ptr->exp = PY_MAX_EXP;

	/* Hack -- upper limit */
	if (p_ptr->max_exp > PY_MAX_EXP) p_ptr->max_exp = PY_MAX_EXP;

	/* Hack -- maintain "max" experience */
	if (p_ptr->exp > p_ptr->max_exp) p_ptr->max_exp = p_ptr->exp;

	/* Redraw experience */
	p_ptr->redraw |= (PR_EXP);

	/* Handle stuff */
	handle_stuff();


	/* Lose levels while possible */
	while ((p_ptr->lev > 1) &&
	       (p_ptr->exp < (player_exp[p_ptr->lev - 2] * p_ptr->expfact / 100L)))
	{
		/* Lose a level */
		p_ptr->lev--;
		lite_spot(py, px);

		/* Update some stuff */
		p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);

		/* Redraw some stuff */
		p_ptr->redraw |= (PR_LEV | PR_TITLE);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER);

		/* Handle stuff */
		handle_stuff();
	}


	/* Gain levels while possible */
	while ((p_ptr->lev < PY_MAX_LEVEL) &&
	       (p_ptr->exp >= (player_exp[p_ptr->lev-1] * p_ptr->expfact / 100L)))
	{
		/* Gain a level */
		p_ptr->lev++;
		lite_spot(py, px);

		/* Save the highest level */
		if (p_ptr->lev > p_ptr->max_plv)
		{
			p_ptr->max_plv = p_ptr->lev;

			if ((p_ptr->pclass == CLASS_CHAOS_WARRIOR) ||
			    (p_ptr->muta2 & MUT2_CHAOS_GIFT))
			{
				level_reward = TRUE;
			}
			if (p_ptr->prace == RACE_BEASTMAN)
			{
				if (randint(5) == 1) level_mutation = TRUE;
			}
		}

		/* Sound */
		sound(SOUND_LEVEL);

		/* Message */
		msg_format("Welcome to level %d.", p_ptr->lev);

		/* Update some stuff */
		p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);

		/* Redraw some stuff */
		p_ptr->redraw |= (PR_LEV | PR_TITLE);

		/* Window stuff */
		p_ptr->window |= (PW_PLAYER | PW_SPELL);

		/* Handle stuff */
		handle_stuff();

		if (level_reward)
		{
			gain_level_reward(0);
			level_reward = FALSE;
		}

		if (level_mutation)
		{
			msg_print("You feel different...");
			(void)gain_random_mutation(0);
			level_mutation = FALSE;
		}
	}
}


/*
 * Hack -- Return the "automatic coin type" of a monster race
 * Used to allocate proper treasure when "Creeping coins" die
 *
 * XXX XXX XXX Note the use of actual "monster names"
 */
static int get_coin_type(monster_race *r_ptr)
{
	cptr name = (r_name + r_ptr->name);

	/* Analyze "coin" monsters */
	if (r_ptr->d_char == '$')
	{
		/* Look for textual clues */
		if (strstr(name, " copper ")) return (2);
		if (strstr(name, " silver ")) return (5);
		if (strstr(name, " gold ")) return (10);
		if (strstr(name, " mithril ")) return (16);
		if (strstr(name, " adamantite ")) return (17);

		/* Look for textual clues */
		if (strstr(name, "Copper ")) return (2);
		if (strstr(name, "Silver ")) return (5);
		if (strstr(name, "Gold ")) return (10);
		if (strstr(name, "Mithril ")) return (16);
		if (strstr(name, "Adamantite ")) return (17);
	}

	/* Assume nothing */
	return (0);
}


/*
 * Handle the "death" of a monster.
 *
 * Disperse treasures centered at the monster location based on the
 * various flags contained in the monster flags fields.
 *
 * Check for "Quest" completion when a quest monster is killed.
 *
 * Note that only the player can induce "monster_death()" on Uniques.
 * Thus (for now) all Quest monsters should be Uniques.
 *
 * Note that monsters can now carry objects, and when a monster dies,
 * it drops all of its objects, which may disappear in crowded rooms.
 */
void monster_death(int m_idx)
{
	int i, j, y, x, ny, nx, i2, j2;

	int dump_item = 0;
	int dump_gold = 0;

	int number_mon;

	int number = 0;

	monster_type *m_ptr = &m_list[m_idx];

	monster_race *r_ptr = &r_info[m_ptr->r_idx];

	bool visible = (m_ptr->ml || (r_ptr->flags1 & RF1_UNIQUE));

	bool good = (r_ptr->flags1 & RF1_DROP_GOOD) ? TRUE : FALSE;
	bool great = (r_ptr->flags1 & RF1_DROP_GREAT) ? TRUE : FALSE;

	bool do_gold = (!(r_ptr->flags1 & RF1_ONLY_ITEM));
	bool do_item = (!(r_ptr->flags1 & RF1_ONLY_GOLD));
	bool cloned = FALSE;
	bool create_stairs = FALSE;
	bool reward = FALSE;
	int force_coin = get_coin_type(r_ptr);

	int quest_num;

	object_type forge;
	object_type *q_ptr;


	/* Get the location */
	y = m_ptr->fy;
	x = m_ptr->fx;

	if (m_ptr->smart & SM_CLONED)
		cloned = TRUE;

	/* Let monsters explode! */
	for (i = 0; i < 4; i++)
	{
		if (r_ptr->blow[i].method == RBM_EXPLODE)
		{
			int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;
			int typ = GF_MISSILE;
			int d_dice = r_ptr->blow[i].d_dice;
			int d_side = r_ptr->blow[i].d_side;
			int damage = damroll(d_dice, d_side);

			switch (r_ptr->blow[i].effect)
			{
				case RBE_HURT:      typ = GF_MISSILE; break;
				case RBE_POISON:    typ = GF_POIS; break;
				case RBE_UN_BONUS:  typ = GF_DISENCHANT; break;
				case RBE_UN_POWER:  typ = GF_MISSILE; break; /* ToDo: Apply the correct effects */
				case RBE_EAT_GOLD:  typ = GF_MISSILE; break;
				case RBE_EAT_ITEM:  typ = GF_MISSILE; break;
				case RBE_EAT_FOOD:  typ = GF_MISSILE; break;
				case RBE_EAT_LITE:  typ = GF_MISSILE; break;
				case RBE_ACID:      typ = GF_ACID; break;
				case RBE_ELEC:      typ = GF_ELEC; break;
				case RBE_FIRE:      typ = GF_FIRE; break;
				case RBE_COLD:      typ = GF_COLD; break;
				case RBE_BLIND:     typ = GF_MISSILE; break;
				case RBE_CONFUSE:   typ = GF_CONFUSION; break;
				case RBE_TERRIFY:   typ = GF_MISSILE; break;
				case RBE_PARALYZE:  typ = GF_MISSILE; break;
				case RBE_LOSE_STR:  typ = GF_MISSILE; break;
				case RBE_LOSE_DEX:  typ = GF_MISSILE; break;
				case RBE_LOSE_CON:  typ = GF_MISSILE; break;
				case RBE_LOSE_INT:  typ = GF_MISSILE; break;
				case RBE_LOSE_WIS:  typ = GF_MISSILE; break;
				case RBE_LOSE_CHR:  typ = GF_MISSILE; break;
				case RBE_LOSE_ALL:  typ = GF_MISSILE; break;
				case RBE_SHATTER:   typ = GF_ROCKET; break;
				case RBE_EXP_10:    typ = GF_MISSILE; break;
				case RBE_EXP_20:    typ = GF_MISSILE; break;
				case RBE_EXP_40:    typ = GF_MISSILE; break;
				case RBE_EXP_80:    typ = GF_MISSILE; break;
				case RBE_DISEASE:   typ = GF_POIS; break;
				case RBE_TIME:      typ = GF_TIME; break;
			}

			project(m_idx, 3, y, x, damage, typ, flg);
			break;
		}
	}

	/* Inside a quest */
	quest_num = p_ptr->inside_quest;

	/* Search for an active quest on this dungeon level */
	if (!quest_num)
	{
		for (i = max_quests - 1; i > 0; i--)
		{
			/* Quest is not active */
			if (quest[i].status != QUEST_STATUS_TAKEN)
				continue;

			/* Quest is not a dungeon quest */
			if (quest[i].flags & QUEST_FLAG_PRESET)
				continue;

			/* Quest is not on this level */
			if ((quest[i].level != dun_level) &&
			    (quest[i].type != QUEST_TYPE_KILL_ANY_LEVEL))
				continue;

			/* Not a "kill monster" quest */
			if ((quest[i].type == QUEST_TYPE_FIND_ARTIFACT) ||
			    (quest[i].type == QUEST_TYPE_FIND_EXIT))
				continue;

			/* Interesting quest */
			if ((quest[i].type == QUEST_TYPE_KILL_NUMBER) ||
			    (quest[i].type == QUEST_TYPE_KILL_ALL))
				break;

			/* Interesting quest */
			if (((quest[i].type == QUEST_TYPE_KILL_LEVEL) ||
			     (quest[i].type == QUEST_TYPE_KILL_ANY_LEVEL) ||
			     (quest[i].type == QUEST_TYPE_RANDOM)) &&
			     (quest[i].r_idx == m_ptr->r_idx))
				break;
		}

		quest_num = i;
	}

	/* Handle the current quest */
	if (quest_num && (quest[quest_num].status == QUEST_STATUS_TAKEN))
	{
		/* Current quest */
		i = quest_num;

		switch (quest[i].type)
		{
			case QUEST_TYPE_KILL_NUMBER:
			{
				quest[i].cur_num++;

				if (quest[i].cur_num >= quest[i].num_mon)
				{
					/* completed quest */
					quest[i].status = QUEST_STATUS_COMPLETED;

					if (!(quest[i].flags & QUEST_FLAG_SILENT))
					{
						msg_print("You just completed your quest!");
						msg_print(NULL);
					}

					quest[i].cur_num = 0;
				}
				break;
			}
			case QUEST_TYPE_KILL_ALL:
			{
				number_mon = 0;

				/* Count all hostile monsters */
				for (i2 = 0; i2 < cur_wid; ++i2)
					for (j2 = 0; j2 < cur_hgt; j2++)
						if (cave[j2][i2].m_idx > 0)
							if (is_hostile(&m_list[cave[j2][i2].m_idx]))
								number_mon++;

				if ((number_mon - 1) == 0)
				{
					/* completed */
					if (quest[i].flags & QUEST_FLAG_SILENT)
					{
						quest[i].status = QUEST_STATUS_FINISHED;
					}
					else
					{
						quest[i].status = QUEST_STATUS_COMPLETED;
						msg_print("You just completed your quest!");
						msg_print(NULL);
					}
				}
				break;
			}
			case QUEST_TYPE_KILL_LEVEL:
			case QUEST_TYPE_RANDOM:
			{
				/* Only count valid monsters */
				if (quest[i].r_idx != m_ptr->r_idx)
					break;

				quest[i].cur_num++;

				if (quest[i].cur_num >= quest[i].max_num)
				{
					/* completed quest */
					quest[i].status = QUEST_STATUS_COMPLETED;
					if (!p_ptr->inside_quest)
						create_stairs = TRUE;

					if (!quest[i].flags & QUEST_FLAG_SILENT)
					{
						msg_print("You just completed your quest!");
						msg_print(NULL);
					}

					/* Finish the two main quests without rewarding */
					if ((i == QUEST_OBERON) || (i == QUEST_SERPENT))
					{
						quest[i].status = QUEST_STATUS_FINISHED;
					}

					if (quest[i].type == QUEST_TYPE_RANDOM)
					{
						reward = TRUE;
						quest[i].status = QUEST_STATUS_FINISHED;
					}
				}
				break;
			}
			case QUEST_TYPE_KILL_ANY_LEVEL:
			{
				quest[i].cur_num++;
				if (quest[i].cur_num >= quest[i].max_num)
				{
					 /* completed quest */
					quest[i].status = QUEST_STATUS_COMPLETED;

					if (!quest[i].flags & QUEST_FLAG_SILENT)
					{
						msg_print("You just completed your quest!");
						msg_print(NULL);
					}
					quest[i].cur_num = 0;
				}
				break;
			}
		}
	}

	/* Create a magical staircase */
	if (create_stairs)
	{
		/* Stagger around */
		while (cave_perma_bold(y, x) || cave[y][x].o_idx)
		{
			/* Pick a location */
			scatter(&ny, &nx, y, x, 1, 0);

			/* Stagger */
			y = ny; x = nx;
		}

		/* Explain the staircase */
		msg_print("A magical staircase appears...");

		/* Create stairs down */
		cave_set_feat(y, x, FEAT_MORE);

		/* Remember to update everything */
		p_ptr->update |= (PU_VIEW | PU_LITE | PU_FLOW | PU_MONSTERS);
	}

	/* Drop objects being carried */
	monster_drop_carried_objects(m_ptr);

	/* Mega^2-hack -- destroying the Stormbringer gives it us! */
	if (strstr((r_name + r_ptr->name), "Stormbringer"))
	{
		/* Get local object */
		q_ptr = &forge;

		/* Prepare to make the Stormbringer */
		object_prep(q_ptr, lookup_kind(TV_SWORD, SV_BLADE_OF_CHAOS));

		/* Mega-Hack -- Name the sword  */
		q_ptr->art_name = quark_add("'Stormbringer'");
		q_ptr->to_h = 16;
		q_ptr->to_d = 16;
		q_ptr->ds = 6;
		q_ptr->dd = 6;
		q_ptr->pval = 2;

		q_ptr->art_flags1 |= (TR1_VAMPIRIC | TR1_STR | TR1_CON);
		q_ptr->art_flags2 |= (TR2_FREE_ACT | TR2_HOLD_LIFE |
		                      TR2_RES_NEXUS | TR2_RES_CHAOS | TR2_RES_NETHER |
		                      TR2_RES_CONF); /* No longer resist_disen */
		q_ptr->art_flags3 |= (TR3_IGNORE_ACID | TR3_IGNORE_ELEC |
		                      TR3_IGNORE_FIRE | TR3_IGNORE_COLD);

		/* Munchkin option -- Prfnoff */
		if (munchkin_weapon)
		{
			q_ptr->art_flags1 |= (TR1_BLOWS);
		}

		/* Just to be sure */
		q_ptr->art_flags3 |= TR3_NO_TELE; /* How's that for a downside? */

		/* For game balance... */
		q_ptr->art_flags3 |= (TR3_CURSED | TR3_HEAVY_CURSE);
		q_ptr->ident |= IDENT_CURSED;

		if (randint(2) == 1)
			q_ptr->art_flags3 |= (TR3_DRAIN_EXP);
		else
			q_ptr->art_flags3 |= (TR3_AGGRAVATE);

#ifdef USE_SCRIPT
		q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

		/* Drop it in the dungeon */
		(void)drop_near(q_ptr, -1, y, x);
	}

	/*
	 * Mega^3-hack: killing a 'Warrior of the Dawn' is likely to
	 * spawn another in the fallen one's place!
	 */
	else if (strstr((r_name + r_ptr->name), "the Dawn"))
	{
		if (randint(20) != 13)
		{
			int wy = py, wx = px;
			int attempts = 100;

			do
			{
				scatter(&wy, &wx, py, px, 20, 0);
			}
			while (!(in_bounds(wy, wx) && cave_floor_bold(wy, wx)) && --attempts);

			if (attempts > 0)
			{
				if (summon_specific(wy, wx, 100, SUMMON_DAWN, FALSE, is_friendly(m_ptr), is_pet(m_ptr)))
				{
					if (player_can_see_bold(wy, wx))
						msg_print("A new warrior steps forth!");
				}
			}
		}
	}

	/* Pink horrors are replaced with 2 Blue horrors */
	else if (strstr((r_name + r_ptr->name), "ink horror"))
	{
		bool notice = FALSE;

		for (i = 0; i < 2; i++)
		{
			int wy = y, wx = x;

			if (summon_specific(wy, wx, 100, SUMMON_BLUE_HORROR, FALSE, is_friendly(m_ptr), is_pet(m_ptr)))
			{
				if (player_can_see_bold(wy, wx))
					notice = TRUE;
			}
		}

		if (notice)
			msg_print("The Pink horror divides!");
	}

	/* One more ultra-hack: An Unmaker goes out with a big bang! */
	else if (strstr((r_name + r_ptr->name), "Unmaker"))
	{
		int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;
		(void)project(m_idx, 6, y, x, 100, GF_CHAOS, flg);
	}

	/* Bloodletters of Khorne may drop a blade of chaos */
	else if (strstr((r_name + r_ptr->name), "Bloodletter") &&
	         (randint(100) < 15))
	{
		/* Get local object */
		q_ptr = &forge;

		/* Prepare to make a Blade of Chaos */
		object_prep(q_ptr, lookup_kind(TV_SWORD, SV_BLADE_OF_CHAOS));

		apply_magic(q_ptr, object_level, FALSE, FALSE, FALSE);

#ifdef USE_SCRIPT
		q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

		/* Drop it in the dungeon */
		(void)drop_near(q_ptr, -1, y, x);
	}

	/* Mega-Hack -- drop "winner" treasures */
	else if (r_ptr->flags1 & RF1_DROP_CHOSEN)
	{
		if (strstr((r_name + r_ptr->name), "Serpent of Chaos"))
		{
			/* Get local object */
			q_ptr = &forge;

			/* Mega-Hack -- Prepare to make "Grond" */
			object_prep(q_ptr, lookup_kind(TV_HAFTED, SV_GROND));

			/* Mega-Hack -- Mark this item as "Grond" */
			q_ptr->name1 = ART_GROND;

			/* Mega-Hack -- Actually create "Grond" */
			apply_magic(q_ptr, -1, TRUE, TRUE, TRUE);

#ifdef USE_SCRIPT
			q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

			/* Drop it in the dungeon */
			(void)drop_near(q_ptr, -1, y, x);

			/* Get local object */
			q_ptr = &forge;

			/* Mega-Hack -- Prepare to make "Morgoth" */
			object_prep(q_ptr, lookup_kind(TV_CROWN, SV_MORGOTH));

			/* Mega-Hack -- Mark this item as "Morgoth" */
			q_ptr->name1 = ART_MORGOTH;

			/* Mega-Hack -- Actually create "Morgoth" */
			apply_magic(q_ptr, -1, TRUE, TRUE, TRUE);

#ifdef USE_SCRIPT
			q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

			/* Drop it in the dungeon */
			(void)drop_near(q_ptr, -1, y, x);
		}
		else
		{
			byte a_idx = 0;
			int chance = 0;

			if (strstr((r_name + r_ptr->name), "Oberon,"))
			{
				if (randint(3) == 1)
				{
					a_idx = ART_THRAIN;
					chance = 33;
				}
				else
				{
					a_idx = ART_GONDOR;
					chance = 50;
				}
			}
			else if (strstr((r_name + r_ptr->name), "Barimen"))
			{
				a_idx = ART_THRAIN;
				chance = 20;
			}
			else if (strstr((r_name + r_ptr->name), "Sauron,"))
			{
				a_idx = ART_POWER;
				chance = 25;
			}
			else if (strstr((r_name + r_ptr->name), "Brand, "))
			{
				if (randint(3) != 1)
				{
					a_idx = ART_BRAND;
					chance = 25;
				}
				else
				{
					a_idx = ART_ANGUIREL;
					chance = 33;
				}
			}
			else if (strstr((r_name + r_ptr->name), "Corwin,"))
			{
				if (randint(3) != 1)
				{
					a_idx = ART_GRAYSWANDIR;
					chance = 33;
				}
				else
				{
					a_idx = ART_CORWIN;
					chance = 33;
				}
			}
			else if (strstr((r_name + r_ptr->name), "Saruman of"))
			{
				a_idx = ART_ELENDIL;
				chance = 20;
			}
			else if (strstr((r_name + r_ptr->name), "Fiona the"))
			{
				a_idx = ART_BELANGIL;
				chance = 50;
			}
			else if (strstr((r_name + r_ptr->name), "Julian, "))
			{
				a_idx = ART_CELEBORN;
				chance = 45;
			}
			else if (strstr((r_name + r_ptr->name), "Klings"))
			{
				a_idx = ART_OROME;
				chance = 40;
			}
			else if (strstr((r_name + r_ptr->name), "Groo"))
			{
				a_idx = ART_GROO;
				chance = 75;
			}
			else if (strstr((r_name + r_ptr->name), "Hagen,"))
			{
				a_idx = ART_NIMLOTH;
				chance = 66;
			}
			else if (strstr((r_name + r_ptr->name), "Caine,"))
			{
				a_idx = ART_ANGRIST;
				chance = 50;
			}

			if ((a_idx > 0) && ((randint(99) < chance) || (wizard)))
			{
				if (a_info[a_idx].cur_num == 0)
				{
					/* Create the artifact */
					create_named_art(a_idx, y, x);

					a_info[a_idx].cur_num = 1;
				}
			}
		}
	}

	/* Determine how much we can drop */
	if ((r_ptr->flags1 & RF1_DROP_60) && (rand_int(100) < 60)) number++;
	if ((r_ptr->flags1 & RF1_DROP_90) && (rand_int(100) < 90)) number++;
	if  (r_ptr->flags1 & RF1_DROP_1D2) number += damroll(1, 2);
	if  (r_ptr->flags1 & RF1_DROP_2D2) number += damroll(2, 2);
	if  (r_ptr->flags1 & RF1_DROP_3D2) number += damroll(3, 2);
	if  (r_ptr->flags1 & RF1_DROP_4D2) number += damroll(4, 2);

	if (cloned) number = 0; /* Clones drop no stuff */

	/* Hack -- handle creeping coins */
	coin_type = force_coin;

	/* Average dungeon and monster levels */
	object_level = (dun_level + r_ptr->level) / 2;

	/* Drop some objects */
	for (j = 0; j < number; j++)
	{
		/* Get local object */
		q_ptr = &forge;

		/* Wipe the object */
		object_wipe(q_ptr);

		/* Make Gold */
		if (do_gold && (!do_item || (rand_int(100) < 50)))
		{
			/* Make some gold */
			if (!make_gold(q_ptr)) continue;

			/* XXX XXX XXX */
			dump_gold++;
		}

		/* Make Object */
		else
		{
			/* Make an object */
			if (!make_object(q_ptr, good, great)) continue;

			/* XXX XXX XXX */
			dump_item++;
		}

#ifdef USE_SCRIPT
		q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

		/* Drop it in the dungeon */
		(void)drop_near(q_ptr, -1, y, x);
	}

	/* Reset the object level */
	object_level = base_level;

	/* Reset "coin" type */
	coin_type = 0;


	/* Take note of any dropped treasure */
	if (visible && (dump_item || dump_gold))
	{
		/* Take notes on treasure */
		lore_treasure(m_idx, dump_item, dump_gold);
	}

	/*
	 * Drop quest reward
	 */
	if (reward)
	{
		/* Get local object */
		q_ptr = &forge;

		/* Wipe the object */
		object_wipe(q_ptr);

		/* Make a great object */
		make_object(q_ptr, TRUE, TRUE);

#ifdef USE_SCRIPT
		q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

		/* Drop it in the dungeon */
		(void)drop_near(q_ptr, -1, y, x);
	}

#ifdef USE_SCRIPT
	kill_monster_callback(m_idx);
#endif /* USE_SCRIPT */

	/* Only process "Quest Monsters" */
	if (!(r_ptr->flags1 & RF1_QUESTOR)) return;

	/* Winner? */
	if (strstr((r_name + r_ptr->name), "Serpent of Chaos"))
	{
		/* Total winner */
		total_winner = TRUE;

		/* Redraw the "title" */
		p_ptr->redraw |= (PR_TITLE);

		/* Congratulations */
		msg_print("*** CONGRATULATIONS ***");
		msg_print("You have won the game!");
		msg_print("You may retire (commit suicide) when you are ready.");
	}
}




/*
 * Decreases monsters hit points, handling monster death.
 *
 * We return TRUE if the monster has been killed (and deleted).
 *
 * We announce monster death (using an optional "death message"
 * if given, and a otherwise a generic killed/destroyed message).
 *
 * Only "physical attacks" can induce the "You have slain" message.
 * Missile and Spell attacks will induce the "dies" message, or
 * various "specialized" messages.  Note that "You have destroyed"
 * and "is destroyed" are synonyms for "You have slain" and "dies".
 *
 * Hack -- unseen monsters yield "You have killed it." message.
 *
 * Added fear (DGK) and check whether to print fear messages -CWS
 *
 * Genericized name, sex, and capitilization -BEN-
 *
 * As always, the "ghost" processing is a total hack.
 *
 * Hack -- we "delay" fear messages by passing around a "fear" flag.
 *
 * XXX XXX XXX Consider decreasing monster experience over time, say,
 * by using "(m_exp * m_lev * (m_lev)) / (p_lev * (m_lev + n_killed))"
 * instead of simply "(m_exp * m_lev) / (p_lev)", to make the first
 * monster worth more than subsequent monsters.  This would also need
 * to induce changes in the monster recall code.
 */
bool mon_take_hit(int m_idx, int dam, bool *fear, cptr note)
{
	monster_type    *m_ptr = &m_list[m_idx];
	monster_race    *r_ptr = &r_info[m_ptr->r_idx];
	s32b            div, new_exp, new_exp_frac;


	/* Redraw (later) if needed */
	if (p_ptr->health_who == m_idx) p_ptr->redraw |= (PR_HEALTH);


	/* Wake it up */
	m_ptr->csleep = 0;

	/* Hurt it */
	m_ptr->hp -= dam;

	/* It is dead now */
	if (m_ptr->hp < 0)
	{
		char m_name[80];

		/* Extract monster name */
		monster_desc(m_name, m_ptr, 0);

		if ((r_ptr->flags3 & RF3_AMBERITE) && (randint(2) == 1))
		{
			int curses = 1 + randint(3);
			bool stop_ty = FALSE; /* Prfnoff */

			msg_format("%^s puts a terrible blood curse on you!", m_name);
			curse_equipment(100, 50);

			do
			{
				stop_ty = activate_ty_curse(stop_ty);
			}
			while (--curses);
		}

		if (r_ptr->flags2 & RF2_CAN_SPEAK)
		{
			char line_got[1024];

			if (speak_unique)
			{
				/* Dump a message */
				if (!get_rnd_line("mondeath.txt", m_ptr->r_idx, line_got))
					msg_format("%^s says: %s", m_name, line_got);
			}

			if ((randint(REWARD_CHANCE) == 1) &&
			    !(r_ptr->flags7 & RF7_FRIENDLY))
			{
				if (!get_rnd_line("crime.txt", m_ptr->r_idx, line_got))
				{
					int reward = 250 * (randint(10) + r_ptr->level - 5);

					/* Force 'good' values */
					if (reward > 32000) reward = 32000;
					else if (reward < 250) reward = 250;

					msg_format("There was a price on %s's head.", m_name);
					msg_format("%^s was wanted for %s", m_name, line_got);
					msg_format("You collect a reward of %d gold pieces.", reward);

					p_ptr->au += reward;
					p_ptr->redraw |= (PR_GOLD);
				}
			}
		}


		/* Make a sound */
		sound(SOUND_KILL);

		/* Death by Missile/Spell attack */
		if (note)
		{
			msg_format("%^s%s", m_name, note);
		}

		/* Death by physical attack -- invisible monster */
		else if (!m_ptr->ml)
		{
			msg_format("You have killed %s.", m_name);
		}

		/* Death by Physical attack -- non-living monster */
		else if (!monster_living(r_ptr))
		{
			msg_format("You have destroyed %s.", m_name);
		}

		/* Death by Physical attack -- living monster */
		else
		{
			msg_format("You have slain %s.", m_name);
		}

		/* Maximum player level */
		div = p_ptr->max_plv;

		/* Give some experience for the kill */
		new_exp = ((long)r_ptr->mexp * r_ptr->level) / div;

		/* Handle fractional experience */
		new_exp_frac = ((((long)r_ptr->mexp * r_ptr->level) % div)
		                * 0x10000L / div) + p_ptr->exp_frac;

		/* Keep track of experience */
		if (new_exp_frac >= 0x10000L)
		{
			new_exp++;
			p_ptr->exp_frac = (u16b)(new_exp_frac - 0x10000L);
		}
		else
		{
			p_ptr->exp_frac = (u16b)new_exp_frac;
		}

		/* Gain experience */
		gain_exp(new_exp);

		/* Generate treasure */
		monster_death(m_idx);

		/* When the player kills a Unique, it stays dead */
		if (r_ptr->flags1 & RF1_UNIQUE) r_ptr->max_num = 0;

		/* Recall even invisible uniques or winners */
		if (m_ptr->ml || (r_ptr->flags1 & RF1_UNIQUE))
		{
			/* Count kills this life */
			if (r_ptr->r_pkills < MAX_SHORT) r_ptr->r_pkills++;

			/* Count kills in all lives */
			if (r_ptr->r_tkills < MAX_SHORT) r_ptr->r_tkills++;

			/* Hack -- Auto-recall */
			monster_race_track(m_ptr->r_idx);
		}

		/* Delete the monster */
		delete_monster_idx(m_idx);

		/* Not afraid */
		(*fear) = FALSE;

		/* Monster is dead */
		return (TRUE);
	}


#ifdef ALLOW_FEAR

	/* Mega-Hack -- Pain cancels fear */
	if (m_ptr->monfear && (dam > 0))
	{
		int tmp = randint(dam);

		/* Cure a little fear */
		if (tmp < m_ptr->monfear)
		{
			/* Reduce fear */
			m_ptr->monfear -= tmp;
		}

		/* Cure all the fear */
		else
		{
			/* Cure fear */
			m_ptr->monfear = 0;

			/* No more fear */
			(*fear) = FALSE;
		}
	}

	/* Sometimes a monster gets scared by damage */
	if (!m_ptr->monfear && !(r_ptr->flags3 & (RF3_NO_FEAR)))
	{
		int percentage;

		/* Percentage of fully healthy */
		percentage = (100L * m_ptr->hp) / m_ptr->maxhp;

		/*
		 * Run (sometimes) if at 10% or less of max hit points,
		 * or (usually) when hit for half its current hit points
		 */
		if (((percentage <= 10) && (rand_int(10) < percentage)) ||
		    ((dam >= m_ptr->hp) && (rand_int(100) < 80)))
		{
			/* Hack -- note fear */
			(*fear) = TRUE;

			/* XXX XXX XXX Hack -- Add some timed fear */
			m_ptr->monfear = (randint(10) +
			                  (((dam >= m_ptr->hp) && (percentage > 7)) ?
			                   20 : ((11 - percentage) * 5)));
		}
	}

#endif


	/* Not dead yet */
	return (FALSE);
}



/*
 * Calculates current boundaries
 * Called below and from "do_cmd_locate()".
 */
void panel_bounds(void)
{
	panel_row_min = panel_row * (SCREEN_HGT / 2);
	panel_row_max = panel_row_min + SCREEN_HGT - 1;
	panel_row_prt = panel_row_min - 1;
	panel_col_min = panel_col * (SCREEN_WID / 2);
	panel_col_max = panel_col_min + SCREEN_WID - 1;
	panel_col_prt = panel_col_min - 13;
}


/*
 * Calculates current boundaries
 * Called below and from "do_cmd_locate()".
 * Modified for "centering player on screen" -- Prfnoff (scroll)
 */
void panel_bounds_center(void)
{
	panel_row = panel_row_min / (SCREEN_HGT / 2);
	panel_row_max = panel_row_min + SCREEN_HGT - 1;
	panel_row_prt = panel_row_min - 1;
	panel_col = panel_col_min / (SCREEN_WID / 2);
	panel_col_max = panel_col_min + SCREEN_WID - 1;
	panel_col_prt = panel_col_min - 13;
}


/*
 * Handle a request to change the current panel
 *
 * Return TRUE if the panel was changed.
 *
 * Also used in do_cmd_locate -- Prfnoff (scroll)
 */
bool change_panel(int dy, int dx)
{
	/* Apply the motion */
	int y = panel_row_min + dy * (SCREEN_HGT / 2);
	int x = panel_col_min + dx * (SCREEN_WID / 2);

	/* Verify the row */
	if (y > max_panel_rows * (SCREEN_HGT / 2)) y = max_panel_rows * (SCREEN_HGT / 2);
	else if (y < 0) y = 0;

	/* Verify the col */
	if (x > max_panel_cols * (SCREEN_WID / 2)) x = max_panel_cols * (SCREEN_WID / 2);
	else if (x < 0) x = 0;

	/* Handle "changes" */
	if ((y != panel_row_min) || (x != panel_col_min))
	{
		/* Save the new panel info */
		panel_row_min = y;
		panel_col_min = x;

		/* Recalculate the boundaries */
		panel_bounds_center();

		/* Update stuff */
		p_ptr->update |= (PU_MONSTERS);

		/* Redraw map */
		p_ptr->redraw |= (PR_MAP);

		/* Handle stuff */
		handle_stuff();

		/* Success */
		return (TRUE);
	}

	/* No change */
	return (FALSE);
}



/*
 * Given an row (y) and col (x), this routine detects when a move
 * off the screen has occurred and figures new borders. -RAK-
 *
 * "Update" forces a "full update" to take place.
 *
 * The map is reprinted if necessary, and "TRUE" is returned.
 */
void verify_panel(void)
{
	int y = py;
	int x = px;

	/* Center on player -- Prfnoff (scroll) */
	if (center_player && (!avoid_center || !running))
	{
		int prow_min;
		int pcol_min;

		int max_prow_min = max_panel_rows * (SCREEN_HGT / 2);
		int max_pcol_min = max_panel_cols * (SCREEN_WID / 2);

		/* Center vertically */
		prow_min = y - SCREEN_HGT / 2;
		if (prow_min > max_prow_min) prow_min = max_prow_min;
		else if (prow_min < 0) prow_min = 0;

		/* Center horizontally */
		pcol_min = x - SCREEN_WID / 2;
		if (pcol_min > max_pcol_min) pcol_min = max_pcol_min;
		else if (pcol_min < 0) pcol_min = 0;

		/* Check for "no change" */
		if ((prow_min == panel_row_min) && (pcol_min == panel_col_min)) return;

		/* Save the new panel info */
		panel_row_min = prow_min;
		panel_col_min = pcol_min;

		/* Recalculate the boundaries */
		panel_bounds_center();
	}

	else
  	{
		int prow = panel_row;
		int pcol = panel_col;

		/* Scroll screen when 2 grids from top/bottom edge */
		if ((y < panel_row_min + 2) || (y > panel_row_max - 2))
		{
			prow = ((y - SCREEN_HGT / 4) / (SCREEN_HGT / 2));
			if (prow > max_panel_rows) prow = max_panel_rows;
			else if (prow < 0) prow = 0;
		}

		/* Scroll screen when 4 grids from left/right edge */
		if ((x < panel_col_min + 4) || (x > panel_col_max - 4))
		{
			pcol = ((x - SCREEN_WID / 4) / (SCREEN_WID / 2));
			if (pcol > max_panel_cols) pcol = max_panel_cols;
			else if (pcol < 0) pcol = 0;
		}

		/* Check for "no change" */
		if ((prow == panel_row) && (pcol == panel_col)) return;

		/* Hack -- optional disturb on "panel change" */
		if (disturb_panel && !center_player) disturb(0, 0); /* Prfnoff (scroll) */

		/* Save the new panel info */
		panel_row = prow;
		panel_col = pcol;

		/* Recalculate the boundaries */
		panel_bounds();
	}

	/* Update stuff */
	p_ptr->update |= (PU_MONSTERS);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD | PW_DUNGEON);
}



/*
 * Monster health description
 */
cptr look_mon_desc(int m_idx)
{
	monster_type *m_ptr = &m_list[m_idx];
	monster_race *r_ptr = &r_info[m_ptr->r_idx];
	bool         living;
	int          perc;


	/* Determine if the monster is "living" */
	living = monster_living(r_ptr);


	/* Healthy monsters */
	if (m_ptr->hp >= m_ptr->maxhp)
	{
		/* No damage */
		return (living ? "unhurt" : "undamaged");
	}


	/* Calculate a health "percentage" */
	perc = 100L * m_ptr->hp / m_ptr->maxhp;

	if (perc >= 60)
	{
		return (living ? "somewhat wounded" : "somewhat damaged");
	}

	if (perc >= 25)
	{
		return (living ? "wounded" : "damaged");
	}

	if (perc >= 10)
	{
		return (living ? "badly wounded" : "badly damaged");
	}

	return (living ? "almost dead" : "almost destroyed");
}



/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort_aux(vptr u, vptr v, int p, int q)
{
	int z, a, b;

	/* Done sort */
	if (p >= q) return;

	/* Pivot */
	z = p;

	/* Begin */
	a = p;
	b = q;

	/* Partition */
	while (TRUE)
	{
		/* Slide i2 */
		while (!(*ang_sort_comp)(u, v, b, z)) b--;

		/* Slide i1 */
		while (!(*ang_sort_comp)(u, v, z, a)) a++;

		/* Done partition */
		if (a >= b) break;

		/* Swap */
		(*ang_sort_swap)(u, v, a, b);

		/* Advance */
		a++, b--;
	}

	/* Recurse left side */
	ang_sort_aux(u, v, p, b);

	/* Recurse right side */
	ang_sort_aux(u, v, b+1, q);
}


/*
 * Angband sorting algorithm -- quick sort in place
 *
 * Note that the details of the data we are sorting is hidden,
 * and we rely on the "ang_sort_comp()" and "ang_sort_swap()"
 * function hooks to interact with the data, which is given as
 * two pointers, and which may have any user-defined form.
 */
void ang_sort(vptr u, vptr v, int n)
{
	/* Sort the array */
	ang_sort_aux(u, v, 0, n-1);
}



/*** Targetting Code ***/


/*
 * Determine is a monster makes a reasonable target
 *
 * The concept of "targetting" was stolen from "Morgul" (?)
 *
 * The player can target any location, or any "target-able" monster.
 *
 * Currently, a monster is "target_able" if it is visible, and if
 * the player can hit it with a projection, and the player is not
 * hallucinating.  This allows use of "use closest target" macros.
 *
 * Future versions may restrict the ability to target "trappers"
 * and "mimics", but the semantics is a little bit weird.
 */
bool target_able(int m_idx)
{
	monster_type *m_ptr = &m_list[m_idx];

	/* Monster must be alive */
	if (!m_ptr->r_idx) return (FALSE);

	/* Monster must be visible */
	if (!m_ptr->ml) return (FALSE);

	/* Monster must be projectable */
	if (!projectable(py, px, m_ptr->fy, m_ptr->fx)) return (FALSE);

	/* Hack -- no targeting hallucinations */
	if (p_ptr->image) return (FALSE);

	/* XXX XXX XXX Hack -- Never target trappers */
	/* if (CLEAR_ATTR && (CLEAR_CHAR)) return (FALSE); */

	/* Assume okay */
	return (TRUE);
}




/*
 * Update (if necessary) and verify (if possible) the target.
 *
 * We return TRUE if the target is "okay" and FALSE otherwise.
 */
bool target_okay(void)
{
	/* Accept stationary targets */
	if (target_who < 0) return (TRUE);

	/* Check moving targets */
	if (target_who > 0)
	{
		/* Accept reasonable targets */
		if (target_able(target_who))
		{
			monster_type *m_ptr = &m_list[target_who];

			/* Acquire monster location */
			target_row = m_ptr->fy;
			target_col = m_ptr->fx;

			/* Good target */
			return (TRUE);
		}
	}

	/* Assume no target */
	return (FALSE);
}



/*
 * Sorting hook -- comp function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by double-distance to the player.
 */
static bool ang_sort_comp_distance(vptr u, vptr v, int a, int b)
{
	byte *x = (byte*)(u);
	byte *y = (byte*)(v);

	int da, db, kx, ky;

	/* Absolute distance components */
	kx = x[a]; kx -= px; kx = ABS(kx);
	ky = y[a]; ky -= py; ky = ABS(ky);

	/* Approximate Double Distance to the first point */
	da = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

	/* Absolute distance components */
	kx = x[b]; kx -= px; kx = ABS(kx);
	ky = y[b]; ky -= py; ky = ABS(ky);

	/* Approximate Double Distance to the first point */
	db = ((kx > ky) ? (kx + kx + ky) : (ky + ky + kx));

	/* Compare the distances */
	return (da <= db);
}


/*
 * Sorting hook -- swap function -- by "distance to player"
 *
 * We use "u" and "v" to point to arrays of "x" and "y" positions,
 * and sort the arrays by distance to the player.
 */
static void ang_sort_swap_distance(vptr u, vptr v, int a, int b)
{
	byte *x = (byte*)(u);
	byte *y = (byte*)(v);

	byte temp;

	/* Swap "x" */
	temp = x[a];
	x[a] = x[b];
	x[b] = temp;

	/* Swap "y" */
	temp = y[a];
	y[a] = y[b];
	y[b] = temp;
}



/*
 * Hack -- help "select" a location (see below)
 */
static s16b target_pick(int y1, int x1, int dy, int dx)
{
	int i, v;

	int x2, y2, x3, y3, x4, y4;

	int b_i = -1, b_v = 9999;


	/* Scan the locations */
	for (i = 0; i < temp_n; i++)
	{
		/* Point 2 */
		x2 = temp_x[i];
		y2 = temp_y[i];

		/* Directed distance */
		x3 = (x2 - x1);
		y3 = (y2 - y1);

		/* Verify quadrant */
		if (dx && (x3 * dx <= 0)) continue;
		if (dy && (y3 * dy <= 0)) continue;

		/* Absolute distance */
		x4 = ABS(x3);
		y4 = ABS(y3);

		/* Verify quadrant */
		if (dy && !dx && (x4 > y4)) continue;
		if (dx && !dy && (y4 > x4)) continue;

		/* Approximate Double Distance */
		v = ((x4 > y4) ? (x4 + x4 + y4) : (y4 + y4 + x4));

		/* XXX XXX XXX Penalize location */

		/* Track best */
		if ((b_i >= 0) && (v >= b_v)) continue;

		/* Track best */
		b_i = i; b_v = v;
	}

	/* Result */
	return (b_i);
}


/*
 * Hack -- determine if a given location is "interesting"
 */
static bool target_set_accept(int y, int x)
{
	cave_type *c_ptr;

	s16b this_o_idx, next_o_idx = 0;


	/* Player grid is always interesting */
	if ((y == py) && (x == px)) return (TRUE);


	/* Handle hallucination */
	if (p_ptr->image) return (FALSE);


	/* Examine the grid */
	c_ptr = &cave[y][x];

	/* Visible monsters */
	if (c_ptr->m_idx)
	{
		monster_type *m_ptr = &m_list[c_ptr->m_idx];

		/* Visible monsters */
		if (m_ptr->ml) return (TRUE);
	}

	/* Scan all objects in the grid */
	for (this_o_idx = c_ptr->o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		/* Acquire object */
		o_ptr = &o_list[this_o_idx];

		/* Acquire next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Memorized object */
		if (o_ptr->marked) return (TRUE);
	}

	/* Interesting memorized features */
	if (c_ptr->info & (CAVE_MARK))
	{
		/* Notice glyphs */
		if (c_ptr->feat == FEAT_GLYPH) return (TRUE);
		if (c_ptr->feat == FEAT_MINOR_GLYPH) return (TRUE);

		/* Notice the Pattern */
		if ((c_ptr->feat <= FEAT_PATTERN_XTRA2) &&
		    (c_ptr->feat >= FEAT_PATTERN_START))
			return (TRUE);

		/* Notice doors */
		if (c_ptr->feat == FEAT_OPEN) return (TRUE);
		if (c_ptr->feat == FEAT_BROKEN) return (TRUE);

		/* Notice stairs */
		if (c_ptr->feat == FEAT_LESS) return (TRUE);
		if (c_ptr->feat == FEAT_MORE) return (TRUE);

		/* Notice shops */
		if (cave_shop_grid(c_ptr)) return (TRUE);

		/* Notice buildings -KMW- */
		if (cave_bldg_grid(c_ptr)) return (TRUE);

		/* Notice traps */
		if (cave_trap_grid(c_ptr)) return (TRUE);

		/* Notice doors */
		if (cave_door_grid(c_ptr)) return (TRUE);

		/* Notice rubble */
		if (c_ptr->feat == FEAT_RUBBLE) return (TRUE);

		/* Notice veins with treasure */
		if (c_ptr->feat == FEAT_MAGMA_K) return (TRUE);
		if (c_ptr->feat == FEAT_QUARTZ_K) return (TRUE);

#if 0
		/* Notice water, lava, ... */
		if (c_ptr->feat == FEAT_DEEP_WATER) return (TRUE);
		if (c_ptr->feat == FEAT_SHAL_WATER) return (TRUE);
		if (c_ptr->feat == FEAT_DEEP_LAVA) return (TRUE);
		if (c_ptr->feat == FEAT_SHAL_LAVA) return (TRUE);
		if (c_ptr->feat == FEAT_DIRT) return (TRUE);
		if (c_ptr->feat == FEAT_GRASS) return (TRUE);
		if (c_ptr->feat == FEAT_DARK_PIT) return (TRUE);
		if (c_ptr->feat == FEAT_TREES) return (TRUE);
		if (c_ptr->feat == FEAT_MOUNTAIN) return (TRUE);
#endif

		/* Notice quest features */
		if (c_ptr->feat == FEAT_QUEST_ENTER) return (TRUE);
		if (c_ptr->feat == FEAT_QUEST_EXIT) return (TRUE);
		if (c_ptr->feat == FEAT_QUEST_DOWN) return (TRUE);
		if (c_ptr->feat == FEAT_QUEST_UP) return (TRUE);
	}

	/* Nope */
	return (FALSE);
}


/*
 * Prepare the "temp" array for "target_set"
 *
 * Return the number of target_able monsters in the set.
 */
static void target_set_prepare(int mode)
{
	int y, x;

	/* Reset "temp" array */
	temp_n = 0;

	/* Scan the current panel */
	for (y = panel_row_min; y <= panel_row_max; y++)
	{
		for (x = panel_col_min; x <= panel_col_max; x++)
		{
			cave_type *c_ptr = &cave[y][x];

			/* Require line of sight, unless "look" is "expanded" */
			if (!expand_look && !player_has_los_bold(y, x)) continue;

			/* Require "interesting" contents */
			if (!target_set_accept(y, x)) continue;

			/* Require target_able monsters for "TARGET_KILL" */
			if ((mode & (TARGET_KILL)) && !target_able(c_ptr->m_idx)) continue;

			/* Save the location */
			temp_x[temp_n] = x;
			temp_y[temp_n] = y;
			temp_n++;
		}
	}

	/* Set the sort hooks */
	ang_sort_comp = ang_sort_comp_distance;
	ang_sort_swap = ang_sort_swap_distance;

	/* Sort the positions */
	ang_sort(temp_x, temp_y, temp_n);
}


/*
 * Examine a grid, return a keypress.
 *
 * The "mode" argument contains the "TARGET_LOOK" bit flag, which
 * indicates that the "space" key should scan through the contents
 * of the grid, instead of simply returning immediately.  This lets
 * the "look" command get complete information, without making the
 * "target" command annoying.
 *
 * The "info" argument contains the "commands" which should be shown
 * inside the "[xxx]" text.  This string must never be empty, or grids
 * containing monsters will be displayed with an extra comma.
 *
 * Note that if a monster is in the grid, we update both the monster
 * recall info and the health bar info to track that monster.
 *
 * Eventually, we may allow multiple objects per grid, or objects
 * and terrain features in the same grid. XXX XXX XXX
 *
 * This function must handle blindness/hallucination.
 */
static int target_set_aux(int y, int x, int mode, cptr info)
{
	cave_type *c_ptr = &cave[y][x];

	s16b this_o_idx, next_o_idx = 0;

	cptr s1, s2, s3;

	bool boring;

	int feat;

	int query;

	char out_val[160];


	/* Repeat forever */
	while (1)
	{
		/* Paranoia */
		query = ' ';

		/* Assume boring */
		boring = TRUE;

		/* Default */
		s1 = "You see ";
		s2 = "";
		s3 = "";

		/* Hack -- under the player */
		if ((y == py) && (x == px))
		{
			/* Description */
			s1 = "You are ";

			/* Preposition */
			s2 = "on ";
		}


		/* Hack -- hallucination */
		if (p_ptr->image)
		{
			cptr name = "something strange";

			/* Display a message */
			sprintf(out_val, "%s%s%s%s [%s]", s1, s2, s3, name, info);
			prt(out_val, 0, 0);
			move_cursor_relative(y, x);
			query = inkey();

			/* Stop on everything but "return" */
			if ((query != '\r') && (query != '\n')) break;

			/* Repeat forever */
			continue;
		}


		/* Actual monsters */
		if (c_ptr->m_idx)
		{
			monster_type *m_ptr = &m_list[c_ptr->m_idx];
			monster_race *r_ptr = &r_info[m_ptr->r_idx];

			/* Visible */
			if (m_ptr->ml)
			{
				bool recall = FALSE;

				char m_name[80];

				/* Not boring */
				boring = FALSE;

				/* Get the monster name ("a kobold") */
				monster_desc(m_name, m_ptr, 0x08);

				/* Hack -- track this monster race */
				monster_race_track(m_ptr->r_idx);

				/* Hack -- health bar for this monster */
				health_track(c_ptr->m_idx);

				/* Hack -- handle stuff */
				handle_stuff();

				/* Interact */
				while (1)
				{
					/* Recall */
					if (recall)
					{
						/* Save */
						screen_save();

						/* Recall on screen */
						screen_roff(m_ptr->r_idx, 0);

						/* Hack -- Complete the prompt (again) */
						Term_addstr(-1, TERM_WHITE, format("  [r,%s]", info));

						/* Command */
						query = inkey();

						/* Restore */
						screen_load();
					}

					/* Normal */
					else
					{
						cptr attitude;

						if (is_pet(m_ptr))
							attitude = " (pet) ";
						else if (is_friendly(m_ptr))
							attitude = " (friendly) ";
						else
							attitude = " ";

						/* Describe, and prompt for recall */
						sprintf(out_val, "%s%s%s%s (%s)%s%s[r,%s]",
						    s1, s2, s3, m_name, look_mon_desc(c_ptr->m_idx),
						    (m_ptr->smart & SM_CLONED ? " (clone)": ""),
						    attitude, info);

						prt(out_val, 0, 0);

						/* Place cursor */
						move_cursor_relative(y, x);

						/* Command */
						query = inkey();
					}

					/* Normal commands */
					if (query != 'r') break;

					/* Toggle recall */
					recall = !recall;
				}

				/* Always stop at "normal" keys */
				if ((query != '\r') && (query != '\n') && (query != ' ')) break;

				/* Sometimes stop at "space" key */
				if ((query == ' ') && !(mode & (TARGET_LOOK))) break;

				/* Change the intro */
				s1 = "It is ";

				/* Hack -- take account of gender */
				if (r_ptr->flags1 & (RF1_FEMALE)) s1 = "She is ";
				else if (r_ptr->flags1 & (RF1_MALE)) s1 = "He is ";

				/* Use a preposition */
				s2 = "carrying ";

				/* Scan all objects being carried */
				for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
				{
					char o_name[80];

					object_type *o_ptr;

					/* Acquire object */
					o_ptr = &o_list[this_o_idx];

					/* Acquire next object */
					next_o_idx = o_ptr->next_o_idx;

					/* Obtain an object description */
					object_desc(o_name, o_ptr, TRUE, 3);

					/* Describe the object */
					sprintf(out_val, "%s%s%s%s [%s]", s1, s2, s3, o_name, info);
					prt(out_val, 0, 0);
					move_cursor_relative(y, x);
					query = inkey();

					/* Always stop at "normal" keys */
					if ((query != '\r') && (query != '\n') && (query != ' ')) break;

					/* Sometimes stop at "space" key */
					if ((query == ' ') && !(mode & (TARGET_LOOK))) break;

					/* Change the intro */
					s2 = "also carrying ";
				}

				/* Double break */
				if (this_o_idx) break;

				/* Use a preposition */
				s2 = "on ";
			}
		}


#ifdef ALLOW_EASY_FLOOR

		/* Scan all objects in the grid */
		if (easy_floor)
		{
			int floor_list[23], floor_num;

			if (scan_floor(floor_list, &floor_num, y, x, 0x02))
			{
				/* Not boring */
				boring = FALSE;

				while (1)
				{
					if (floor_num == 1)
					{
						char o_name[80];

						object_type *o_ptr;

						/* Acquire object */
						o_ptr = &o_list[floor_list[0]];

						/* Describe the object */
						object_desc(o_name, o_ptr, TRUE, 3);

						/* Message */
						sprintf(out_val, "%s%s%s%s [%s]",
							s1, s2, s3, o_name, info);
					}
					else
					{
						/* Message */
						sprintf(out_val, "%s%s%sa pile of %d items [l,%s]",
							s1, s2, s3, floor_num, info);
					}

					prt(out_val, 0, 0);
					move_cursor_relative(y, x);

					/* Command */
					query = inkey();

					/* Display list of items (query == "el", not "won") */
					if ((floor_num > 1) && (query == 'l'))
					{
						/* Save screen */
						screen_save();

						/* Display */
						show_floor(y, x);

						/* Prompt */
						prt("Hit any key to continue", 0, 0);

						/* Wait */
						(void) inkey();

						/* Load screen */
						screen_load();
					}
					else
					{
						/* Stop */
						break;
					}
				}

				/* Stop */
				break;
			}
		}

#endif /* ALLOW_EASY_FLOOR */


		/* Scan all objects in the grid */
		for (this_o_idx = c_ptr->o_idx; this_o_idx; this_o_idx = next_o_idx)
		{
			object_type *o_ptr;

			/* Acquire object */
			o_ptr = &o_list[this_o_idx];

			/* Acquire next object */
			next_o_idx = o_ptr->next_o_idx;

			/* Describe it */
			if (o_ptr->marked)
			{
				char o_name[80];

				/* Not boring */
				boring = FALSE;

				/* Obtain an object description */
				object_desc(o_name, o_ptr, TRUE, 3);

				/* Describe the object */
				sprintf(out_val, "%s%s%s%s [%s]", s1, s2, s3, o_name, info);
				prt(out_val, 0, 0);
				move_cursor_relative(y, x);
				query = inkey();

				/* Always stop at "normal" keys */
				if ((query != '\r') && (query != '\n') && (query != ' ')) break;

				/* Sometimes stop at "space" key */
				if ((query == ' ') && !(mode & TARGET_LOOK)) break;

				/* Change the intro */
				s1 = "It is ";

				/* Plurals */
				if (o_ptr->number != 1) s1 = "They are ";

				/* Preposition */
				s2 = "on ";
			}
		}

		/* Double break */
		if (this_o_idx) break;

		if (c_ptr->mimic)
		{
			feat = c_ptr->mimic;
		}
		else
		{
			feat = f_info[c_ptr->feat].mimic;
		}

		/* Require knowledge about grid, or ability to see grid */
		if (!(c_ptr->info & CAVE_MARK) && !player_can_see_bold(y, x))
		{
			/* Forget feature */
			feat = FEAT_NONE;
		}

		/* Terrain feature if needed */
		if (boring || (feat > FEAT_INVIS))
		{
			cptr name;

			/* Hack -- special handling for building doors */
			if ((feat >= FEAT_BLDG_HEAD) && (feat <= FEAT_BLDG_TAIL))
			{
				name = building[feat - FEAT_BLDG_HEAD].name;
			}
			else
			{
				name = f_name + f_info[feat].name;
			}

			/* Hack -- handle unknown grids */
			if (feat == FEAT_NONE) name = "unknown grid";

			/* Pick a prefix */
			if (*s2 && ((feat >= FEAT_MINOR_GLYPH) &&
			   (feat <= FEAT_PATTERN_XTRA2)))
			{
				s2 = "on ";
			}
			else if (*s2 && ((feat >= FEAT_DOOR_HEAD) &&
				(feat <= FEAT_PERM_SOLID)))
			{
				s2 = "in ";
			}

			/* Hack -- special introduction for store & building doors -KMW- */
			if (((feat >= FEAT_SHOP_HEAD) && (feat <= FEAT_SHOP_TAIL)) ||
			    ((feat >= FEAT_BLDG_HEAD) && (feat <= FEAT_BLDG_TAIL)))
			{
				s3 = "the entrance to the ";
			}
			else if ((feat == FEAT_FLOOR) || (feat == FEAT_DIRT))
			{
				s3 = "";
			}
			else
			{
				/* Pick proper indefinite article */
				s3 = (is_a_vowel(name[0])) ? "an " : "a ";
			}

			/* Display a message */
			sprintf(out_val, "%s%s%s%s [%s]", s1, s2, s3, name, info);
			prt(out_val, 0, 0);
			move_cursor_relative(y, x);
			query = inkey();

			/* Always stop at "normal" keys */
			if ((query != '\r') && (query != '\n') && (query != ' ')) break;
		}

		/* Stop on everything but "return" */
		if ((query != '\r') && (query != '\n')) break;
	}

	/* Keep going */
	return (query);
}




/*
 * Handle "target" and "look".
 *
 * Note that this code can be called from "get_aim_dir()".
 *
 * All locations must be on the current panel.  Consider the use of
 * "panel_bounds()" to allow "off-panel" targets, perhaps by using
 * some form of "scrolling" the map around the cursor.  XXX XXX XXX
 * That is, consider the possibility of "auto-scrolling" the screen
 * while the cursor moves around.  This may require changes in the
 * "update_mon()" code to allow "visibility" even if off panel, and
 * may require dynamic recalculation of the "temp" grid set.
 *
 * Hack -- targetting/observing an "outer border grid" may induce
 * problems, so this is not currently allowed.
 *
 * The player can use the direction keys to move among "interesting"
 * grids in a heuristic manner, or the "space", "+", and "-" keys to
 * move through the "interesting" grids in a sequential manner, or
 * can enter "location" mode, and use the direction keys to move one
 * grid at a time in any direction.  The "t" (set target) command will
 * only target a monster (as opposed to a location) if the monster is
 * target_able and the "interesting" mode is being used.
 *
 * The current grid is described using the "look" method above, and
 * a new command may be entered at any time, but note that if the
 * "TARGET_LOOK" bit flag is set (or if we are in "location" mode,
 * where "space" has no obvious meaning) then "space" will scan
 * through the description of the current grid until done, instead
 * of immediately jumping to the next "interesting" grid.  This
 * allows the "target" command to retain its old semantics.
 *
 * The "*", "+", and "-" keys may always be used to jump immediately
 * to the next (or previous) interesting grid, in the proper mode.
 *
 * The "return" key may always be used to scan through a complete
 * grid description (forever).
 *
 * This command will cancel any old target, even if used from
 * inside the "look" command.
 */
bool target_set(int mode)
{
	int		i, d, m, t, bd; /* Prfnoff -- scroll */
	int		y = py;
	int		x = px;

	bool	done = FALSE;

	bool	flag = TRUE;

	char	query;

	char	info[80];

	cave_type		*c_ptr;


	/* Cancel target */
	target_who = 0;


	/* Cancel tracking */
	/* health_track(0); */


	/* Prepare the "temp" array */
	target_set_prepare(mode);

	/* Start near the player */
	m = 0;

	/* Interact */
	while (!done)
	{
		/* Interesting grids */
		if (flag && temp_n)
		{
			y = temp_y[m];
			x = temp_x[m];

			/* Access */
			c_ptr = &cave[y][x];

			/* Allow target */
			if (target_able(c_ptr->m_idx))
			{
				strcpy(info, "q,t,p,o,+,-,<dir>");
			}

			/* Dis-allow target */
			else
			{
				strcpy(info, "q,p,o,+,-,<dir>");
			}

			/* Describe and Prompt */
			query = target_set_aux(y, x, mode, info);

			/* Cancel tracking */
			/* health_track(0); */

			/* Assume no "direction" */
			d = 0;

			/* Analyze */
			switch (query)
			{
				case ESCAPE:
				case 'q':
				{
					done = TRUE;
					break;
				}

				case 't':
				case '.':
				case '5':
				case '0':
				{
					if (target_able(c_ptr->m_idx))
					{
						health_track(c_ptr->m_idx);
						target_who = c_ptr->m_idx;
						target_row = y;
						target_col = x;
						done = TRUE;
					}
					else
					{
						bell();
					}
					break;
				}

				case ' ':
				case '*':
				case '+':
				{
					if (++m == temp_n)
					{
						m = 0;
						if (!expand_list) done = TRUE;
					}
					break;
				}

				case '-':
				{
					if (m-- == 0)
					{
						m = temp_n - 1;
						if (!expand_list) done = TRUE;
					}
					break;
				}

				case 'p':
				{
					/* Recenter the map around the player -- Prfnoff (scroll) */
					verify_panel();

					/* Update stuff */
					p_ptr->update |= (PU_MONSTERS);

					/* Redraw map */
					p_ptr->redraw |= (PR_MAP);

					/* Window stuff */
					p_ptr->window |= (PW_OVERHEAD);

					/* Handle stuff */
					handle_stuff();

					/* Recalculate interesting grids */
					target_set_prepare(mode);

					y = py;
					x = px;
				}

				case 'o':
				{
					flag = FALSE; /* Prfnoff (scroll) */
					break;
				}

				case 'm':
				{
					break;
				}

				default:
				{
					/* Extract the action (if any) */
					d = get_keymap_dir(query);

					if (!d) bell();
					break;
				}
			}

			/* Hack -- move around */
			if (d)
			{
				/* Modified to scroll to monster -- Prfnoff (scroll) */
				int y2 = panel_row_min;
				int x2 = panel_col_min;

				/* Find a new monster */
				i = target_pick(temp_y[m], temp_x[m], ddy[d], ddx[d]);

				/* Request to target past last interesting grid */
				while (flag && (i < 0))
				{
					/* Note the change */
					if (change_panel(ddy[d], ddx[d]))
					{
						int v = temp_y[m];
						int u = temp_x[m];

						/* Recalculate interesting grids */
						target_set_prepare(mode);

						/* Look at interesting grids */
						flag = TRUE;

						/* Find a new monster */
						i = target_pick(v, u, ddy[d], ddx[d]);

						/* Use that grid */
						if (i >= 0) m = i;
					}

					/* Nothing interesting */
					else
					{
						int dx = ddx[d];
						int dy = ddy[d];

						/* Restore previous position */
						panel_row_min = y2;
						panel_col_min = x2;
						panel_bounds_center();

						/* Update stuff */
						p_ptr->update |= (PU_MONSTERS);

						/* Redraw map */
						p_ptr->redraw |= (PR_MAP);

						/* Window stuff */
						p_ptr->window |= (PW_OVERHEAD);

						/* Handle stuff */
						handle_stuff();

						/* Recalculate interesting grids */
						target_set_prepare(mode);

						/* Look at boring grids */
						flag = FALSE;

						/* Move */
						x += dx;
						y += dy;

						/* Do not move horizontally if unnecessary */
						if (((x < panel_col_min + SCREEN_WID / 2) && (dx > 0)) ||
							((x > panel_col_min + SCREEN_WID / 2) && (dx < 0)))
						{
							dx = 0;
						}

						/* Do not move vertically if unnecessary */
						if (((y < panel_row_min + SCREEN_HGT / 2) && (dy > 0)) ||
							((y > panel_row_min + SCREEN_HGT / 2) && (dy < 0)))
						{
							dy = 0;
						}

						/* Apply the motion */
						if ((y >= panel_row_min+SCREEN_HGT) || (y < panel_row_min) ||
						    (x >= panel_col_min+SCREEN_WID) || (x < panel_col_min))
						{
							if (change_panel(dy, dx)) target_set_prepare(mode);
						}

						/* Slide into legality */
						if (x >= cur_wid-1) x = cur_wid - 2;
						else if (x <= 0) x = 1;

						/* Slide into legality */
						if (y >= cur_hgt-1) y = cur_hgt- 2;
						else if (y <= 0) y = 1;
					}
				}

				/* Use that grid */
				m = i;
			}
		}

		/* Arbitrary grids */
		else
		{
			/* Access */
			c_ptr = &cave[y][x];

			/* Default prompt */
			strcpy(info, "q,t,p,m,+,-,<dir>");

			/* Describe and Prompt (enable "TARGET_LOOK") */
			query = target_set_aux(y, x, mode | TARGET_LOOK, info);

			/* Cancel tracking */
			/* health_track(0); */

			/* Assume no direction */
			d = 0;

			/* Analyze the keypress */
			switch (query)
			{
				case ESCAPE:
				case 'q':
				{
					done = TRUE;
					break;
				}

				case 't':
				case '.':
				case '5':
				case '0':
				{
					target_who = -1;
					target_row = y;
					target_col = x;
					done = TRUE;
					break;
				}

				case ' ':
				case '*':
				case '+':
				case '-':
				{
					break;
				}

				case 'p':
				{
					/* Recenter the map around the player -- Prfnoff (scroll) */
					verify_panel();

					/* Update stuff */
					p_ptr->update |= (PU_MONSTERS);

					/* Redraw map */
					p_ptr->redraw |= (PR_MAP);

					/* Window stuff */
					p_ptr->window |= (PW_OVERHEAD);

					/* Handle stuff */
					handle_stuff();

					/* Recalculate interesting grids */
					target_set_prepare(mode);

					y = py;
					x = px;
				}

				case 'o':
				{
					break;
				}

				case 'm': /* Prfnoff -- scroll */
				{
					flag = TRUE;

					m = 0;
					bd = 999;

					/* Pick a nearby monster */
					for (i = 0; i < temp_n; i++)
					{
						t = distance(y, x, temp_y[i], temp_x[i]);

						/* Pick closest */
						if (t < bd)
						{
							m = i;
							bd = t;
						}
					}

					/* Nothing interesting */
					if (bd == 999) flag = FALSE;

					break;
				}

				default:
				{
					/* Extract the action (if any) */
					d = get_keymap_dir(query);

					if (!d) bell();
					break;
				}
			}

			/* Handle "direction" */
			if (d)
			{
				int dx = ddx[d];
				int dy = ddy[d];

				/* Move */
				x += dx;
				y += dy;

				/* Do not move horizontally if unnecessary */
				if (((x < panel_col_min + SCREEN_WID / 2) && (dx > 0)) ||
					((x > panel_col_min + SCREEN_WID / 2) && (dx < 0)))
				{
					dx = 0;
				}

				/* Do not move vertically if unnecessary */
				if (((y < panel_row_min + SCREEN_HGT / 2) && (dy > 0)) ||
					((y > panel_row_min + SCREEN_HGT / 2) && (dy < 0)))
				{
					dy = 0;
				}

				/* Apply the motion -- Prfnoff (scroll) */
				if ((y >= panel_row_min+SCREEN_HGT) || (y < panel_row_min) ||
				    (x >= panel_col_min+SCREEN_WID) || (x < panel_col_min))
				{
					if (change_panel(dy, dx)) target_set_prepare(mode);
				}

				/* Slide into legality */
				if (x >= cur_wid-1) x = cur_wid - 2;
				else if (x <= 0) x = 1;

				/* Slide into legality */
				if (y >= cur_hgt-1) y = cur_hgt- 2;
				else if (y <= 0) y = 1;
			}
		}
	}

	/* Forget */
	temp_n = 0;

	/* Clear the top line */
	prt("", 0, 0);

	/* Recenter the map around the player */
	verify_panel();

	/* Update stuff */
	p_ptr->update |= (PU_MONSTERS);

	/* Redraw map */
	p_ptr->redraw |= (PR_MAP);

	/* Window stuff */
	p_ptr->window |= (PW_OVERHEAD);

	/* Handle stuff */
	handle_stuff();

	/* Failure to set target */
	if (!target_who) return (FALSE);

	/* Success */
	return (TRUE);
}



/*
 * Get an "aiming direction" from the user.
 *
 * The "dir" is loaded with 1,2,3,4,6,7,8,9 for "actual direction", and
 * "0" for "current target", and "-1" for "entry aborted".
 *
 * Note that "Force Target", if set, will pre-empt user interaction,
 * if there is a usable target already set.
 *
 * Note that confusion over-rides any (explicit?) user choice.
 */
bool get_aim_dir(int *dp)
{
	int		dir;

	char	command;

	cptr	p;

#ifdef ALLOW_REPEAT /* TNB */

	if (repeat_pull(dp))
	{
		/* Confusion? */

		/* Verify */
		if (!(*dp == 5 && !target_okay()))
		{
			return (TRUE);
		}
	}

#endif /* ALLOW_REPEAT -- TNB */

	/* Initialize */
	(*dp) = 0;

	/* Global direction */
	dir = command_dir;

	/* Hack -- auto-target if requested */
	if (use_old_target && target_okay()) dir = 5;

	/* Ask until satisfied */
	while (!dir)
	{
		/* Choose a prompt */
		if (!target_okay())
		{
			p = "Direction ('*' to choose a target, Escape to cancel)? ";
		}
		else
		{
			p = "Direction ('5' for target, '*' to re-target, Escape to cancel)? ";
		}

		/* Get a command (or Cancel) */
		if (!get_com(p, &command)) break;

		/* Convert various keys to "standard" keys */
		switch (command)
		{
			/* Use current target */
			case 'T':
			case 't':
			case '.':
			case '5':
			case '0':
			{
				dir = 5;
				break;
			}

			/* Set new target */
			case '*':
			{
				if (target_set(TARGET_KILL)) dir = 5;
				break;
			}

			default:
			{
				/* Extract the action (if any) */
				dir = get_keymap_dir(command);

				break;
			}
		}

		/* Verify requested targets */
		if ((dir == 5) && !target_okay()) dir = 0;

		/* Error */
		if (!dir) bell();
	}

	/* No direction */
	if (!dir) return (FALSE);

	/* Save the direction */
	command_dir = dir;

	/* Check for confusion */
	if (p_ptr->confused)
	{
		/* XXX XXX XXX */
		/* Random direction */
		dir = ddd[rand_int(8)];
	}

	/* Notice confusion */
	if (command_dir != dir)
	{
		/* Warn the user */
		msg_print("You are confused.");
	}

	/* Save direction */
	(*dp) = dir;

#ifdef ALLOW_REPEAT /* TNB */

	repeat_push(dir);

#endif /* ALLOW_REPEAT -- TNB */

	/* A "valid" direction was entered */
	return (TRUE);
}



/*
 * Request a "movement" direction (1,2,3,4,6,7,8,9) from the user,
 * and place it into "command_dir", unless we already have one.
 *
 * This function should be used for all "repeatable" commands, such as
 * run, walk, open, close, bash, disarm, spike, tunnel, etc, as well
 * as all commands which must reference a grid adjacent to the player,
 * and which may not reference the grid under the player.  Note that,
 * for example, it is no longer possible to "disarm" or "open" chests
 * in the same grid as the player.
 *
 * Direction "5" is illegal and will (cleanly) abort the command.
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", to which "confusion" is applied.
 */
bool get_rep_dir(int *dp)
{
	int dir;

#ifdef ALLOW_REPEAT /* TNB */

	if (repeat_pull(dp))
	{
		return (TRUE);
	}

#endif /* ALLOW_REPEAT -- TNB */

	/* Initialize */
	(*dp) = 0;

	/* Global direction */
	dir = command_dir;

	/* Get a direction */
	while (!dir)
	{
		char ch;

		/* Get a command (or Cancel) */
		if (!get_com("Direction (Escape to cancel)? ", &ch)) break;

		/* Look up the direction */
		dir = get_keymap_dir(ch);

		/* Oops */
		if (!dir) bell();
	}

	/* Prevent weirdness */
	if (dir == 5) dir = 0;

	/* Aborted */
	if (!dir) return (FALSE);

	/* Save desired direction */
	command_dir = dir;

	/* Apply "confusion" */
	if (p_ptr->confused)
	{
		/* Standard confusion */
		if (rand_int(100) < 75)
		{
			/* Random direction */
			dir = ddd[rand_int(8)];
		}
	}

	/* Notice confusion */
	if (command_dir != dir)
	{
		/* Warn the user */
		msg_print("You are confused.");
	}

	/* Save direction */
	(*dp) = dir;

#ifdef ALLOW_REPEAT /* TNB */

	repeat_push(dir);

#endif /* ALLOW_REPEAT -- TNB */

	/* Success */
	return (TRUE);
}


int get_chaos_patron(void)
{
	return ((p_ptr->age + p_ptr->sc) % MAX_PATRON);
}


void gain_level_reward(int chosen_reward)
{
	object_type *q_ptr;
	object_type forge;
	char        wrath_reason[32] = "";
	int         nasty_chance = 6;
	int         dummy = 0, dummy2 = 0;
	int         type, effect;


	if (!chosen_reward)
	{
		if (multi_rew) return;
		else multi_rew = TRUE;
	}


	if (p_ptr->lev == 13) nasty_chance = 2;
	else if (!(p_ptr->lev % 13)) nasty_chance = 3;
	else if (!(p_ptr->lev % 14)) nasty_chance = 12;

	if (randint(nasty_chance) == 1)
		type = randint(20); /* Allow the 'nasty' effects */
	else
		type = randint(15) + 5; /* Or disallow them */

	if (type < 1) type = 1;
	if (type > 20) type = 20;
	type--;


	sprintf(wrath_reason, "the Wrath of %s",
		chaos_patrons[p_ptr->chaos_patron]);

	effect = chaos_rewards[p_ptr->chaos_patron][type];

	if ((randint(6) == 1) && !chosen_reward)
	{
		msg_format("%^s rewards you with a mutation!",
			chaos_patrons[p_ptr->chaos_patron]);
		(void)gain_random_mutation(0);
		return;
	}

	switch (chosen_reward ? chosen_reward : effect)
	{
		case REW_POLY_SLF:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou needst a new form, mortal!'");
			do_poly_self();
			break;
		case REW_GAIN_EXP:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Well done, mortal! Lead on!'");
			if (p_ptr->exp < PY_MAX_EXP)
			{
				s32b ee = (p_ptr->exp / 2) + 10;
				if (ee > 100000L) ee = 100000L;
				msg_print("You feel more experienced.");
				gain_exp(ee);
			}
			break;
		case REW_LOSE_EXP:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou didst not deserve that, slave.'");
			lose_exp(p_ptr->exp / 6);
			break;
		case REW_GOOD_OBJ:
			msg_format("The voice of %s whispers:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Use my gift wisely.'");
			acquirement(py, px, 1, FALSE, FALSE);
			break;
		case REW_GREA_OBJ:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Use my gift wisely.'");
			acquirement(py, px, 1, TRUE, FALSE);
			break;
		case REW_CHAOS_WP:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thy deed hath earned thee a worthy blade.'");
			/* Get local object */
			q_ptr = &forge;
			dummy = TV_SWORD;
			switch (randint(p_ptr->lev))
			{
				case 0: case 1:
					dummy2 = SV_DAGGER;
					break;
				case 2: case 3:
					dummy2 = SV_MAIN_GAUCHE;
					break;
				case 4:
					dummy2 = SV_TANTO;
					break;
				case 5: case 6:
					dummy2 = SV_RAPIER;
					break;
				case 7: case 8:
					dummy2 = SV_SMALL_SWORD;
					break;
				case 9: case 10:
					dummy2 = SV_BASILLARD;
					break;
				case 11: case 12: case 13:
					dummy2 = SV_SHORT_SWORD;
					break;
				case 14: case 15:
					dummy2 = SV_SABRE;
					break;
				case 16: case 17:
					dummy2 = SV_CUTLASS;
					break;
				case 18:
					dummy2 = SV_WAKIZASHI;
					break;
				case 19:
					dummy2 = SV_KHOPESH;
					break;
				case 20:
					dummy2 = SV_TULWAR;
					break;
				case 21:
					dummy2 = SV_BROAD_SWORD;
					break;
				case 22: case 23:
					dummy2 = SV_LONG_SWORD;
					break;
				case 24: case 25:
					dummy2 = SV_SCIMITAR;
					break;
				case 26:
					dummy2 = SV_NINJATO;
					break;
				case 27:
					dummy2 = SV_KATANA;
					break;
				case 28: case 29:
					dummy2 = SV_BASTARD_SWORD;
					break;
				case 30:
					dummy2 = SV_GREAT_SCIMITAR;
					break;
				case 31:
					dummy2 = SV_CLAYMORE;
					break;
				case 32:
					dummy2 = SV_ESPADON;
					break;
				case 33:
					dummy2 = SV_TWO_HANDED_SWORD;
					break;
				case 34:
					dummy2 = SV_FLAMBERGE;
					break;
				case 35:
					dummy2 = SV_NO_DACHI;
					break;
				case 36:
					dummy2 = SV_EXECUTIONERS_SWORD;
					break;
				case 37:
					dummy2 = SV_ZWEIHANDER;
					break;
				default:
					dummy2 = SV_BLADE_OF_CHAOS;
			}

			object_prep(q_ptr, lookup_kind(dummy, dummy2));
			q_ptr->to_h = 3 + randint(dun_level) % 10;
			q_ptr->to_d = 3 + randint(dun_level) % 10;
			random_resistance(q_ptr, FALSE, randint(34) + 4);
			q_ptr->name2 = EGO_CHAOTIC;

#ifdef USE_SCRIPT
			q_ptr->python = object_create_callback(q_ptr);
#endif /* USE_SCRIPT */

			/* Drop it in the dungeon */
			(void)drop_near(q_ptr, -1, py, px);
			break;
		case REW_GOOD_OBS:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thy deed hath earned thee a worthy reward.'");
			acquirement(py, px, randint(2) + 1, FALSE, FALSE);
			break;
		case REW_GREA_OBS:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Behold, mortal, how generously I reward thy loyalty.'");
			acquirement(py, px, randint(2) + 1, TRUE, FALSE);
			break;
		case REW_TY_CURSE:
			msg_format("The voice of %s thunders:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou art growing arrogant, mortal.'");
			(void)activate_ty_curse(FALSE); /* Prfnoff */
			break;
		case REW_SUMMON_M:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'My pets, destroy the arrogant mortal!'");
			for (dummy = 0; dummy < randint(5) + 1; dummy++)
			{
				(void)summon_specific(py, px, dun_level, 0, TRUE, FALSE, FALSE);
			}
			break;
		case REW_H_SUMMON:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou needst worthier opponents!'");
			activate_hi_summon();
			break;
		case REW_DO_HAVOC:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Death and destruction! This pleaseth me!'");
			call_chaos();
			break;
		case REW_GAIN_ABL:
			msg_format("The voice of %s rings out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Stay, mortal, and let me mold thee.'");
			if ((randint(3) == 1) && !(chaos_stats[p_ptr->chaos_patron] < 0))
				do_inc_stat(chaos_stats[p_ptr->chaos_patron]);
			else
				do_inc_stat(rand_int(6));
			break;
		case REW_LOSE_ABL:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'I grow tired of thee, mortal.'");
			if ((randint(3) == 1) && !(chaos_stats[p_ptr->chaos_patron] < 0))
				do_dec_stat(chaos_stats[p_ptr->chaos_patron]);
			else
				(void)do_dec_stat(rand_int(6));
			break;
		case REW_RUIN_ABL:
			msg_format("The voice of %s thunders:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou needst a lesson in humility, mortal!'");
			msg_print("You feel less powerful!");
			for (dummy = 0; dummy < 6; dummy++)
			{
				(void)dec_stat(dummy, 10 + randint(15), TRUE);
			}
			break;
		case REW_POLY_WND:
			msg_format("You feel the power of %s touch you.",
				chaos_patrons[p_ptr->chaos_patron]);
			do_poly_wounds();
			break;
		case REW_AUGM_ABL:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Receive this modest gift from me!'");
			for (dummy = 0; dummy < 6; dummy++)
			{
				(void)do_inc_stat(dummy);
			}
			break;
		case REW_HURT_LOT:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Suffer, pathetic fool!'");
			fire_ball(GF_DISINTEGRATE, 0, p_ptr->lev * 4, 4);
			take_hit(p_ptr->lev * 4, wrath_reason);
			break;
	   case REW_HEAL_FUL:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Rise, my servant!'");
			restore_level();
			(void)set_poisoned(0);
			(void)set_blind(0);
			(void)set_confused(0);
			(void)set_image(0);
			(void)set_stun(0);
			(void)set_cut(0);
			hp_player(5000);
			for (dummy = 0; dummy < 6; dummy++)
			{
				(void)do_res_stat(dummy);
			}
			break;
		case REW_CURSE_WP:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou reliest too much on thy weapon.'");
			(void)curse_weapon();
			break;
		case REW_CURSE_AR:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Thou reliest too much on thine equipment.'");
			(void)curse_armor();
			break;
		case REW_PISS_OFF:
			msg_format("The voice of %s whispers:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Now thou shalt pay for annoying me.'");
			switch (randint(4))
			{
				case 1:
					(void)activate_ty_curse(FALSE); /* Prfnoff */
					break;
				case 2:
					activate_hi_summon();
					break;
				case 3:
					if (randint(2) == 1) (void)curse_weapon();
					else (void)curse_armor();
					break;
				default:
					for (dummy = 0; dummy < 6; dummy++)
					{
						(void)dec_stat(dummy, 10 + randint(15), TRUE);
					}
			}
			break;
		case REW_WRATH:
			msg_format("The voice of %s thunders:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Die, mortal!'");
			take_hit(p_ptr->lev * 4, wrath_reason);
			for (dummy = 0; dummy < 6; dummy++)
			{
				(void)dec_stat(dummy, 10 + randint(15), FALSE);
			}
			activate_hi_summon();
			(void)activate_ty_curse(FALSE); /* Prfnoff */
			if (randint(2) == 1) (void)curse_weapon();
			if (randint(2) == 1) (void)curse_armor();
			break;
		case REW_DESTRUCT:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Death and destruction! This pleaseth me!'");
			destroy_area(py, px, 25, TRUE);
			break;
		case REW_GENOCIDE:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Let me relieve thee of thine oppressors!'");
			(void)genocide(FALSE);
			break;
		case REW_MASS_GEN:
			msg_format("The voice of %s booms out:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_print("'Let me relieve thee of thine oppressors!'");
			(void)mass_genocide(FALSE);
			break;
		case REW_DISPEL_C:
			msg_format("You can feel the power of %s assault your enemies!",
				chaos_patrons[p_ptr->chaos_patron]);
			(void)dispel_monsters(p_ptr->lev * 4);
			break;
		case REW_IGNORE:
			msg_format("%s ignores you.",
				chaos_patrons[p_ptr->chaos_patron]);
			break;
		case REW_SER_DEMO:
			msg_format("%s rewards you with a demonic servant!",chaos_patrons[p_ptr->chaos_patron]);
			if (!summon_specific(py, px, dun_level, SUMMON_DEMON, FALSE, TRUE, TRUE))
				msg_print("Nobody ever turns up...");
			break;
		case REW_SER_MONS:
			msg_format("%s rewards you with a servant!",chaos_patrons[p_ptr->chaos_patron]);
			if (!summon_specific(py, px, dun_level, SUMMON_NO_UNIQUES, FALSE, TRUE, TRUE))
				msg_print("Nobody ever turns up...");
			break;
		case REW_SER_UNDE:
			msg_format("%s rewards you with an undead servant!",chaos_patrons[p_ptr->chaos_patron]);
			if (!summon_specific(py, px, dun_level, SUMMON_UNDEAD, FALSE, TRUE, TRUE))
				msg_print("Nobody ever turns up...");
			break;
		default:
			msg_format("The voice of %s stammers:",
				chaos_patrons[p_ptr->chaos_patron]);
			msg_format("'Uh... uh... the answer's %d/%d, what's the question?'", type, effect);
	}
}


/*
 * old -- from PsiAngband.
 */
bool tgt_pt(int *x, int *y)
{
	char ch = 0;
	int d, cu, cv;
	bool success = FALSE;

	*x = px;
	*y = py;

	cu = Term->scr->cu;
	cv = Term->scr->cv;
	Term->scr->cu = 0;
	Term->scr->cv = 1;
	msg_print("Select a point and press space.");

	while ((ch != 27) && (ch != ' '))
	{
		move_cursor_relative(*y,*x);
		ch = inkey();
		switch (ch)
		{
		case 27:
			break;
		case ' ':
			success = TRUE; break;
		default:
			/* Look up the direction */
			d = get_keymap_dir(ch);

			if (!d) break;

			*x += ddx[d];
			*y += ddy[d];

			/* Hack -- Verify x */
			if ((*x>=cur_wid-1) || (*x>=panel_col_min + SCREEN_WID)) (*x)--;
			else if ((*x<=0) || (*x<=panel_col_min)) (*x)++;

			/* Hack -- Verify y */
			if ((*y>=cur_hgt-1) || (*y>=panel_row_min + SCREEN_HGT)) (*y)--;
			else if ((*y<=0) || (*y<=panel_row_min)) (*y)++;

			break;
		}
	}

	Term->scr->cu = cu;
	Term->scr->cv = cv;
	Term_fresh();
	return success;
}


bool get_hack_dir(int *dp)
{
	int		dir;
	cptr    p;
	char    command;


	/* Initialize */
	(*dp) = 0;

	/* Global direction */
	dir = 0;

	/* (No auto-targetting */

	/* Ask until satisfied */
	while (!dir)
	{
		/* Choose a prompt */
		if (!target_okay())
		{
			p = "Direction ('*' to choose a target, Escape to cancel)? ";
		}
		else
		{
			p = "Direction ('5' for target, '*' to re-target, Escape to cancel)? ";
		}

		/* Get a command (or Cancel) */
		if (!get_com(p, &command)) break;

		/* Convert various keys to "standard" keys */
		switch (command)
		{
			/* Use current target */
			case 'T':
			case 't':
			case '.':
			case '5':
			case '0':
			{
				dir = 5;
				break;
			}

			/* Set new target */
			case '*':
			{
				if (target_set(TARGET_KILL)) dir = 5;
				break;
			}

			default:
			{
				/* Look up the direction */
				dir = get_keymap_dir(command);

				break;
			}
		}

		/* Verify requested targets */
		if ((dir == 5) && !target_okay()) dir = 0;

		/* Error */
		if (!dir) bell();
	}

	/* No direction */
	if (!dir) return (FALSE);

	/* Save the direction */
	command_dir = dir;

	/* Check for confusion */
	if (p_ptr->confused)
	{
		/* XXX XXX XXX */
		/* Random direction */
		dir = ddd[rand_int(8)];
	}

	/* Notice confusion */
	if (command_dir != dir)
	{
		/* Warn the user */
		msg_print("You are confused.");
	}

	/* Save direction */
	(*dp) = dir;

	/* A "valid" direction was entered */
	return (TRUE);
}
