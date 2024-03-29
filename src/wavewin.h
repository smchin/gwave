/*
 * wavewin.h - part of the gwave waveform viewer
 * Declarations related to main waveform window.
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
 * You should have received a copy of the GNU General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef WAVEWIN_H
#define WAVEWIN_H

#ifndef SCWM_GUILE_H__
#include <scwm_guile.h>
#endif

#undef EXTERN
#undef EXTERN_SET
#ifdef WAVEWIN_IMPLEMENTATION
#define EXTERN
#define EXTERN_SET(x,y) x = y
#else
#define EXTERN extern
#define EXTERN_SET(x,y) extern x
#endif

/*
 * WavePanel -- describes a single panel containing zero or more waveforms.
 */
struct _WavePanel {
	SCM smob;
	int outstanding_smob;	/* if guile has a pointer, defer freeing. */
	int valid;	/* 1 if valid, 0 if awaiting deletion */
	int selected;

	GList *vwlist;	/* list of VisibleWaves shown in this panel.
			   Like any GList, NULL if list empty */
	double min_yval; /* min/max data x/y values over whole vwlist */
	double max_yval;
	double min_xval;	
	double max_xval;

	/* starting and ending drawn x-value (independent var),
	* copied from corresponding wtable values when we scroll/zoom,
	* later we may allow individual panels to be "locked" 
	* from global scroll/zoom or otherwise controlled independently */
	double start_xval;	
	double end_xval;
	/* ditto for the y-value dimension; start_yval is the bottom. 
	 * invariant: if man_yzoom is false these are
	 * the same as min_yval and max_yval.
	 */
	double start_yval;
	double end_yval;
	int man_yzoom;

	GtkWidget *lmvbox;
	GtkWidget *lmtable;	/* label and measurement table */
	GtkWidget *lab_min, *lab_max;
	GtkWidget *lab_min_hbox, *lab_max_hbox;
	GtkWidget *lab_logscale;
	GtkWidget *drawing; /* DrawingArea for waveforms */
	GdkPixmap *pixmap;
	int req_height;	/* requested height */
	int width, height; /* actual size */
	int nextcolor;	/* color to use for next added waveform */
	int logy;   /* Y axis scaling: 0=linear 1=log base 10 */
};


/* Stuff to wrap WavePanel as a SMOB */

EXTERN long scm_tc16_scwm_WavePanel;

#define WavePanel_P(X) (SCM_NIMP(X) && SCM_SMOB_PREDICATE(scm_tc16_scwm_WavePanel, X))
#define WavePanel(X)  ((WavePanel *)SCM_SMOB_DATA(X))
#define SAFE_WavePanel(X)  (WavePanel_P((X))? WavePanel((X)) : NULL)

#define VALIDATE_ARG_WavePanel(pos,scm) \
  do { \
  if (!WavePanel_P(scm)) scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  } while (0)

#define VALIDATE_ARG_WavePanel_COPY(pos,scm,cvar) \
  do { \
  if (!WavePanel_P(scm)) scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  else cvar = WavePanel(scm); \
  } while (0)

#define VALIDATE_ARG_WavePanel_COPY_USE_NULL(pos,scm,cvar) \
  do { \
  if (UNSET_SCM(scm) || scm == SCM_BOOL_F) cvar = NULL; \
  else if (!WavePanel_P(scm)) scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  else cvar = WavePanel(scm); \
  } while (0)


/***********************************************************************
 * VisibleWave -- a waveform shown in a panel.
 */

struct _VisibleWave {
	SCM smob;
	int outstanding_smob;	/* if guile has a pointer, defer freeing. */
	int valid;	/* 1 if valid, 0 if awaiting deletion */
	WaveVar *var;
	GWDataFile *gdf;
	WavePanel *wp;
	char *varname;	/* the variable name from the file */
	int colorn;
	GdkGC *gc;
	GtkWidget *button;
	GtkWidget *label;
	MeasureBtn *mbtn[2];
};

/* VisibleWave as a SMOB */ 
EXTERN long scm_tc16_scwm_VisibleWave;

#define VisibleWave_P(X) (SCM_NIMP(X) && SCM_SMOB_PREDICATE(scm_tc16_scwm_VisibleWave, X))
#define VisibleWave(X)  ((VisibleWave *)SCM_SMOB_DATA(X))
#define SAFE_VisibleWave(X)  (VisibleWave_P((X))? VisibleWave((X)) : NULL)

#define VALIDATE_ARG_VisibleWave(pos,scm) \
  do { \
  if (!VisibleWave_P(scm)) scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  } while (0)

#define VALIDATE_ARG_VisibleWave_COPY(pos,scm,cvar) \
  do { \
  if (!VisibleWave_P(scm)) scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  else cvar = VisibleWave(scm); \
  } while (0)

#define VALIDATE_ARG_VisibleWave_COPY_USE_NULL(pos,scm,cvar) \
  do { \
  if (UNSET_SCM(scm) || scm == SCM_BOOL_F) cvar = NULL; \
  else if (!VisibleWave_P(scm)) scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  else cvar = VisibleWave(scm); \
  } while (0)

// this one always gets the WaveVarH*
#define VALIDATE_ARG_VisibleWaveOrWaveVar_COPY(pos,scm,cvar) \
  do { \
  if (VisibleWave_P(scm)) cvar = VisibleWave(scm)->var; \
  else if(WaveVarH_P(scm)) cvar = SAFE_WaveVar(scm); \
  else scm_wrong_type_arg(FUNC_NAME,pos,scm); \
  } while (0)

/***********************************************************************
 * state related to selecting ranges/regions in X, Y, and XY
 * pixel space of a wavepanel.
 */
typedef enum _SelRangeType SelRangeType;
enum _SelRangeType { SR_X=1, SR_Y=2, SR_XY=3 };

struct _SelRange {
	int drawn;
	SelRangeType type;
	WavePanel *wp;
	GdkGC *gc;
	GdkColor gdk_color;
	int y1, y2;
	int x1, x2;
	int x1_root, y1_root;
	SCM done_proc;
};

/* defined in wavewin.c */
extern WavePanel *new_wave_panel();
extern void create_wdata_submenuitem(GWDataFile *wdata, GtkWidget *submenu);
extern void setup_waveform_window();
extern void vw_get_label_string(char *buf, int buflen, VisibleWave *vw);
extern void vw_wp_create_button(VisibleWave *vw, WavePanel *wp);
extern void wavewin_insert_panel(WavePanel *wp, int minheight, int showlabels);
extern void wavewin_delete_panel(WavePanel *wp);
extern void cmd_popup_delete_panel(GtkWidget *w);
extern void cmd_popup_insert_panel(GtkWidget *w);
extern void cmd_append_panel(GtkWidget *w);
extern void wavepanel_draw_labels(WavePanel *wp);
extern WavePanel *first_selected_wavepanel();

extern SCM wavepanel_mouse_binding[];
extern void draw_wavepanel_labels(WavePanel *wp);
extern void setup_wavepanel_lmtable(WavePanel *wp, int showlabels);
extern void destroy_wave_panel(WavePanel *wp);
extern void setup_wave_panel(WavePanel *wp, int minheight, int showlabels);

#endif
