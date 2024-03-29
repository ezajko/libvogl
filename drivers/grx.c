/*
 * GRX driver for VOGL c1993 by Gary Murphy (garym@virtual.rose.utoronto.ca)
 *
 * To compile:
 *
 *      1) add GRX to device.c and mash-up your makefiles for MsDOS
 *      2) compile with DOBJ=-DPOSTSCRIPT -DHPGL -DGRX and MFLAGS=-O2
 *
 * To run:
 *
 *      set VDEVICE=grx
 *
 * grateful thanks to Lance Norskog (thinman@netcom.com) and Bernie
 * Kirby (bernie@ecr.mu.oz.au) --- should either of you be in my
 * neighbourhood, my offer of an Ice Beer is still open!
 * (Things in #ifdef BLART disabled by bernie...)
 */

#include <stdio.h>
#include <assert.h>

#include <stdlib.h>
#include <memory.h>
#include <grx.h>
#include <mousex.h>

#define MSG( m ) fprintf(stderr, "\n%s: %d: %s", __FILE__, __LINE__, (m))
#define ERROR1( m, p ) fprintf(stderr, "\n%s: %d: " m, __FILE__, __LINE__, (p))

#include	"vogl.h"

#define MAXCOLOR 256

static struct {

  GR_graphics_modes old_mode;

  int width, height, planes;

  GrContext *cbuf; /* current context */
  GrContext *fbuf;
  GrContext *bbuf;

  int palette[8];

  GrLineOption lopt; /* pen drawing options */
  int fg;	/* foreground/background colours */
  int bg;

  int has_mouse;
  MouseEvent last_mouse_event; /* most recently received (for cheating) */

  GrFont *font;
  GrFont *lfont;
  GrFont *sfont;

  int cx;
  int cy;

} grx;

/* access functions: */

/* I'm going to need this to fudge in stereo graphics ... */

GrContext *setBackBuffer( GrContext *newBB )
{
  GrContext *oldBB = grx.bbuf;
  assert( newBB != NULL);
  
  grx.bbuf = newBB;
  return oldBB;
}

static int grx_lwidth( int w );

static int
grx_init()
{
	grx.old_mode = GrCurrentMode();
	GrSetMode(GR_default_graphics);

#ifndef VOGLE
	vdevice.devname = "Grx";
#endif

	/* set the VOGL device */
	vdevice.sizeX = GrSizeY(); /* square max, was GrScreenX(); */
	vdevice.sizeY = GrSizeY();

	grx.width = vdevice.sizeSx = GrScreenX();
	grx.height = vdevice.sizeSy = GrScreenY();
	grx.planes = vdevice.depth = GrNumPlanes();

	grx.scrsize=( GrPlaneSize(grx.width, grx.height)* grx.planes)/sizeof(long);

	/* setup default palette */
	GrSetRGBcolorMode();
	grx.lopt.lno_color = grx.fg = GrWhite();
	grx.bg = GrBlack();

	grx.palette[BLACK] = GrAllocColor( 0, 0, 0 );
	grx.palette[RED] = GrAllocColor( 255, 0, 0 );
	grx.palette[GREEN] = GrAllocColor( 0, 255, 0 );
	grx.palette[YELLOW] = GrAllocColor( 255, 255, 0 );
	grx.palette[BLUE] = GrAllocColor( 0, 0, 255 );
	grx.palette[MAGENTA] = GrAllocColor( 255, 0, 255 );
	grx.palette[CYAN] = GrAllocColor( 0, 255, 255 );
	grx.palette[WHITE] = GrAllocColor( 255, 255, 255 );

	/* setup back/front buffers:
	 * frontbuffer is the current screen, back is a ram context
	*/
	grx.cbuf = grx.fbuf = GrSaveContext( NULL );
	grx.bbuf = NULL;

	/* initialize mouse */
	if ((grx.has_mouse = MouseDetect())==TRUE)
	  {
		/* dare I do interrupts? ... */
		MouseEventMode(1);
		MouseInit();

		/* no keyboard (use getch) */
		MouseEventEnable(0, 1);

		/* cheezy mouse speed algorithm (blame Lance for the pun) */
		if (grx.width * grx.height < 100000) 	
			MouseSetSpeed(6);
		else if (grx.width * grx.height < 200000) 	
			MouseSetSpeed(4);
		else if (grx.width * grx.height < 500000)
			MouseSetSpeed(3);
		else
			MouseSetSpeed(2);

		MouseWarp(1,1);
		MouseDisplayCursor();
	  };

	/* initial drawing style to thin solid lines */
	grx.lopt.lno_width = 1;
	grx.lopt.lno_pattlen = 0;
	grx.lopt.lno_dashpat = NULL;
	/* load initial fonts */
	if(getenv("GRXFONT") == NULL) GrSetFontPath("fonts");

	grx.font = grx.sfont = grx.lfont = NULL;

	vdevice.hwidth = 8.0;
	vdevice.hheight = 8.0;

	return (1);
}

/*
 * grx_frontbuffer, grx_backbuffer, grx_swapbuffers
 *
*/
static
int grx_frontbuffer() 
{ grx.cbuf = grx.fbuf; GrSetContext( grx.fbuf ); return (0); }

static
int grx_backbuffer() 
{ 
/* if they want a backbuffer, we'd better make one ... */

  if (grx.bbuf == NULL) 
	grx.bbuf = GrCreateContext( GrSizeX(), GrSizeY(), NULL, NULL );

  assert( grx.bbuf != NULL);

  grx.cbuf = grx.bbuf;
  GrSetContext( grx.bbuf ); 
  return (0); 
}

static
int grx_swapbuffers() 
{ 
  if (grx.cbuf == grx.fbuf )
	grx_backbuffer();
  else
	{
	  /* there are rumours of a portable VGA backbuffer using VESA
	   * but I've yet to track it down.
	   *
	   * the following copies by long words from back to front buffer
	   * modify this for regions by triming the first x-limit and y-limit
	   * and upping the pointers to the start of your subcontext
	   */

	  MouseEraseCursor();

	  /* WARNING WILL ROBINSON - WARNING WILL ROBINSON */
	  /* We're using the NC version so I can copy a 2W by H/2 backbuffer
	   * in my stereo graphics 'interlaced' mode
	   */

	  GrBitBltNC(grx.fbuf, 0,0, 
				 grx.bbuf, 0, 0, 
				 grx.bbuf->gc_xmax,grx.bbuf->gc_ymax, GrWRITE );

	MouseDisplayCursor();
  

	}
  return (0); 
}

#ifdef VOGLE
/*
 * grx_vclear
 *
 *	Clear the screen to current colour
 */
grx_vclear()
{
	GrClearScreen(grx.fg);
}

#else

/*
 * grx_vclear
 *
 *	Clear the viewport to current colour
 */
static int
grx_vclear()
{
  unsigned int    vw = vdevice.maxVx - vdevice.minVx;
  unsigned int    vh = vdevice.maxVy - vdevice.minVy;

  if ((vdevice.sizeSx == vw) && (vdevice.sizeSy == vh))
	{
	 GrClearContext(grx.fg);	/* full screen */
  }
  else
	GrFilledBox(
				vdevice.minVx,
				vdevice.sizeSy - vdevice.maxVy, 
				grx.width, 
				grx.height,
				grx.fg);
  return (1);
}

#endif

/*
 * grx_exit
 *
 *	Sets the display back to text mode.
 */
static int
grx_exit()
{
   	MouseUnInit();	/* disable mouse/keyboard interrupts */

	GrSetMode( grx.old_mode );
	GrDestroyContext( grx.bbuf );
	return (1);
}

static int grx_sync () { return (0); };

static	int
noop()
{
	return (-1);
}

/*
 * grx_font : load either of the fonts
*/
static int
grx_font(char *name)
{
  GrUnloadFont( grx.font );
  grx.font = GrLoadFont( name );
  vdevice.hheight = 16;
  vdevice.hwidth = 8;
  return (1);
}

static
int grx_char( int c )
{
  static char s[2] = "\0\0";

  s[0] = c;
  GrTextXY(vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy + 16, s, grx.fg, 0);
  return (1);
};

static int
grx_string(char *s)
{
	GrTextXY(vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy + 16, s, grx.fg, 0);
	return (1);
}


/*
 *	Everything is supposed to have been through the higher up clippers
 *	in vogl.. so no need to clip here..
 *
 *	Draw a solid 1 pixel wide line...
 *	libgrx checks for horizontal and vertical lines for us.
 */
static int
grx_solid( int x, int y )
{
	GrLineNC(x, vdevice.sizeSy -  y, 
		 vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy, 
		grx.fg
	);

	vdevice.cpVx = x;
	vdevice.cpVy = y;

	return(0);
}

/*
 * 	Draw a patterned and/or > 1 pixel wide line.
 *	(Waiting for libgrx to actually implement this...)
 */
static int
grx_pattern( int x, int y )
{
	GrCustomLine(vdevice.cpVx, vdevice.sizeSy - vdevice.cpVy, 
		     x, vdevice.sizeSy - y, 
		     &grx.lopt
	);

	vdevice.cpVx = x;
	vdevice.cpVy = y;

	return(0);
};

static int
grx_colour(int i)
{
  if (i<MAXCOLOR) grx.fg = grx.palette[i]; /* for now */
  else grx.fg = GrBlack();

  grx.lopt.lno_color = grx.fg;

  return (0);
};

/*
 * grx_mapcolor
 *
 *	change index i in the color map to the appropriate r, g, b, value.
 */
static int
grx_mapcolor(int c, int r, int g, int b)
{
	int	j;

	if (c >= MAXCOLOR || vdevice.depth == 1)
		return(-1);

	grx.palette[c] = GrAllocColor(r, g, b);
}
	

static int
grx_fill( int sides, int *x, int *y )
{
  int i;
  int points[sides][2];

  for (i=0; i<sides; i++)
   {
 	  points[i][0] = x[i];
 	  points[i][1] = grx.height - y[i];
	}

  GrFilledPolygon( sides, points, grx.fg );

  return (0);
};

static int
grx_checkkey()
{
	char c;

	if (kbhit()) {
		if ((c = getkey()) == 3) {	/* control-c */
			grx_exit();
			/* don't call vexit(), avoid back-refs */
			exit(0);
		} else
			return c;
	} else	return 0;
}

static int
grx_locator(int *x, int *y)
{
  static ox = 0, oy = 0, obuttons = 0;

  if (!grx.has_mouse) 
	{
	  *x = *y = 0;
	  return (-1);
	}

/*  if (MousePendingEvent())
	{
*/
	  MouseGetEvent( M_MOTION | M_BUTTON_CHANGE | M_POLL, &mEv );

	  if (mEv.flags & M_MOTION) {
		ox = mEv.x;
		oy = vdevice.sizeSy - mEv.y;
	  }
/*
 *	HACK... the RIGHT button is the second button and we want it
 *	to return 2...
 */
 
	  if (mEv.flags & M_BUTTON_CHANGE) {
		obuttons = ((mEv.buttons & M_LEFT) ? 1 : 0) |
		  ((mEv.buttons & M_MIDDLE) ? 2 : 0) |
			((mEv.buttons & M_RIGHT) ? 2 : 0);
	  }
/*	}
*/

  *x = ox;
  *y = oy;

  return (obuttons);
}

static
int grx_lwidth( int w )
{
	grx.lopt.lno_width = w;
	if (w == 1 && grx.lopt.lno_pattlen == 0)
		vdevice.dev.Vdraw = grx_solid;
	else
		vdevice.dev.Vdraw = grx_pattern;
}

static
int grx_lstyle( int s )
{
	/* BASICALLY UNIMPLEMENTED */
	grx.lopt.lno_pattlen = s;
	if (grx.lopt.lno_width == 1 && s == 0)
		vdevice.dev.Vdraw = grx_solid;
	else
		vdevice.dev.Vdraw = grx_pattern;
}

static DevEntry grxdev = {
	"grx",
	"@:pc8x8.fnt",
	"@:pc8x16.fnt",
	grx_backbuffer, /* backbuffer */
	grx_char, /* hardware char */
	grx_checkkey, /* keyhit */
	grx_vclear, /* clear viewport to current colour */
	grx_colour, /* set current colour */
	grx_solid, /* draw line */
	grx_exit, /* close graphics & exit */
	grx_fill, /* fill polygon */
	grx_font, /* set hardware font */
	grx_frontbuffer, /* front buffer */
	getkey, /* wait for and get key */
	grx_init, /* begin graphics */
	grx_locator, /* get mouse position */
	grx_mapcolor, /* map colour (set indices) */
#ifndef VOGLE
	grx_lstyle, /* set linestyle */
#endif
	grx_lwidth, /* set line width */
	grx_string, /* draw string of chars */
	grx_swapbuffers, /* swap buffers */
	grx_sync  /* sync display */
};

/*
 * _grx_devcpy
 *
 *	copy the pc device into vdevice.dev. (as listed in drivers.c)
 */
int _grx_devcpy()
{
	vdevice.dev = grxdev;
	return(0);
}

