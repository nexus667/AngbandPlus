/* File: main-x11.c */

/* Purpose: One (awful) way to run Angband under X11	-BEN- */


#include "angband.h"


#ifdef USE_X11


#include "z-util.h"
#include "z-virt.h"
#include "z-form.h"


#ifndef __MAKEDEPEND__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#endif /* __MAKEDEPEND__ */


/*
 * Notes on Colors:
 *
 *   1) On a monochrome (or "fake-monochrome") display, all colors
 *   will be "cast" to "fg," except for the bg color, which is,
 *   obviously, cast to "bg".  Thus, one can ignore this setting.
 *
 *   2) Because of the inner functioning of the color allocation
 *   routines, colors may be specified as (a) a typical color name,
 *   (b) a hexidecimal color specification (preceded by a pound sign),
 *   or (c) by strings such as "fg", "bg", "zg".
 *
 *   3) Due to the workings of the init routines, many colors
 *   may also be dealt with by their actual pixel values.  Note that
 *   the pixel with all bits set is "zg = (1<<metadpy->depth)-1", which
 *   is not necessarily either black or white.
 */





/**** Available Types ****/

/*
 * An X11 pixell specifier
 */
typedef unsigned long Pixell;

/*
 * The structures defined below
 */
typedef struct _metadpy metadpy;
typedef struct _infowin infowin;
typedef struct _infoclr infoclr;
typedef struct _infofnt infofnt;


/*
 * A structure summarizing a given Display.
 *
 *	- The Display itself
 *	- The default Screen for the display
 *	- The virtual root (usually just the root)
 *	- The default colormap (from a macro)
 *
 *	- The "name" of the display
 *
 *	- The socket to listen to for events
 *
 *	- The width of the display screen (from a macro)
 *	- The height of the display screen (from a macro)
 *	- The bit depth of the display screen (from a macro)
 *
 *	- The black Pixell (from a macro)
 *	- The white Pixell (from a macro)
 *
 *	- The background Pixell (default: black)
 *	- The foreground Pixell (default: white)
 *	- The maximal Pixell (Equals: ((2 ^ depth)-1), is usually ugly)
 *
 *	- Bit Flag: Force all colors to black and white (default: !color)
 *	- Bit Flag: Allow the use of color (default: depth > 1)
 *	- Bit Flag: We created 'dpy', and so should nuke it when done.
 */

struct _metadpy {

  Display	*dpy;
  Screen	*screen;
  Window	root;
  Colormap	cmap;

  char		*name;

  int		fd;

  uint		width;
  uint		height;
  uint		depth;

  Pixell	black;
  Pixell	white;

  Pixell	bg;
  Pixell	fg;
  Pixell	zg;

  uint		mono:1;
  uint		color:1;
  uint		nuke:1;
};



/*
 * A Structure summarizing Window Information.
 *
 * I assume that a window is at most 30000 pixels on a side.
 * I assume that the root windw is also at most 30000 square.
 *
 *	- The Window
 *	- The current Input Event Mask
 *
 *	- The location of the window
 *	- The width, height of the window
 *	- The border width of this window
 *
 *	- Byte: 1st Extra byte
 *
 *	- Bit Flag: This window is currently Mapped
 *	- Bit Flag: This window needs to be redrawn
 *	- Bit Flag: This window has been resized
 *
 *	- Bit Flag: We should nuke 'win' when done with it
 *
 *	- Bit Flag: 1st extra flag
 *	- Bit Flag: 2nd extra flag
 *	- Bit Flag: 3rd extra flag
 *	- Bit Flag: 4th extra flag
 */

struct _infowin {

  Window		win;
  long			mask;

  s16b			x, y;
  s16b			w, h;
  u16b			b;

  byte			byte1;

  uint			mapped:1;
  uint			redraw:1;
  uint			resize:1;

  uint			nuke:1;

  uint			flag1:1;
  uint			flag2:1;
  uint			flag3:1;
  uint			flag4:1;
};






/*
 * A Structure summarizing Operation+Color Information
 *
 *	- The actual GC corresponding to this info
 *
 *	- The Foreground Pixell Value
 *	- The Background Pixell Value
 *
 *	- Num (0-15): The operation code (As in Clear, Xor, etc)
 *	- Bit Flag: The GC is in stipple mode
 *	- Bit Flag: Destroy 'gc' at Nuke time.
 */

struct _infoclr {

  GC			gc;

  Pixell		fg;
  Pixell		bg;

  uint			code:4;
  uint			stip:1;
  uint			nuke:1;
};



/*
 * A Structure to Hold Font Information
 *
 *	- The 'XFontStruct*' (yields the 'Font')
 *
 *	- The font name
 *
 *	- The default character width
 *	- The default character height
 *	- The default character ascent
 *
 *	- Byte: Pixel offset used during fake mono
 *
 *	- Flag: Force monospacing via 'wid'
 *	- Flag: Nuke info when done
 */

struct _infofnt {

  XFontStruct	*info;

  cptr			name;

  s16b			wid;
  s16b			hgt;
  s16b			asc;

  byte			off;

  uint			mono:1;
  uint			nuke:1;
};





/* OPEN: x-metadpy.h */




/**** Available Functions ****/

/* Initialize a new metadpy */
extern errr Metadpy_init_2();

/* Nuke an existing metadpy */
extern errr Metadpy_nuke();



/**** Available Macros ****/


/* Set current metadpy (Metadpy) to 'M' */
#define Metadpy_set(M) \
        Metadpy = M


/* Initialize 'M' using Display 'D' */
#define Metadpy_init_dpy(D) \
        Metadpy_init_2(D,cNULL)

/* Initialize 'M' using a Display named 'N' */
#define Metadpy_init_name(N) \
        Metadpy_init_2((Display*)(NULL),N)

/* Initialize 'M' using the standard Display */
#define Metadpy_init() \
        Metadpy_init_name("")



/**** Other Functions ****/

/* Do Flush/Sync/Discard */
extern errr Metadpy_update (/*F,S,D*/);

/* Prepare/Produce a sound */
extern errr Metadpy_prepare_sound (/*V,P,D*/);
extern errr Metadpy_produce_sound (/*V*/);

/* Make a beep */
extern errr Metadpy_do_beep ();


/* SHUT: x-metadpy.h */


/* OPEN: x-infowin.h */



/**** Available Functions ****/

extern errr Infowin_nuke ();
extern errr Infowin_init_real ();
extern errr Infowin_init_data ();

extern errr Infowin_set_name ();
extern errr Infowin_set_icon_name ();


/**** Passable Functions for ADT's and such ***/

extern vptr infowin_datakey(/*W*/);
extern uint infowin_keyhash(/*K,S*/);
extern int infowin_keycomp(/*K1,K2*/);


/**** Available Macros ****/

/* Init an infowin by giving father as an (info_win*) (or NULL), and data */
#define Infowin_init_dad(D,X,Y,W,H,B,FG,BG) \
        Infowin_init_data(((D) ? ((D)->win) : (Window)(None)), \
        X,Y,W,H,B,FG,BG)


/* Init a top level infowin by pos,size,bord,Colors */
#define Infowin_init_top(X,Y,W,H,B,FG,BG) \
        Infowin_init_data(None,X,Y,W,H,B,FG,BG)


/* Request a new standard window by giving Dad infowin and X,Y,W,H */
#define Infowin_init_std(D,X,Y,W,H,B) \
        Infowin_init_dad(D,X,Y,W,H,B,Metadpy->fg,Metadpy->bg)


/* Set the current Infowin */
#define Infowin_set(I) \
        (Infowin = (I))



/* SHUT: x-infowin.h */


/* OPEN: x-infoclr.h */



/**** Available Functions ****/

/* Convert an Opcode name to an Opcode (i.e. "cpy") */
extern int Infoclr_Opcode(/*N*/);

/* Convert a Pixell Name to a Pixell (i.e. "red") */
extern Pixell Infoclr_Pixell(/*N*/);


/* Nuke Infoclr */
extern errr Infoclr_nuke();

/* Initialize Infoclr from a 'gc' */
extern errr Infoclr_init_1(/*G*/);

/* Initialize Infoclr from some data (Op, Fg, Bg, Stip) */
extern errr Infoclr_init_data(/*F,B,O,S*/);



/**** Available Macros  ****/

/* Set the current Infoclr */
#define Infoclr_set(C) \
        (Infoclr = (C))



/**** Available Macros (Requests) ****/

#define Infoclr_init_ppo(F,B,O,M) \
        Infoclr_init_data(F,B,O,M)

#define Infoclr_init_cco(F,B,O,M) \
        Infoclr_init_ppo(Infoclr_Pixell(F),Infoclr_Pixell(B),O,M)

#define Infoclr_init_ppn(F,B,O,M) \
        Infoclr_init_ppo(F,B,Infoclr_Opcode(O),M)

#define Infoclr_init_ccn(F,B,O,M) \
        Infoclr_init_cco(F,B,Infoclr_Opcode(O),M)


/* SHUT: x-infoclr.h */


/* OPEN: x-infofnt.h */



/**** Available Functions ****/

extern errr Infofnt_nuke();
extern errr Infofnt_init_real();
extern errr Infofnt_init_data();



/**** Available Macros ****/

/* Set the current infofnt */
#define Infofnt_set(I) \
        (Infofnt = (I))


/* SHUT: x-infofnt.h */



/* OPEN: r-metadpy.h */

/**** Available Functions ****/

extern Pixmap Pixmap_from_data (/*bits,w,h*/);
extern Cursor Cursor_from_data (/*bits,mask,w,h,x,y*/);


/* SHUT: r-metadpy.h */


/* OPEN: r-infowin.h */

/**** Available Functions ****/


/* Select events for an Infowin */
extern errr Infowin_set_mask(/*M*/);


/* Map/Unmap an Infowin */
extern errr Infowin_map();
extern errr Infowin_unmap();

/* Raise/Lower Infowin */
extern errr Infowin_raise();
extern errr Infowin_lower();

/* Impell Infowin to a new location */
extern errr Infowin_impell(/*X,Y*/);

/* Resize Infowin to a new size */
extern errr Infowin_resize(/*W,H*/);

/* Impell AND Resize an info_win to a new location & size */
extern errr Infowin_locate(/*X,Y,W,H*/);


/* Wipe an info_win clean */
extern errr Infowin_wipe();

/* Fill an info_win */
extern errr Infowin_fill();

/* Refresh an info_win by a weird Root based) method */
extern errr Infowin_refresh();


#if 0

/* Copy a Pixmap onto an info_win */
extern errr Metadpy_draw_pixmap(/*X,Y,W,H,P*/);

#endif



/**** Available macros ****/

/* Errr: Expose Infowin */
#define Infowin_expose() \
        (!(Infowin->redraw = 1))

/* Errr: Unxpose Infowin */
#define Infowin_unexpose() \
        (Infowin->redraw = 0)


/* SHUT: r-infowin.h */


/* OPEN: r-infoclr.h */

/*
 *
 * Optional support for the infoclr structure
 *
 * Functions included deal with drawing/filling lines and shapes.
 *
 * There are two types of routines: 'draw' and 'fill' routines.
 *
 * The routines are named 'Infoclr_OPER_TYPE_MODE (ARGS)'
 *	Ex: Infoclr_draw_line_8 (X1,Y1,X2,Y2)
 *
 * The arguments are:
 *	W:    An (info_win*) saying where to do it
 *	C:    An (info_clr*) saying how (what color, etc) to do it
 *	ARGS: Various parms, like Coordinates (see 'MODE' below)
 *
 * The OPER's are:
 *	draw: Usually involve drawing an outline (or a line/ point)
 *	fill: Usually involve filling something
 *
 * The TYPE's are:
 *	point: A single point (no 'MODE' allowed)
 *	line:  A line (mode 8 == mode 4)
 *	rect:  A rectangle (see 'square')
 *	oval:  An oval (see 'circle')
 *
 * The MODE's are:
 *	0: Takes a center point and a radius
 *	1: Takes a top left point and a side length
 *	2: Takes a top left point and a width+height
 *	4: Takes a top left point and a bottom right point
 *	8: Takes any two points, no ordering necessary
 *
 * General note:
 *	Keep in mind that due to the functioning of internal routines,
 *	filling a rectangle is one pixel different from drawing it.
 *	This could perhaps be fixed via changing the params to draw.
 */



/**** Available Functions ****/

/* Draw a Rectangle around Non Ordered Endpoints */
extern errr Infoclr_draw_rect_8 (/*X1,Y1,X2,Y2*/);

/* Draw an Oval around Non Ordered Endpoints */
extern errr Infoclr_draw_oval_8 (/*X1,Y1,X2,Y2*/);


/* Draw a pixmap (P) at (X,Y) with size (W,H) */
extern errr Infoclr_draw_pixmap(/*X,Y,W,H,P*/);




/**** Available Macros (that Map directly to X commands) ****/

#define Infoclr_draw_point(X,Y) \
        (XDrawPoint(Metadpy->dpy,Infowin->win,Infoclr->gc,X,Y),0)

#define Infoclr_draw_line_4(X1,Y1,X2,Y2) \
        (XDrawLine(Metadpy->dpy,Infowin->win,Infoclr->gc,X1,Y1,X2,Y2),0)

#define Infoclr_draw_rect_2(X,Y,W,H) \
        (XDrawRectangle(Metadpy->dpy,Infowin->win,Infoclr->gc,X,Y,W,H),0)

#define Infoclr_draw_oval_2(X,Y,W,H) \
        (XDrawArc(Metadpy->dpy,Infowin->win,Infoclr->gc,X,Y,W,H,0,64*360),0)


/**** Available Macros (Initial Point, Width Height) ****/

#define Infoclr_draw_line_2(X,Y,W,H) \
        (Infoclr_draw_line_4(X,Y,X+W,Y+H))


/**** Available Macros (Initial Point, Final Point) ****/

#define Infoclr_draw_rect_4(X1,Y1,X2,Y2) \
        (Infoclr_draw_rect_2(X1,Y1,X2-X1,Y2-Y1))

#define Infoclr_draw_oval_4(X1,Y1,X2,Y2) \
        (Infoclr_draw_oval_2(X1,Y1,X2-X1,Y2-Y1))


/**** Available Macros (Initial Point, Single Side) ****/

#define Infoclr_draw_rect_1(X,Y,S) \
        (Infoclr_draw_rect_2(X,Y,S,S))

#define Infoclr_draw_oval_1(X,Y,S) \
        (Infoclr_draw_oval_2(X,Y,S,S))


/**** Available Macros (Initial Point, Radius) ****/

#define Infoclr_draw_rect_0(X,Y,R) \
        (Infoclr_draw_rect_2(X-R,Y-R,R+R,R+R))

#define Infoclr_draw_oval_0(X,Y,R) \
        (Infoclr_draw_oval_2(X-R,Y-R,R+R,R+R))



/**** Simple (Draw) Aliases ****/

#define Infoclr_draw_line_8	Infoclr_draw_line_4

#define Infoclr_draw_square_1	Infoclr_draw_rect_1
#define Infoclr_draw_square_0	Infoclr_draw_rect_0

#define Infoclr_draw_circle_1	Infoclr_draw_oval_1
#define Infoclr_draw_circle_0	Infoclr_draw_oval_0





/**** Available (Fill) Functions ****/

/* Fill a Rectangle around Non Ordered Endpoints */
extern errr Infoclr_fill_rect_8 (/*X1,Y1,X2,Y2*/);

/* Fill a Rectangle around Non Ordered Endpoints */
extern errr Infoclr_fill_oval_8 (/*X1,Y1,X2,Y2*/);




/**** Available (Fill) Macros (that Map directly to X commands) ****/

#define Infoclr_fill_rect_2(X,Y,W,H) \
        (XFillRectangle(Metadpy->dpy,Infowin->win,Infoclr->gc,X,Y,W,H),0)

#define Infoclr_fill_oval_2(X,Y,W,H) \
        (XFillArc(Metadpy->dpy,Infowin->win,Infoclr->gc,X,Y,W,H,0,64*360),0)


/**** Available (Fill) Macros (Initial Point, Final Point) ****/

#define Infoclr_fill_rect_4(X1,Y1,X2,Y2) \
        (Infoclr_fill_rect_2(X1,Y1,X2-X1,Y2-Y1))

#define Infoclr_fill_oval_4(X1,Y1,X2,Y2) \
        (Infoclr_fill_oval_2(X1,Y1,X2-X1,Y2-Y1))


/**** Available (Fill) Macros (Initial Point, Single Side) ****/

#define Infoclr_fill_rect_1(X,Y,S) \
        (Infoclr_fill_rect_2(X,Y,S,S))

#define Infoclr_fill_oval_1(X,Y,S) \
        (Infoclr_fill_oval_2(X,Y,S,S))


/**** Available (Fill) Macros (Initial Point, Radius) ****/

#define Infoclr_fill_rect_0(X,Y,R) \
        (Infoclr_fill_rect_2(X-R,Y-R,R+R,R+R))

#define Infoclr_fill_oval_0(X,Y,R) \
        (Infoclr_fill_oval_2(X-R,Y-R,R+R,R+R))




/**** Simple (Fill) Aliases ****/

#define Infoclr_fill_square_1	Infoclr_fill_rect_1
#define Infoclr_fill_square_0	Infoclr_fill_rect_0

#define Infoclr_fill_circle_1	Infoclr_fill_oval_1
#define Infoclr_fill_circle_0	Infoclr_fill_oval_0


/* SHUT: r-infoclr.h */


/* OPEN: r-infofnt.h */

/*
 * Explanation of Text Flags:
 *
 * Horizontal placement:
 *   TEXT_J_LT:    Line up text with its leftmost edge at 'x'
 *   TEXT_J_RT:    Line up text with its right-most edge at 'x'
 *   Neither:      Center text, snap to GRID, exact FILL
 *   Both:         Center text, shift GRID, infinite FILL
 *
 * Vertical placement:
 *   TEXT_J_UP:    Line up text with its top at 'y'
 *   TEXT_J_DN:    Line up text with its bottom at 'y'
 *   Default:      Center text around 'y'
 *   Both:         Line up text with its baseline at 'y'
 *
 * Special:
 *   TEXT_QUERY:   Query for font info (vs Use Infofnt)
 *   TEXT_GRID:    Count rows and columns (vs Count Pixels)
 *   TEXT_FILL:    Paint a rectangle (vs Draw Actual Text)
 *   TEXT_WIPE:    Erase before drawing (vs Stipple Draw)
 */


/**** Available Constants ****/

/* Simple Flag Combinations for the Text Drawing routines */

#define TEXT_NONE       0x00

#define TEXT_QUERY      0x01

#define TEXT_GRID       0x02
#define TEXT_FILL       0x04
#define TEXT_WIPE       0x08

#define TEXT_J_LT       0x10
#define TEXT_J_RT       0x20
#define TEXT_J_UP       0x40
#define TEXT_J_DN       0x80



/**** Available Functions ****/


/* Draw (or Fill) Text (or Paint) on the Screen (using various flags) */
extern errr Infofnt_text (/*X,Y,S,L,M*/);


/* SHUT: r-infofnt.h */




/* File: xtra-x11.c */

/*
 * The "default" values
 */
static metadpy metadpy_default;


/*
 * The "current" variables
 */
metadpy *Metadpy = &metadpy_default;
infowin *Infowin = (infowin*)(NULL);
infoclr *Infoclr = (infoclr*)(NULL);
infofnt *Infofnt = (infofnt*)(NULL);





/* OPEN: x-metadpy.c */


/*
 * Init the current metadpy, with various initialization stuff.
 *
 * Inputs:
 *	dpy:  The Display* to use (if NULL, create it)
 *	name: The name of the Display (if NULL, the current)
 *
 * Notes:
 *	If 'name' is NULL, but 'dpy' is set, extract name from dpy
 *	If 'dpy' is NULL, then Create the named Display
 *	If 'name' is NULL, and so is 'dpy', use current Display
 */

errr Metadpy_init_2 (Display *dpy, cptr name)
{
  metadpy *m = Metadpy;


  /*** Open the display if needed ***/

  /* If no Display given, attempt to Create one */
  if (!dpy)
  {
    /* Attempt to open the display */
    dpy = XOpenDisplay (name);

    /* Failure */
    if (!dpy)
    {
      /* No name given, extract DISPLAY */
      if (!name) name = getenv ("DISPLAY");

      /* No DISPLAY extracted, use default */
      if (!name) name = "(default)";

#if 0
      /* Indicate that we could not open that display */
      plog_fmt("Unable to open the display '%s'", name);
#endif

      /* Error */
      return (-1);
    }

    /* We WILL have to Nuke it when done */
    m->nuke = 1;
  }

  /* Since the Display was given, use it */
  else
  {
    /* We will NOT have to Nuke it when done */
    m->nuke = 0;
  }


  /*** Save some information ***/

  /* Save the Display itself */
  m->dpy = dpy;

  /* Get the Screen and Virtual Root Window */
  m->screen = DefaultScreenOfDisplay (dpy);
  m->root = RootWindowOfScreen (m->screen);

  /* Get the default colormap */
  m->cmap = DefaultColormapOfScreen (m->screen);

  /* Extract the true name of the display */
  m->name = DisplayString (dpy);

  /* Extract the fd */
  m->fd = ConnectionNumber (Metadpy->dpy);

  /* Save the Size and Depth of the screen */
  m->width = WidthOfScreen (m->screen);
  m->height = HeightOfScreen (m->screen);
  m->depth = DefaultDepthOfScreen (m->screen);

  /* Save the Standard Colors */
  m->black = BlackPixelOfScreen (m->screen);
  m->white = WhitePixelOfScreen (m->screen);


  /*** Make some clever Guesses ***/

  /* Guess at the desired 'fg' and 'bg' Pixell's */
  m->bg = m->black;
  m->fg = m->white;

  /* Calculate the Maximum allowed Pixel value.  */
  m->zg = (1 << m->depth) - 1;

  /* Save various default Flag Settings */
  m->color = ((m->depth > 1) ? 1 : 0);
  m->mono = ((m->color) ? 0 : 1);


  /*** All done ***/

  /* Return "success" ***/
  return (0);
}



/*
 * Nuke the current metadpy
 */

errr Metadpy_nuke ()
{
  metadpy *m = Metadpy;


  /* If required, Free the Display */
  if (m->nuke)
  {
    /* Close the Display */
    XCloseDisplay (m->dpy);

    /* Forget the Display */
    m->dpy = (Display*)(NULL);

    /* Do not nuke it again */
    m->nuke = 0;
  }

  /* Return Success */
  return (0);
}




/*
 * General Flush/ Sync/ Discard routine
 */

errr Metadpy_update (int flush, int sync, int discard)
{
  /* Flush if desired */
  if (flush) XFlush (Metadpy->dpy);

  /* Sync if desired, using 'discard' */
  if (sync) XSync (Metadpy->dpy, discard);

  /* Success */
  return (0);
}




/*
 * Set the pitch and duration for succeeding Beeps
 *
 * The volume is a base percent (0 to 100)
 * The pitch is given in Hertz (0 to 10000)
 * The duration is in millisecs (0 to 1000)
 */

errr Metadpy_prepare_sound (int vol, int pit, int dur)
{
  XKeyboardControl data;
  unsigned long mask;

  /* Set the important fields */
  data.bell_percent =  vol;
  data.bell_pitch =    pit;
  data.bell_duration = dur;

  /* Set the mask fields */
  mask = KBBellPercent | KBBellPitch | KBBellDuration;

  /* Apply the change */
  XChangeKeyboardControl (Metadpy->dpy, mask, &data);

  /* Success */
  return (0);
}




/*
 * Make a sound prepared above, vol is "percent of base"
 */

errr Metadpy_produce_sound (int vol)
{
  /* Make a sound */
  XBell (Metadpy->dpy, vol);

  return (0);
}



/*
 * Make a simple beep
 */

errr Metadpy_do_beep ()
{
  /* Make a simple beep */
  XBell (Metadpy->dpy, 100);

  return (0);
}



/* SHUT: x-metadpy.c */


/* OPEN: x-metadpy.c */


/*
 * Set the name (in the title bar) of Infowin
 */

errr Infowin_set_name (cptr name)
{
  Status st;
  XTextProperty tp;
  st = XStringListToTextProperty (&name, 1, &tp);
  if (st) XSetWMName (Metadpy->dpy, Infowin->win, &tp);
  return (0);
}


/*
 * Set the icon name of Infowin
 */

errr Infowin_set_icon_name (cptr name)
{
  Status st;
  XTextProperty tp;
  st = XStringListToTextProperty (&name, 1, &tp);
  if (st) XSetWMIconName (Metadpy->dpy, Infowin->win, &tp);
  return (0);
}



/*
 * Nuke Infowin
 */

errr Infowin_nuke ()
{
  infowin *iwin = Infowin;

  /* Nuke if requested */
  if (iwin->nuke)
  {
    /* Destory the old window */
    XDestroyWindow (Metadpy->dpy, iwin->win);
  }

  /* Success */
  return (0);
}





/*
 * Prepare a new 'infowin'.
 */

static errr Infowin_prepare (Window xid)
{
  infowin *iwin = Infowin;

  Window tmp_win;
  XWindowAttributes xwa;
  int x, y, w, h, b, d;

  /* Assign stuff */
  iwin->win = xid;

  /* Check For Error XXX Extract some ACTUAL data from 'xid' */
  XGetGeometry (Metadpy->dpy, xid, &tmp_win, &x, &y, &w, &h, &b, &d);

  /* Apply the above info */
  iwin->x = x;
  iwin->y = y;
  iwin->w = w;
  iwin->h = h;
  iwin->b = b;

  /* Check Error XXX Extract some more ACTUAL data */
  XGetWindowAttributes (Metadpy->dpy, xid, &xwa);

  /* Apply the above info */
  iwin->mask = xwa.your_event_mask;
  iwin->mapped = ((xwa.map_state == IsUnmapped) ? 0 : 1);

  /* And assume that we are exposed */
  iwin->redraw = 1;

  /* Success */
  return (0);
}





/*
 * Initialize a new 'infowin'.
 */

errr Infowin_init_real (Window xid)
{
  /* Wipe it clean */
  WIPE(Infowin, infowin);

  /* Start out non-nukable */
  Infowin->nuke = 0;

  /* Attempt to Prepare ourself */
  return (Infowin_prepare (xid));
}






/*
 * Init an infowin by giving some data.
 *
 * Inputs:
 *	dad: The Window that should own this Window (if any)
 *	x,y: The position of this Window
 *	w,h: The size of this Window
 *	b,d: The border width and pixel depth
 *
 * Notes:
 *	If 'dad == None' assume 'dad == root'
 */

errr Infowin_init_data (Window dad, int x, int y, int w, int h,
                        int b, Pixell fg, Pixell bg)
{
  Window xid;


  /* Wipe it clean */
  WIPE(Infowin, infowin);


  /*** Error Check XXX ***/


  /*** Create the Window 'xid' from data ***/

  /* If no parent given, depend on root */
  if (dad == None) dad = Metadpy->root;

  /* Create the Window XXX Error Check */
  xid = XCreateSimpleWindow (Metadpy->dpy, dad, x, y, w, h, b, fg, bg);

  /* Start out selecting No events */
  XSelectInput (Metadpy->dpy, xid, 0L);


  /*** Prepare the new infowin ***/

  /* Mark it as nukable */
  Infowin->nuke = 1;

  /* Attempt to Initialize the infowin */
  return (Infowin_prepare (xid));
}


/* SHUT: x-infowin.c */


/* OPEN: x-infoclr.c */


/*
 * A NULL terminated pair list of legal "operation names"
 *
 * Pairs of values, first is texttual name, second is the string
 * holding the decimal value that the operation corresponds to.
 */

static cptr opcode_pairs[] = {

  "cpy", "3",
  "xor", "6",
  "and", "1",
  "ior", "7",
  "nor", "8",
  "inv", "10",
  "clr", "0",
  "set", "15",

  "src", "3",
  "dst", "5",

  "dst & src", "1",
  "src & dst", "1",

  "dst | src", "7",
  "src | dst", "7",

  "dst ^ src", "6",
  "src ^ dst", "6",

  "+andReverse", "2",
  "+andInverted", "4",
  "+noop", "5",
  "+equiv", "9",
  "+orReverse", "11",
  "+copyInverted", "12",
  "+orInverted", "13",
  "+nand", "14",
  NULL
};


/*
 * Parse a word into an operation "code"
 *
 * Inputs:
 *	str: A string, hopefully representing an Operation
 *
 * Output:
 *	0-15: if 'str' is a valid Operation
 *	-1:   if 'str' could not be parsed
 */

int Infoclr_Opcode (cptr str)
{
  register int i;

  /* Scan through all legal operation names */
  for (i = 0; opcode_pairs[i*2]; ++i)
  {
    /* Is this the right oprname? */
    if (streq (opcode_pairs[i*2], str))
    {
      /* Convert the second element in the pair into a Code */
      return (atoi (opcode_pairs[i*2+1]));
    }
  }

  /* The code was not found, return -1 */
  return (-1);
}



/*
 * Request a Pixell by name.  Note: uses 'Metadpy'.
 *
 * Inputs:
 *      name: The name of the color to try to load (see below)
 *
 * Output:
 *	The Pixell value that metched the given name
 *	'Metadpy->fg' if the name was unparseable
 *
 * Valid forms for 'name':
 *	'fg', 'bg', 'zg', '<name>' and '#<code>'
 */

Pixell Infoclr_Pixell(cptr name)
{
  XColor scrn;


  /* Attempt to Parse the name */
  if (name && name[0])
  {
    /* The 'bg' color is available */
    if (streq (name, "bg")) return (Metadpy->bg);

    /* The 'fg' color is available */
    if (streq (name, "fg")) return (Metadpy->fg);

    /* The 'zg' color is available */
    if (streq (name, "zg")) return (Metadpy->zg);

    /* The 'white' color is available */
    if (streq (name, "white")) return (Metadpy->white);

    /* The 'black' color is available */
    if (streq (name, "black")) return (Metadpy->black);

    /* Attempt to parse 'name' into 'scrn' */
    if (!(XParseColor (Metadpy->dpy, Metadpy->cmap, name, &scrn)))
    {
      plog_fmt("Warning: Couldn't parse color '%s'\n", name);
    }

    /* Attempt to Allocate the Parsed color */
    if (!(XAllocColor (Metadpy->dpy, Metadpy->cmap, &scrn)))
    {
      plog_fmt("Warning: Couldn't allocate color '%s'\n", name);
    }

    /* The Pixel was Allocated correctly */
    else return (scrn.pixel);
  }

  /* Warn about the Default being Used */
  plog_fmt("Warning: Using 'fg' for unknown color '%s'\n", name);

  /* Default to the 'Foreground' color */
  return (Metadpy->fg);
}








/*
 * Initialize a new 'infoclr' with a real GC.
 */

errr Infoclr_init_1 (GC gc)
{
  infoclr *iclr = Infoclr;

  /* Wipe the iclr clean */
  WIPE(iclr, infoclr);

  /* Assign the GC */
  iclr->gc = gc;

  /* Success */
  return (0);
}



/*
 * Nuke an old 'infoclr'.
 */

errr Infoclr_nuke ()
{
  infoclr *iclr = Infoclr;

  /* Deal with 'GC' */
  if (iclr->nuke)
  {
    /* Free the GC */
    XFreeGC(Metadpy->dpy, iclr->gc);
  }

  /* Forget the current */
  Infoclr = (infoclr*)(NULL);

  /* Success */
  return (0);
}





/*
 * Initialize an infoclr with some data
 *
 * Inputs:
 *	fg:   The Pixell for the requested Foreground (see above)
 *	bg:   The Pixell for the requested Background (see above)
 *	op:   The Opcode for the requested Operation (see above)
 *	stip: The stipple mode
 */

errr Infoclr_init_data (Pixell fg, Pixell bg, int op, int stip)
{
  infoclr *iclr = Infoclr;

  GC gc;
  XGCValues gcv;
  unsigned long gc_mask;



  /*** Simple error checking of opr and clr ***/

  /* Check the 'Pixells' for realism */
  if (bg > Metadpy->zg) return (-1);
  if (fg > Metadpy->zg) return (-1);

  /* Check the data for trueness */
  if ((op < 0) || (op > 15)) return (-1);


  /*** Create the requested 'GC' ***/

  /* Assign the proper GC function */
  gcv.function = op;

  /* Assign the proper GC background */
  gcv.background = bg;

  /* Assign the proper GC foreground */
  gcv.foreground = fg;

  /* Hack -- Handle XOR (xor is code 6) by hacking bg and fg */
  if (op == 6) gcv.background = 0;
  if (op == 6) gcv.foreground = (bg ^ fg);

  /* Assign the proper GC Fill Style */
  gcv.fill_style = (stip ? FillStippled : FillSolid);

  /* Turn off 'Give exposure events for pixmap copying' */
  gcv.graphics_exposures = False;

  /* Set up the GC mask */
  gc_mask = GCFunction | GCBackground | GCForeground |
            GCFillStyle | GCGraphicsExposures;

  /* Create the GC detailed above */
  gc = XCreateGC(Metadpy->dpy, Metadpy->root, gc_mask, &gcv);


  /*** Initialize ***/

  /* Wipe the iclr clean */
  WIPE(iclr, infoclr);

  /* Assign the GC */
  iclr->gc = gc;

  /* Nuke it when done */
  iclr->nuke = 1;

  /* Assign the parms */
  iclr->fg = fg;
  iclr->bg = bg;
  iclr->code = op;
  iclr->stip = stip ? 1 : 0;

  /* Success */
  return (0);
}


/* SHUT: x-infoclr.c */


/* OPEN: x-infofnt.c */


/*
 * Nuke an old 'infofnt'.
 */

errr Infofnt_nuke ()
{
  infofnt *ifnt = Infofnt;

  /* Deal with 'name' */
  if (ifnt->name)
  {
    /* Free the name */
    string_free (ifnt->name);
  }

  /* Nuke info if needed */
  if (ifnt->nuke)
  {
    /* Free the font */
    XFreeFont (Metadpy->dpy, ifnt->info);
  }

  /* Success */
  return (0);
}



/*
 * Prepare a new 'infofnt'
 */

static errr Infofnt_prepare (XFontStruct *info)
{
  infofnt *ifnt = Infofnt;

  XCharStruct *cs;

  /* Assign the struct */
  ifnt->info = info;

  /* Jump into the max bouonds thing */
  cs = &(info->max_bounds);

  /* Extract default sizing info */
  ifnt->asc = cs->ascent;
  ifnt->hgt = (cs->ascent + cs->descent);
  ifnt->wid = cs->width;

  /* Success */
  return (0);
}





/*
 * Initialize a new 'infofnt'.
 */

errr Infofnt_init_real (XFontStruct *info)
{
  /* Wipe the thing */
  WIPE(Infofnt, infofnt);

  /* No nuking */
  Infofnt->nuke = 0;

  /* Attempt to prepare it */
  return (Infofnt_prepare (info));
}




/*
 * Init an infofnt by its Name
 *
 * Inputs:
 *	name: The name of the requested Font
 */

errr Infofnt_init_data (cptr name)
{
  XFontStruct *info;


  /*** Load the info Fresh, using the name ***/

  /* If the name is not given, report an error */
  if (!name) return (-1);

  /* Attempt to load the font */
  info = XLoadQueryFont (Metadpy->dpy, name);

  /* The load failed, try to recover */
  if (!info) return (-1);


  /*** Init the font ***/

  /* Wipe the thing */
  WIPE(Infofnt, infofnt);

  /* Attempt to prepare it */
  if (Infofnt_prepare (info))
  {
    /* Free the font */
    XFreeFont (Metadpy->dpy, info);

    /* Fail */
    return (-1);
  }

  /* Save a copy of the font name */
  Infofnt->name = string_make (name);

  /* Mark it as nukable */
  Infofnt->nuke = 1;

  /* Success */
  return (0);
}


/* SHUT: x-infofnt.c */







/* OPEN: r-metadpy.c */


/*
 * Attempt to create a Pixmap from data on Metadpy
 *
 * Note that the "optimal" sized pixmap is created.
 * So be sure that 'drawing' uses the original size.
 *
 * Inputs:
 *   bits: The bits (NULL for empty Pixmap)
 *   w, h: The minimum size of the bitmap
 */

Pixmap Pixmap_from_data (byte *bits, int w, int h)
{
  Pixmap temp = None, map1;
  int w1, h1;

  GC tempgc;


  /* Illegal size induces "None" Pixmap */
  if ((w <= 0) || (h <= 0)) return (None);


  /* If 'bits' are given */
  if (bits)
  {
    /* First create the bitmap as a template */
    temp = XCreateBitmapFromData (Metadpy->dpy, Metadpy->root, bits, w, h);

    /* Make sure the template took */
    if (temp == None)
    {
      plog("Pixmap_from_data(): Unable to create Bitmap");
      return (None);
    }
  }

  /* Make sure the server will allow a big enough pixmap */
  if (!XQueryBestSize (Metadpy->dpy, TileShape, Metadpy->root,
                       w, h, &w1, &h1))
  {
    plog("Pixmap_from_data(): Unable to find size");
    return (None);
  }

  /* Oh well, the server is annoying */
  if (w1 < w || h1 < h)
  {
    plog("Pixmap_from_data(): Xserver small sized us: Ignoring.");
    w1 = w; h1 = h;
  }


  /* Create the actual Pixmap (Depth 1) */
  map1 = XCreatePixmap (Metadpy->dpy, Metadpy->root, w1, h1, 1);

  /* Oh well, silly server */
  if (!map1)
  {
    plog("Pixmap_from_data(): Unable to create True pixmap");
    return (None);
  }

  /* Allocate a temporary GC */
  tempgc = XCreateGC (Metadpy->dpy, map1, 0L, NULL);
  XSetForeground (Metadpy->dpy, tempgc, 0L);

  /* Clean out the new pixmap */
  XFillRectangle (Metadpy->dpy, map1, tempgc, 0, 0, w1, h1);


  /* Use the bits if any were given */
  if (bits)
  {
    /* Save the created pixmap data */
    XCopyArea (Metadpy->dpy, temp, map1, tempgc, 0, 0, w, h, 0, 0);

    /* Release the temporary pixmap */
    XFreePixmap (Metadpy->dpy, temp);
  }

  /* Return the New Pixmap */
  return (map1);
}




/*
 * Attempt to makes a Cursor from data on Metadpy
 */

Cursor Cursor_from_data (byte *bits, byte *mask, int w, int h,
                         int x, int y, Pixell fg, Pixell bg)
{
  XColor fcol, bcol;
  Pixmap bpix, mpix;
  Cursor cursor = None;

  /* Try to Make the pixmap for the cursor */
  bpix = Pixmap_from_data (bits, w, h);

  /* Be sure it worked */
  if (!bpix)
  {
    plog("Failed to make Cursor Bitmap");
    return (cursor);
  }

  /* Make the Mask for the cursor */
  mpix = Pixmap_from_data (mask, w, h);

  /* Try to make the Mask */
  if (!mpix)
  {
    plog("Failed to make Cursor Mask");
    XFreePixmap (Metadpy->dpy, bpix);
    return (cursor);
  }

  /* Build the Foreground XColor */
  fcol.pixel = fg;
  XQueryColor (Metadpy->dpy, Metadpy->cmap, &fcol);

  /* Build the Background XColor */
  bcol.pixel = bg;
  XQueryColor (Metadpy->dpy, Metadpy->cmap, &bcol);

  /* Create the Cursor */
  cursor = XCreatePixmapCursor (Metadpy->dpy,
               bpix, mpix, &fcol, &bcol, x, y);

  /* Free up space */
  XFreePixmap (Metadpy->dpy, bpix);
  XFreePixmap (Metadpy->dpy, mpix);

  /* Return new Cursor (or the Failure) */
  return (cursor);
}

/* SHUT: r-metadpy.c */

/* OPEN: r-infowin.c */

/*
 * Modify the event mask of an Infowin
 */

errr Infowin_set_mask (long mask)
{
  /* Save the new setting */
  Infowin->mask = mask;

  /* Execute the Mapping */
  XSelectInput (Metadpy->dpy, Infowin->win, Infowin->mask);

  /* Success */
  return (0);
}








/*
 * Request that Infowin be mapped
 */

errr Infowin_map ()
{
  /* Execute the Mapping */
  XMapWindow (Metadpy->dpy, Infowin->win);

  /* Success */
  return (0);
}


/*
 * Request that Infowin be unmapped
 */

errr Infowin_unmap ()
{
  /* Execute the Un-Mapping */
  XUnmapWindow (Metadpy->dpy, Infowin->win);

  /* Success */
  return (0);
}



/*
 * Request that Infowin be raised
 */

errr Infowin_raise ()
{
  /* Raise towards visibility */
  XRaiseWindow (Metadpy->dpy, Infowin->win);

  /* Success */
  return (0);
}


/*
 * Request that Infowin be lowered
 */

errr Infowin_lower ()
{
  /* Lower towards invisibility */
  XLowerWindow (Metadpy->dpy, Infowin->win);

  /* Success */
  return (0);
}





/*
 * Request that Infowin be moved to a new location
 */

errr Infowin_impell (int x, int y)
{
  /* Execute the request */
  XMoveWindow (Metadpy->dpy, Infowin->win, x, y);

  /* Success */
  return (0);
}



/*
 * Resize an infowin
 */

errr Infowin_resize (int w, int h)
{
  /* Execute the request */
  XResizeWindow (Metadpy->dpy, Infowin->win, w, h);

  /* Success */
  return (0);
}


/*
 * Move and Resize an infowin
 */

errr Infowin_locate (int x, int y, int w, int h)
{
  /* Execute the request */
  XMoveResizeWindow (Metadpy->dpy, Infowin->win, x, y, w, h);

  /* Success */
  return (0);
}






/*
 * Visually clear Infowin
 */

errr Infowin_wipe ()
{
  /* Execute the request */
  XClearWindow (Metadpy->dpy, Infowin->win);

  /* Success */
  return (0);
}


/*
 * Visually Paint Infowin with the current color
 */

errr Infowin_fill ()
{
  /* Execute the request */
  XFillRectangle (Metadpy->dpy, Infowin->win, Infoclr->gc,
                  0, 0, Infowin->w, Infowin->h);

  /* Success */
  return (0);
}


/* SHUT: r-infowin.c */


/* OPEN: r-infoclr.c */


/*
 * Draw a rectangle (in free floating corners format)
 */

errr Infoclr_draw_rect_8 (int x1, int y1, int x2, int y2)
{
  register int tmp;

  /* Force (x1<x2) and (y1<y2) */
  if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
  if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }

  /* Send to the ordered corner routine */
  return (Infoclr_draw_rect_4 (x1, y1, x2, y2));
}



/*
 * Draw an oval (in free floating corners format)
 */

errr Infoclr_draw_oval_8 (int x1, int y1, int x2, int y2)
{
  register int tmp;

  /* Force (x1<x2) and (y1<y2) */
  if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
  if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }

  /* Send to the ordered corner routine */
  return (Infoclr_draw_oval_4 (x1, y1, x2, y2));
}



/*
 * Fill a rectangle (in free floating corners format)
 */

errr Infoclr_fill_rect_8 (int x1, int y1, int x2, int y2)
{
  register int tmp;

  /* Force (x1<x2) and (y1<y2) */
  if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
  if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }

  /* Send to the ordered corner routine */
  return (Infoclr_fill_rect_4 (x1, y1, x2, y2));
}



/*
 * Fill an oval (in free floating corners format)
 */

errr Infoclr_fill_oval_8 (int x1, int y1, int x2, int y2)
{
  register int tmp;

  /* Force (x1<x2) and (y1<y2) */
  if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
  if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }

  /* Send to the ordered corner routine */
  return (Infoclr_fill_oval_4 (x1, y1, x2, y2));
}




/*
 * Draw the top plane of a True Pixmap
 */

errr Infoclr_draw_pixmap (int x, int y, uint w, uint h, Drawable pixmap)
{
  /* Copy the pixmap onto the screen at topleft (x,y) size (w,h) */
  XCopyPlane (Metadpy->dpy, pixmap, Infowin->win, Infoclr->gc,
              0, 0, w, h, x, y, 0x1);

  /* Success */
  return (0);
}


/* SHUT: r-infoclr.c */


/* OPEN: r-infofnt.c */



/*
 * Standard Text
 */

errr Infofnt_text_std (int x, int y, cptr str, int len, uint mode)
{
  int i;
  int dir, asc, desc;
  XCharStruct ov;


  /*** Do a brief info analysis ***/

  /* Do nothing if the string is null */
  if (!str || !*str) return (-1);

  /* Get the length of the string */
  if (len < 0) len = strlen (str);


  /*** Decide where to place the string ***/

  /* Both TEXT_GRID and TEXT_QUERY makes no sense */
  if ((mode & TEXT_QUERY) && (mode & TEXT_GRID))
  {
    /* Not very graceful */
    core("Infofnt: Impossible Request: QUERY+ GRID\n");
  }


  /* Assume TEXT_QUERY over-rides TEXT_GRID */
  else if (mode & TEXT_QUERY)
  {
    /*** Query the Info - Why? ***/

    /* Get the "size" of the string */
    XTextExtents (Infofnt->info, str, len, &dir, &asc, &desc, &ov);



    /*** Decide where to place the string, vertically ***/

    /* Use y as the baseline location */
    if ((mode & TEXT_J_UP) && (mode & TEXT_J_DN))
    {
      /* Use the standard Baseline */
    }

    /* Use y as the top location */
    else if (mode & TEXT_J_UP)
    {
      y = y + ov.ascent;
    }

    /* Use y as the bottom location */
    else if (mode & TEXT_J_DN)
    {
      y = y - ov.descent;
    }

    /* Center vertically around y */
    else
    {
      y = y + (ov.ascent - ov.descent) / 2;
    }


    /*** Decide where to place the string, horizontally ***/

    /* Same as Centering below */
    if ((mode & TEXT_J_LT) && (mode & TEXT_J_RT))
    {
      x = x - (ov.width / 2);
    }

    /* Line up with x at left edge (XXX cleanly?) */
    else if (mode & TEXT_J_LT)
    {
      /* x = x + ov.lbearing; */
    }

    /* Line up with x at right edge (XXX cleanly?) */
    else if (mode & TEXT_J_RT)
    {
      x = x - (ov.width);
    }

    /* Default -- Center horizontally (XXX cleanly?) */
    else
    {
      x = x - (ov.width / 2);
    }
  }


  /* Use (row,col) positional info */
  else if (mode & TEXT_GRID)
  {
    /*** Decide where to place the string, vertically ***/

    /* Ignore Vertical Justifications */
    y = (y * Infofnt->hgt) + Infofnt->asc;


    /*** Decide where to place the string, horizontally ***/

    /* Center, allow half-grid column leeway */
    if ((mode & TEXT_J_LT) && (mode & TEXT_J_RT))
    {
      x = (x * Infofnt->wid) - (len * Infofnt->wid / 2);
    }

    /* Line up with x at left edge of column 'x' */
    else if (mode & TEXT_J_LT)
    {
      x = (x * Infofnt->wid);
    }

    /* Line up with x at right edge of column 'x' */
    else if (mode & TEXT_J_RT)
    {
      x = (x - (len - 1)) * Infofnt->wid;
    }

    /* Center horizontally, snap to grid columns */
    else
    {
      x = (x - (len / 2)) * Infofnt->wid;
    }
  }


  /* Neither TEXT_GRID nor TEXT_QUERY, this is very common */
  else
  {
    /*** Decide where to place the string, vertically ***/

    /* Use y as the baseline location */
    if ((mode & TEXT_J_UP) && (mode & TEXT_J_DN))
    {
      /* Use standard base line */
    }

    /* Use y as the top location */
    else if (mode & TEXT_J_UP)
    {
      y = y + Infofnt->asc;
    }

    /* Use y as the bottom location */
    else if (mode & TEXT_J_DN)
    {
      y = y + Infofnt->asc - Infofnt->hgt;
    }

    /* Center vertically around y */
    else
    {
      y = y + Infofnt->asc - (Infofnt->hgt / 2);
    }


    /*** Decide where to place the string, horizontally ***/

    /* Same as standard centering */
    if ((mode & TEXT_J_LT) && (mode & TEXT_J_RT))
    {
      x = x - ((len * Infofnt->wid) / 2);
    }

    /* Line up with x at left edge (XXX cleanly?) */
    else if (mode & TEXT_J_LT)
    {
      /* Just use (messy) left edge */
    }

    /* Line up with x at right edge (XXX cleanly?) */
    else if (mode & TEXT_J_RT)
    {
      x = x - (len * Infofnt->wid);
    }

    /* Center horizontally (XXX cleanly?) */
    else
    {
      x = x - ((len * Infofnt->wid) / 2);
    }
  }


  /*** Actually draw 'str' onto the infowin ***/

  /* Be sure the correct font is ready */
  XSetFont (Metadpy->dpy, Infoclr->gc, Infofnt->info->fid);


  /*** Handle the fake mono we can enforce on fonts ***/

  /* Monotize the font */
  if (Infofnt->mono)
  {
    /* Do each character */
    for (i = 0; i < len; ++i)
    {
      /* Perhaps draw it and clear behind it */
      if (mode & TEXT_WIPE)
      {
        /* Note that the Infoclr is set up to contain the Infofnt */
        XDrawImageString (Metadpy->dpy, Infowin->win, Infoclr->gc,
                          x + i * Infofnt->wid + Infofnt->off, y, str + i, 1);
      }

      /* Else draw it without clearing behind it */
      else
      {
        /* Note that the Infoclr is set up to contain the Infofnt */
        XDrawString (Metadpy->dpy, Infowin->win, Infoclr->gc,
                     x + i * Infofnt->wid + Infofnt->off, y, str + i, 1);
      }
    }
  }

  /* Assume monoospaced font */
  else
  {
    /* Perhaps draw it and clear behind it */
    if (mode & TEXT_WIPE)
    {
      /* Note that the Infoclr is set up to contain the Infofnt */
      XDrawImageString (Metadpy->dpy, Infowin->win, Infoclr->gc,
                        x, y, str, len);
    }

    /* Else draw it without clearing behind it */
    else
    {
      /* Note that the Infoclr is set up to contain the Infofnt */
      XDrawString (Metadpy->dpy, Infowin->win, Infoclr->gc,
                   x, y, str, len);
    }
  }


  /* Success */
  return (0);
}






/*
 * Painting where text would be (assume TEXT_GRID)
 */

errr Infofnt_text_non (int x, int y, cptr str, int len, uint mode)
{
  int w, h;


  /*** Do a brief info analysis ***/

  /* If 'str' is NULL, fill all the way to the Infowin edge */
  if (!str)
  {
    /*** Find the X and W dimensions ***/

    /* Fill entire row */
    if ((mode & TEXT_J_LT) && (mode & TEXT_J_RT))
    {
      w = Infowin->w;
      x = 0;
    }

    /* Line up with x at left edge of column 'x', fill to right edge */
    else if (mode & TEXT_J_LT)
    {
      x = x * Infofnt->wid;
      w = Infowin->w - x;
    }

    /* Line up with x at right edge of column 'x', fill to left edge */
    else if (mode & TEXT_J_RT)
    {
      w = x * Infofnt->wid;
      x = 0;
    }

    /* Fill the entire Row */
    else
    {
      w = Infowin->w;
      x = 0;
    }
  }


  /* Otherwise, perform like (TEXT_GRID + not TEXT_FILL) above */
  else
  {
    /*** Find the width ***/

    /* Negative length is a flag to count the characters in str */
    if (len < 0) len = strlen (str);

    /* The total width will be 'len' chars * standard width */
    w = len * Infofnt->wid;


    /*** Find the X dimensions ***/

    /* Center horizontally, allow half-frid column leeway */
    if ((mode & TEXT_J_LT) && (mode & TEXT_J_RT))
    {
      x = (x * Infofnt->wid) - (w / 2);
    }

    /* Line up with x at left edge of column 'x' */
    else if (mode & TEXT_J_LT)
    {
      x = x * Infofnt->wid;
    }

    /* Line up with x at right edge of column 'x' */
    else if (mode & TEXT_J_RT)
    {
      x = (x - (len - 1)) * Infofnt->wid;
    }

    /* Center horizontally, snap to grid. */
    else
    {
      x = (x - (len / 2)) * Infofnt->wid;
    }
  }


  /*** Find other dimensions ***/

  /* Simply do 'Infofnt->hgt' (a single row) high */
  h = Infofnt->hgt;

  /* Simply do "at top" in row 'y' */
  y = y * h;


  /*** Actually 'paint' the area ***/

  /* Just do a Fill Rectangle */
  XFillRectangle (Metadpy->dpy, Infowin->win, Infoclr->gc, x, y, w, h);

  /* Success */
  return (0);
}



/*
 * Draw something resembling text somewhere with various options
 *
 * Globals:
 *   Metadpy: The base metadpy to draw on
 *   Infowin: The base infowin to draw on
 *   Infofnt: The base infofnt to draw with
 *   Infoclr: The base infoclr to draw with
 *
 * Inputs:
 *   x, y: The (x,y) location (in some representation)
 *   str:  The string to use, if any (else NULL)
 *   len:  The length of str, if known (else -1)
 *   mode: The mode (see header file) to employ
 *
 * Position:
 *   The (x,y) can be a pixel location, or a (row,col) position
 *   The text can be centered, or anchored to the left or right
 *   The text can be vertically centered, or anchored weirdly
 *   Vertical anchors include top, bottom, and baseline
 *   The default is to be centered horizontally and vertically
 *   The size of the letters can be accepted, or queried
 *   Text can be drawn, wiped, or painted
 *
 * Note:
 *   When in the 'fill_text' routine, if 'str' is NULL, then
 *   assume that the "string" extends forever towards various
 *   edges (both if neither RIGHT nor LEFT justified)
 */

errr Infofnt_text (int x, int y, cptr str, int len, uint mode)
{
  int i;

  /* Branch on mode */
  i = ((mode & TEXT_FILL) ?
       (Infofnt_text_non (x, y, str, len, mode)) :
       (Infofnt_text_std (x, y, str, len, mode)));

  /* Return result */
  return (i);
}


/* SHUT: r-infofnt.c */




#ifndef IsModifierKey

/*
 * Keysym macros, used on Keysyms to test for classes of symbols
 * These were stolen from one of the X11 header files
 */

#define IsKeypadKey(keysym) \
  (((unsigned)(keysym) >= XK_KP_Space) && ((unsigned)(keysym) <= XK_KP_Equal))

#define IsCursorKey(keysym) \
  (((unsigned)(keysym) >= XK_Home)     && ((unsigned)(keysym) <  XK_Select))

#define IsPFKey(keysym) \
  (((unsigned)(keysym) >= XK_KP_F1)     && ((unsigned)(keysym) <= XK_KP_F4))

#define IsFunctionKey(keysym) \
  (((unsigned)(keysym) >= XK_F1)       && ((unsigned)(keysym) <= XK_F35))

#define IsMiscFunctionKey(keysym) \
  (((unsigned)(keysym) >= XK_Select)   && ((unsigned)(keysym) <  XK_KP_Space))

#define IsModifierKey(keysym) \
  (((unsigned)(keysym) >= XK_Shift_L)  && ((unsigned)(keysym) <= XK_Hyper_R))

#endif


/*
 * Checks if the keysym is a special key or a normal key
 * Assume that XK_MISCELLANY keysyms are special
 */
#define IsSpecialKey(keysym) \
  ((unsigned)(keysym) >= 0xFF00)


/*
 * Hack -- cursor color
 */
static infoclr *xor;

/*
 * Color table
 */
static infoclr *clr[16];


/*
 * Forward declare
 */
typedef struct _term_data term_data;

/*
 * A structure for each "term"
 */
struct _term_data {

  term t;

  infofnt *fnt;

  infowin *outer;
  infowin *inner;
};


/*
 * The three term_data's
 */
static term_data screen;
static term_data recall;
static term_data choice;




/*
 * Set the size hints of Infowin
 */
static errr Infowin_set_size(int w, int h, int r_w, int r_h, bool fixed)
{
  XSizeHints *sh;

  /* Make Size Hints */
  sh = XAllocSizeHints();

  /* Oops */
  if (!sh) return (1);

  /* Fixed window size */
  if (fixed)
  {
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = sh->max_width = w;
    sh->min_height = sh->max_height = h;
  }

  /* Variable window size */
  else
  {
    sh->flags = PMinSize;
    sh->min_width = r_w + 2;
    sh->min_height = r_h + 2;
  }

  /* Standard fields */
  sh->width = w;
  sh->height = h;
  sh->width_inc = r_w;
  sh->height_inc = r_h;
  sh->base_width = 2;
  sh->base_height = 2;

  /* Useful settings */
  sh->flags |= PSize | PResizeInc | PBaseSize;

  /* Use the size hints */
  XSetWMNormalHints(Metadpy->dpy, Infowin->win, sh);

  /* Success */
  return 0;
}


/*
 * Set the name (in the title bar) of Infowin
 */
static errr Infowin_set_class_hint(cptr name)
{
  XClassHint *ch;

  char res_name[20];
  char res_class[20];

  ch = XAllocClassHint();
  if (ch == NULL) return (1);

  strcpy(res_name, name);
  res_name[0] = FORCELOWER(res_name[0]);
  ch->res_name = res_name;

  strcpy(res_class, "Angband");
  ch->res_class = res_class;

  XSetClassHint(Metadpy->dpy, Infowin->win, ch);

  return (0);
}



/*
 * Process a keypress event
 */
static void react_keypress(XEvent *xev)
{
  int i, n, mc, ms, mo, mx;

  uint ks1;

  XKeyEvent *ev = (XKeyEvent*)(xev);

  KeySym ks;

  char buf[128];
  char msg[128];


  /* Check for "normal" keypresses */
  n = XLookupString(ev, buf, 125, &ks, NULL);

  /* Terminate */
  buf[n] = '\0';

  /* Hack -- convert into an unsigned int */
  ks1 = (uint)(ks);

  /* Extract four "modifier flags" */
  mc = (ev->state & ControlMask) ? TRUE : FALSE;
  ms = (ev->state & ShiftMask) ? TRUE : FALSE;
  mo = (ev->state & Mod1Mask) ? TRUE : FALSE;
  mx = (ev->state & Mod2Mask) ? TRUE : FALSE;


  /* Hack -- Ignore "modifier keys" */
  if (IsModifierKey(ks)) return;


  /* Normal keys with no modifiers */
  if (n && !mo && !mx && !IsSpecialKey(ks))
  {
    /* Enqueue the normal key(s) */
    for (i = 0; buf[i]; i++) Term_keypress(buf[i]);

    /* All done */
    return;
  }


  /* Handle a few standard keys */
  switch (ks1)
  {
    case XK_Escape:
      Term_keypress(ESCAPE); return;

    case XK_Return:
      Term_keypress('\r'); return;

    case XK_Tab:
      Term_keypress('\t'); return;

    case XK_Delete:
    case XK_BackSpace:
      Term_keypress('\010'); return;
  }


#if 0
  /* Hack -- Handle a few special KeySym codes */
  switch (ks1)
  {
    case XK_Up:
      Term_keypress(30); Term_keypress('8'); return;

    case XK_Down:
      Term_keypress(30); Term_keypress('2'); return;

    case XK_Left:
      Term_keypress(30); Term_keypress('4'); return;

    case XK_Right:
      Term_keypress(30); Term_keypress('6'); return;
  }
#endif


  /* Hack -- Use the KeySym */
  if (ks)
  {
    sprintf(buf, "%c%s%s%s%s_%lX%c", 31,
                mc ? "N" : "", ms ? "S" : "",
                mo ? "O" : "", mx ? "M" : "",
                (unsigned long)(ks), 13);
  }

  /* Hack -- Use the Keycode */
  else
  {
    sprintf(msg, "%c%s%s%s%sK_%X%c", 31,
                mc ? "N" : "", ms ? "S" : "",
                mo ? "O" : "", mx ? "M" : "",
                ev->keycode, 13);
  }

  /* Enqueue the "fake" string */
  for (i = 0; msg[i]; i++) Term_keypress(msg[i]);


  /* Hack -- dump an "extra" string */
  if (n)
  {
    /* Start the "extra" string */
    Term_keypress(28);

    /* Enqueue the "real" string */
    for (i = 0; buf[i]; i++) Term_keypress(buf[i]);

    /* End the "extra" string */
    Term_keypress(28);
  }
}




/*
 * Process an event (or just check for one)
 */
static errr CheckEvent(bool check)
{
  term_data *old_td = (term_data*)(Term->data);

  XEvent xev_body, *xev = &xev_body;

  term_data *td = NULL;
  infowin *iwin = NULL;

  int flag = 0;

  int x, y, data;


  /* No events ready, and told to just check */
  if (check && !XPending(Metadpy->dpy)) return (1);

  /* Load the Event */
  XNextEvent(Metadpy->dpy, xev);


  /* Notice new keymaps */
  if (xev->type == MappingNotify)
  {
    XRefreshKeyboardMapping(&xev->xmapping);
    return 0;
  }


  /* Main screen, inner window */
  if (xev->xany.window == screen.inner->win)
  {
    td = &screen;
    iwin = td->inner;
  }

  /* Main screen, outer window */
  else if (xev->xany.window == screen.outer->win)
  {
    td = &screen;
    iwin = td->outer;
  }


  /* Recall window, inner window */
  else if (xev->xany.window == recall.inner->win)
  {
    td = &recall;
    iwin = td->inner;
  }

  /* Recall Window, outer window */
  else if (xev->xany.window == recall.outer->win)
  {
    td = &recall;
    iwin = td->outer;
  }


  /* Choice window, inner window */
  else if (xev->xany.window == choice.inner->win)
  {
    td = &choice;
    iwin = td->inner;
  }

  /* Choice Window, outer window */
  else if (xev->xany.window == choice.outer->win)
  {
    td = &choice;
    iwin = td->outer;
  }


  /* Unknown window */
  if (!td || !iwin) return (0);


  /* Hack -- activate the Term */
  Term_activate(&td->t);

  /* Hack -- activate the window */
  Infowin_set(iwin);


  /* Switch on the Type */
  switch (xev->type)
  {
    /* A Button Press Event */
    case ButtonPress:
    {
      /* Set flag, then fall through */
      flag = 1;
    }

    /* A Button Release (or ButtonPress) Event */
    case ButtonRelease:
    {
      /* Which button is involved */
      if      (xev->xbutton.button == Button1) data = 1;
      else if (xev->xbutton.button == Button2) data = 2;
      else if (xev->xbutton.button == Button3) data = 3;
      else if (xev->xbutton.button == Button4) data = 4;
      else if (xev->xbutton.button == Button5) data = 5;

      /* Where is the mouse */
      x = xev->xbutton.x;
      y = xev->xbutton.y;

      /* XXX Handle */

      break;
    }

    /* An Enter Event */
    case EnterNotify:
    {
      /* Note the Enter, Fall into 'Leave' */
      flag = 1;
    }

    /* A Leave (or Enter) Event */
    case LeaveNotify:
    {
      /* Where is the mouse */
      x = xev->xcrossing.x;
      y = xev->xcrossing.y;

      /* XXX Handle */

      break;
    }

    /* A Motion Event */
    case MotionNotify:
    {
      /* Where is the mouse */
      x = xev->xmotion.x;
      y = xev->xmotion.y;

      /* XXX Handle */

      break;
    }

    /* A KeyRelease */
    case KeyRelease:
    {
      /* Nothing */
      break;
    }

    /* A KeyPress */
    case KeyPress:
    {
      /* Save the mouse location */
      x = xev->xkey.x;
      y = xev->xkey.y;

      /* Hack -- use "old" term */
      Term_activate(&old_td->t);

      /* Process the key */
      react_keypress(xev);

      break;
    }

    /* An Expose Event */
    case Expose:
    {
      /* Ignore "extra" exposes */
      if (xev->xexpose.count) break;

      /* Clear the window */
      Infowin_wipe();

      /* Redraw (if allowed) */
      if (iwin == td->inner) Term_redraw();

      break;
    }

    /* A Mapping Event */
    case MapNotify:
    {
      Infowin->mapped = 1;
      break;
    }

    /* An UnMap Event */
    case UnmapNotify:
    {
      /* Save the mapped-ness */
      Infowin->mapped = 0;
      break;
    }

    /* A Move AND/OR Resize Event */
    case ConfigureNotify:
    {
      int x1, y1, w1, h1;
      int cols, rows, wid, hgt;

      /* Save the Old information */
      x1 = Infowin->x;
      y1 = Infowin->y;
      w1 = Infowin->w;
      h1 = Infowin->h;

      /* Save the new Window Parms */
      Infowin->x = xev->xconfigure.x;
      Infowin->y = xev->xconfigure.y;
      Infowin->w = xev->xconfigure.width;
      Infowin->h = xev->xconfigure.height;

      /* Detemine "proper" number of rows/cols */
      cols = ((Infowin->w - 2) / td->fnt->wid);
      rows = ((Infowin->h - 2) / td->fnt->hgt);

      /* Hack -- do not allow resize of main screen */
      if (td == &screen) cols = 80;
      if (td == &screen) rows = 24;

      /* Hack -- minimal size */
      if (cols < 1) cols = 1;
      if (rows < 1) rows = 1;

      /* Desired size of "outer" window */
      wid = cols * td->fnt->wid;
      hgt = rows * td->fnt->hgt;

      /* Resize the windows if any "change" is needed */
      if ((Infowin->w != wid + 2) || (Infowin->h != hgt + 2))
      {
        Infowin_set(td->outer);
        Infowin_resize(wid + 2, hgt + 2);
        Infowin_set(td->inner);
        Infowin_resize(wid, hgt);
      }

      break;
    }
  }


  /* Hack -- Activate the old term */
  Term_activate(&old_td->t);

  /* Hack -- Activate the proper "inner" window */
  Infowin_set(old_td->inner);


  /* XXX XXX Hack -- map/unmap as needed */


  /* Success */
  return (0);
}







/*
 * Handle "activation" of a term
 */
static errr Term_xtra_x11_level(int v)
{
  term_data *td = (term_data*)(Term->data);

  /* Only handle "activate" */
  if (v != TERM_LEVEL_SOFT_OPEN) return (1);

  /* Activate the "inner" window */
  Infowin_set(td->inner);

  /* Activate the "inner" font */
  Infofnt_set(td->fnt);

  /* Success */
  return (0);
}


/*
 * Handle a "special request"
 */
static errr Term_xtra_x11(int n, int v)
{
    /* Handle a subset of the legal requests */
    switch (n)
    {
        /* Make a noise */
        case TERM_XTRA_NOISE: Metadpy_do_beep(); return (0);

        /* Flush the output */
        case TERM_XTRA_FLUSH: Metadpy_update(1,0,0); return (0);

        /* Check for a single event */
        case TERM_XTRA_CHECK: return (CheckEvent(TRUE));

        /* Wait for a single event */
        case TERM_XTRA_EVENT: return (CheckEvent(FALSE));

        /* Handle change in the "level" */
        case TERM_XTRA_LEVEL: return (Term_xtra_x11_level(v));
    }

    /* Unknown */
    return (1);
}



/*
 * Erase a number of characters
 */
static errr Term_wipe_x11(int x, int y, int w, int h)
{
  int k;

  /* Erase (use black) */
  Infoclr_set(clr[0]);

  /* Hack -- Erase each row */
  for (k = 0; k < h; ++k)
  {
    /* Mega-Hack -- Erase some space */
    Infofnt_text (x, y+k, "", w, TEXT_FILL | TEXT_GRID | TEXT_J_LT);
  }

  /* Success */
  return (0);
}



/*
 * Draw the cursor (XXX by hiliting)
 */
static errr Term_curs_x11(int x, int y, int z)
{
  /* Draw the cursor */
  Infoclr_set(xor);

  /* Hilite the cursor character */
  Infofnt_text(x, y, " ", 1, TEXT_GRID | TEXT_J_LT | TEXT_FILL);

  /* Success */
  return (0);
}


/*
 * Draw a number of characters
 */
static errr Term_text_x11(int x, int y, int n, byte a, cptr s)
{
  /* First, erase behind the chars */
  Term_wipe(x, y, n, 1);

  /* Draw the text in Xor */
  Infoclr_set(clr[a]);

  /* Draw the text, left justified, in a grid */
  Infofnt_text(x, y, s, n, TEXT_GRID | TEXT_J_LT);

  /* Success */
  return (0);
}



/*
 * Initialize a term_data
 */
static errr term_data_init(term_data *td, int w, int h, int k, bool fixed, \
                           cptr name, cptr font)
{
  term *t = &td->t;

  int wid, hgt;

  /* Prepare the standard font */
  MAKE(td->fnt, infofnt);
  Infofnt_set(td->fnt);
  Infofnt_init_data(font);

  /* Extract the font sizes, add a border */
  wid = w * td->fnt->wid;
  hgt = h * td->fnt->hgt;

  /* Create a top-window (border 5) */
  MAKE(td->outer, infowin);
  Infowin_set(td->outer);
  Infowin_init_top(0, 0, wid + 2, hgt + 2, 1, Metadpy->fg, Metadpy->bg);
  Infowin_set_mask(StructureNotifyMask | KeyPressMask);
  Infowin_set_name(name);
  Infowin_set_class_hint(name);
  Infowin_set_size(wid+2, hgt+2, td->fnt->wid, td->fnt->hgt, fixed);
  Infowin_map();

  /* Create a sub-window for playing field */
  MAKE(td->inner, infowin);
  Infowin_set(td->inner);
  Infowin_init_std(td->outer, 1, 1, wid, hgt, 0);
  Infowin_set_mask(ExposureMask);
  Infowin_map();

  /* Initialize the term (full size) */
  term_init(t, 80, 24, k);

  /* Hooks */
  t->xtra_hook = Term_xtra_x11;
  t->curs_hook = Term_curs_x11;
  t->wipe_hook = Term_wipe_x11;
  t->text_hook = Term_text_x11;

  /* We are not a dumb terminal */
  t->soft_cursor = TRUE;
  t->scan_events = TRUE;

  /* Save the data */
  t->data = td;

  /* Activate (important) */
  Term_activate(t);

  /* Success */
  return (0);
}


/*
 * Names of the 16 colors
 *   Black, White, Slate, Orange,    Red, Green, Blue, Umber
 *   D-Gray, L-Gray, Violet, Yellow, L-Red, L-Green, L-Blue, L-Umber
 *
 * Colors courtesy of: Torbj|rn Lindgren <tl@ae.chalmers.se>
 *
 * These colors may no longer be valid...
 */
static cptr color_name[16] = {
      "black",        /* BLACK 15 */
      "white",        /* WHITE 0 */
      "#d7d7d7",      /* GRAY 13 */
      "#ff9200",      /* ORANGE 2 */
      "#ff0000",      /* RED 3 */
      "#00cd00",      /* GREEN 9 */
      "#0000fe",      /* BLUE 6 */
      "#c86400",      /* BROWN 10 */
      "#a3a3a3",      /* DARKGRAY 14 */
      "#ebebeb",      /* LIGHTGRAY 12 */
      "#a500ff",      /* PURPLE 5 */
      "#fffd00",      /* YELLOW 1 */
      "#ff00bc",      /* PINK 4 */
      "#00ff00",      /* LIGHTGREEN 8 */
      "#00c8ff",      /* LIGHTBLUE 7 */
      "#ffcc80",      /* LIGHTBROWN 11 */
};


/*
 * Initialization function for an "X11" module to Angband
 */
errr init_x11(void)
{
  int i;

  cptr fnt_name;

  cptr dpy_name = "";


  /* Init the Metadpy if possible */
  if (Metadpy_init_name(dpy_name)) return (-1);


  /* Prepare color "xor" (for cursor) */
  MAKE(xor, infoclr);
  Infoclr_set (xor);
  Infoclr_init_ccn ("fg", "bg", "xor", 0);

  /* Prepare the colors (including "black") */
  for (i = 0; i < 16; ++i)
  {
    cptr cname = color_name[0];
    MAKE(clr[i], infoclr);
    Infoclr_set (clr[i]);
    if (Metadpy->color) cname = color_name[i];
    else if (i) cname = color_name[1];
    Infoclr_init_ccn (cname, "bg", "cpy", 0);
  }


  /* Check environment for "screen" font */
  fnt_name = getenv("ANGBAND_X11_FONT_SCREEN");

  /* Check environment for "base" font */
  if (!fnt_name) fnt_name = getenv("ANGBAND_X11_FONT");

  /* No environment variables, use the default */
  if (!fnt_name) fnt_name = DEFAULT_X11_FONT_SCREEN;

  /* Initialize the screen */
  term_data_init(&screen, 80, 24, 1024, TRUE, "Angband", fnt_name);
  term_screen = Term;


  /* Check environment for "recall" font */
  fnt_name = getenv("ANGBAND_X11_FONT_RECALL");

  /* Check environment for "base" font */
  if (!fnt_name) fnt_name = getenv("ANGBAND_X11_FONT");

  /* No environment variables, use the default */
  if (!fnt_name) fnt_name = DEFAULT_X11_FONT_RECALL;

  /* Initialize the recall window */
  term_data_init(&recall, 80, 8, 16, FALSE, "Recall", fnt_name);
  term_recall = Term;


  /* Check environment for "choice" font */
  fnt_name = getenv("ANGBAND_X11_FONT_CHOICE");

  /* Check environment for "base" font */
  if (!fnt_name) fnt_name = getenv("ANGBAND_X11_FONT");

  /* No environment variables, use the default */
  if (!fnt_name) fnt_name = DEFAULT_X11_FONT_CHOICE;

  /* Initialize the choice window */
  term_data_init(&choice, 80, 24, 16, FALSE, "Choice", fnt_name);
  term_choice = Term;


  /* Activate the "Angband" window screen */
  Term_activate(&screen.t);

  /* Raise the "Angband" window */
  Infowin_set(screen.outer);
  Infowin_raise();


  /* Success */
  return (0);
}

#endif


