/* 
 * This file contains stuff I used to be able to compile NewAngband 1.1.0 in Windows.
 * Eytan Zweig
 */

/* Thanks Eytan for making this nice file! :) I shall now continue your work! :) */
/* -- Variaz */
/* FINALLY UPDATED FOR NEWANGBAND 1.7.0...was about time! I'm just so lazy... ;) */

/* birth.c */
extern void no_more_items();
extern void no_more_kills();
extern void change_back_to_apprentice();
extern void update_and_handle();
extern void make_sword_devastation(void);
extern void do_cmd_create_dagger(void);
extern void do_cmd_create_sword(void);
extern void do_cmd_create_2hsword(void);
extern void do_cmd_create_flaming(void);
extern void get_hellqueen_history();
extern void apply_valkyrie_weapon_magic();
extern void apply_skatter_quiver_magic();
extern void valkyrie_create_spear(void);
extern void create_mage_staff(void);
extern void do_cmd_create_cold(void);
extern void do_cmd_create_elec(void);
extern void do_cmd_create_super_defender(void);
extern void do_cmd_change_class(void);
extern void quest_again(int questnum);
extern void make_gold_pile(void);
extern void do_cmd_evolve();
extern void no_more_items_variaz(void);
extern void change_class_basic(void);
extern void change_class_advanced(void);
extern void pick_lord_element();
extern void get_a_monster_body();

/* cave.c */
void reveal_spell(int x, int y, byte rad);

/* cmd1.c */
extern void do_cmd_damage_weapon();
extern s32b monster_damage_reduction(s32b damages, monster_type *m_ptr, bool magicattack);
extern bool player_hit_monster(monster_type *m_ptr, int bonus);
extern bool monster_hit_player(monster_type *m_ptr, int bonus);
extern bool monster_hit_monster(monster_type *m_ptr, monster_type *t_ptr);
extern bool always_hit_check();
extern bool protection_check();
extern bool standing_on_forest();
extern void monstrous_wave();
extern s32b weapon_damages();

/* cmd2.c */
extern void change_age();
extern void do_cmd_dark_aura_info(void);
extern void turn_in_crystal(monster_type *m_ptr);
extern void accurate_teleport();
extern void revive_monster();
extern void raise_skeleton();
extern void raise_bonewall();
extern void necro_revive_monster();
extern void change_background(void);
extern int digging_ability();
extern void agility_jump(void);
extern void simulacrum();
extern void warp_on_trees();
extern void called_shots();
extern int get_a_dir();
extern void sealing_light();
extern void soul_energize();

/* cmd3.c */
extern void do_cmd_auto_wield(object_type *o_ptr);
extern int max_carry();
extern bool summoned_item(object_type *o_ptr);

/* cmd5.c */
extern void hellqueen_mana_blast();
extern void do_cmd_bless_weapon(void);
extern void do_cmd_lite_charm();
extern void do_cmd_divine_bolt();
extern void wave_kick();
extern bool energy_spin();
extern void skatter_enchant_bow();
extern void skatter_create_arrows(void);
extern void firelord_fireball();
extern bool firelord_fireaura();
extern void firelord_firestorm();
extern void repair_weapon();
extern void get_history(void);
extern void do_cmd_staff_balance();
extern void skatter_exploding_arrow();
extern bool circle_of_force();
extern void devastation_beam();
extern void do_cmd_flooding();
extern void special_weapon_charge();
extern void object_eternality();
extern void make_item_magic();
extern bool corpse_explode(s32b dam, int x, int y, int rad, int typ);
extern void corpse_explosion();
extern void mana_shield();
extern void barrier_master();
extern void assassin_poison_weapon();
extern void assassin_trap_weapon();
extern void assassin_sleep_dart();
extern void place_field(int ftype, byte rad, int x, int y, s32b dam);
extern void place_field_ability();
extern void explosive_throw();
extern void do_cmd_tweak(object_type *o_ptr);
extern void dispel_entity();
extern void sacrifice_weapon();
extern void ice_lord_frost();
extern void ice_shatter();
extern void morph_memorize();
extern void morph_into_memorized();
extern void call_dryad();
extern void do_cmd_unevolve_monster(monster_type *m_ptr);
extern void tree_explosion();
extern void battle_weapon_explode();
extern void scan_targetting();
extern void recharge_crystal();
extern void sharpen_ammos();
extern void dagger_fatal_stab();
extern void enchanted_blood();
extern void do_cmd_evolve_monster(monster_type *m_ptr);
extern void capture_soul(int x, int y);
extern void soul_bind();
extern int spirit_sword(object_type *o_ptr, s32b dam);
extern bool cave_lit(int y, int x);
extern void drain_object();
extern void stone_to_gold();
extern void animate_knight();
extern void mace_of_heaven();
extern void set_spike_trap();
extern void add_slay_brand(object_type *o_ptr);
extern void set_gas_trap();
extern void set_poison_trap();
extern void ranger_entangle();
extern void ranger_thorned_vines();
extern void ranger_force_of_nature();
extern void blade_of_purity();
extern void paladin_shining_armor();
extern void monk_throw_counter(monster_type *m_ptr);
extern void conjure_item(int itemtval, int itemsval, int duration, bool magic, bool special);
extern void justice_bless_weapon();
extern void combine_items();
extern bool is_alchemy(object_type *o_ptr);
extern void decompose_item();
extern void zelar_leg_throw_execute(monster_type *m_ptr);
extern void dark_mist_ability();

/* cmd6.c */
extern void do_cmd_asmodis(object_type *o_ptr);

/* cmd7.c */
extern void do_cmd_check_flooding();
extern bool fear_monsters(void);

/* Files.c */
extern void show_itemmake_prices(void);
extern void edit_background(void);

/* learn.c */
extern bool ring(int rsval);
extern int use_monster_power(bool great, bool only_number);
extern void pet_skills(int m_idx);

/* load2.c */
extern void rd_magic_spells();

/* melee1.c */
extern void damage_armor();
extern bool shield_has();
extern bool sword_has();
extern bool hafted_has();
extern bool polearm_has();
extern bool rod_has();
extern bool unarmed();
extern bool heavy_armor();

/* melee2.c */
extern void levelup_friend(monster_type *m_ptr);
extern void monster_speak(monster_type *m_ptr);
extern bool seduction(monster_type *m_ptr);
extern bool leader_class();
extern bool counterspell(monster_type *m_ptr);

/* monster1.c */
extern void info_boss_abilities(monster_type *m_ptr);
extern void screen_roff_boss(int r_idx, int remember, monster_type *m_ptr);
extern void roff_aux_boss(int r_idx, int remem, monster_type *m_ptr);
extern void vision_scan_monster(monster_type *m_ptr);
extern void open_monster_generator();

/* monster2.c */
extern void apply_monster_level_hp(monster_type *m_ptr);
extern void get_boss_ability(monster_type *m_ptr, int number);
extern bool place_monster_one_no_boss(int y, int x, int r_idx, bool slp, bool charm);
extern s16b place_monster_one_return_no_boss(int y, int x, int r_idx, bool slp, bool charm, int petlevel, s32b pethp, s32b petmaxhp);
extern bool place_monster_aux_no_boss(int y, int x, int r_idx, bool slp, bool grp, bool charm);
extern s16b place_monster_one_simulacrum(int y, int x, int r_idx, bool slp, bool charm, int petlevel, s32b pethp);
extern bool summon_specific_friendly_kind(int y1, int x1, int lev, char kind, bool Group_ok);
extern int get_mon_num_kind(int lev, char kind);
extern bool summon_specific_friendly_name(int y1, int x1, char name[30], bool Group_ok);
extern int get_mon_num_name(char name[30]);
extern bool place_monster_one_image(int y, int x, int r_idx, bool slp, bool charm);
extern s16b place_monster_animated(int y, int x, int r_idx, bool slp, bool charm, int basehp, int hit_bonus, int d_d, int d_s);

/* object1.c */
extern void do_cmd_make_item(object_type *o_ptr);
extern void do_cmd_breed();
extern void do_cmd_more_pval();
extern char *get_item_type_name(object_type *o_ptr);

/* object2.c */
extern bool oriental_check();

/* save.c */
extern void wr_magic_spells();

/* spells1.c */
extern void move_monster_spot(int m_idx, int xspot, int yspot);
extern bool lord_piercing(int basechance, int factor, int typ);

/* spells2.c */
extern bool dark_lord_aura(s32b dam, int rad);
extern bool valkyrie_aura(s32b dam, int rad);
extern bool elem_lord_aura(s32b dam, int rad);
extern bool fire_ball_pets(monster_type *m_ptr, int typ, s32b dam, int rad);
extern bool project_hook_pets(monster_type *m_ptr, int typ, int dir, s32b dam, int flg);
extern bool fire_ball_spot(int tx, int ty, int typ, s32b dam, int rad);
extern bool chain_attack(int dir, int typ, s32b dam, int rad, int range);
extern bool chain_attack_fields(int dir, int typ, s32b dam, int rad, int range, int fldtype, int fldam);
extern bool lava_burst();
extern bool attack_aura(int typ, s32b dam, int rad);
extern bool spin_kick(s32b dam, int rad);
extern bool spin_attack();
extern bool glacial_spin();
extern bool sword_spin();
extern bool hard_kick(int dir, s32b dam, int range);
extern bool smash(int dir, s32b dam, int range);
extern bool dizzy_smash(int dir, s32b dam, int range);
extern bool shattering_blow(int dir);
extern bool power_punch(int dir);
extern bool stunning_blow(int dir);
extern bool eye_stab(int dir);
extern bool fatal_stab(int dir, object_type *o_ptr);
extern bool axe_chop(int dir, s32b dam);
extern bool mutilate_legs(int dir, s32b dam);
extern bool mutilate_arms(int dir, s32b dam);
extern bool samurai_slice(int dir);
extern void slice_kill(monster_type *m_ptr, int m_idx);
extern bool hire_befriend(int dir);
extern void makefriend(monster_type *m_ptr, int m_idx);
extern void give_refusal_message();
extern void give_approval_message();
extern bool solid_block(cave_type *c_ptr);
extern bool anihilate_monsters();
extern bool vampire_drain(int dir);
extern bool accurate_strike(int dir);
extern void counter_attack(monster_type *m_ptr);
extern void leaping_spin();
extern bool godly_wrath();
extern void wilderness_lore();
extern bool aura_of_life();
extern bool smite_evil(int dir);
extern bool word_of_peace();
extern bool element_strike(int dir);
extern void elem_wave();
extern bool aura_repulse_evil(int rad);
extern bool zelar_leg_throw();

/* store.c */
extern void give_silly_love_message();

/* util.c */
char *get_element_name(int element);

/* xtra1.c */
extern bool safety_check();

/* xtra2.c */
extern void know_body_monster();
extern void gain_exp_kill(s32b amount, monster_type *m_ptr);
extern void describe_soul(object_type *o_ptr);
extern bool set_str_boost(int v);
extern bool set_int_boost(int v);
extern bool set_wis_boost(int v);
extern bool set_dex_boost(int v);
extern bool set_con_boost(int v);
extern bool set_chr_boost(int v);
extern bool set_pres(int v);
extern bool set_mres(int v);
extern bool set_ac_boost(int v);
extern void add_class_kill(monster_type *m_ptr);
extern void gain_class_level();
extern bool set_elem_shield(int v);

/* Learn.c */
extern int use_monster_power(bool great, bool only_number);
extern bool ability(int abil);
extern void learn_ability();
extern void pet_skills(int m_idx);
extern bool ring(int rsval);
extern s16b examine_bow();
extern bool dagger_check();
extern bool axe_check();
extern bool is_weapon(object_type *o_ptr);
extern bool is_ammo(object_type *o_ptr);
extern int add_item_ability(object_type *o_ptr);
extern void improve_skills();
extern int use_monster_soul();
extern void use_soul_power();
extern void use_psychic_power();
extern void spell_making();
extern int get_name_manacost(char name[30]);
extern void delete_spell();
extern void delete_all_spells();
extern int get_object_manacost(char name[30]);
extern void conjure_specific_item(char name[30], int dur, bool magic, bool special);

/* mongen.c */
extern void monster_generator();
extern char *complete_mon_name(char monracename[80]);
extern char *get_random_mon_adj();
extern char get_attr_color();
extern char *blows_random_method(char monattr);
extern char *blows_random_effect();
extern char *get_basic_mon_resist();
extern char *get_extra_mon_resist();
extern char *get_basic_mon_spell();
extern char *get_medium_mon_spell();
extern char *get_major_mon_spell();
extern char *mon_random_description(char monattr);
