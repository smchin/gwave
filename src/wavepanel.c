/*
 * wavepanel.c, part of the gwave waveform viewer tool
 *
 * Functions in this file handle wave panels.
 *
 * Copyright (C) 1998, 1999, 2000 Stephen G. Tell.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
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
#include <guile-gtk.h>

#include <config.h>
#include <scwm_guile.h>
#include <gwave.h>
#include <wavelist.h>
#include <wavewin.h>

#define WAVEPANEL_MIN_WIDTH 400
#define WAVEPANEL_MIN_HEIGHT 20
#define WAVEPANEL_MAX_REQHEIGHT 400

SCM_HOOK(new_wavepanel_hook,"new-wavepanel-hook", 1, (SCM wp),
"This hook is invoked with one WavePanel argument, WP, when the
WavePanel is first created and added to the waveform window.
The main purpose of this hook is to allow creation of 
popup menus and other event bindings for the wavepanel.");

SCM wavepanel_mouse_binding[6];
WavePanel *last_drop_wavepanel;

/* generate VisibleWave button label string,
 * for both initial setup and updating.
 */
void
vw_get_label_string(char *buf, int buflen, VisibleWave *vw)
{
	GWDataFile *gdf;
	double xval, dval;
	int n, l;

	gdf = vw->gdf;
	g_assert(gdf != NULL);

	l = buflen - strlen(gdf->ftag) - 10;
	n = MIN(l, 15);
	xval = wtable->cursor[0]->xval;
	if(vw->var->wv_iv->wds->min <= xval 
	   && xval <= vw->var->wv_iv->wds->max) {

		dval = wv_interp_value(vw->var, xval);
		sprintf(buf, "%s: %.*s %s",
			gdf->ftag, l, vw->varname, val2txt(dval, 0));
	} else {
		/* should keep track of label state (name vs. name+val)
		 * and only re-do this if necessary */
		sprintf(buf, "%s: %.*s    ", gdf->ftag, l, vw->varname);
	}
}

/*
 * vw_wp_create_button -- called from g_list_foreach to 
 * create button and label widgets for one VisibleWave in a WavePanel,
 */
void
vw_wp_create_button(VisibleWave *vw, WavePanel *wp)
{
	char lbuf[64];

	vw_get_label_string(lbuf, 64, vw);
	vw->label = gtk_label_new(lbuf);
	vw->button = gtk_toggle_button_new();
	gtk_box_pack_start(GTK_BOX(wp->lvbox), vw->button,
			   FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vw->button), vw->label);
	sprintf(lbuf, "wavecolor%d", vw->colorn);
	gtk_widget_set_name(vw->label, lbuf);
	gtk_widget_show(vw->label);
	gtk_widget_set_name(vw->button, "wavebutton");
	gtk_widget_show(vw->button);
}


/* Allocate a new WavePanel and do basic setup */
WavePanel *
new_wave_panel()
{
	WavePanel *wp;
	wp = g_new0(WavePanel, 1);
	wp->valid = 1;

	SGT_NEWCELL_SMOB(wp->smob, WavePanel, wp);
	scm_protect_object(wp->smob);
	wp->outstanding_smob = 1;
	call1_hooks(new_wavepanel_hook, wp->smob);

	return wp;
}

/*
 * Construct label area on the left side of a WavePanel
 */
void setup_wavepanel_lvbox(WavePanel *wp, int showlabels)
{
	char lbuf[128];

	/* y-axis labels and signal names, all in a vbox */
	wp->lvbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_set_usize(wp->lvbox, 160, -1);
	gtk_widget_show(wp->lvbox);

	wp->lab_max_hbox = gtk_hbox_new(FALSE, 0);
	strcpy(lbuf, val2txt(wp->max_yval, 0));
	wp->lab_max = gtk_label_new(lbuf);
	gtk_box_pack_start(GTK_BOX(wp->lvbox), wp->lab_max_hbox,
			   FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(wp->lab_max_hbox), wp->lab_max,
			 FALSE, FALSE, 0);
	gtk_widget_show(wp->lab_max);

	wp->lab_logscale = gtk_label_new("LogY");
	gtk_box_pack_start(GTK_BOX(wp->lab_max_hbox), wp->lab_logscale,
			 FALSE, FALSE, 0);

	wp->lab_min_hbox = gtk_hbox_new(FALSE, 0);
	strcpy(lbuf, val2txt(wp->min_yval, 0));
	wp->lab_min = gtk_label_new(lbuf);
	gtk_box_pack_end(GTK_BOX(wp->lvbox), wp->lab_min_hbox,
			 FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(wp->lab_min_hbox), wp->lab_min,
			 FALSE, FALSE, 0);
	gtk_widget_show(wp->lab_min);

	g_list_foreach(wp->vwlist, (GFunc)vw_wp_create_button, wp);

	if(showlabels) {
		gtk_widget_show(wp->lab_min_hbox);
		gtk_widget_show(wp->lab_max_hbox);
	}
}

/*
 * Set up widgets for a newly-created WavePanel -
 *    construct lvbox and drawing area
 */ 
void setup_wave_panel(WavePanel *wp, int minheight, int showlabels)
{
	if(v_flag) {
		fprintf(stderr, "setup_wave_panel minheight%d showlabels=%d\n",
			minheight, showlabels);
	}
	wp->start_xval = wtable->start_xval;
	wp->end_xval = wtable->end_xval;

	setup_wavepanel_lvbox(wp, showlabels);

	/* drawing area for waveform */
	wp->drawing = gtk_drawing_area_new();
	gtk_signal_connect(
		GTK_OBJECT(wp->drawing), "expose_event", 
		(GtkSignalFunc)expose_handler, (gpointer)wp);
	gtk_signal_connect(
		GTK_OBJECT(wp->drawing), "button_press_event", 
		(GtkSignalFunc)button_press_handler, (gpointer)wp);
	gtk_signal_connect(
		GTK_OBJECT(wp->drawing), "button_release_event", 
		(GtkSignalFunc)button_release_handler, (gpointer)wp);
	gtk_signal_connect(
		GTK_OBJECT(wp->drawing), "motion_notify_event", 
		(GtkSignalFunc)motion_handler, (gpointer)wp);

	if(minheight < WAVEPANEL_MIN_HEIGHT)
		wp->req_height = WAVEPANEL_MIN_HEIGHT;
	else if(minheight > WAVEPANEL_MAX_REQHEIGHT)
		wp->req_height = WAVEPANEL_MAX_REQHEIGHT;
	else
		wp->req_height = minheight;

	gtk_drawing_area_size(GTK_DRAWING_AREA(wp->drawing), 
			      wp->width ? wp->width : WAVEPANEL_MIN_WIDTH,
			      wp->req_height);
	gtk_widget_show(wp->drawing);

	dnd_setup_target(wp->drawing, wp);

	gtk_widget_set_events(wp->drawing, 
			      GDK_EXPOSURE_MASK|GDK_BUTTON_RELEASE_MASK|
			      GDK_BUTTON_PRESS_MASK|
			      GDK_BUTTON1_MOTION_MASK|GDK_BUTTON2_MOTION_MASK);
}

/*
 * Delete a wavepanel structure and all data structures referenced from it.
 */
void destroy_wave_panel(WavePanel *wp)
{
	VisibleWave *vw;

	while((vw = g_list_nth_data(wp->vwlist, 0)) != NULL) {
		remove_wave_from_panel(wp, vw);
	}
	gtk_widget_destroy(wp->lvbox);
	gtk_widget_destroy(wp->drawing);
	gdk_pixmap_unref(wp->pixmap);
	wp->valid = 0;
	scm_unprotect_object(wp->smob);
	if(wp->outstanding_smob == 0)
		g_free(wp);
}



SCM_DEFINE(set_wavepanel_ylabels_visible_x, "set-wavepanel-ylabels-visible!", 2, 0, 0,
	   (SCM wavepanel, SCM show),
"If SHOW is #t, make the Y-axis labels on the left side of WAVEPANEL
visible.  If show is #f, hide the labels.  Hiding the labels allows
shrinking WAVEPANEL's height a little further.  This is useful when you have
a lot of panels, for example with digital circuits.")
#define FUNC_NAME s_set_wavepanel_ylabels_visible_x
{
	WavePanel *wp;
	int ishow;
	VALIDATE_ARG_WavePanel_COPY(1,wavepanel,wp);
	VALIDATE_ARG_BOOL_COPY(2, show, ishow);

	if(ishow) {
		gtk_widget_show(wp->lab_min_hbox);
		gtk_widget_show(wp->lab_max_hbox);
	} else {
		gtk_widget_hide(wp->lab_min_hbox);
		gtk_widget_hide(wp->lab_max_hbox);
	}
	return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE(set_wavepanel_ylogscale_x, "set-wavepanel-ylogscale!", 2, 0, 0,
	   (SCM wavepanel, SCM logscale),
"If LOGSCALE is #t, The Y-axis of WAVEPANEL is set to have
Logarithmic scaling.  Otherwise, scaling is linear.")
#define FUNC_NAME s_set_wavepanel_ylogscale_x
{
	WavePanel *wp;
	int logy;
	VALIDATE_ARG_WavePanel_COPY(1,wavepanel,wp);
	VALIDATE_ARG_BOOL_COPY(2, logscale, logy);
	
	if(wp->logy != logy) {
		wp->logy = logy;
		if(logy)
			gtk_widget_show(wp->lab_logscale);
		else
			gtk_widget_hide(wp->lab_logscale);
		draw_wavepanel(wp->drawing, NULL, wp);
	}
	return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE(wavepanel_ylogscale_p, "wavepanel-ylogscale?", 1, 0, 0,
	   (SCM wavepanel),
	   "If WAVEPANEL is set to Logarithmic scaling, return #t.")
#define FUNC_NAME s_wavepanel_ylogscale_p
{
	WavePanel *wp;
	int logy;
	VALIDATE_ARG_WavePanel_COPY(1,wavepanel,wp);

	if(wp->logy)
		return SCM_BOOL_T;
	else
		return SCM_BOOL_F;
}
#undef FUNC_NAME


SCM  /* Helper for wavepanel_visiblewaves */
wavepanel_to_scm(WavePanel *wp)
{
	return wp->smob;
}

SCM_DEFINE(wavepanel_visiblewaves, "wavepanel-visiblewaves", 1, 0, 0,
	   (SCM wavepanel),
	   "Return a list of the VisibleWaves contained in WAVEPANEL.")
#define FUNC_NAME s_wavepanel_visiblewaves
{
	WavePanel *wp;

	VALIDATE_ARG_WavePanel_COPY(1,wavepanel,wp);

	return glist2scm(wp->vwlist, wavepanel_to_scm);
}
#undef FUNC_NAME


/* 
 * this binding stuff is really quite a hack; we should build a common
 * mechanism for mouse and keyboard bindings, together with modifiers.
 * But the scheme interface here isn't too far from what that would implement,
 * and this simple thing lets us get the rest of the old menus into guile.
 */
SCM_DEFINE(wavepanel_bind_mouse, "wavepanel-bind-mouse", 2, 0, 0,
           (SCM button, SCM proc),
"binds a mouse BUTTON to the procedure PROC in all wavepanels. 
PROC is called with 1 argument, the wavepanel that the mouse was in.")
#define FUNC_NAME s_wavepanel_bind_mouse
{
	int bnum;
	VALIDATE_ARG_INT_RANGE_COPY(1, button, 1, 5, bnum);
	VALIDATE_ARG_PROC(2, proc);

/* TODO: find right way to initialize and test to remove old binding.
  if(gh_procedure_p(wavepanel_mouse_binding[bnum])) {
                scm_unprotect_object(wavepanel_mouse_binding[bnum]);
        }
*/
	if (!UNSET_SCM(proc)) {
		scm_protect_object(proc);
		wavepanel_mouse_binding[bnum] = proc;
	}
		
	return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

SCM_DEFINE(wavepanel_x2val, "wavepanel-x2val", 2, 0, 0,
	   (SCM wavepanel, SCM xpixel),
"Given an XPIXEL coordinate in WAVEPANEL, 
return the value of the independent variable at that position
in the waveform.")
#define FUNC_NAME s_wavepanel_x2val
{
	WavePanel *wp;
	int x;
	double val;
	VALIDATE_ARG_WavePanel_COPY_USE_NULL(1,wavepanel,wp);
	VALIDATE_ARG_INT_COPY(2,xpixel,x);
	val = x2val(wp, x, wtable->logx);
	return gh_double2scm(val);
}
#undef FUNC_NAME

SCM_DEFINE(set_wavepanel_minheight_x, "set-wavepanel-minheight!", 2, 0, 0,
	   (SCM wavepanel, SCM height),
"Set the minimum height of WAVEPANEL to HEIGHT pixels.  Adding multiple
VisibleWaves to the wavepanel can cause the actual height to increase
beyond this minimum, but it will never be smaller.")
#define FUNC_NAME s_set_wavepanel_minheight_x
{
	WavePanel *wp;
	int min_height;
	VALIDATE_ARG_WavePanel_COPY(1,wavepanel,wp);
	VALIDATE_ARG_INT_RANGE_COPY(2,height,WAVEPANEL_MIN_HEIGHT,400,min_height);
	wp->req_height = min_height;
	gtk_drawing_area_size(GTK_DRAWING_AREA(wp->drawing), 
			      wp->width ? wp->width : WAVEPANEL_MIN_WIDTH,
			      wp->req_height);

	return SCM_UNSPECIFIED;
}
#undef FUNC_NAME

/*****************************************************************************
 * Standard stuff for the WavePanel SMOB */

scm_sizet
free_WavePanel(SCM obj)
{
	WavePanel *wp = WavePanel(obj);
	wp->outstanding_smob = 0;

	if(wp->valid == 0) { /* if C has already invalidated, free it up */
		if(v_flag)
			fprintf(stderr, "free WavePanel 0x%x in gc\n", wp);
		g_free(wp);
		return sizeof(WavePanel);
	}
	else
		return 0;
}

SCM
mark_WavePanel(SCM obj)
{
	return SCM_BOOL_F;
}

int 
print_WavePanel(SCM obj, SCM port, scm_print_state *ARG_IGNORE(pstate))
{
	scm_puts("#<WavePanel ", port);
	scm_intprint((long)WavePanel(obj), 16, port);
  	if(!WavePanel(obj)->valid)
		scm_puts("invalid", port);
	scm_putc('>', port);
	return 1;
}

SCM_DEFINE(WavePanel_p, "WavePanel?", 1, 0, 0,
           (SCM obj),
	   "Returns #t if OBJ is a gwave data file object, otherwise #f.")
#define FUNC_NAME s_WavePanel_p
{
	return SCM_BOOL_FromBool(WavePanel_P(obj));
}
#undef FUNC_NAME

/*****************************************************************************
 * Standard stuff for the VisibleWave SMOB */

scm_sizet
free_VisibleWave(SCM obj)
{
	VisibleWave *wp = VisibleWave(obj);
	wp->outstanding_smob = 0;

	if(wp->valid == 0) { /* if C has already invalidated, free it up */
		if(v_flag)
			fprintf(stderr, "free VisibleWave 0x%x in gc\n", wp);
		g_free(wp);
		return sizeof(VisibleWave);
	}
	else
		return 0;
}

SCM
mark_VisibleWave(SCM obj)
{
	return SCM_BOOL_F;
}

int 
print_VisibleWave(SCM obj, SCM port, scm_print_state *ARG_IGNORE(pstate))
{
	scm_puts("#<VisibleWave ", port);
	scm_intprint((long)VisibleWave(obj), 16, port);
  	if(!VisibleWave(obj)->valid)
		scm_puts("invalid", port);
	scm_putc('>', port);
	return 1;
}

SCM_DEFINE(VisibleWave_p, "VisibleWave?", 1, 0, 0,
           (SCM obj),
	   "Returns #t if OBJ is a gwave data file object, otherwise #f.")
#define FUNC_NAME s_VisibleWave_p
{
	return SCM_BOOL_FromBool(VisibleWave_P(obj));
}
#undef FUNC_NAME

/*********************************************************************** 
 * guile initialization 
 */

MAKE_SMOBFUNS(WavePanel);
MAKE_SMOBFUNS(VisibleWave);

void init_wavepanel()
{
        REGISTER_SCWMSMOBFUNS(WavePanel);
        REGISTER_SCWMSMOBFUNS(VisibleWave);

#ifndef SCM_MAGIC_SNARFER
#include "wavepanel.x"
#endif
}
