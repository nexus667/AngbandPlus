/* File: files.c */

/* Purpose: code dealing with files (and death) */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#include "angband.h"



/*
 * Semi-Portable High Score List Entry (128 bytes) -- BEN
 *
 * All fields listed below are null terminated ascii strings.
 *
 * In addition, the "number" fields are right justified, and
 * space padded, to the full available length (minus the "null").
 *
 * Note that "string comparisons" are thus valid on "pts".
 */

typedef struct _high_score high_score;

struct _high_score {

  char what[8];		/* Version info (string) */

  char pts[10];		/* Total Score (number) */

  char gold[10];	/* Total Gold (number) */

  char turns[10];	/* Turns Taken (number) */

  char day[10];		/* Time stamp (string) */

  char who[16];		/* Player Name (string) */

  char uid[8];		/* Player UID (number) */

  char sex[2];		/* Player Sex (string) */
  char p_r[3];		/* Player Race (number) */
  char p_c[3];		/* Player Class (number) */

  char cur_lev[4];	/* Current Player Level (number) */
  char cur_dun[4];	/* Current Dungeon Level (number) */
  char max_lev[4];	/* Max Player Level (number) */
  char max_dun[4];	/* Max Dungeon Level (number) */

  char how[32];		/* Method of death (string) */
};




/*
 * XXX XXX XXX Hack -- Ignore "POSIX" methods which are dangerous,
 * according to "Michael H. Price II" <mhp1@Ra.MsState.Edu>
 * But it looks like Michael is confused.  A lot. :-(
 */
/* #undef _POSIX_SAVED_IDS */


/*
 * Hack -- drop permissions
 */
void safe_setuid_drop(void)
{

#if defined(SET_UID) && defined(SAFE_SETUID)

# ifdef _POSIX_SAVED_IDS

    if (setuid(getuid()) != 0) {
        quit("setuid(): cannot set permissions correctly!");
    }
    if (setgid(getgid()) != 0) {
        quit("setgid(): cannot set permissions correctly!");
    }

# else

    if (setreuid(geteuid(), getuid()) != 0) {
        quit("setreuid(): cannot set permissions correctly!");
    }
    if (setregid(getegid(), getgid()) != 0) {
        quit("setregid(): cannot set permissions correctly!");
    }

# endif

#endif

}


/*
 * Hack -- grab permissions
 */
void safe_setuid_grab(void)
{

#if defined(SET_UID) && defined(SAFE_SETUID)

# ifdef _POSIX_SAVED_IDS

    if (setuid(player_euid) != 0) {
        quit("setuid(): cannot set permissions correctly!");
    }
    if (setgid(player_egid) != 0) {
        quit("setgid(): cannot set permissions correctly!");
    }

#else

    if (setreuid(geteuid(), getuid()) != 0) {
        quit("setreuid(): cannot set permissions correctly!");
    }
    if (setregid(getegid(), getgid()) != 0) {
        quit("setregid(): cannot set permissions correctly!");
    }

# endif

#endif

}


/*
 * Parse a sub-file of the "extra info" (format shown below)
 *
 * Each "command" has a "command symbol" in the first column,
 * followed by a colon, followed by some command specific info,
 * usually with "fields" separated by colons.  Blank lines and
 * lines starting with pound signs ("#") are ignores as comments.
 *
 * Currently, <a>, <c>, <num>, <tv>, <sv> must be integers.
 *
 * Parse another file (recursively):
 *   %:<filename>		<-- see next function
 *
 * Specify the attr/char values for "monsters":
 *   R:<num>:<a>/<c>		<-- attr/char by race index
 *   M:<a>/<c>:<a>/<c>		<-- attr/char by race attr/char
 *
 * Specify the attr/char values for "objects":
 *   K:<num>:<a>/<c>		<-- attr/char by kind index
 *   I:<a>/<c>:<a>/<c>		<-- attr/char by kind attr/char
 *   T:<tv>,<sv>:<a>/<c>	<-- attr/char by kind tval/sval
 *
 * Specify the attr/char values for "objects" by "tval":
 *   E:<tv>:<a>			<-- inventory attr by tval
 *   S:<tv>:<c>			<-- inventory char by tval
 *
 * Specify macros and command-macros:
 *   A:<str>			<-- macro action (encoded)
 *   P:<str>			<-- macro pattern (encoded)
 *   C:<str>			<-- command macro pattern (encoded)
 */
static errr process_pref_file_aux(cptr name)
{
    int i, i1, i2, n1, n2;

    FILE *fp;

    /* Current input line */
    char buf[1024];

    /* Current macro data */
    char pat[1024] = "";
    char act[1024] = "";


    /* Open the file */
    fp = my_tfopen(name, "r");

    /* Catch errors */
    if (!fp) return (-1);

    /* Process the file */
    while (1) {

        /* Read a line from the file, stop when done */
        if (!fgets(buf, 1024, fp)) break;

        /* Skip comments */
        if (buf[0] == '#') continue;

        /* See how long the input is */
        i = strlen(buf);

        /* Strip the final newline (and spaces) */
        while (i && isspace(buf[i-1])) buf[--i] = '\0';

        /* Skip blank lines */
        if (!buf[0]) continue;

        /* The line better have a colon and such */
        if (buf[1] != ':') return (1);

        /* Process "%:<fname>" */
        if (buf[0] == '%') {

            /* Attempt to Process the given file */
            (void)process_pref_file(buf + 2);
        }

        /* Process "R:<num>:<a>/<c>" */
        else if (buf[0] == 'R') {
            monster_lore *l_ptr;
            if (sscanf(buf, "R:%d:%d/%d", &i, &n1, &n2) != 3) continue;
            l_ptr = &l_list[i];
            if (n1) l_ptr->l_attr = n1;
            if (n2) l_ptr->l_char = n2;
        }

        /* Process "M:<a>/<c>:<a>/<c>" */
        else if (buf[0] == 'M') {
            if (sscanf(buf, "M:%d/%d:%d/%d", &i1, &i2, &n1, &n2) != 4) continue;
            for (i = 1; i < MAX_R_IDX; i++) {
                monster_race *r_ptr = &r_list[i];
                monster_lore *l_ptr = &l_list[i];
                if ((!i1 || r_ptr->r_attr == i1) &&
                    (!i2 || r_ptr->r_char == i2)) {
                    if (n1) l_ptr->l_attr = n1;
                    if (n2) l_ptr->l_char = n2;
                }
            }
        }

        /* Process "K:<num>:<a>/<c>" */
        else if (buf[0] == 'K') {
            if (sscanf(buf, "K:%d:%d/%d", &i, &n1, &n2) != 3) continue;
            if (n1) x_list[i].x_attr = n1;
            if (n2) x_list[i].x_char = n2;
        }

        /* Process "I:<a>/<c>:<a>/<c>" */
        else if (buf[0] == 'I') {
            if (sscanf(buf, "I:%d/%d:%d/%d", &i1, &i2, &n1, &n2) != 4) continue;
            for (i = 0; i < MAX_K_IDX; i++) {
                if ((!i1 || k_list[i].k_attr == i1) &&
                    (!i2 || k_list[i].k_char == i2)) {
                    if (n1) x_list[i].x_attr = n1;
                    if (n2) x_list[i].x_char = n2;
                }
            }
        }

        /* Process "T:<tv>,<sv>:<a>/<c>" */
        else if (buf[0] == 'T') {
            if (sscanf(buf, "T:%d,%d:%d/%d", &i1, &i2, &n1, &n2) != 4) continue;
            for (i = 0; i < MAX_K_IDX; i++) {
                if ((!i1 || k_list[i].tval == i1) &&
                    (!i2 || k_list[i].sval == i2)) {
                    if (n1) x_list[i].x_attr = n1;
                    if (n2) x_list[i].x_char = n2;
                }
            }
        }

        /* Process "E:<tv>:<a>" -- Changes default object attrs */
        else if (buf[0] == 'E') {
            if (sscanf(buf, "E:%d:%d", &i1, &n1) != 2) continue;
            tval_to_attr[i1] = n1;
        }

        /* Process "S:<tv>:<a>" -- Changes default object chars */
        else if (buf[0] == 'S') {
            if (sscanf(buf, "S:%d:%d", &i1, &n1) != 2) continue;
            tval_to_char[i1] = n1;
        }

        /* Process "A:<str>" -- save an "action" for later */
        else if (buf[0] == 'A') {
            text_to_ascii(act, buf+2);
        }

        /* Process "P:<str>" -- normal-macro trigger */
        else if (buf[0] == 'P') {
            text_to_ascii(pat, buf+2);
            macro_add(pat, act, FALSE);
        }

        /* Process "C:<str>" -- command-macro trigger */
        else if (buf[0] == 'C') {
            text_to_ascii(pat, buf+2);
            macro_add(pat, act, TRUE);
        }
    }

    /* Close the file */
    fclose(fp);

    /* Success */
    return (0);
}


/*
 * Find a pref file with the given name and process it
 * Looks in the current directory, and both "PREF" directories
 */
errr process_pref_file(cptr name)
{
    char tmp[1024];
    
    /* Try the given file */
    if (0 == process_pref_file_aux(name)) return (0);

    /* Look in the "special" directory */
    sprintf(tmp, "%s-%s%s%s", ANGBAND_DIR_PREF, ANGBAND_SYS, PATH_SEP, name);

    /* Attempt to process that file */
    if (0 == process_pref_file_aux(tmp)) return (0);

    /* Look in the "standard" directory */
    sprintf(tmp, "%s%s%s", ANGBAND_DIR_PREF, PATH_SEP, name);

    /* Attempt to process that file */
    if (0 == process_pref_file_aux(tmp)) return (0);

    /* Oh well */
    return (1);
}




/*
 * The "highscore" file descriptor
 */
static int highscore_fd = -1;


/*
 * Open the highscore file (for reading/writing).  Create if needed.
 * Set "highscore_fd" which is used by the "highscore_*" functions.
 */
static errr highscore_open(void)
{
    int fd;
    char buf[1024];

    /* Permissions for file */
    int mode = 0644;

    /* Extract the name of the High Score File (not really a "raw" file) */
    sprintf(buf, "%s%s%s", ANGBAND_DIR_DATA, PATH_SEP, "scores.raw");

#ifdef MACINTOSH
    /* Global -- "Data file" */
    _ftype = 'DATA';
#endif

    /* Open (or create) a BINARY file, for reading/writing */
    fd = my_topen(buf, O_RDWR | O_CREAT | O_BINARY, mode);

    /* Save the fd */
    highscore_fd = fd;

    /* Check for success */
    if (fd >= 0) return (0);


    /* Warning message */
    plog_fmt("cannot create the '%s' score file", buf);

    /* No "scores", but yes "news" */
    if (!my_tfopen(ANGBAND_NEWS, "r")) {

        /* Warning message */
        plog_fmt("cannot access the '%s' news file", ANGBAND_NEWS);
    }

    /* Abort */
    quit("fatal error attempting to access the Angband 'lib' directory");

    /* Failure */
    return (1);
}


/*
 * Close the highscore file
 */
static errr highscore_close()
{
    /* Already closed */
    if (highscore_fd < 0) return (1);

    /* All done */
    (void)close(highscore_fd);

    /* No fd */
    highscore_fd = -1;

    /* Success */
    return (0);
}


/*
 * Lock the highscore file
 */
static errr highscore_lock(void)
{
    int oops;

    /* Paranoia -- it may not have opened */
    if (highscore_fd < 0) return (1);

#if !defined(MACINTOSH) && !defined(MSDOS) && !defined(AMIGA) && \
    !defined(_Windows) && !defined(__EMX__)

/* First, get a lock on the high score file so no-one else tries */
/* to write to it while we are using it */
/* added sys_v call to lockf - cba */

#ifdef USG
    oops = (lockf(highscore_fd, F_LOCK, 0) != 0);
#else
    oops = (0 != flock(highscore_fd, LOCK_EX));
#endif

    /* Trouble */
    if (oops) return (1);

#endif

    /* Success */
    return (0);
}


/*
 * Unlock the highscore file
 */
static errr highscore_unlock()
{
    /* Paranoia -- it may not have opened */
    if (highscore_fd < 0) return (1);

#if !defined(MACINTOSH) && !defined(MSDOS) && !defined(AMIGA) && \
    !defined(_Windows) && !defined(__EMX__)

/* added usg lockf call - cba */
#ifdef USG
    lockf(highscore_fd, F_ULOCK, 0);
#else
    (void)flock(highscore_fd, LOCK_UN);
#endif

#endif

    /* Success */
    return (0);
}


/*
 * Seek score 'i' in the highscore file
 */
static int highscore_seek(int i)
{
    long p = (long)(i) * sizeof(high_score);

    /* Paranoia -- it may not have opened */
    if (highscore_fd < 0) return (1);

    /* Seek for the requested record */
    if (lseek(highscore_fd, p, L_SET) < 0) return (2);

    /* Success */
    return (0);
}


#if 0

/*
 * Chop all scores from 'i' on from the highscore file
 */
static errr highscore_chop(int i)
{
    /* Paranoia -- it may not have opened */
    if (highscore_fd < 0) return (1);

#if defined(sun) || defined(ultrix) || defined(NeXT)
    ftruncate(highscore_fd, i * sizeof(high_score));
#endif

    /* Success */
    return (0);
}

#endif


/*
 * Read one score from the highscore file
 */
static errr highscore_read(high_score *score)
{
    int num;

    /* Paranoia -- it may not have opened */
    if (highscore_fd < 0) return (1);

    /* Read a record, note failure */
    num = read(highscore_fd, (char*)(score), sizeof(high_score));

    /* Nothing read, means end of file */
    if (!num) return (1);

    /* Partial read, means major error */
    if (num != sizeof(high_score)) return (-1);

    /* Success */
    return (0);
}


/*
 * Write one score to the highscore file
 */
static int highscore_write(high_score *score)
{
    int num;

    /* Paranoia -- it may not have been opened */
    if (highscore_fd < 0) return (1);

    /* Write the record */
    num = write(highscore_fd, (char*)(score), sizeof(high_score));

    /* Fail */
    if (num != sizeof(high_score)) return (-1);

    /* Success */
    return (0);
}



/*
 * Just determine where a new score *would* be placed
 * Return the location (0 is best) or -1 on failure
 */
static int highscore_where(high_score *score)
{
    int			i;
    high_score		the_score;

    /* Go to the start of the highscore file */
    if (highscore_seek(0)) return (-1);

    /* Read until we get to a higher score */
    for (i = 0; i < MAX_HISCORES; i++) {
        if (highscore_read(&the_score)) return (i);
        if (strcmp(the_score.pts, score->pts) < 0) return (i);
    }

    /* The "last" entry is always usable */
    return (MAX_HISCORES - 1);
}


/*
 * Actually place an entry into the high score file
 * Return the location (0 is best) or -1 on "failure"
 */
static int highscore_add(high_score *score)
{
    int			i, slot;
    bool		done = FALSE;

    high_score		the_score, tmpscore;


    /* Determine where the score should go */
    slot = highscore_where(score);

    /* Hack -- Not on the list */
    if (slot < 0) return (-1);

    /* Hack -- prepare to dump the new score */
    the_score = (*score);

    /* Slide all the scores down one */
    for (i = slot; !done && (i < MAX_HISCORES); i++) {

        /* Read the old guy, note errors */
        if (highscore_seek(i)) return (-1);
        if (highscore_read(&tmpscore)) done = TRUE;

        /* Back up and dump the score we were holding */
        if (highscore_seek(i)) return (-1);
        if (highscore_write(&the_score)) return (-1);

        /* Hack -- Save the old score, for the next pass */
        the_score = tmpscore;
    }

    /* Return location used */
    return (slot);
}





/*
 * Note that this function is called BEFORE "Term_init()"
 *
 * Open the score file while we still have the setuid privileges.
 * Later when the score is being written out, you must be sure
 * to flock the file so we don't have multiple people trying to
 * write to it at the same time.
 *
 * Notice that a failure to open the high score file often indicates
 * incorrect directory structure or starting directory or permissions.
 *
 * Note that a LOT of functions in this file assume that this
 * function call will succeed, so if not, we quit.
 */
void init_scorefile()
{
    /* Open the scorefile */
    (void)highscore_open();
}


/*
 * Shut down the scorefile
 */
void nuke_scorefile()
{
    /* Close the high score file */
    (void)(highscore_close());
}





#ifdef CHECK_TIME

/*
 * Operating hours for ANGBAND	-RAK-
 *	 X = Open; . = Closed
 */
static char days[7][29] = {
    "SUN:XXXXXXXXXXXXXXXXXXXXXXXX",
    "MON:XXXXXXXX.........XXXXXXX",
    "TUE:XXXXXXXX.........XXXXXXX",
    "WED:XXXXXXXX.........XXXXXXX",
    "THU:XXXXXXXX.........XXXXXXX",
    "FRI:XXXXXXXX.........XXXXXXX",
    "SAT:XXXXXXXXXXXXXXXXXXXXXXXX"
};

#endif


/*
 * Handle CHECK_TIME
 */
errr check_time(void)
{

#ifdef CHECK_TIME

    time_t              c;
    struct tm		*tp;

    /* Check for time violation */
    c = time((time_t *)0);
    tp = localtime(&c);

    /* Violation */
    if (days[tp->tm_wday][tp->tm_hour + 4] != 'X') return (1);

#endif

    /* Success */
    return (0);
}



/*
 * Initialize CHECK_TIME
 */
errr check_time_init(void)
{

#ifdef CHECK_TIME

    FILE        *fp;

    char	buf[1024];


    /* Access the "hours" file */
    sprintf(buf, "%s%s%s", ANGBAND_DIR_FILE, PATH_SEP, "hours.txt");

    /* Open the file */
    fp = my_tfopen(buf, "r");

    /* Oops.  No such file */
    if (!fp) {
        msg_print("There is no 'lib/file/hours.txt' file!");
        msg_print(NULL);
        exit_game();
    }

    /* Parse the file */
    while (fgets(buf, 80, fp)) {
        if (prefix(buf, "SUN:")) strcpy(days[0], buf);
        if (prefix(buf, "MON:")) strcpy(days[1], buf);
        if (prefix(buf, "TUE:")) strcpy(days[2], buf);
        if (prefix(buf, "WED:")) strcpy(days[3], buf);
        if (prefix(buf, "THU:")) strcpy(days[4], buf);
        if (prefix(buf, "FRI:")) strcpy(days[5], buf);
        if (prefix(buf, "SAT:")) strcpy(days[6], buf);
    }

    /* Close it */
    fclose(fp);

    /* Make sure the game is "open" */
    if (0 != check_time()) {
        msg_print("The gates to Angband are closed.");
        msg_print("Please try again at a different time.");
        msg_print(NULL);
        exit_game();
    }

#endif

    /* Success */
    return (0);    
}



#ifdef CHECK_LOAD

typedef struct statstime {

    int                 cp_time[4];
    int                 dk_xfer[4];
    unsigned int        v_pgpgin;
    unsigned int        v_pgpgout;
    unsigned int        v_pswpin;
    unsigned int        v_pswpout;
    unsigned int        v_intr;
    int                 if_ipackets;
    int                 if_ierrors;
    int                 if_opackets;
    int                 if_oerrors;
    int                 if_collisions;
    unsigned int        v_swtch;
    long                avenrun[3];
    struct timeval      boottime;
    struct timeval      curtime;

} statstime;

/*
 * Hack -- used for CHECK_LOAD
 */
static int LOAD = 0;

#endif


/*
 * Handle CHECK_LOAD
 */
errr check_load(void)
{

#ifdef CHECK_LOAD

    struct statstime    st;

    /* Check for load violation */
    if (!rstat("localhost", &st)) {
        if (((int)((double)st.avenrun[2] / (double)FSCALE)) >= LOAD) {
            return (1);
        }
    }

#endif

    /* Success */
    return (0);
}


/*
 * Initialize CHECK_LOAD
 */
errr check_load_init(void)
{

#ifdef CHECK_LOAD

    FILE        *fp;

    char	buf[1024];

    char	temphost[MAXHOSTNAMELEN+1];
    char	thishost[MAXHOSTNAMELEN+1];
    char	discard[120];


    /* Access the "load" file */
    sprintf(buf, "%s%s%s", ANGBAND_DIR_FILE, PATH_SEP, "load.txt");

    /* Open the "load" file */
    fp = my_tfopen(buf, "r");

    /* Oops.  No such file */
    if (!fp) {
        msg_print("There is no 'lib/file/load.txt' file!");
        msg_print(NULL);
        exit_game();
    }

    /* Get the host name */
    (void)gethostname(thishost, (sizeof thishost) - 1);

    /* Find ourself */
    while (1) {

        /* Oops */
        if (fscanf(fp, "%s%d", temphost, &LOAD) == EOF) {
            LOAD = 100;
            break;
        }

        /* Hack -- Discard comments */
        if (temphost[0] == '#') {
            (void)fgets(discard, (sizeof discard)-1, fp);
            continue;
        }

        /* Did we find the entry? */
        if (streq(temphost,thishost) || streq(temphost,"localhost")) break;
    }

    /* Close the file */
    fclose(fp);

    /* Make sure the game is "open" */
    if (0 != check_load()) {
        msg_print("The gates to Angband are closed.");
        msg_print("Please try again at a less busy time.");
        msg_print(NULL);
        exit_game();
    }

#endif
 
    /* Success */
    return (0);
}


/*
 * Attempt to open, and display, the intro "news" file		-RAK-
 */
void show_news(void)
{
    int i;
    char	buf[1024];
    FILE        *fp;


    /* Try to open the News file */
    fp = my_tfopen(ANGBAND_NEWS, "r");

    /* Error? */
    if (!fp) {
        msg_print("Cannot read 'lib/file/news.txt' file!");
        msg_print(NULL);
        quit("cannot open 'news' file");
    }

    /* Clear the screen */
    clear_screen();

    /* Dump the file (nuking newlines) */
    for (i = 0; (i < 24) && fgets(buf, 80, fp); i++) {
        buf[strlen(buf)-1]=0;
        put_str(buf, i, 0);
    }

    /* Close */
    fclose(fp);

    /* Flush it */
    Term_fresh();
}



/*
 * Recursive "help file" perusal.  Return FALSE on "ESCAPE".
 */
static bool do_cmd_help_aux(cptr name, int line)
{
    int		i, k, n;

    /* Number of "real" lines passed by */
    int		next = 0;

    /* Number of "real" lines in the file */
    int		size = 0;

    /* Backup value for "line" */
    int		back = 0;
    
    /* This screen has sub-screens */
    bool	menu = FALSE;

    /* Current help file */
    FILE	*fff = NULL;

    /* Find this string (if any) */
    cptr	find = NULL;

    /* Hold a string to find */    
    char	finder[128] = "";
    
    /* General buffer */
    char	buf[1024];

    /* Sub-menu information */
    char	hook[10][32];


    /* Wipe the hooks */
    for (i = 0; i < 10; i++) hook[i][0] = '\0';


    /* Build the proper file name */
    sprintf(buf, "%s%s%s", ANGBAND_DIR_HELP, PATH_SEP, name);

    /* Open the file */
    fff = my_tfopen(buf, "r");

    /* Oops */
    if (!fff) {
        msg_print(format("Cannot open help file '%s'.", name));
        msg_print("See 'help.hlp' for how to get new help files.");
        return (TRUE);
    }


    /* Pre-Parse the file */
    while (TRUE) {

        /* Read a line from the file */
        if (!fgets(buf, 128, fff)) break;

        /* Strip trailing spaces and stuff */
        for (n = strlen(buf) - 1; (n > 0) && (isspace(buf[n-1])); n--);

        /* Truncate the text */
        buf[n] = '\0';

        /* XXX Parse "menu" items */
        if (prefix(buf, "***** ")) {

            char b1 = '[', b2 = ']';
            
            /* Notice "menu" requests */
            if ((buf[6] == b1) && isdigit(buf[7]) &&
                (buf[8] == b2) && (buf[9] == ' ')) {
            
                /* This is a menu file */
                menu = TRUE;

                /* Extract the menu item */
                k = buf[7] - '0';

                /* Extract the menu item */
                strcpy(hook[k], buf + 10);
            }

            /* Skip this */
            continue;
        }

        /* Count the "real" lines */
        next++;
    }

    /* Save the number of "real" lines */
    size = next;



    /* Display the file */
    while (TRUE) {


        /* Clear the screen */
        clear_screen();


        /* Restart when necessary */
        if (line >= size) line = 0;


        /* Re-open the file if needed */
        if (next > line) {

            /* Close it */
            fclose(fff);

            /* Build the proper file name */
            sprintf(buf, "%s%s%s", ANGBAND_DIR_HELP, PATH_SEP, name);

            /* Open the file */
            fff = my_tfopen(buf, "r");

            /* Oops */
            if (!fff) return (FALSE);

            /* File has been restarted */
            next = 0;
        }

        /* Skip lines if needed */
        for ( ; next < line; next++) {

            /* Skip a line */
            if (!fgets(buf, 128, fff)) break;
        }


        /* Dump the next 20 lines of the file */
        for (i = 0; i < 20; ) {

            /* Hack -- track the "first" line */
            if (!i) line = next;

            /* Get a line of the file */
            if (!fgets(buf, 128, fff)) break;

            /* Check the length of the line */
            n = strlen(buf) - 1;

            /* Never more than 80 characters */
            if (n > 80) n = 80;

            /* Terminate the line */
            buf[n] = '\0';

            /* Hack -- skip "special" lines */
            if (prefix(buf, "***** ")) continue;

            /* Count the "real" lines */
            next++;

            /* Hack -- keep searching */
            if (find && !i && !strstr(buf, find)) continue;
            
            /* Hack -- stop searching */
            find = NULL;
            
            /* Dump the lines */
            put_str(buf, i+2, 0);

            /* Count the printed lines */
            i++;
        }

        /* Hack -- failed search */
        if (find) {
            bell();
            line = back;
            find = NULL;
            continue;
        }
        

        /* Show a title of sorts */
        prt(format("=== Helpfile %s (line %d/%d) ===",
                   name, line, size), 0, 0);


        /* Prompt -- menu screen */
        if (menu) {

            /* Wait for it */
            prt("[Press a Number, or ESC to exit.]", 23, 0);
        }

        /* Prompt -- small files */
        else if (size <= 20) {

            /* Wait for it */
            prt("[Press ESC to exit.]", 23, 0);
        }

        /* Prompt -- large files */
        else {

            /* Wait for it */
            prt("[Press Return, Space, -, /, or ESC to exit.]", 23, 0);
        }

        /* Get a keypress */
        k = inkey();

        /* Hack -- return to last screen */
        if (k == '?') break;

        /* Hack -- try searching */
        if (k == '/') {
            prt("Find: ", 23, 0);
            if (askfor_aux(finder, 80)) {
                find = finder;
                back = line;
                line = line + 1;
            }
        }

        /* Hack -- Allow backing up */
        if (k == '-') {
            line = line - 10;
            if (line < 0) line = 0;
        }

        /* Hack -- Advance a single line */
        if ((k == '\n') || (k == '\r')) {
            line = line + 1;
        }

        /* Advance one page */
        if (k == ' ') {
            line = line + 20;
        }
        
        /* Recurse on numbers */
        if (menu && isdigit(k) && hook[k-'0'][0]) {

            /* Recurse on that file */
            if (!do_cmd_help_aux(hook[k-'0'], 0)) k = ESCAPE;
        }

        /* Exit on escape */
        if (k == ESCAPE) break;
    }

    /* Close the file */
    fclose(fff);

    /* Escape */
    if (k == ESCAPE) return (FALSE);

    /* Normal return */
    return (TRUE);
}


/*
 * Peruse the On-Line-Help, starting at the given file.
 */
void do_cmd_help(cptr name)
{
    /* Help is always free */
    energy_use = 0;

    /* Hack -- default file */
    if (!name) name = "help.hlp";

    /* Save the screen */
    save_screen();

    /* Peruse the main help file */
    (void)do_cmd_help_aux(name, 0);

    /* Restore the screen */
    restore_screen();
}




/*
 * Determine a "title" for the player
 */
cptr title_string()
{
    cptr p;

    if (p_ptr->lev < 1) {
        p = "Novice";
    }
    else if (p_ptr->lev <= MAX_PLAYER_LEVEL) {
        p = player_title[p_ptr->pclass][p_ptr->lev - 1];
    }
    else if (p_ptr->male) {
        p = "**KING**";
    }
    else {
        p = "**QUEEN**";
    }

    return p;
}


/*
 * Print the character to a file or device.
 */
int file_character(cptr filename1)
{
    int		i;
    int				fd = -1;
    inven_type			*i_ptr;
    cptr			p;
    cptr			colon = ":";
    cptr			blank = " ";

    FILE		*fp;

    int			tmp;
    int                 xbth, xbthb, xfos, xsrh;
    int			xstl, xdis, xsave, xdev;
    char                xinfra[32];

    int			show_tohit, show_todam;

    store_type		*st_ptr = &store[7];

    char		out_val[160];
    char		prt1[160];
    char		prt2[160];


    /* Drop priv's */
    safe_setuid_drop();

#ifdef MACINTOSH

    /* Global file type */
    _ftype = 'TEXT';

    /* Open the file (already verified by mac_file_character) */
    fp = fopen(filename1, "w");

#else

    fd = my_topen(filename1, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0 && errno == EEXIST) {
        (void)sprintf(out_val, "Replace existing file %s?", filename1);
        if (get_check(out_val)) {
            fd = my_topen(filename1, O_WRONLY, 0644);
        }
    }
    if (fd >= 0) {
        /* on some non-unix machines, fdopen() is not reliable, */
        /* hence must call close() and then fopen()  */
        (void)close(fd);
        fp = my_tfopen(filename1, "w");
    }
    else {
        fp = NULL;
    }

#endif


    /* Invalid file */
    if (!fp) {

        message(format("Cannot open '%s'.", filename1), 0);
        message(NULL, 0);

        /* Grab priv's */
        safe_setuid_grab();

        return FALSE;
    }


    /* Skill with current weapon */
    tmp = p_ptr->ptohit + inventory[INVEN_WIELD].tohit;
    xbth = p_ptr->bth + tmp * BTH_PLUS_ADJ +
           (class_level_adj[p_ptr->pclass][CLA_BTH] * p_ptr->lev);

    /* Skill with current bow */
    tmp = p_ptr->ptohit + inventory[INVEN_BOW].tohit;
    xbthb = p_ptr->bthb + tmp * BTH_PLUS_ADJ +
            (class_level_adj[p_ptr->pclass][CLA_BTHB] * p_ptr->lev);

    /* Basic abilities */
    xfos = 40 - p_ptr->fos;
    if (xfos < 0) xfos = 0;
    xsrh = p_ptr->srh;
    xstl = p_ptr->stl + 1;
    xdis = (p_ptr->disarm + todis_adj() + stat_adj(A_INT) +
            (class_level_adj[p_ptr->pclass][CLA_DISARM] * p_ptr->lev / 3));
    xsave = (p_ptr->save + stat_adj(A_WIS) +
             (class_level_adj[p_ptr->pclass][CLA_SAVE] * p_ptr->lev / 3));
    xdev = (p_ptr->save + stat_adj(A_INT) +
            (class_level_adj[p_ptr->pclass][CLA_DEVICE] * p_ptr->lev / 3));

    /* Infravision string */
    (void)sprintf(xinfra, "%d feet", p_ptr->see_infra * 10);

    /* Basic to hit/dam bonuses */
    show_tohit = p_ptr->dis_th;
    show_todam = p_ptr->dis_td;

    /* Check the weapon */
    i_ptr = &inventory[INVEN_WIELD];

    /* Hack -- add in weapon info if known */
    if (inven_known_p(i_ptr)) show_tohit += i_ptr->tohit;
    if (inven_known_p(i_ptr)) show_todam += i_ptr->todam;


    /* Notify user */
    message(format("Dumping character to '%s'...", filename1), 0);
    Term_fresh();

    colon = ":";
    blank = " ";

    fprintf(fp, " Name%9s %-23s", colon, player_name);
    fprintf(fp, "Age%11s %6d ", colon, (int)p_ptr->age);
    cnv_stat(p_ptr->use_stat[A_STR], prt1);
    fprintf(fp, "   STR : %s\n", prt1);
    fprintf(fp, " Race%9s %-23s", colon, rp_ptr->trace);
    fprintf(fp, "Height%8s %6d ", colon, (int)p_ptr->ht);
    cnv_stat(p_ptr->use_stat[A_INT], prt1);
    fprintf(fp, "   INT : %s\n", prt1);
    fprintf(fp, " Sex%10s %-23s", colon,
                  (p_ptr->male ? "Male" : "Female"));
    fprintf(fp, "Weight%8s %6d ", colon, (int)p_ptr->wt);
    cnv_stat(p_ptr->use_stat[A_WIS], prt1);
    fprintf(fp, "   WIS : %s\n", prt1);
    fprintf(fp, " Class%8s %-23s", colon,
                  cp_ptr->title);
    fprintf(fp, "Social Class : %6d ", p_ptr->sc);
    cnv_stat(p_ptr->use_stat[A_DEX], prt1);
    fprintf(fp, "   DEX : %s\n", prt1);
    fprintf(fp, " Title%8s %-23s", colon, title_string());
    fprintf(fp, "%22s", blank);
    cnv_stat(p_ptr->use_stat[A_CON], prt1);
    fprintf(fp, "   CON : %s\n", prt1);
    fprintf(fp, "%34s", blank);
    fprintf(fp, "%26s", blank);
    cnv_stat(p_ptr->use_stat[A_CHR], prt1);
    fprintf(fp, "   CHR : %s\n\n", prt1);

    fprintf(fp, " + To Hit    : %6d", show_tohit);
    fprintf(fp, "%7sLevel      :%9d", blank, (int)p_ptr->lev);
    fprintf(fp, "   Max Hit Points : %6d\n", p_ptr->mhp);
    fprintf(fp, " + To Damage : %6d", show_todam);
    fprintf(fp, "%7sExperience :%9ld", blank, (long)p_ptr->exp);
    fprintf(fp, "   Cur Hit Points : %6d\n", p_ptr->chp);
    fprintf(fp, " + To AC     : %6d", p_ptr->dis_tac);
    fprintf(fp, "%7sMax Exp    :%9ld", blank, (long)p_ptr->max_exp);
    fprintf(fp, "   Max Mana%8s %6d\n", colon, p_ptr->mana);
    fprintf(fp, "   Total AC  : %6d", p_ptr->dis_ac);

    if (p_ptr->lev >= MAX_PLAYER_LEVEL) {
        fprintf(fp, "%7sExp to Adv.:%9s", blank, "****");
    }
    else {
        fprintf(fp, "%7sExp to Adv.:%9ld", blank,
                      (long) (player_exp[p_ptr->lev - 1] *
                               p_ptr->expfact / 100L));
    }

    fprintf(fp, "   Cur Mana%8s %6d\n", colon, p_ptr->cmana);
    fprintf(fp, "%28sGold%8s%9ld\n", blank, colon, (long)p_ptr->au);


    /* Dump the misc. abilities */
    fprintf(fp, "\n(Miscellaneous Abilities)\n");
    fprintf(fp, " Fighting    : %-10s", likert(xbth, 12));
    fprintf(fp, "   Stealth     : %-10s", likert(xstl, 1));
    fprintf(fp, "   Perception  : %s\n", likert(xfos, 3));
    fprintf(fp, " Bows/Throw  : %-10s", likert(xbthb, 12));
    fprintf(fp, "   Disarming   : %-10s", likert(xdis, 8));
    fprintf(fp, "   Searching   : %s\n", likert(xsrh, 6));
    fprintf(fp, " Saving Throw: %-10s", likert(xsave, 6));
    fprintf(fp, "   Magic Device: %-10s", likert(xdev, 6));
    fprintf(fp, "   Infra-Vision: %s\n\n", xinfra);

    /* Write out the character's history     */
    fprintf(fp, "Character Background\n");
    for (i = 0; i < 4; i++) {
        fprintf(fp, " %s\n", history[i]);
    }

    fprintf(fp, "\n\n");

    /* Dump the equipment */
    fprintf(fp, "  [Character's Equipment List]\n\n");
    if (!equip_ctr) {
        fprintf(fp, "  Character has no equipment in use.\n");
    }
    else {
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++) {
            p = mention_use(i);
            objdes(prt2, &inventory[i], TRUE);
            fprintf(fp, "  %c) %-19s: %s\n", index_to_label(i), p, prt2);
        }
    }

    fprintf(fp, "\n\n");

    /* Dump the inventory */
    fprintf(fp, "  [General Inventory List]\n\n");
    if (!inven_ctr) {
        fprintf(fp, "  Character has no objects in inventory.\n");
    }
    else {
        for (i = 0; i < inven_ctr; i++) {
            objdes(prt2, &inventory[i], TRUE);
            fprintf(fp, "%c) %s\n", index_to_label(i), prt2);
        }
    }

    fprintf(fp, "\n\n");

    /* Dump the Home */
    fprintf(fp, "  [Home Inventory]\n\n");	
    if (st_ptr->store_ctr == 0) {
        fprintf(fp, "  Character has no objects at home.\n");
    }
    else {
        for (i = 0; i < st_ptr->store_ctr; i++) {
            if (i == 12) fprintf(fp, "\n");
            objdes(prt2, &st_ptr->store_item[i], TRUE);
            fprintf(fp, "%c) %s\n", (i%12)+'a', prt2);
        }
    }

    fprintf(fp, "\n");

    fclose(fp);

    message("Done.", 0);
    Term_fresh();
    
    /* Grab priv's */
    safe_setuid_grab();

    return TRUE;
}




#if defined(_Windows)

/*
 * Hack -- extract a "clean" savefile name from a Windows player name
 * This function takes a pointer to a buffer and modifies it in place.
 */
static void process_name(char *buf)
{
    int k;
    cptr s;

    /* Extract useful letters */
    for (k = 0, s = buf; *s; s++) {
        if (isalpha(*s) || isdigit(*s)) buf[k++] = *s;
        else if (*s == ' ') buf[k++] = '_';
    }

    /* Hack -- max length */
    if (k > 8) k = 8;

    /* Terminate */
    buf[k] = '\0';

    /* Prevent "empty" savefile names */
    if (!buf[0]) strcpy(buf, "PLAYER");
}

#endif



/*
 * Gets a name for the character, reacting to name changes.
 *
 * Assumes that "display_player()" has just been called
 * XXX Perhaps we should NOT ask for a name (at "birth()") on Unix?
 */
void get_name()
{
    char tmp[32];

    /* Prompt and ask */
    prt("Enter your player's name above [press <RETURN> when finished]", 21, 2);

    /* Ask until happy */
    while (1) {

        /* Go to the "name" field */
        move_cursor(2, 15);
        
        /* Save the player name */
        strcpy(tmp, player_name);
        
        /* Get an input, ignore "Escape" */
        if (askfor_aux(tmp, 15)) strcpy(player_name, tmp);
        
        /* Hack -- Prevent "empty" names */
        if (!player_name[0]) strcpy(player_name, "PLAYER");

#ifdef SAVEFILE_MUTABLE

#if defined(_Windows)
        /* Hack -- Process the player name */
        process_name(tmp);
#else
        /* Normally, just use the whole name */
        strcpy(tmp, player_name);
#endif

#ifdef SAVEFILE_USE_UID
        /* Rename the savefile, using the player UID and character name */
        (void)sprintf(savefile, "%s%s%d%s",
                        ANGBAND_DIR_SAVE, PATH_SEP, player_uid, tmp);
#else
        /* Rename the savefile, using the character name */
        (void)sprintf(savefile, "%s%s%s",
                        ANGBAND_DIR_SAVE, PATH_SEP, tmp);
#endif

#endif

        /* All done */
        break;
    }

    /* Pad the name (to clear junk) */
    sprintf(tmp, "%-15.15s", player_name);

    /* Re-Draw the name (in light blue) */
    c_put_str(TERM_L_BLUE, tmp, 2, 15);

    /* Erase the prompt, etc */
    clear_from(20);
}


/*
 * Display character, allow name change or character dump.
 */
void change_name()
{
    char	c;

    int		flag;

    char	temp[160];


    display_player();

    for (flag = FALSE; !flag; ) {

        prt("<f>ile character description. <c>hange character name.", 21, 2);
        c = inkey();
        switch (c) {

          case 'c':
            get_name();
            flag = TRUE;
            break;

          case 'f':
#ifdef MACINTOSH
            if (mac_file_character()) flag = TRUE;
#else
            prt("File name:", 0, 0);
            sprintf(temp, "%s.txt", player_name);
            if (askfor_aux(temp, 60) && temp[0]) {
                if (file_character(temp)) flag = TRUE;
            }
#endif
            break;

          case ESCAPE:
          case ' ':
          case '\n':
          case '\r':
            flag = TRUE;
            break;

          default:
            bell();
            break;
        }
    }
}



/*
 * Hack -- Calculates the total number of points earned		-JWT-	
 */
long total_points(void)
{
    return (p_ptr->max_exp + (100 * p_ptr->max_dlv));
}



/*
 * Centers a string within a 31 character string		-JWT-	
 */
static char *center_string(char *centered_str, cptr in_str)
{
    int i, j;

    i = strlen(in_str);
    j = 15 - i / 2;
    (void)sprintf(centered_str, "%*s%s%*s", j, "", in_str, 31 - i - j, "");
    return centered_str;
}


/*
 * Save a "bones" file for a dead character
 * Should probably attempt some form of locking...
 */
static void make_bones(void)
{
    FILE                *fp;

    char                str[1024];


    /* Dead non-wizards create a bones file */
    if (death && !(noscore & 0x0002)) {

        /* Ignore people who die in town */
        if (dun_level) {

            /* XXX Perhaps the player's level should be taken into account */

            /* Get the proper "Bones File" name */
            sprintf(str, "%s%s%d", ANGBAND_DIR_BONE, PATH_SEP, dun_level);

            /* Attempt to open the bones file */
            fp = my_tfopen(str, "r");

            /* Close it right away */
            if (fp) fclose(fp);

            /* Do not over-write a previous ghost */
            if (fp) return;

#ifdef MACINTOSH
            _ftype = 'TEXT';
#endif

            /* Try to write a new "Bones File" */
            fp = my_tfopen(str, "w");

            /* Not allowed to write it?  Weird. */
            if (!fp) return;

            /* Save the info */
            fprintf(fp, "%s\n", player_name);
            fprintf(fp, "%d\n", p_ptr->mhp);
            fprintf(fp, "%d\n", p_ptr->prace);
            fprintf(fp, "%d\n", p_ptr->pclass);

            /* Close and save the Bones file */
            fclose(fp);
        }
    }
}


/*
 * Prints the gravestone of the character  -RAK-
 */
static void print_tomb()
{
    int		i;
    cptr	p;

    char	day[32];

    char	str[160];
    char	tmp_str[160];

    time_t	ct = time((time_t)0);


    /* Draw a tombstone */
    clear_screen();
    put_str("_______________________", 1, 15);
    put_str("/", 2, 14);
    put_str("\\         ___", 2, 38);
    put_str("/", 3, 13);
    put_str("\\ ___   /   \\      ___", 3, 39);
    put_str("/            RIP            \\   \\  :   :     /   \\", 4, 12);
    put_str("/", 5, 11);
    put_str("\\  : _;,,,;_    :   :", 5, 41);
    (void)sprintf(str, "/%s\\,;_          _;,,,;_",
                  center_string(tmp_str, player_name));
    put_str(str, 6, 10);
    put_str("|               the               |   ___", 7, 9);
    p = total_winner ? "Magnificent" : title_string();
    (void)sprintf(str, "| %s |  /   \\", center_string(tmp_str, p));
    put_str(str, 8, 9);
    put_str("|", 9, 9);
    put_str("|  :   :", 9, 43);
    if (!total_winner) {
        p = cp_ptr->title;
    }
    else if (p_ptr->male) {
        p = "*King*";
    }
    else {
        p = "*Queen*";
    }

    (void)sprintf(str, "| %s | _;,,,;_   ____", center_string(tmp_str, p));
    put_str(str, 10, 9);
    (void)sprintf(str, "Level : %d", (int)p_ptr->lev);
    (void)sprintf(str, "| %s |          /    \\", center_string(tmp_str, str));
    put_str(str, 11, 9);
    (void)sprintf(str, "%ld Exp", (long)p_ptr->exp);
    (void)sprintf(str, "| %s |          :    :", center_string(tmp_str, str));
    put_str(str, 12, 9);
    (void)sprintf(str, "%ld Au", (long)p_ptr->au);
    (void)sprintf(str, "| %s |          :    :", center_string(tmp_str, str));
    put_str(str, 13, 9);
    (void)sprintf(str, "Died on Level : %d", dun_level);
    (void)sprintf(str, "| %s |         _;,,,,;_", center_string(tmp_str, str));
    put_str(str, 14, 9);
    put_str("|            killed by            |", 15, 9);
    p = died_from;
    i = strlen(p);
    died_from[i] = '.';			   /* add a trailing period */
    died_from[i + 1] = '\0';
    (void)sprintf(str, "| %s |", center_string(tmp_str, p));
    put_str(str, 16, 9);
    died_from[i] = '\0';		   /* strip off the period */
    sprintf(day, "%-.24s", ctime(&ct));
    (void)sprintf(str, "| %s |", center_string(tmp_str, day));
    put_str(str, 17, 9);
    put_str("*|   *     *     *    *   *     *  | *", 18, 8);
    put_str("________)/\\\\_)_/___(\\/___(//_\\)/_\\//__\\\\(/_|_)_______",
               19, 0);
}


/*
 * Display some character info
 */
static void show_info(void)
{
    int			i, j, k;

    inven_type		*i_ptr;
    store_type		*st_ptr;

    char		t1[160];
    char		t2[160];
    char		str[160];


    /* Access the home */
    st_ptr = &store[7];

    /* Hack -- Know everything in the inven/equip */
    for (i = 0; i < INVEN_TOTAL; i++) {
        i_ptr = &inventory[i];
        if (!i_ptr || !i_ptr->tval) continue;
        inven_aware(i_ptr);
        inven_known(i_ptr);
    }

    /* Hack -- Know everything in the home */
    for (i = 0; i < st_ptr->store_ctr; i++) {
        i_ptr = &st_ptr->store_item[i];
        inven_aware(i_ptr);
        inven_known(i_ptr);
    }

    /* Hack -- Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);
    
    /* Handle non-visual stuff */
    handle_stuff(FALSE);


    /* Flush all input keys */
    Term_flush();


    /* Describe options */
    prt("You may now dump a character record to one or more files.", 21, 0);
    prt("Then, hit RETURN to see the character, or ESCAPE to abort.", 22, 0);

    /* Dump character records as requested */
    while (TRUE) {

        /* Flush old messages */
        msg_flag = FALSE;

        /* Get a filename (or escape) */
        put_str("Filename: ", 23, 0);
        if (!askfor(str, 60)) return;

        /* Return means "show on screen" */
        if (!str[0]) break;

        /* Dump a character file */
        file_character(str);
    }


    /* Show player on screen */
    clear_screen();
    display_player();

    /* Prompt for inventory */
    prt("Hit any key to see more information (ESCAPE to abort): ", 23, 0);

    /* Allow abort at this point */
    if (inkey() == ESCAPE) return;


    /* Show equipment and inventory */

    /* Equipment -- if any */
    if (equip_ctr) {
        clear_screen();
        msg_print("You are using:");
        show_equip(INVEN_WIELD, INVEN_TOTAL-1);
        msg_print(NULL);
    }

    /* Inventory -- if any */
    if (inven_ctr) {
        clear_screen();
        msg_print("You are carrying:");
        show_inven(0, inven_ctr - 1);
        msg_print(NULL);
    }



    /* Home -- if anything there */
    if (st_ptr->store_ctr) {

        /* show home's inventory... */
        for (k = 0, i = 0; i < st_ptr->store_ctr; k++) {

            /* What did they hoard? */
            clear_screen();
            sprintf(t2, "You have stored at your house (page %d):", k+1);
            msg_print(t2);

            /* Show 12 (XXX) items (why bother with indexes?) */
            for (j = 0; j < 12 && i < st_ptr->store_ctr; j++, i++) {
                i_ptr = &st_ptr->store_item[i];
                objdes(t1, i_ptr, TRUE);
                sprintf(t2, "%c) ", 'a'+j);
                prt(t2, j+2, 4);
                c_prt(tval_to_attr[i_ptr->tval], t1, j+2, 7);
            }

            /* Flush it */
            msg_print(NULL);
        }
    }
}


/*
 * Display the scores in a given range.
 * Assumes the high score list is already open.
 * Only five entries per line, too much info.
 *
 * Mega-Hack -- allow "fake" entry at the given position.
 */
static void display_scores_aux(int from, int to, int note, high_score *score)
{
    int		i, j, k, n, attr, place;

    high_score	the_score;

    char	buf[256];
    char	tmp_str[160];


    /* Assume we will show the first 10 */
    if (from < 0) from = 0;
    if (to < 0) to = 10;
    if (to > MAX_HISCORES) to = MAX_HISCORES;


    /* Seek to the beginning */
    if (highscore_seek(0)) return;

    /* Hack -- Count the high scores */
    for (i = 0; i < MAX_HISCORES; i++) {
        if (highscore_read(&the_score)) break;
    }

    /* Hack -- allow "fake" entry to be last */
    if ((note == i) && score) i++;

    /* Forget about the last entries */
    if (i > to) i = to;


    /* Show 5 per page, until "done" */
    for (k = from, place = k+1; k < i; k += 5) {

        /* Clear those */
        clear_screen();

        put_str("                Angband Hall of Fame", 0, 0);

        /* Indicate non-top scores */
        if (k > 0) {
            sprintf(tmp_str, "(from position %d)", k + 1);
            put_str(tmp_str, 0, 40);
        }

        /* Dump 5 entries */
        for (j = k, n = 0; j < i && n < 5; place++, j++, n++) {

            int pr, pc, clev, mlev, cdun, mdun;

            cptr user, gold, when, aged;


            /* Hack -- indicate death in yellow */
            attr = (j == note) ? TERM_YELLOW : TERM_WHITE;


            /* Mega-Hack -- insert a "fake" record */
            if ((note == j) && score) {
                the_score = (*score);
                attr = TERM_L_GREEN;
                score = NULL;
                note = -1;
                j--;
            }

            /* Read a normal record */
            else {
                /* Read the proper record */
                if (highscore_seek(j)) break;
                if (highscore_read(&the_score)) break;
            }

            /* Extract the race/class */
            pr = atoi(the_score.p_r);
            pc = atoi(the_score.p_c);

            /* Extract the level info */
            clev = atoi(the_score.cur_lev);
            mlev = atoi(the_score.max_lev);
            cdun = atoi(the_score.cur_dun);
            mdun = atoi(the_score.max_dun);

            /* Hack -- extract the gold and such */
            for (user = the_score.uid; isspace(*user); user++);
            for (when = the_score.day; isspace(*when); when++);
            for (gold = the_score.gold; isspace(*gold); gold++);
            for (aged = the_score.turns; isspace(*aged); aged++);

            /* Dump some info */
            sprintf(buf, "%3d.%9s  %s the %s %s, Level %d",
                    place, the_score.pts, the_score.who,
                    race_info[pr].trace, class_info[pc].title,
                    clev);

            /* Append a "maximum level" */
            if (mlev > clev) strcat(buf, format(" (Max %d)", mlev));

            /* Dump the first line */
            c_put_str(attr, buf, n*4 + 2, 0);

            /* Another line of info */
            sprintf(buf, "               Killed by %s on %s %d",
                    the_score.how, "Dungeon Level", cdun);

            /* Hack -- some people die in the town */
            if (!cdun) {
                sprintf(buf, "               Killed by %s in the Town",
                        the_score.how);
            }

            /* Append a "maximum level" */
            if (mdun > cdun) strcat(buf, format(" (Max %d)", mdun));

            /* Dump the info */
            c_put_str(attr, buf, n*4 + 3, 0);

            /* And still another line of info */
            sprintf(buf, "               (User %s, Date %s, Gold %s, Turn %s).",
                    user, when, gold, aged);
            c_put_str(attr, buf, n*4 + 4, 0);
        }


        /* Wait for response. */
        prt("[Press ESC to quit, any other key to continue.]", 23, 17);
        j = inkey();
        prt("", 23, 0);

        /* Hack -- notice Escape */
        if (j == ESCAPE) break;
    }
}


/*
 * Display the scores in a given range.
 * Assumes the high score list is already open.
 *
 * We may want to be slightly more "clean" on empty score lists.
 */
void display_scores(int from, int to)
{
    display_scores_aux(from, to, -1, NULL);
}



/*
 * Enters a players name on a hi-score table, if "legal".
 */
static errr top_twenty(void)
{
    int          j;

    high_score   the_score;

    time_t ct = time((time_t*)0);


    /* Wipe screen */
    clear_screen();

#ifndef SCORE_WIZARDS
    /* Wizard-mode pre-empts scoring */
    if (noscore & 0x000F) {
        msg_print("Score not registered for wizards.");
        display_scores(0, 10);
        return (0);
    }
#endif

#ifndef SCORE_CHEATERS
    /* Cheaters are not scored */
    if (noscore & 0xFF00) {
        msg_print("Score not registered for cheaters.");
        display_scores(0, 10);
        return (0);
    }
#endif

    /* Interupted */
    if (!total_winner && streq(died_from, "Interrupting")) {
        msg_print("Score not registered due to interruption.");
        display_scores(0, 10);
        return (0);
    }

    /* Quitter */
    if (!total_winner && streq(died_from, "Quitting")) {
        msg_print("Score not registered due to quitting.");
        display_scores(0, 10);
        return (0);
    }


    /* Clear the record */
    WIPE(&the_score, high_score);

    /* Save the version */
    sprintf(the_score.what, "%u.%u.%u",
            CUR_VERSION_MAJ, CUR_VERSION_MIN, CUR_PATCH_LEVEL);

    /* Calculate and save the points */
    sprintf(the_score.pts, "%9lu", (long)total_points());
    the_score.pts[9] = '\0';
    
    /* Save the current gold */
    sprintf(the_score.gold, "%9lu", (long)p_ptr->au);
    the_score.gold[9] = '\0';

    /* Save the current turn */
    sprintf(the_score.turns, "%9lu", (long)turn);
    the_score.turns[9] = '\0';

#ifdef HIGHSCORE_DATE_HACK
    /* Save the date in a hacked up form (9 chars) */
    sprintf(the_score.day, "%-.6s %-.2s", ctime(&ct) + 4, ctime(&ct) + 22);
#else
    /* Save the date in standard form (8 chars) */
    strftime(the_score.day, 9, "%m/%d/%y", localtime(&ct));
#endif

    /* Save the player name (15 chars) */
    sprintf(the_score.who, "%-.15s", player_name);

    /* Save the player info */
    sprintf(the_score.uid, "%7u", player_uid);
    sprintf(the_score.sex, "%c", (p_ptr->male ? 'm' : 'f'));
    sprintf(the_score.p_r, "%2d", p_ptr->prace);
    sprintf(the_score.p_c, "%2d", p_ptr->pclass);

    /* Save the level and such */
    sprintf(the_score.cur_lev, "%3d", p_ptr->lev);
    sprintf(the_score.cur_dun, "%3d", dun_level);
    sprintf(the_score.max_lev, "%3d", p_ptr->max_plv);
    sprintf(the_score.max_dun, "%3d", p_ptr->max_dlv);

    /* Save the cause of death (31 chars) */
    sprintf(the_score.how, "%-.31s", died_from);


    /* Ignore "suspend" signal */
    signals_ignore_tstp();

    /* Lock the highscore file, or fail */
    if (highscore_lock()) return (1);

    /* Add a new entry to the score list, see where it went */
    j = highscore_add(&the_score);

    /* Unlock the highscore file */
    highscore_unlock();


    /* Hack -- Display the top fifteen scores */
    if (j < 10) {
        display_scores_aux(0, 15, j, NULL);
    }

    /* Display the scores surrounding the player */
    else {
        display_scores_aux(0, 5, j, NULL);
        display_scores_aux(j - 2, j + 7, j, NULL);
    }


    /* Success */
    return (0);
}


/*
 * Predict the players location, and display it.
 */
static errr predict_score(void)
{
    int          j;

    high_score   the_score;


    /* Save the version */
    sprintf(the_score.what, "%u.%u.%u",
            CUR_VERSION_MAJ, CUR_VERSION_MIN, CUR_PATCH_LEVEL);

    /* Calculate and save the points */
    sprintf(the_score.pts, "%9lu", (long)total_points());

    /* Save the current gold */
    sprintf(the_score.gold, "%9lu", (long)p_ptr->au);

    /* Save the current turn */
    sprintf(the_score.turns, "%9lu", (long)turn);

    /* Hack -- no time needed */
    strcpy(the_score.day, "TODAY");

    /* Save the player name (15 chars) */
    sprintf(the_score.who, "%-.15s", player_name);

    /* Save the player info */
    sprintf(the_score.uid, "%7u", player_uid);
    sprintf(the_score.sex, "%c", (p_ptr->male ? 'm' : 'f'));
    sprintf(the_score.p_r, "%2d", p_ptr->prace);
    sprintf(the_score.p_c, "%2d", p_ptr->pclass);

    /* Save the level and such */
    sprintf(the_score.cur_lev, "%3d", p_ptr->lev);
    sprintf(the_score.cur_dun, "%3d", dun_level);
    sprintf(the_score.max_lev, "%3d", p_ptr->max_plv);
    sprintf(the_score.max_dun, "%3d", p_ptr->max_dlv);

    /* Hack -- no cause of death */
    strcpy(the_score.how, "nobody (yet!)");


    /* See where the entry would be placed */
    j = highscore_where(&the_score);


    /* Hack -- Display the top fifteen scores */
    if (j < 10) {
        display_scores_aux(0, 15, j, &the_score);
    }

    /* Display some "useful" scores */
    else {
        display_scores_aux(0, 5, -1, NULL);
        display_scores_aux(j - 2, j + 7, j, &the_score);
    }


    /* Success */
    return (0);
}








/*
 * Change the player into a King!			-RAK-	
 */
static void kingly()
{
    cptr p;

    /* Hack -- retire in town */
    dun_level = 0;

    /* Change the character attributes.		 */
    (void)strcpy(died_from, "Ripe Old Age");
    (void)restore_level();
    p_ptr->lev += MAX_PLAYER_LEVEL;
    p_ptr->au += 250000L;
    p_ptr->max_exp += 5000000L;
    p_ptr->exp = p_ptr->max_exp;

    /* Let the player know that he did good.	 */
    clear_screen();
    put_str("#", 1, 34);
    put_str("#####", 2, 32);
    put_str("#", 3, 34);
    put_str(",,,  $$$  ,,,", 4, 28);
    put_str(",,=$   \"$$$$$\"   $=,,", 5, 24);
    put_str(",$$        $$$        $$,", 6, 22);
    put_str("*>         <*>         <*", 7, 22);
    put_str("$$         $$$         $$", 8, 22);
    put_str("\"$$        $$$        $$\"", 9, 22);
    put_str("\"$$       $$$       $$\"", 10, 23);
    p = "*#########*#########*";
    put_str(p, 11, 24);
    put_str(p, 12, 24);
    put_str("Veni, Vidi, Vici!", 15, 26);
    put_str("I came, I saw, I conquered!", 16, 21);
    if (p_ptr->male) {
        put_str("All Hail the Mighty King!", 17, 22);
    }
    else {
        put_str("All Hail the Mighty Queen!", 17, 22);
    }

    Term_flush();

    pause_line(23);
}


/*
 * Exit the game
 *   Save the player to a save file
 *   Display a gravestone / death info
 *   Display the top twenty scores
 *
 * Note -- the player does not have to be dead.
 */
void exit_game(void)
{
    /* Flush the messages */
    msg_print(NULL);

    /* Flush the input */
    Term_flush();

    /* No suspending now */
    signals_ignore_tstp();


    /* XXX This may never happen */
    if (!turn) {
        display_scores(0, 10);
        prt("", 23, 0);
        quit(NULL);
    }


    /* Handle death */
    if (death) {

        /* Add a message to the message recall */
        message_new(" ", -1);
        message_new("================", -1);
        message_new("=== You died ===", -1);
        message_new("================", -1);
        message_new(" ", -1);

        /* Handle retirement */
        if (total_winner) kingly();

        /* Dump bones file */
        make_bones();

        /* You are dead */
        print_tomb();

        /* Show more info */
        show_info();

        /* Handle score, show Top scores */
        top_twenty();

        /* Save the player or his memories */
        (void)save_player();
    }

    /* Still alive */
    else {

        /* Begin the save */
        msg_print("Saving game...");

        /* Not dead yet */
        (void)strcpy(died_from, "(saved)");

        /* Save the player. */
        (void)save_player();

        /* Predict the player's score (allow "escape") */
        msg_flag = FALSE;
        prt("Saving game... done.  Press Return. ", 0, 0);
        if (inkey() != ESCAPE) predict_score();	
    }


    /* Actually stop the process */
    quit(NULL);
}


/*
 * Handle abrupt death of the visual system
 * This routine is called only in very rare situations
 */
void exit_game_panic(void)
{
    /* If nothing important has happened, just quit */
    if (!character_generated || character_saved) quit("end of input");

    /* Hack -- prevent infinite loops */
    msg_flag = FALSE;

    /* Hack -- turn off some things */
    disturb(1, 0);

    /* Panic save */
    panic_save = 1;

    /* XXX XXX Hack -- clear the death flag when creating a HANGUP */
    /* save file so that player can see tombstone when restart. */

    /* Hack -- Not dead yet */
    death = FALSE;

    /* Panic save */
    (void)strcpy(died_from, "(end of input: panic saved)");
    if (save_player()) quit("end of input: panic save succeeded");

    /* Unsuccessful panic save.  Oh well. */
    quit("end of input: panic save failed!");
}


