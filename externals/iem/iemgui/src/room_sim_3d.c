/* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

iemgui written by Thomas Musil, Copyright (c) IEM KUG Graz Austria 2000 - 2006 */

#include "m_pd.h"
#include "iemlib.h"
#include "iemgui.h"
#include "g_canvas.h"
#include "g_all_guis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef MSW
#include <io.h>
#else
#include <unistd.h>
#endif

#define IEM_GUI_ROOMSIM_3D_MAX_NR_SRC 30

/* ---------- room_sim_3d my gui-canvas for a window ---------------- */

t_widgetbehavior room_sim_3d_widgetbehavior;
static t_class *room_sim_3d_class;

typedef struct _room_sim_3d
{
  t_iemgui  x_gui;
  t_float   x_rho_head;
  int       x_fontsize;
  int       x_nr_src;
  int       x_pix_src_x[IEM_GUI_ROOMSIM_3D_MAX_NR_SRC+1];
  int       x_pix_src_y[IEM_GUI_ROOMSIM_3D_MAX_NR_SRC+1];
  int       x_pix_src_z[IEM_GUI_ROOMSIM_3D_MAX_NR_SRC+1];
  int       x_col_src[IEM_GUI_ROOMSIM_3D_MAX_NR_SRC+1];
  int       x_pos_x;
  int       x_pos_y;
  int       x_pos_z;
  int       x_height_z;
  int       x_sel_index;
  int       x_pix_rad;
  t_float   x_cnvrt_roomlx2pixh;
  t_float   x_r_ambi;
  t_float   x_room_x;
  t_float   x_room_y;
  t_float   x_room_z;
  void      *x_out_para;
  void      *x_out_rho;
  t_symbol  *x_s_head_xyz;
  t_symbol  *x_s_src_xyz;
  t_atom    x_at[6];
  int       x_steady;
  int       x_bcol;
  int       x_fcol;
} t_room_sim_3d;

static void room_sim_3d_out_rho(t_room_sim_3d *x)
{
  outlet_float(x->x_out_rho, x->x_rho_head);
}

static void room_sim_3d_out_para(t_room_sim_3d *x)
{
  int i, n = x->x_nr_src;
  int w2=x->x_gui.x_w/2, h2=x->x_gui.x_h/2;
  
  SETFLOAT(x->x_at, 0.0f);
  SETSYMBOL(x->x_at+1, x->x_s_head_xyz);
  SETFLOAT(x->x_at+2, (t_float)(h2 - x->x_pix_src_y[0])/x->x_cnvrt_roomlx2pixh);
  SETFLOAT(x->x_at+3, (t_float)(w2 - x->x_pix_src_x[0])/x->x_cnvrt_roomlx2pixh);
  SETFLOAT(x->x_at+4, (t_float)(x->x_pix_src_z[0])/x->x_cnvrt_roomlx2pixh);
  outlet_list(x->x_out_para, &s_list, 5, x->x_at);
  for(i=1; i<=n; i++)
  {
    SETFLOAT(x->x_at, (t_float)i);
    SETSYMBOL(x->x_at+1, x->x_s_src_xyz);
    SETFLOAT(x->x_at+2, (t_float)(h2 - x->x_pix_src_y[i])/x->x_cnvrt_roomlx2pixh);
    SETFLOAT(x->x_at+3, (t_float)(w2 - x->x_pix_src_x[i])/x->x_cnvrt_roomlx2pixh);
    SETFLOAT(x->x_at+4, (t_float)(x->x_pix_src_z[i])/x->x_cnvrt_roomlx2pixh);
    outlet_list(x->x_out_para, &s_list, 5, x->x_at);
  }
}

static void room_sim_3d_draw_update(t_room_sim_3d *x, t_glist *glist)
{
  if(glist_isvisible(glist))
  {
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    int dx, dy;
    t_canvas *canvas=glist_getcanvas(glist);
    
    dx = -(int)((t_float)x->x_pix_rad*(t_float)sin(x->x_rho_head*0.0174533f) + 0.49999f);
    dy = -(int)((t_float)x->x_pix_rad*(t_float)cos(x->x_rho_head*0.0174533f) + 0.49999f);
//    sys_vgui(".x%x.c coords %xNOSE %d %d %d %d\n",
//      canvas, x, xpos+x->x_pix_src_x[0], ypos+x->x_pix_src_y[0],
//      xpos+x->x_pix_src_x[0]+dx, ypos+x->x_pix_src_y[0]+dy);

    gui_vmess("gui_room_sim_update", "xxiiiii",
      canvas,
      x,
      x->x_pix_src_x[0],
      x->x_pix_src_y[0],
      dx,
      dy,
      x->x_pix_rad
    );
  }
}

void room_sim_3d_draw_unmap(t_room_sim_3d *x, t_glist *glist)
{
    gui_vmess("gui_room_sim_erase", "xx",
        x->x_gui.x_glist, x);
}

void room_sim_3d_draw_map(t_room_sim_3d *x, t_glist *glist)
{
  int xpos = text_xpix(&x->x_gui.x_obj, glist);
  int ypos = text_ypix(&x->x_gui.x_obj, glist);
  int dx, dy;
  int H = (int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2 = H*H;
  int rad2;
  int i, n = x->x_nr_src;
  int fsi, fs = x->x_fontsize;
  t_canvas *canvas = glist_getcanvas(glist);
  char bcol[MAXPDSTRING];
  char fcol[MAXPDSTRING];
  char elemcol[MAXPDSTRING];
  sprintf(fcol, "#%6.6x", x->x_fcol);
  sprintf(bcol, "#%6.6x", x->x_bcol);
  
  gui_start_vmess("gui_room_sim_map", "xxiiifiiiss",
    canvas,
    x,
    x->x_gui.x_w,
    x->x_gui.x_h,
    x->x_pix_rad,
    x->x_rho_head,
    x->x_pix_src_x[0],
    x->x_pix_src_y[0],
    x->x_fontsize,
    fcol,
    bcol
  );
  gui_start_array();
  for (i = 1; i <= n; i++)
  {
    fsi = H2 + (H + 2 * x->x_pix_src_z[i]) * x->x_pix_src_z[i];
    fsi *= fs;
    fsi /= 2*H2;
    gui_start_array();
    gui_i(x->x_pix_src_x[i]);
    gui_i(x->x_pix_src_y[i]);
    sprintf(elemcol, "#%6.6x", x->x_col_src[i]);
    gui_s(elemcol);
    gui_i(fsi);
    gui_end_array();
//    sys_vgui(".x%x.c create text %d %d -text {%d} -anchor c \
//      -font {times %d bold} -fill #%6.6x -tags %xSRC%d\n",
//      canvas, xpos+x->x_pix_src_x[i], ypos+x->x_pix_src_y[i], i, fsi,
//      x->x_col_src[i], x, i);
  }
  gui_end_array();
  
//  sys_vgui(".x%x.c create oval %d %d %d %d -outline #%6.6x -tags %xHEAD\n",
//    canvas, xpos+x->x_pix_src_x[0]-x->x_pix_rad, ypos+x->x_pix_src_y[0]-x->x_pix_rad,
//    xpos+x->x_pix_src_x[0]+x->x_pix_rad-1, ypos+x->x_pix_src_y[0]+x->x_pix_rad-1,
//    x->x_fcol, x);

  rad2 = H2 + (H + 2 * x->x_pix_src_z[0]) * x->x_pix_src_z[0];
  rad2 *= x->x_pix_rad;
  rad2 /= 8*H2;
  gui_start_array();
  gui_i(x->x_pix_src_x[0] - rad2);
  gui_i(x->x_pix_src_y[0] - rad2);
  gui_i(x->x_pix_src_x[0] + rad2 - 1);
  gui_i(x->x_pix_src_y[0] + rad2 - 1);
  gui_end_array();
  gui_end_vmess();
//  sys_vgui(".x%x.c create oval %d %d %d %d -outline #%6.6x -tags %xHEAD2\n",
//    canvas, xpos+x->x_pix_src_x[0]-rad2, ypos+x->x_pix_src_y[0]-rad2,
//    xpos+x->x_pix_src_x[0]+rad2-1, ypos+x->x_pix_src_y[0]+rad2-1,
//    x->x_fcol, x);
//  dx = -(int)((t_float)x->x_pix_rad*(t_float)sin(x->x_rho_head*0.0174533f) + 0.49999f);
//  dy = -(int)((t_float)x->x_pix_rad*(t_float)cos(x->x_rho_head*0.0174533f) + 0.49999f);
//  sys_vgui(".x%x.c create line %d %d %d %d -width 3 -fill #%6.6x -tags %xNOSE\n",
//    canvas, xpos+x->x_pix_src_x[0], ypos+x->x_pix_src_y[0],
//    xpos+x->x_pix_src_x[0]+dx, ypos+x->x_pix_src_y[0]+dy,
//    x->x_fcol, x);
}

void room_sim_3d_draw_new(t_room_sim_3d *x, t_glist *glist)
{
  int xpos=text_xpix(&x->x_gui.x_obj, glist);
  int ypos=text_ypix(&x->x_gui.x_obj, glist);
  int dx, dy;
  int H=(int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2=H*H;
  int rad2;
  int i, n=x->x_nr_src;
  int fsi, fs=x->x_fontsize;
  t_canvas *canvas=glist_getcanvas(glist);
  
  gui_vmess("gui_room_sim_new", "xxxxiiiii",
    canvas,
    x->x_gui.x_glist,
    x->x_gui.x_glist->gl_owner,
    x,
    xpos,
    ypos,
    x->x_gui.x_w,
    x->x_gui.x_h,
    glist_istoplevel(glist)
  );

  room_sim_3d_draw_map(x, glist);
}

/*
void room_sim_3d_draw_move(t_room_sim_3d *x, t_glist *glist)
{
  int xpos=text_xpix(&x->x_gui.x_obj, glist);
  int ypos=text_ypix(&x->x_gui.x_obj, glist);
  int dx, dy;
  int H=(int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2=H*H;
  int rad2;
  int i, n=x->x_nr_src;
  int fsi, fs=x->x_fontsize;
  t_canvas *canvas=glist_getcanvas(glist);
  
  sys_vgui(".x%x.c coords %xBASE %d %d %d %d\n",
    canvas, x, xpos, ypos, xpos + x->x_gui.x_w, ypos + x->x_gui.x_h);
  n = x->x_nr_src;
  for(i=1; i<=n; i++)
  {
    fsi = H2 + (H + 2 * x->x_pix_src_z[i]) * x->x_pix_src_z[i];
    fsi *= fs;
    fsi /= 2*H2;
    sys_vgui(".x%x.c coords %xSRC%d %d %d\n",
      canvas, x, i, xpos+x->x_pix_src_x[i], ypos+x->x_pix_src_y[i]);
    sys_vgui(".x%x.c itemconfigure %xSRC%d -font {times %d bold}\n", canvas, x, i, fsi);
  }
  
  sys_vgui(".x%x.c coords %xHEAD %d %d %d %d\n",
    canvas, x, xpos+x->x_pix_src_x[0]-x->x_pix_rad, ypos+x->x_pix_src_y[0]-x->x_pix_rad,
    xpos+x->x_pix_src_x[0]+x->x_pix_rad-1, ypos+x->x_pix_src_y[0]+x->x_pix_rad-1);
  rad2 = H2 + (H + 2 * x->x_pix_src_z[0]) * x->x_pix_src_z[0];
  rad2 *= x->x_pix_rad;
  rad2 /= 8*H2;
  sys_vgui(".x%x.c coords %xHEAD2 %d %d %d %d\n",
    canvas, x, xpos+x->x_pix_src_x[0]-rad2, ypos+x->x_pix_src_y[0]-rad2,
    xpos+x->x_pix_src_x[0]+rad2-1, ypos+x->x_pix_src_y[0]+rad2-1);
  dx = -(int)((t_float)x->x_pix_rad*(t_float)sin(x->x_rho_head*0.0174533f) + 0.49999f);
  dy = -(int)((t_float)x->x_pix_rad*(t_float)cos(x->x_rho_head*0.0174533f) + 0.49999f);
  sys_vgui(".x%x.c coords %xNOSE %d %d %d %d\n",
    canvas, x, xpos+x->x_pix_src_x[0], ypos+x->x_pix_src_y[0],
    xpos+x->x_pix_src_x[0]+dx, ypos+x->x_pix_src_y[0]+dy);
}
*/

void room_sim_3d_draw(t_room_sim_3d *x, t_glist *glist, int mode)
{
  if (mode == IEM_GUI_DRAW_MODE_UPDATE)
    room_sim_3d_draw_update(x, glist);
  else if (mode == IEM_GUI_DRAW_MODE_MOVE)
    iemgui_base_draw_move(&x->x_gui);
  else if (mode == IEM_GUI_DRAW_MODE_NEW)
    room_sim_3d_draw_new(x, glist);
/*
  else if (mode == IEM_GUI_DRAW_MODE_SELECT)
    room_sim_3d_draw_select(x, glist);
  else if (mode == IEM_GUI_DRAW_MODE_ERASE)
    room_sim_3d_draw_erase(x, glist);
*/
}

/* ------------------------ cnv widgetbehaviour----------------------------- */

static void room_sim_3d_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
  t_room_sim_3d *x = (t_room_sim_3d *)z;
  
  *xp1 = text_xpix(&x->x_gui.x_obj, glist);
  *yp1 = text_ypix(&x->x_gui.x_obj, glist);
  *xp2 = *xp1 + x->x_gui.x_w;
  *yp2 = *yp1 + x->x_gui.x_h;
}

static void room_sim_3d_save(t_gobj *z, t_binbuf *b)
{
  t_room_sim_3d *x = (t_room_sim_3d *)z;
  int i, j, c, n=x->x_nr_src;
  
  binbuf_addv(b, "ssiis", gensym("#X"),gensym("obj"),
    (t_int)x->x_gui.x_obj.te_xpix, (t_int)x->x_gui.x_obj.te_ypix, 
    atom_getsymbol(binbuf_getvec(x->x_gui.x_obj.te_binbuf))
   );
  binbuf_addv(b, "ifffi", x->x_nr_src, x->x_cnvrt_roomlx2pixh, x->x_rho_head, x->x_r_ambi, x->x_fontsize);
  c = x->x_bcol;
  j = (((0xfc0000 & c) >> 6)|((0xfc00 & c) >> 4)|((0xfc & c) >> 2));
  binbuf_addv(b, "ifff", j, x->x_room_x, x->x_room_y, x->x_room_z);
  c = x->x_fcol;
  j = (((0xfc0000 & c) >> 6)|((0xfc00 & c) >> 4)|((0xfc & c) >> 2));
  binbuf_addv(b, "iiii", j, x->x_pix_src_x[0], x->x_pix_src_y[0], x->x_pix_src_z[0]);
  for(i=1; i<=n; i++)
  {
    c = x->x_col_src[i];
    j = (((0xfc0000 & c) >> 6)|((0xfc00 & c) >> 4)|((0xfc & c) >> 2));
    binbuf_addv(b, "iiii", j, x->x_pix_src_x[i], x->x_pix_src_y[i], x->x_pix_src_z[i]);
  }
  binbuf_addv(b, ";");
}

static void room_sim_3d_motion(t_room_sim_3d *x, t_floatarg dx, t_floatarg dy)
{
  int i, n = x->x_nr_src;
  int pixrad = x->x_pix_rad;
  int sel = x->x_sel_index;
  int xpos = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int ypos = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int ddx, ddy;
  int fs = x->x_fontsize, fsi;
  int H = (int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2 = H*H;
  int rad2;
  t_canvas *canvas = glist_getcanvas(x->x_gui.x_glist);
  
  if (x->x_gui.x_finemoved && (sel == 0))
  {
    if (x->x_steady)/*alt-key, rhoy, rhox*/
    {
    }
    else
    {
      x->x_rho_head -= dy;
      if (x->x_rho_head <= -180.0f)
        x->x_rho_head += 360.0f;
      if (x->x_rho_head > 180.0f)
        x->x_rho_head -= 360.0f;
      room_sim_3d_out_rho(x);
      (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    }
  }
  else if (sel == 0)
  {
    if (x->x_steady) /*alt-key, move head in z */
    {
      x->x_pix_src_x[0] = x->x_pos_x;
      x->x_pix_src_y[0] = x->x_pos_y;
      x->x_pix_src_z[0] -= (int)dy;
      if (x->x_pix_src_z[0] < 0)
        x->x_pix_src_z[0] = 0;
      if (x->x_pix_src_z[0] > x->x_height_z)
        x->x_pix_src_z[0] = x->x_height_z;
    }
    else
    {
      x->x_pix_src_z[0] = x->x_pos_z;
      x->x_pos_x += (int)dx;
      x->x_pos_y += (int)dy;
      x->x_pix_src_x[0] = x->x_pos_x;
      x->x_pix_src_y[0] = x->x_pos_y;
      if (x->x_pix_src_x[0] < 0)
        x->x_pix_src_x[0] = 0;
      if (x->x_pix_src_x[0] > x->x_gui.x_w)
        x->x_pix_src_x[0] = x->x_gui.x_w;
      if (x->x_pix_src_y[0] < 0)
        x->x_pix_src_y[0] = 0;
      if (x->x_pix_src_y[0] > x->x_gui.x_h)
        x->x_pix_src_y[0] = x->x_gui.x_h;
    }
    room_sim_3d_out_para(x);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
//    sys_vgui(".x%x.c coords %xHEAD %d %d %d %d\n",
//      canvas, x, xpos+x->x_pix_src_x[0]-pixrad, ypos+x->x_pix_src_y[0]-pixrad,
//      xpos+x->x_pix_src_x[0]+pixrad-1, ypos+x->x_pix_src_y[0]+pixrad-1);
    rad2 = H2 + (H + 2 * x->x_pix_src_z[0]) * x->x_pix_src_z[0];
    rad2 *= x->x_pix_rad;
    rad2 /= 8*H2;
//    sys_vgui(".x%x.c coords %xHEAD2 %d %d %d %d\n",
//      canvas, x, xpos+x->x_pix_src_x[0]-rad2, ypos+x->x_pix_src_y[0]-rad2,
//      xpos+x->x_pix_src_x[0]+rad2-1, ypos+x->x_pix_src_y[0]+rad2-1);
    gui_vmess("gui_room_sim_head2", "xxiiii",
      canvas,
      x,
      x->x_pix_src_x[0] - rad2,
      x->x_pix_src_y[0] - rad2,
      x->x_pix_src_x[0] + rad2 - 1,
      x->x_pix_src_y[0] + rad2 - 1
    );
//    ddx = -(int)((t_float)pixrad*(t_float)sin(x->x_rho_head*0.0174533f) + 0.49999f);
//    ddy = -(int)((t_float)pixrad*(t_float)cos(x->x_rho_head*0.0174533f) + 0.49999f);
//    sys_vgui(".x%x.c coords %xNOSE %d %d %d %d\n",
//      canvas, x, xpos+x->x_pix_src_x[0], ypos+x->x_pix_src_y[0],
//      xpos+x->x_pix_src_x[0]+ddx, ypos+x->x_pix_src_y[0]+ddy);
  }
  else
  {
    if (x->x_steady) /*alt-key, move src in z*/
    {
      x->x_pix_src_z[sel] -= (int)dy;
      if (x->x_pix_src_z[sel] < 0)
        x->x_pix_src_z[sel] = 0;
      if (x->x_pix_src_z[sel] > x->x_height_z)
        x->x_pix_src_z[sel] = x->x_height_z;
      
      room_sim_3d_out_para(x);
      fsi = H2 + (H + 2 * x->x_pix_src_z[sel]) * x->x_pix_src_z[sel];
      fsi *= fs;
      fsi /= 2*H2;
      char col[MAXPDSTRING];
      sprintf(col, "#%6.6x", x->x_col_src[sel]);
      gui_vmess("gui_room_sim_update_src", "xxiiiis",
        canvas,
        x,
        sel - 1,
        x->x_pix_src_x[sel],
        x->x_pix_src_y[sel],
        fsi,
        col
      );
//      sys_vgui(".x%x.c itemconfigure %xSRC%d -font {times %d bold}\n", canvas, x, sel, fsi);
    }
    else
    {
      x->x_pos_x += (int)dx;
      x->x_pos_y += (int)dy;
      x->x_pix_src_x[sel] = x->x_pos_x;
      x->x_pix_src_y[sel] = x->x_pos_y;
      x->x_pix_src_z[sel] = x->x_pos_z;
      if (x->x_pix_src_x[sel] < 0)
        x->x_pix_src_x[sel] = 0;
      if (x->x_pix_src_x[sel] > x->x_gui.x_w)
        x->x_pix_src_x[sel] = x->x_gui.x_w;
      if (x->x_pix_src_y[sel] < 0)
        x->x_pix_src_y[sel] = 0;
      if (x->x_pix_src_y[sel] > x->x_gui.x_h)
        x->x_pix_src_y[sel] = x->x_gui.x_h;
      
      room_sim_3d_out_para(x);
      fsi = H2 + (H + 2 * x->x_pix_src_z[sel]) * x->x_pix_src_z[sel];
      fsi *= fs;
      fsi /= 2*H2;
      char col[MAXPDSTRING];
      sprintf(col, "#%6.6x", x->x_col_src[sel]);
      gui_vmess("gui_room_sim_update_src", "xxiiiis",
        canvas,
        x,
        sel - 1,
        x->x_pix_src_x[sel],
        x->x_pix_src_y[sel],
        fsi,
        col
      );
      //sys_vgui(".x%x.c coords %xSRC%d %d %d\n",
      //  canvas, x, sel, xpos+x->x_pix_src_x[sel], ypos+x->x_pix_src_y[sel]);
      //sys_vgui(".x%x.c itemconfigure %xSRC%d -font {times %d bold}\n", canvas, x, sel, fsi);
    }
  }
}

static void room_sim_3d_click(t_room_sim_3d *x, t_floatarg xpos, t_floatarg ypos,
                              t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
  int w = (int)xpos - text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int h = (int)ypos - text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int i, n=x->x_nr_src;
  int pixrad=x->x_pix_rad;
  int fs=x->x_fontsize, fsi;
  int H=(int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2=H*H;
  int diff, maxdiff=10000, sel=-1;
  
  i = 0;/* head */
  if((w >= (x->x_pix_src_x[i]-pixrad)) && (w <= (x->x_pix_src_x[i]+pixrad)) && (h >= (x->x_pix_src_y[i]-pixrad)) && (h <= (x->x_pix_src_y[i]+pixrad)))
  {
    diff = w - x->x_pix_src_x[i];
    if(diff < 0)
      diff *= -1;
    if(diff < maxdiff)
    {
      maxdiff = diff;
      sel = i;
    }
    diff = h - x->x_pix_src_y[i];
    if(diff < 0)
      diff *= -1;
    if(diff < maxdiff)
    {
      maxdiff = diff;
      sel = i;
    }
  }
  
  for(i=1; i<=n; i++)
  {
    fsi = H2 + (H + 2 * x->x_pix_src_z[i]) * x->x_pix_src_z[i];
    fsi *= fs;
    fsi /= 2*H2;
    if(fsi < fs)
      fsi = fs;
    fsi *= 2;
    fsi /= 3;
    if((w >= (x->x_pix_src_x[i]-fsi)) && (w <= (x->x_pix_src_x[i]+fsi)) && 
      (h >= (x->x_pix_src_y[i]-fsi)) && (h <= (x->x_pix_src_y[i]+fsi)))
    {
      diff = w - x->x_pix_src_x[i];
      if(diff < 0)
        diff *= -1;
      if(diff < maxdiff)
      {
        maxdiff = diff;
        sel = i;
      }
      diff = h - x->x_pix_src_y[i];
      if(diff < 0)
        diff *= -1;
      if(diff < maxdiff)
      {
        maxdiff = diff;
        sel = i;
      }
    }
  }
  if(sel >= 0)
  {
    x->x_sel_index = sel;
    x->x_pos_x = x->x_pix_src_x[sel];
    x->x_pos_y = x->x_pix_src_y[sel];
    x->x_pos_z = x->x_pix_src_z[sel];
    glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g, (t_glistmotionfn)room_sim_3d_motion, 0, 0, xpos, ypos, 0);
  }
}

static int room_sim_3d_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
  t_room_sim_3d* x = (t_room_sim_3d *)z;
  
  if(doit)
  {
    room_sim_3d_click( x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt);
    if(alt)
      x->x_steady = 1;
    else
      x->x_steady = 0;
    
    if(shift)
    {
      x->x_gui.x_finemoved = 1;
      room_sim_3d_out_rho(x);
    }
    else
    {
      x->x_gui.x_finemoved = 0;
      room_sim_3d_out_para(x);
    }
  }
  return (1);
}

static void room_sim_3d_bang(t_room_sim_3d *x)
{
  room_sim_3d_out_para(x);
  room_sim_3d_out_rho(x);
}

static void room_sim_3d_src_font(t_room_sim_3d *x, t_floatarg ff)
{
  int fs=(int)(ff + 0.49999f), fsi;
  int i, n=x->x_nr_src;
  int H=(int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2=H*H;
  t_canvas *canvas=glist_getcanvas(x->x_gui.x_glist);
  
  if(fs < 8)
    fs = 8;
  if(fs > 250)
    fs = 250;
  x->x_fontsize = fs;
  
  for(i=1; i<=n; i++)
  {
    fsi = H2 + (H + 2 * x->x_pix_src_z[i]) * x->x_pix_src_z[i];
    fsi *= fs;
    fsi /= 2*H2;

    char col[MAXPDSTRING];
    sprintf(col, "#%6.6x", x->x_col_src[i]);
    gui_vmess("gui_room_sim_update_src", "xxiiiis",
      canvas,
      x,
      i - 1,
      x->x_pix_src_x[i],
      x->x_pix_src_y[i],
      fsi,
      col
    );
    //sys_vgui(".x%x.c itemconfigure %xSRC%d -font {times %d bold}\n", canvas, x, i, fsi);
  }
}

static void room_sim_3d_set_rho(t_room_sim_3d *x, t_floatarg rhoz)
{
  while(rhoz <= -180.0f)
    rhoz += 360.0f;
  while(rhoz > 180.0f)
    rhoz -= 360.0f;
  x->x_rho_head = rhoz;
  (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void room_sim_3d_rho(t_room_sim_3d *x, t_floatarg rhoz)
{
  room_sim_3d_set_rho(x, rhoz);
  room_sim_3d_out_rho(x);
}

static void room_sim_3d_set_src_xyz(t_room_sim_3d *x, t_symbol *s, int argc, t_atom *argv)
{
  t_float xsrc, ysrc, zsrc;
  t_float roomx2=0.5f*x->x_room_x, roomy2=0.5f*x->x_room_y, roomz=x->x_room_z;
  int i, n=x->x_nr_src;
  int pixrad=x->x_pix_rad;
  int xpos=text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int ypos=text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int w2=x->x_gui.x_w/2, h2=x->x_gui.x_h/2;
  int H=(int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2=H*H;
  int fsi, fs=x->x_fontsize;
  t_canvas *canvas=glist_getcanvas(x->x_gui.x_glist);
  
  if(argc < 4)
  {
    post("room_sim_3d ERROR: src_xyz-input needs 1 index + 3 float-dimensions: src_index, x [m], y [m], z [m]");
    return;
  }
  i = (int)atom_getint(argv++);
  if((i > 0)&&(i <= n))
  {
    ysrc = atom_getfloat(argv++);
    xsrc = atom_getfloat(argv++);
    zsrc = atom_getfloat(argv);
    
    if(xsrc < -roomy2)
      xsrc = -roomy2;
    if(xsrc > roomy2)
      xsrc = roomy2;
    if(ysrc < -roomx2)
      ysrc = -roomx2;
    if(ysrc > roomx2)
      ysrc = roomx2;
    if(zsrc < 0.0f)
      zsrc = 0.0f;
    if(zsrc > roomz)
      zsrc = roomz;
    
    x->x_pix_src_x[i] = w2 - (int)(x->x_cnvrt_roomlx2pixh * xsrc + 0.49999f);
    x->x_pix_src_y[i] = h2 - (int)(x->x_cnvrt_roomlx2pixh * ysrc + 0.49999f);
    x->x_pix_src_z[i] = (int)(x->x_cnvrt_roomlx2pixh * zsrc + 0.49999f);
    
    fsi = H2 + (H + 2 * x->x_pix_src_z[i]) * x->x_pix_src_z[i];
    fsi *= fs;
    fsi /= 2*H2;

    char col[MAXPDSTRING];
    sprintf(col, "#%6.6x", x->x_col_src[i]);
    gui_vmess("gui_room_sim_update_src", "xxiiiis",
      canvas,
      x,
      i - 1,
      x->x_pix_src_x[i],
      x->x_pix_src_y[i],
      fsi,
      col
    );
//    sys_vgui(".x%x.c coords %xSRC%d %d %d\n",
//      canvas, x, i, xpos+x->x_pix_src_x[i], ypos+x->x_pix_src_y[i]);
//    sys_vgui(".x%x.c itemconfigure %xSRC%d -font {times %d bold}\n", canvas, x, i, fsi);
  }
}

static void room_sim_3d_src_xyz(t_room_sim_3d *x, t_symbol *s, int argc, t_atom *argv)
{
  room_sim_3d_set_src_xyz(x, s, argc, argv);
  room_sim_3d_out_para(x);
}

static void room_sim_3d_set_head_xyz(t_room_sim_3d *x, t_symbol *s, int argc, t_atom *argv)
{
  t_float roomx2=0.5f*x->x_room_x, roomy2=0.5f*x->x_room_y, roomz=x->x_room_z;
  int pixrad=x->x_pix_rad;
  int xpos=text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int ypos=text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
  int ddx, ddy;
  int H=(int)(0.5f * x->x_room_z * x->x_cnvrt_roomlx2pixh + 0.49999f);
  int H2=H*H;
  int rad2;
  t_float xh, yh, zh;
  t_canvas *canvas=glist_getcanvas(x->x_gui.x_glist);
  
  if(argc < 3)
  {
    post("room_sim_3d ERROR: head_xyz-input needs 3 float-dimensions: x [m], y [m], z [m]");
    return;
  }
  yh = atom_getfloat(argv++);
  xh = atom_getfloat(argv++);
  zh = atom_getfloat(argv);
  if(xh < -roomy2)
    xh = -roomy2;
  if(xh > roomy2)
    xh = roomy2;
  if(yh < -roomx2)
    yh = -roomx2;
  if(yh > roomx2)
    yh = roomx2;
  if(zh < 0.0f)
    zh = 0.0f;
  if(zh > roomz)
    zh = roomz;
  x->x_pix_src_x[0] = x->x_gui.x_w/2 - (int)(x->x_cnvrt_roomlx2pixh * xh + 0.49999f);
  x->x_pix_src_y[0] = x->x_gui.x_h/2 - (int)(x->x_cnvrt_roomlx2pixh * yh + 0.49999f);
  x->x_pix_src_z[0] = (int)(x->x_cnvrt_roomlx2pixh * zh + 0.49999f);
  
  (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);

//  sys_vgui(".x%x.c coords %xHEAD %d %d %d %d\n",
//    canvas, x, xpos+x->x_pix_src_x[0]-pixrad, ypos+x->x_pix_src_y[0]-pixrad,
//    xpos+x->x_pix_src_x[0]+pixrad-1, ypos+x->x_pix_src_y[0]+pixrad-1);
  rad2 = H2 + (H + 2 * x->x_pix_src_z[0]) * x->x_pix_src_z[0];
  rad2 *= pixrad;
  rad2 /= 8*H2;
  gui_vmess("gui_room_sim_head2", "xxiiii",
    canvas,
    x,
    x->x_pix_src_x[0] - rad2,
    x->x_pix_src_y[0] - rad2,
    x->x_pix_src_x[0] + rad2 - 1,
    x->x_pix_src_y[0] + rad2 - 1
  );
//  sys_vgui(".x%x.c coords %xHEAD2 %d %d %d %d\n",
//    canvas, x, xpos+x->x_pix_src_x[0]-rad2, ypos+x->x_pix_src_y[0]-rad2,
//    xpos+x->x_pix_src_x[0]+rad2-1, ypos+x->x_pix_src_y[0]+rad2-1);
//  ddx = -(int)((t_float)pixrad*(t_float)sin(x->x_rho_head*0.0174533f) + 0.49999f);
//  ddy = -(int)((t_float)pixrad*(t_float)cos(x->x_rho_head*0.0174533f) + 0.49999f);
//  sys_vgui(".x%x.c coords %xNOSE %d %d %d %d\n",
//    canvas, x, xpos+x->x_pix_src_x[0], ypos+x->x_pix_src_y[0],
//    xpos+x->x_pix_src_x[0]+ddx, ypos+x->x_pix_src_y[0]+ddy);
}

static void room_sim_3d_head_xyz(t_room_sim_3d *x, t_symbol *s, int argc, t_atom *argv)
{
  room_sim_3d_set_head_xyz(x, s, argc, argv);
  room_sim_3d_out_para(x);
}

static void room_sim_3d_room_dim(t_room_sim_3d *x, t_symbol *s, int argc, t_atom *argv)
{
  int i, n=x->x_nr_src;
  
  if(argc < 3)
  {
    post("room_sim_3d ERROR: room_dim-input needs 3 float-dimensions: x-Length [m], y-Width [m], z-Height [m]");
    return;
  }
  x->x_room_x = atom_getfloat(argv++);
  x->x_room_y = atom_getfloat(argv++);
  x->x_room_z = atom_getfloat(argv);
  
  if(x->x_room_x < 1.0f)
    x->x_room_x = 1.0f;
  if(x->x_room_y < 1.0f)
    x->x_room_y = 1.0f;
  if(x->x_room_z < 1.0f)
    x->x_room_z = 1.0f;
  
  x->x_gui.x_h = (int)(x->x_cnvrt_roomlx2pixh * x->x_room_x + 0.49999f);
  x->x_gui.x_w = (int)(x->x_cnvrt_roomlx2pixh * x->x_room_y + 0.49999f);
  x->x_height_z = (int)(x->x_cnvrt_roomlx2pixh * x->x_room_z + 0.49999f);
  x->x_pix_rad = (int)(x->x_cnvrt_roomlx2pixh * x->x_r_ambi + 0.49999f);
  
  for (i = 0; i <= n; i++)
  {
    if (x->x_pix_src_x[i] > x->x_gui.x_w)
      x->x_pix_src_x[i] = x->x_gui.x_w;
    if (x->x_pix_src_y[i] > x->x_gui.x_h)
      x->x_pix_src_y[i] = x->x_gui.x_h;
    if (x->x_pix_src_z[i] > x->x_height_z)
      x->x_pix_src_z[i] = x->x_height_z;
  }
  
  room_sim_3d_out_para(x);
  (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
  canvas_fixlinesfor(glist_getcanvas(x->x_gui.x_glist), (t_text*)x);
}

static void room_sim_3d_room_col(t_room_sim_3d *x, t_floatarg fcol)
{
  int col=(int)fcol;
  char fgstring[MAXPDSTRING];
  char bgstring[MAXPDSTRING];
  int i;
  t_canvas *canvas=glist_getcanvas(x->x_gui.x_glist);
  
  if (col < 0)
  {
    i = -1 - col;
    x->x_bcol = ((i & 0x3f000) << 6)|((i & 0xfc0) << 4)|((i & 0x3f) << 2);
  }
  else
  {
    if (col > 29)
      col = 29;
    x->x_bcol = my_iemgui_color_hex[col];
  }
  sprintf(fgstring, "#%6.6x", x->x_fcol);
  sprintf(bgstring, "#%6.6x", x->x_bcol);
  gui_vmess("gui_room_sim_colors", "xxss",
    canvas,
    x,
    fgstring,
    bgstring
  );
//  sys_vgui(".x%x.c itemconfigure %xBASE -fill #%6.6x\n", canvas, x, x->x_bcol);
}

static void room_sim_3d_head_col(t_room_sim_3d *x, t_floatarg fcol)
{
  int col=(int)fcol;
  char fgstring[MAXPDSTRING];
  char bgstring[MAXPDSTRING];
  int i;
  t_canvas *canvas=glist_getcanvas(x->x_gui.x_glist);
  
  if(col < 0)
  {
    i = -1 - col;
    x->x_fcol = ((i & 0x3f000) << 6)|((i & 0xfc0) << 4)|((i & 0x3f) << 2);
  }
  else
  {
    if(col > 29)
      col = 29;
    x->x_fcol = my_iemgui_color_hex[col];
  }
  sprintf(fgstring, "#%6.6x", x->x_fcol);
  sprintf(bgstring, "#%6.6x", x->x_bcol);
  gui_vmess("gui_room_sim_colors", "xxss",
    canvas,
    x,
    fgstring,
    bgstring
  );
//  sys_vgui(".x%x.c itemconfigure %xHEAD -outline #%6.6x\n", canvas, x, x->x_fcol);
//  sys_vgui(".x%x.c itemconfigure %xHEAD2 -outline #%6.6x\n", canvas, x, x->x_fcol);
//  sys_vgui(".x%x.c itemconfigure %xNOSE -fill #%6.6x\n", canvas, x, x->x_fcol);
}

static void room_sim_3d_src_col(t_room_sim_3d *x, t_symbol *s, int argc, t_atom *argv)
{
  int col;
  char colstring[MAXPDSTRING];
  int i, j, n=x->x_nr_src;
  t_canvas *canvas=glist_getcanvas(x->x_gui.x_glist);
  
  if ((argc >= 2)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1))
  {
    j = (int)atom_getintarg(0, argc, argv);
    if ((j > 0)&&(j <= n))
    {
      col = (int)atom_getintarg(1, argc, argv);
      if (col < 0)
      {
        i = -1 - col;
        x->x_col_src[j] = ((i & 0x3f000) << 6)|((i & 0xfc0) << 4)|((i & 0x3f) << 2);
      }
      else
      {
        if (col > 29)
          col = 29;
        x->x_col_src[j] = my_iemgui_color_hex[col];
      }
      sprintf(colstring, "#%6.6x", x->x_col_src[j]);
      gui_vmess("gui_room_sim_update_src", "xxiiiis",
        canvas,
        x,
        j-1,
        x->x_pix_src_x[j],
        x->x_pix_src_y[j],
        x->x_fontsize,
        colstring
      );
//      sys_vgui(".x%x.c itemconfigure %xSRC%d -fill #%6.6x\n", canvas, x, j, x->x_col_src[j]);
    }
  }
}

static void room_sim_3d_pix_per_m_ratio(t_room_sim_3d *x, t_floatarg ratio)
{
  t_float rr;
  int i, n=x->x_nr_src;
  
  if(ratio < 1.0f)
    ratio = 1.0f;
  if(ratio > 200.0f)
    ratio = 200.0f;
  rr = ratio / x->x_cnvrt_roomlx2pixh;
  x->x_cnvrt_roomlx2pixh = ratio;
  x->x_gui.x_w = (int)(x->x_cnvrt_roomlx2pixh * x->x_room_y + 0.49999f);
  x->x_gui.x_h = (int)(x->x_cnvrt_roomlx2pixh * x->x_room_x + 0.49999f);
  x->x_height_z = (int)(x->x_cnvrt_roomlx2pixh * x->x_room_z + 0.49999f);
  x->x_pix_rad = (int)(x->x_cnvrt_roomlx2pixh * x->x_r_ambi + 0.49999f);
  for(i=0; i<=n; i++)
  {
    x->x_pix_src_x[i] = (int)((t_float)x->x_pix_src_x[i]*rr + 0.49999f);
    x->x_pix_src_y[i] = (int)((t_float)x->x_pix_src_y[i]*rr + 0.49999f);
    x->x_pix_src_z[i] = (int)((t_float)x->x_pix_src_z[i]*rr + 0.49999f);
  }
  (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
  canvas_fixlinesfor(glist_getcanvas(x->x_gui.x_glist), (t_text*)x);
}

static void room_sim_3d_r_ambi(t_room_sim_3d *x, t_floatarg r_ambi)
{
  if(r_ambi < 0.1f)
    r_ambi = 0.1f;
  x->x_r_ambi = r_ambi;
  x->x_pix_rad = (int)(x->x_cnvrt_roomlx2pixh*r_ambi + 0.49999f);
  room_sim_3d_out_para(x);
  (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
}

static void room_sim_3d_nr_src(t_room_sim_3d *x, t_floatarg fnr_src)
{
  int nr_src = (int)fnr_src;
  int old_nr_src, i, j;
  
  if(nr_src < 1)
    nr_src = 1;
  else if(nr_src > IEM_GUI_ROOMSIM_3D_MAX_NR_SRC)
    nr_src = IEM_GUI_ROOMSIM_3D_MAX_NR_SRC;
  
  if(nr_src != x->x_nr_src)
  {
    if (glist_isvisible(x->x_gui.x_glist))
      room_sim_3d_draw_unmap(x, x->x_gui.x_glist);
//      (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_ERASE);
    
    old_nr_src = x->x_nr_src;
    x->x_nr_src = nr_src;
    j = (old_nr_src + 1) % 7;
    for(i=old_nr_src+1; i<=nr_src; i++)
    {
      x->x_col_src[i] = simularca_color_hex[j];
      if(i & 1)
        x->x_pix_src_x[i] = 125 + (IEM_GUI_ROOMSIM_3D_MAX_NR_SRC - i)*4;
      else
        x->x_pix_src_x[i] = 125 - (IEM_GUI_ROOMSIM_3D_MAX_NR_SRC - i)*4;
      x->x_pix_src_y[i] = 100;
      x->x_pix_src_z[i] = 42;
      j++;
      j %= 7;
    }
    
    if (glist_isvisible(x->x_gui.x_glist))
      room_sim_3d_draw_map(x, x->x_gui.x_glist);
//      (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_NEW);
  }
}

/* we may no longer need h_dragon... */
static void room_sim_3d__clickhook(t_scalehandle *sh, int newstate)
{
    t_room_sim_3d *x = (t_room_sim_3d *)(sh->h_master);
    if (newstate)
    {
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
        if (!sh->h_scale) /* click on a label handle */
            scalehandle_click_label(sh);
    }
    /* We no longer need this "clickhook", as we can handle the dragging
       either in the GUI (for the label handle) or or in canvas_doclick */
    //iemgui__clickhook3(sh,newstate);
    sh->h_dragon = newstate;
}

static void room_sim_3d__motionhook(t_scalehandle *sh,
                    t_floatarg mouse_x, t_floatarg mouse_y)
{
    if (sh->h_scale)
    {
        t_room_sim_3d *x = (t_room_sim_3d *)(sh->h_master);
        int width = mouse_x - text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist),
            height = mouse_y - text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist),
            minx = IEM_GUI_MINSIZE,
            miny = IEM_GUI_MINSIZE;
        x->x_gui.x_w = maxi(width, minx);
        x->x_gui.x_h = maxi(height, miny);
//        slider_check_length(x, x->x_orient ? x->x_gui.x_h : x->x_gui.x_w);
        if (glist_isvisible(x->x_gui.x_glist))
        {
            room_sim_3d_draw_unmap(x, x->x_gui.x_glist);
            room_sim_3d_draw_map(x, x->x_gui.x_glist);
            scalehandle_unclick_scale(sh);
        }

        //int properties = gfxstub_haveproperties((void *)x);
        //if (properties)
        //{
            /* No properties for room_sim externals atm */
            //properties_set_field_int(properties,"width",new_w);
            //properties_set_field_int(properties,"height",new_h);
        //}
    }
    scalehandle_dragon_label(sh,mouse_x, mouse_y);
}

/* from the old header... */
#define IEM_GUI_COLNR_GREEN          16
#define IEM_GUI_COLNR_D_ORANGE       24

void *room_sim_3d_new(t_symbol *s, int argc, t_atom *argv)
{
  t_room_sim_3d *x = (t_room_sim_3d *)pd_new(room_sim_3d_class);
  int i, j, n=1, c;
  
  if((argc >= 1)&&IS_A_FLOAT(argv,0))
  {
    n = (int)atom_getintarg(0, argc, argv);
    if(n < 1)
      n = 1;
    if(n > IEM_GUI_ROOMSIM_3D_MAX_NR_SRC)
      n = IEM_GUI_ROOMSIM_3D_MAX_NR_SRC;
    x->x_nr_src = n;
  }
  if(argc == (4*n + 13))
  {
    x->x_cnvrt_roomlx2pixh = atom_getfloatarg(1, argc, argv);
    x->x_rho_head = atom_getfloatarg(2, argc, argv);
    x->x_r_ambi = atom_getfloatarg(3, argc, argv);
    x->x_fontsize = (int)atom_getintarg(4, argc, argv);
    c = (int)atom_getintarg(5, argc, argv);
    x->x_bcol = ((c & 0x3f000) << 6)|((c & 0xfc0) << 4)|((c & 0x3f) << 2);
    x->x_room_x = atom_getfloatarg(6, argc, argv);
    x->x_room_y = atom_getfloatarg(7, argc, argv);
    x->x_room_z = atom_getfloatarg(8, argc, argv);
    c = (int)atom_getintarg(9, argc, argv);
    x->x_fcol = ((c & 0x3f000) << 6)|((c & 0xfc0) << 4)|((c & 0x3f) << 2);
    x->x_pix_src_x[0] = (int)atom_getintarg(10, argc, argv);
    x->x_pix_src_y[0] = (int)atom_getintarg(11, argc, argv);
    x->x_pix_src_z[0] = (int)atom_getintarg(12, argc, argv);
    j = 13;
    for(i=1; i<=n; i++)
    {
      c = (int)atom_getintarg(j, argc, argv);
      j++;
      x->x_col_src[i] = ((c & 0x3f000) << 6)|((c & 0xfc0) << 4)|((c & 0x3f) << 2);
      x->x_pix_src_x[i] = (int)atom_getintarg(j, argc, argv);
      j++;
      x->x_pix_src_y[i] = (int)atom_getintarg(j, argc, argv);
      j++;
      x->x_pix_src_z[i] = (int)atom_getintarg(j, argc, argv);
      j++;
    }
  }
  else
  {
    x->x_cnvrt_roomlx2pixh = 25.0f;
    x->x_rho_head = 0.0f;
    x->x_r_ambi = 1.4f;
    x->x_fontsize = 12;
    x->x_bcol = my_iemgui_color_hex[IEM_GUI_COLNR_GREEN];
    x->x_room_x = 12.0f;
    x->x_room_y = 10.0f;
    x->x_room_z = 5.0f;
    x->x_fcol = my_iemgui_color_hex[IEM_GUI_COLNR_D_ORANGE];
    x->x_pix_src_x[0] = 125;
    x->x_pix_src_y[0] = 200;
    x->x_pix_src_z[0] = 42;
    j = 0;
    for (i = 1; i <= n; i++)
    {
      x->x_col_src[i] = simularca_color_hex[j];
      if(i & 1)
        x->x_pix_src_x[i] = 125 + (IEM_GUI_ROOMSIM_3D_MAX_NR_SRC - i)*4;
      else
        x->x_pix_src_x[i] = 125 - (IEM_GUI_ROOMSIM_3D_MAX_NR_SRC - i)*4;
      x->x_pix_src_y[i] = 100;
      x->x_pix_src_z[i] = 42;
      j++;
      j %= 7;
    }
  }
  
  x->x_gui.x_w = (int)(x->x_room_y*x->x_cnvrt_roomlx2pixh + 0.49999f);
  x->x_gui.x_h = (int)(x->x_room_x*x->x_cnvrt_roomlx2pixh + 0.49999f);
  x->x_height_z = (int)(x->x_room_z*x->x_cnvrt_roomlx2pixh + 0.49999f);
  x->x_pix_rad = (int)(x->x_r_ambi*x->x_cnvrt_roomlx2pixh + 0.49999f);
  
  x->x_gui.x_draw = (t_iemfunptr)room_sim_3d_draw;
  x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
  
  x->x_out_para = outlet_new(&x->x_gui.x_obj, &s_list);
  x->x_out_rho = outlet_new(&x->x_gui.x_obj, &s_float);
  
  x->x_s_head_xyz = gensym("head_xyz");
  x->x_s_src_xyz = gensym("src_xyz");

  x->x_gui.x_lab = s_empty;

  x->x_gui.x_obj.te_iemgui = 1;
  x->x_gui.x_handle = scalehandle_new((t_object *)x,
      x->x_gui.x_glist, 1, room_sim_3d__clickhook, room_sim_3d__motionhook);
  x->x_gui.x_lhandle = scalehandle_new((t_object *)x,
      x->x_gui.x_glist, 0, room_sim_3d__clickhook, room_sim_3d__motionhook);

  // dummy colors to ensure g_all_guis.c does not crash
  x->x_gui.x_bcol = gensym("#FCFCFCFF");
  x->x_gui.x_fcol = gensym("#000000FF");
  x->x_gui.x_lcol = gensym("#000000FF");

  return (x);
}

static void room_sim_3d_ff(t_room_sim_3d *x)
{
  gfxstub_deleteforkey(x);
}

void room_sim_3d_setup(void)
{
  room_sim_3d_class = class_new(gensym("room_sim_3d"), (t_newmethod)room_sim_3d_new,
    (t_method)room_sim_3d_ff, sizeof(t_room_sim_3d), 0, A_GIMME, 0);
//  class_addcreator((t_newmethod)room_sim_3d_new, gensym("room_sim_3d"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_click, gensym("click"),
    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_motion, gensym("motion"),
    A_FLOAT, A_FLOAT, 0);
  class_addbang(room_sim_3d_class, (t_method)room_sim_3d_bang);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_room_dim, gensym("room_dim"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_r_ambi, gensym("r_ambi"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_room_col, gensym("room_col"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_head_col, gensym("head_col"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_src_col, gensym("src_col"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_rho, gensym("rho"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_src_xyz, gensym("src_xyz"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_head_xyz, gensym("head_xyz"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_set_rho, gensym("set_rho"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_set_src_xyz, gensym("set_src_xyz"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_set_head_xyz, gensym("set_head_xyz"), A_GIMME, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_pix_per_m_ratio, gensym("pix_per_m_ratio"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_src_font, gensym("src_font"), A_DEFFLOAT, 0);
  class_addmethod(room_sim_3d_class, (t_method)room_sim_3d_nr_src, gensym("nr_src"), A_DEFFLOAT, 0);
  
  room_sim_3d_widgetbehavior.w_getrectfn = room_sim_3d_getrect;
  room_sim_3d_widgetbehavior.w_displacefnwtag = iemgui_displace_withtag;
  room_sim_3d_widgetbehavior.w_displacefn = iemgui_displace;
  room_sim_3d_widgetbehavior.w_selectfn = iemgui_select;
  room_sim_3d_widgetbehavior.w_activatefn = NULL;
  room_sim_3d_widgetbehavior.w_deletefn = iemgui_delete;
  room_sim_3d_widgetbehavior.w_visfn = iemgui_vis;
  room_sim_3d_widgetbehavior.w_clickfn = room_sim_3d_newclick;
  
#if defined(PD_MAJOR_VERSION) && (PD_MINOR_VERSION >= 37)
  class_setsavefn(room_sim_3d_class, room_sim_3d_save);
#else
  room_sim_3d_widgetbehavior.w_propertiesfn = NULL;
  room_sim_3d_widgetbehavior.w_savefn = room_sim_3d_save;
#endif
  
  class_setwidget(room_sim_3d_class, &room_sim_3d_widgetbehavior);
//  class_sethelpsymbol(room_sim_3d_class, gensym("iemhelp2/help-room_sim_3d"));
}
