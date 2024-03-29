/*
 * draw.c, part of the gwave waveform viewer tool
 *
 * Functions for drawing waveforms
 *
 * Copyright (C) 1998, 1999 Stephen G. Tell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <gtk/gtk.h>

#include <gwave.h>
#include <wavewin.h>

/* convert double value to printable text.
 * Two modes:
 * 0: using suffixes
 *    We always try to print 4 significant figures, with one nonzero digit
 *    to the left of the decimal point.
 *    maximum field width: 7 characters
 *
 * 1: use scientific notation, printf %g format.
 *    maximum field width appears to be 10 characters
 */
char *val2txt(double val, int mode)
{
	static char buf[64];
	double aval = fabs(val);
	double sval, asval;
	char suffix;
	int ddigits;

	switch(mode) {
	case 1:
		sprintf(buf, "% .5g", val);
		break;
	case 0:
	default:
		if(1e12 <= aval) {
			suffix = 'T';
			sval = val / 1e12;
		} else if(1e9 <= aval && aval < 1e12) {
			suffix = 'G';
			sval = val / 1e9;
		} else if(1e6 <= aval && aval < 1e9) {
			suffix = 'M';
			sval = val / 1e6;
		} else if(1e3 <= aval && aval < 1e6) {
			suffix = 'K';
			sval = val / 1000;
		} else if(1e-3 <= aval && aval < 1) {
			suffix = 'm';
			sval = val * 1000;
		} else if(1e-6 <= aval && aval < 1e-3) {
			suffix = 'u';
			sval = val * 1e6;
		} else if(1e-9 <= aval && aval < 1e-6) {
			suffix = 'n';
			sval = val * 1e9;
		} else if(1e-12 <= aval && aval < 1e-9) {
			suffix = 'p';
			sval = val * 1e12;
		} else if(1e-15 <= aval && aval < 1e-12) {
			suffix = 'f';
			sval = val * 1e15;
		} else if(DBL_EPSILON < aval && aval < 1e-15) {
			suffix = 'a';
			sval = val * 1e18;
		} else {
			suffix = ' ';
			sval = val;
		}
		asval = fabs(sval);
		if(1.0 <=  asval && asval < 10.0) 
			ddigits = 4;
		else if(10.0 <=  asval && asval < 100.0) 
			ddigits = 3;
		else 
			ddigits = 2;
		sprintf(buf, "% .*f%c", ddigits, sval, suffix);
		break;
	}	
	return buf;
}

/* convert pixmap Y coordinate to user dependent-variable value */
double y2val(WavePanel *wp, int y)
{
	int h = wp->drawing->allocation.height;
	double frac = - (double)(y - h + 3) / (double)(h - 6);
	
	if(wp->logy) {
		double a;
		a = frac * (log10(wp->end_yval) - log10(wp->start_yval)) 
			+ log10(wp->start_yval);
		return pow(10, a);
	} else {
		return frac * (wp->end_yval - wp->start_yval) 
			+ wp->start_yval;
	}
}

/* convert user value to pixmap y coord */
int val2y(WavePanel *wp, double val)
{
	double top = wp->end_yval;
	double bot = wp->start_yval;
	int h = wp->drawing->allocation.height;
	double frac;
	
	if(wp->logy) {
		if(bot < 0 || val < 0)
			return -1;

		frac = (log10(val) - log10(bot)) / 
			(log10(top) - log10(bot));
	} else {
		frac = (val - bot ) / (top - bot);
	}

	return h - ((h-6) * frac) - 3;
}

/* convert pixmap X coordinate to user independent-variable value */
double x2val(WavePanel *wp, int x, int log)
{
	int w = wp->drawing->allocation.width;
	double frac = (double)x / (double)w;

	if(log) {
		double a;
		a = frac * (log10(wp->end_xval) - log10(wp->start_xval)) 
			+ log10(wp->start_xval);
		return pow(10, a);
	} else {
		return frac * (wp->end_xval - wp->start_xval) 
			+ wp->start_xval;
	}
}

/* convert independent-variable value to pixmap X coordinate */
int val2x(WavePanel *wp, double val, int log)
{
	int w = wp->drawing->allocation.width;
	double frac;

	if(log) {
		if(val < 0 || val < wp->start_xval)
			return -1;

		frac = (log10(val) - log10(wp->start_xval)) /
			(log10(wp->end_xval) - log10(wp->start_xval));
	} else {
		frac = (val - wp->start_xval) /
			(wp->end_xval - wp->start_xval);
	}
	return w * frac;
}

typedef void (*WaveDrawFunc) (VisibleWave *vw, WavePanel *wp);
struct wavedraw_method {
	WaveDrawFunc func;
	char *desc;
};

void vw_wp_draw_ppixel(VisibleWave *vw, WavePanel *wp);
void vw_wp_draw_lineclip(VisibleWave *vw, WavePanel *wp);

struct wavedraw_method wavedraw_method_tab[] = {
	vw_wp_draw_ppixel, "per-pixel",
	vw_wp_draw_lineclip, "correct-line"
};

const int n_wavedraw_methods = sizeof(wavedraw_method_tab)/sizeof(struct wavedraw_method);


/*
 * We know how to do this right, but working on other things has taken
 * precedence.  Remarkably, this isn't particularly slow or ugly looking.
 *
 * TODO: smarter partial redraws on scrolling, expose, etc.
 */
void
vw_wp_visit_draw(VisibleWave *vw, WavePanel *wp)
{
	if(!vw->gc) {
		if(!vw->label) {
			fprintf(stderr, "visit_draw(%s): label=NULL\n",
				vw->varname);
			return;
		}
		if(!gdk_color_alloc(win_colormap, 
				    &vw->label->style->fg[GTK_STATE_NORMAL])) {
			fprintf(stderr, 
				"visit_draw(%s): gdk_color_alloc failed\n",
				vw->varname);
			return;
		}
		vw->gc = gdk_gc_new(wp->drawing->window);
		gdk_gc_set_foreground(vw->gc,
				      &vw->label->style->fg[GTK_STATE_NORMAL]);
	}
	g_assert(vw->gc != NULL);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vw->button)))
                 gdk_gc_set_line_attributes(vw->gc,
                                            2, GDK_LINE_SOLID, GDK_CAP_BUTT,
                                            GDK_JOIN_ROUND);
         else
                 gdk_gc_set_line_attributes(vw->gc,
                                            1, GDK_LINE_SOLID, GDK_CAP_BUTT,
                                            GDK_JOIN_ROUND);
	
	(wavedraw_method_tab[1].func)(vw, wp);
}

/* finish what we started in vw_wp_visit_draw(),
 * using various different drawing algorithms
 */

/* half-assed pixel-steping wave-drawing routine.
 * gets data value and draws a line for every pixel.
 * will exhibit aliasing if data has samples at higher frequency than
 * the screen has pixels.
 * Fast but can alias badly. 
 */
void
vw_wp_draw_ppixel(VisibleWave *vw, WavePanel *wp)
{
	int x0, x1;
	int y0, y1;
	int i;
	double xstep;
	double xval;
	double yval;
	int w = wp->drawing->allocation.width;
	int h = wp->drawing->allocation.height;

	xstep = (wp->end_xval - wp->start_xval)/w;  /* linear only */

	x1 = 0;
	yval = wv_interp_value(vw->var, wp->start_xval);
	y1 = val2y(wp, yval);

	for(i = 0, xval = wp->start_xval; i < w; i++ ) {
		x0 = x1; y0 = y1;
		x1 = x0 + 1;
		if(vw->var->wv_iv->wds->min <= xval
		   && xval <= vw->var->wv_iv->wds->max) {

			yval = wv_interp_value(vw->var, xval);
			y1 = val2y(wp, yval);
			gdk_draw_line(wp->pixmap, vw->gc, x0,y0, x1,y1);
		}
		if(wtable->logx)
			xval = x2val(wp, x0+1, wtable->logx);
		else
			xval += xstep;
	}

}

int point_code (double x, double y, 
                double xmn, double ymn,
                double xmx, double ymx);
void swap_i (int *a, int *b);
void swap_d (double *a, double *b);

/*-----------------------------------------------------------------------
  ROUTINE:  line_clip
  PURPOSE:  To clip a line segment against a rectangular window.
 
  INPUT:    x1, y1,    double endpoints of line segment to be clipped.
            x2, y2
            xmn, ymn   double corners of clipping window.
            xmx, ymx
            returns    int  TRUE if line segment inside window.
                            FALSE if outside.
  LOCAL:    i1, i2     int codes indicating location of points
                       relative to viewport boundaries.  (see
                       description in point_code()).
 
  KEYWORD:  TWODIM * CLIP LINE VIEWPORT
 
  REMARKS:  The direction of the line segment may be reversed.
 
  DATE:     July '80  J.A.T, May 2002 C.D.
 -----------------------------------------------------------------------*/
 int line_clip (double *x1, double *y1, double *x2, double *y2,
                double xmn, double ymn, double xmx, double ymx) {

	int    i1, i2;

/* Code each point.   */
	i1 = point_code (*x1, *y1, xmn, ymn, xmx, ymx);
	i2 = point_code (*x2, *y2, xmn, ymn, xmx, ymx);

/* ARE BOTH ENDPOINTS IN VIEWING PORT? */
	while(i1 != 0 || i2 != 0) {
		if((i1 & i2) != 0)	/* Is segment outside viewport?   */
			return FALSE;
		if(i1 == 0) {		/* Is point 1 outside viewport?   */
			swap_i(&i1, &i2);
			swap_d(x1, x2);
			swap_d(y1, y2);
		}

		if((i1 & 1) != 0) {			/* Move toward left edge.   */
			*y1 = *y1 + (*y2 - *y1) * (xmn - *x1) / (*x2 - *x1);
			*x1 = xmn;
		} else
			if((i1 & 2) != 0) {		/* Move toward right edge.   */
				*y1 = *y1 + (*y2 - *y1) * (xmx - *x1) / (*x2 - *x1);
				*x1 = xmx;
			} else
				if((i1 & 4) != 0) {	/* Move toward bottom.   */
					*x1 = *x1 + (*x2 - *x1) * (ymn - *y1) / (*y2 - *y1);
					*y1 = ymn;
				} else {			/* Move toward top.   */
					*x1 = *x1 + (*x2 - *x1) * (ymx - *y1) / (*y2 - *y1);
					*y1 = ymx;
				}
		i1 = point_code(*x1, *y1, xmn, ymn, xmx, ymx);
	}
	return TRUE;
}

/*-----------------------------------------------------------------------
  FUNCTION: point_code
  PURPOSE:  To encode a point.
  REMARKS:  This routine returns an integer code for
            the point based on this mapping:
 
                      *      *
                 1001 * 1000 * 1010
                      *      *
                ********************
                      *      *
                 0001 * 0000 * 0010
                      *      *
                ********************
                      *      *
                 0101 * 0100 * 0110
                      *      *
 
  INPUT:    x,y        double coordinates of point.
            xmn, ymn   double corners of window
            xmx, ymx
            returns    int code from above (0-10)
 
  KEYWORD:  TWODIM * CODE POINT
 
  DATE:     July '80  J.A.T. May 2002 C.D.
 -----------------------------------------------------------------------*/
int point_code(double x, double y, 
               double xmn, double ymn,
               double xmx, double ymx) {
	int  ic;

	ic = 0;       
	if(x < xmn) ic = 1;
	if(x > xmx) ic = 2;
	if(y < ymn) ic+=4;
	if(y > ymx) ic+=8;
	return ic;
}

void swap_i(int *a, int *b) {
	int temp;
	temp = *a;
	*a = *b;
	*b = temp;
	return;
}

void swap_d(double *a, double *b) {
	double temp;
	temp = *a;
	*a = *b;
	*b = temp;
	return;
}

/* visit all data points, applying line-clipping algorithm */
void
vw_wp_draw_lineclip(VisibleWave *vw, WavePanel *wp)
{
	int x0, x1;
	int y0, y1;
	int i;
	double xstep;
	double xval;
	double yval;
        double xval0, yval0, xval1, yval1;
        double xval0d, yval0d, xval1d, yval1d;

        xval1 = wds_get_point(&vw->var->wv_iv->wds[0], 0);
        yval1 = wds_get_point(&vw->var->wds[0], 0);

        for(i = 1; i < vw->var->wtable->nvalues; i++) {
                xval0d = xval1;
                yval0d = yval1;
                xval1d = xval1 = wds_get_point(&vw->var->wv_iv->wds[0], i);
                yval1d = yval1 = wds_get_point(&vw->var->wds[0], i);

                if(line_clip(&xval0d, &yval0d, &xval1d, &yval1d,
                             wp->start_xval, wp->start_yval,
                             wp->end_xval, wp->end_yval))  {
                        x0 = val2x(wp, xval0d, wtable->logx);
                        y0 = val2y(wp, yval0d);
                        x1 = val2x(wp, xval1d, wtable->logx);
                        y1 = val2y(wp, yval1d);
                        if(x0 != x1 || y0 != y1)
                                gdk_draw_line(wp->pixmap, vw->gc, x0,y0, x1,y1);
                }
        }
}

/*
 * Repaint all or part of a wavepanel.
 */
void 
draw_wavepanel(GtkWidget *widget, GdkEventExpose *event, WavePanel *wp)
{
	int w = widget->allocation.width;
	int h = widget->allocation.height;
	int x, y;
	int i;

	if(wp->pixmap == NULL)
		return;

	gdk_draw_rectangle(wp->pixmap, bg_gdk_gc, TRUE, 0,0, w,h);

	if(wp->selected) {
/*		gdk_draw_line(wp->pixmap, hl_gdk_gc, 0,   0,  w-1, 0);
		gdk_draw_line(wp->pixmap, hl_gdk_gc, w-1, 0,  w-1, h-1);
		gdk_draw_line(wp->pixmap, hl_gdk_gc, w-1, h-1,  0, h-1);
		gdk_draw_line(wp->pixmap, hl_gdk_gc, 0,   h-1,  0, 0);
*/
		gdk_draw_line(wp->pixmap, hl_gdk_gc, 1,   1,  w-2, 1);
		gdk_draw_line(wp->pixmap, hl_gdk_gc, w-2, 1,  w-2, h-2);
		gdk_draw_line(wp->pixmap, hl_gdk_gc, w-2, h-2,  1, h-2);
		gdk_draw_line(wp->pixmap, hl_gdk_gc, 1,   h-2,  1, 1);

	}
	/* draw horizontal line at y=zero.  future: do real graticule here */
	if(wp->start_yval < 0 && wp->end_yval > 0) {
		y = val2y(wp, 0);
		gdk_draw_line(wp->pixmap, pg_gdk_gc, 0, y, w, y);
	}

	/* draw waves */
	g_list_foreach(wp->vwlist, (GFunc)vw_wp_visit_draw, wp); 

	/* draw cursors */
	for(i = 0; i < 2; i++) {			
		VBCursor *csp = wtable->cursor[i];
		if(csp->shown) {
			if(wp->start_xval <= csp->xval 
			   && csp->xval <= wp->end_xval) {
				x = val2x(wp, csp->xval, wtable->logx);
				gdk_draw_line(wp->pixmap, csp->gdk_gc,
					      x, 0, x, h);
			}
		}
	}
	/* draw select-range line, if in this WavePanel */
	if(wtable->srange->drawn && wtable->srange->wp == wp)
		draw_srange(wtable->srange);

	if(event) {
	  /* Draw the exposed portions of the pixmap in its window. */
		gdk_draw_pixmap(widget->window,
				widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
		  wp->pixmap,
		  event->area.x, event->area.y,
		  event->area.x, event->area.y,
		  event->area.width, event->area.height);
	} else {
	  /* Draw the whole thing. */
		gdk_draw_pixmap(widget->window,
				widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
		  wp->pixmap,
		  0, 0, 0, 0, w, h);
	}
}

/*
 * update text labeling the waveform graphs' X-axis
 */
void draw_labels(WaveTable *wt)
{
	gtk_label_set(GTK_LABEL(wt->xlabel_left), val2txt(wt->start_xval, 0));
	gtk_label_set(GTK_LABEL(wt->xlabel_right), val2txt(wt->end_xval, 0));
}

/*
 * redraw contents of all wavepanels
 */
SCM_DEFINE(wtable_redraw_x, "wtable-redraw!", 0, 0, 0, (),
	   "Redraw the waveforms in all wavepanels")
#define FUNC_NAME s_wtable_redraw_x
{
	int i;
	WavePanel *wp;
	wtable->suppress_redraw = 0;
	for(i = 0; i < wtable->npanels; i++) {
		wp = wtable->panels[i];
		draw_wavepanel(wp->drawing, NULL, wp);
	}
	return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/* Color allocation and related stuff for waveform drawing area
 * background and cursors, done on first expose event.
 * Actually, we do it all on the first expose of the first drawing area,
 * and hope that this is OK.  They are all in the same GtkWindow.
 */
void alloc_colors(GtkWidget *widget)
{
	int i;
	if(win_colormap == NULL)
		win_colormap = gdk_window_get_colormap(widget->window);

	/* background */
	if(bg_color_name) {  /* explicitly set background color */
		gdk_color_alloc(win_colormap, &bg_gdk_color);
		bg_gdk_gc = gdk_gc_new(widget->window);
		gdk_gc_set_foreground(bg_gdk_gc, &bg_gdk_color);
	} else {  /* use the widget's default one - usually grey */
		bg_gdk_color = widget->style->bg[GTK_WIDGET_STATE(widget)];
		bg_gdk_gc = widget->style->bg_gc[GTK_WIDGET_STATE(widget)];
	}
	if(v_flag)
		fprintf(stderr, "panel background pixel=0x%x rgb=%d,%d,%d\n",
			bg_gdk_color.pixel,
			bg_gdk_color.red,
			bg_gdk_color.green,
			bg_gdk_color.blue);

	/* vertical bar cursors */
	for(i = 0; i < 2; i++) {
		GdkColor tmp_color;
		VBCursor *csp = wtable->cursor[i];
		if(!gdk_colormap_alloc_color(win_colormap, &csp->gdk_color, FALSE, TRUE)) {
			fprintf(stderr, "gdk_color_alloc failed for cursor %d\n", i);
			exit(2);
		}
		csp->gdk_gc = gdk_gc_new(widget->window);
		if(!csp->gdk_gc) {
			fprintf(stderr, "couldn't allocate cursor %d gc\n", i);
			exit(2);
		}
		/* compute pixel to draw so XOR makes it come out right
		 * on the background.
		 */
		tmp_color.pixel = csp->gdk_color.pixel ^ bg_gdk_color.pixel;
		gdk_gc_set_foreground(csp->gdk_gc, &tmp_color);
		gdk_gc_set_function(csp->gdk_gc, GDK_XOR);

		if(v_flag)
			fprintf(stderr, "cursor[%d] pixel=0x%x drawpix=0x%x rgb=%d,%d,%d\n",
				i, csp->gdk_color.pixel, tmp_color.pixel,
				csp->gdk_color.red,
				csp->gdk_color.green,
				csp->gdk_color.blue);
	}

	/* graticule or zero-line */
	gdk_colormap_alloc_color(win_colormap, &pg_gdk_color, FALSE, TRUE);
	pg_gdk_gc = gdk_gc_new(widget->window);
	if(!pg_gdk_gc) {
			fprintf(stderr, "couldn't allocate graticule gc\n");
			exit(2);
	}
	gdk_gc_set_foreground(pg_gdk_gc, &pg_gdk_color);

	/* panel highlight */
	gdk_colormap_alloc_color(win_colormap, &hl_gdk_color, FALSE, TRUE);
	hl_gdk_gc = gdk_gc_new(widget->window);
	if(!hl_gdk_gc) {
			fprintf(stderr, "couldn't allocate highlight gc\n");
			exit(2);
	}
	gdk_gc_set_foreground(hl_gdk_gc, &hl_gdk_color);
}


/* TODO: figure out how to get these colors from styles in wv.gtkrc
 * without the hack that we use for waveform colors (picking them up from
 * labels of the same color).
 */
void setup_colors(WaveTable *wt)
{
	int i;

	/* cursors */
	wt->cursor[0]->color_name = "white";
	wt->cursor[1]->color_name = "yellow";
	for(i = 0; i < 2; i++) {
		if(!gdk_color_parse(wt->cursor[i]->color_name, 
				    &wt->cursor[i]->gdk_color)) {
			fprintf(stderr, "failed to parse cursor %d color\n", i);
			exit(1);
		}
	}

	/* range-select line */
	if(!gdk_color_parse("white", &wt->srange->gdk_color)) {
		fprintf(stderr, "failed to parse selrange color\n");
		exit(1);
	}

	/* waveform background */
	if(bg_color_name) {
		if(!gdk_color_parse(bg_color_name, &bg_gdk_color)) {
			fprintf(stderr, "failed to parse bg color\n");
			exit(1);
		}
	}

	/* waveform panel axis lines or graticule */
	if(pg_color_name) {
		if(!gdk_color_parse(pg_color_name, &pg_gdk_color)) {
			fprintf(stderr, "failed to parse panel graticule color\n");
			exit(1);
		}
	}
	/* waveform panel axis lines or graticule */
	if(hl_color_name) {
		if(!gdk_color_parse(hl_color_name, &hl_gdk_color)) {
			fprintf(stderr, "failed to parse highlight color\n");
			exit(1);
		}
	}
}

/* given a string containing a number, possibly with a spice-syntax suffix,
 * return the value  */
double spice2val(char *s)
{
	double val, mul;
	char *ep;
	val = strtod(s, &ep);

	if(ep && ep != s && *ep) {
		switch(*ep) {
		case 'T':
			mul = 1e12;
			break;
		case 'G':
			mul = 1e9;
			break;
		case 'M':
			mul = 1e6;
			break;
		case 'k':
		case 'K':
			mul = 1e3;
			break;
		case 'm':
			mul = 1e-3;
			break;
		case 'u':
			mul = 1e-6;
			break;
		case 'n':
			mul = 1e-9;
			break;
		case 'p':
			mul = 1e-12;
			break;
		case 'f':
			mul = 1e-15;
			break;
		case 'a':
			mul = 1e-18;
			break;
		default:
			mul = 1;
			break;
		}

		return val * mul;
	} else {
		return val;
	}
}

SCM_DEFINE(spice_number, "spice->number", 1, 0, 0, (SCM str),
"Given a string SSTR containing a representation of a number,"
"possibly containing spice-style multiplier suffixes, return a real number.")
#define FUNC_NAME s_spice_number
{
	double dval;
	char *s;
	VALIDATE_ARG_STR_NEWCOPY(1, str, s);

	dval = spice2val(s);
	free(s);
	return scm_make_real(dval);
}
#undef FUNC_NAME

SCM_DEFINE(number_spice, "number->spice", 1, 0, 0, (SCM val),
 "Given a real number VAL, return a string representation "
"in spice suffix syntax.")
#define FUNC_NAME s_number_spice
{
	double dval;
	char *s;
	VALIDATE_ARG_DBL_COPY(1, val, dval);
	s = val2txt(dval, 0);
	return scm_makfrom0str(s);
}
#undef FUNC_NAME

/* guile initialization */
void init_draw()
{

#ifndef SCM_MAGIC_SNARF_INITS
#include "draw.x"
#endif
}
