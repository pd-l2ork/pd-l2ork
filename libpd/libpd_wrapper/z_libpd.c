/*
 * Copyright (c) 2010 Peter Brinkmann (peter.brinkmann@gmail.com)
 * Copyright (c) 2012-2019 libpd team
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/libpd/libpd/wiki for documentation
 *
 */

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifndef LIBPD_NO_NUMERIC
# include <locale.h>
#endif
#include "z_libpd.h"
#include "x_libpdreceive.h"
#include "z_hooks.h"
#include "s_stuff.h"
#include "m_imp.h"
#include "g_canvas.h"
#include "g_all_guis.h"

#if PD_MINOR_VERSION < 46
# define HAVE_SCHED_TICK_ARG
#endif

#ifdef HAVE_SCHED_TICK_ARG
# define SCHED_TICK(x) sched_tick(x)
#else
# define SCHED_TICK(x) sched_tick()
#endif

// forward declares
void pd_init(void);
int sys_startgui(const char *libdir);
void sys_stopgui(void);
int sys_pollgui(void);

// (optional) setup functions for built-in "extra" externals
#ifdef LIBPD_EXTRA
  void bob_tilde_setup(void);
  void bonk_tilde_setup(void);
  void choice_setup(void);
  void fiddle_tilde_setup(void);
  void loop_tilde_setup(void);
  void lrshift_tilde_setup(void);
  void pique_setup(void);
  void sigmund_tilde_setup(void);
  void stdout_setup(void);
  void adaptive_setup(void);
  void arraysize_setup(void);
  void autotune_tilde_setup(void);
  void bassemu_tilde_setup(void);
  void aenv_tilde_setup(void);
  void susloop_tilde_setup(void);
  void svf_tilde_setup(void);
  void zhzxh_tilde_setup(void);
  void comport_setup(void);
  void abs_tilde_setup(void);
  void bdiag_tilde_setup(void);
  void bfft_tilde_setup(void);
  void bitsplit_tilde_setup(void);
  void blocknorm_tilde_setup(void);
  void bmatrix_tilde_setup(void);
  void bwin_tilde_setup(void);
  void cheby_tilde_setup(void);
  void clog_tilde_setup(void);
  void diag_tilde_setup(void);
  void dist_tilde_setup(void);
  void dwt_tilde_setup(void);
  void dynwav_tilde_setup(void);
  void ead_tilde_setup(void);
  void eadsr_tilde_setup(void);
  void ear_tilde_setup(void);
  void fdn_tilde_setup(void);
  void ffpoly_setup(void);
  void fwarp_setup(void);
  void junction_tilde_setup(void);
  void lattice_tilde_setup(void);
  void permut_tilde_setup(void);
  void qmult_tilde_setup(void);
  void qnorm_tilde_setup(void);
  void ramp_tilde_setup(void);
  void ratio_setup(void);
  void resofilt_tilde_setup(void);
  void sbosc_tilde_setup(void);
  void scrollgrid1D_tilde_setup(void);
  void statwav_tilde_setup(void);
  void tabreadmix_tilde_setup(void);
  void xfm_tilde_setup(void);
  void cxc_setup(void);
  void disis_munger_tilde_setup(void);
  void disis_netreceive_setup(void);
  void disis_netsend_setup(void);
  void disis_phasor_tilde_setup(void);
  void earplug_tilde_setup(void);
  void cup_setup(void);
  void cupd_setup(void);
  void doubledelta_setup(void);
  void framescore_tilde_setup(void);
  void framespect_tilde_setup(void);
  void hasc_tilde_setup(void);
  void hssc_tilde_setup(void);
  void list_sum_setup(void);
  void listmoses_setup(void);
  void lpc_tilde_setup(void);
  void lpreson_tilde_setup(void);
  void maskxor_setup(void);
  void peakit_tilde_setup(void);
  void polymap_setup(void);
  void polystat_setup(void);
  void sieve_setup(void);
  void simile_setup(void);
  void simile_tilde_setup(void);
  void steady_setup(void);
  void valve_setup(void);
  void voicing_detector_tilde_setup(void);
  void weightonset_setup(void);
  void zeroxpos_tilde_setup(void);
  void ext13_setup(void);
  void bthresher_tilde_setup(void);
  void burrow_tilde_setup(void);
  void cavoc_tilde_setup(void);
  void cavoc27_tilde_setup(void);
  void centerring_tilde_setup(void);
  void codepend_tilde_setup(void);
  void cross_tilde_setup(void);
  void dentist_tilde_setup(void);
  void disarrain_tilde_setup(void);
  void disarray_tilde_setup(void);
  void drown_tilde_setup(void);
  void enrich_tilde_setup(void);
  void ether_tilde_setup(void);
  void leaker_tilde_setup(void);
  void mindwarp_tilde_setup(void);
  void morphine_tilde_setup(void);
  void multyq_tilde_setup(void);
  void pileup_tilde_setup(void);
  void pvcompand_tilde_setup(void);
  void pvgrain_tilde_setup(void);
  void pvharm_tilde_setup(void);
  void pvoc_tilde_setup(void);
  void pvtuner_tilde_setup(void);
  void pvwarp_tilde_setup(void);
  void pvwarpb_tilde_setup(void);
  void reanimator_tilde_setup(void);
  void resent_tilde_setup(void);
  void residency_buffer_tilde_setup(void);
  void residency_tilde_setup(void);
  void schmear_tilde_setup(void);
  void scrape_tilde_setup(void);
  void shapee_tilde_setup(void);
  void swinger_tilde_setup(void);
  void taint_tilde_setup(void);
  void thresher_tilde_setup(void);
  void vacancy_tilde_setup(void);
  void xsyn_tilde_setup(void);
  void knob_setup(void);
  void freeverb_tilde_setup(void);
  void constant_setup(void);
  void getdir_setup(void);
  void inv_tilde_setup(void);
  void inv_setup(void);
  void qread_setup(void);
  void rl_setup(void);
  void serial_bird_setup(void);
  void serial_ms_setup(void);
  void serial_mt_setup(void);
  void serialize_setup(void);
  void shell_setup(void);
  void sinh_setup(void);
  void sl_setup(void);
  void stripdir_setup(void);
  void unserialize_setup(void);
  void unwonk_setup(void);
  void fofsynth_tilde_setup(void);
  void shuffle_setup(void);
  void tabwrite4_tilde_setup(void);
  void bandpass_setup(void);
  void equalizer_setup(void);
  void highpass_setup(void);
  void highshelf_setup(void);
  void hlshelf_setup(void);
  void lowpass_setup(void);
  void lowshelf_setup(void);
  void moog_tilde_setup(void);
  void notch_setup(void);
  void atan2_tilde_setup(void);
  void mixer_tilde_setup(void);
  void sfwrite_tilde_setup(void);
  void streamin_tilde_setup(void);
  void streamout_tilde_setup(void);
  void canvas_name_setup(void);
  void ce_path_setup(void);
  void classpath_setup(void);
  void colorpanel_setup(void);
  void cursor_setup(void);
  void folder_list_setup(void);
  void helppath_setup(void);
  void ifeel_setup(void);
  void passwd_setup(void);
  void screensize_setup(void);
  void setenv_setup(void);
  void split_path_setup(void);
  void sql_query_setup(void);
  void stat_setup(void);
  void sys_gui_setup(void);
  void uname_setup(void);
  void unsetenv_setup(void);
  void version_setup(void);
  void window_name_setup(void);
  void iem_adaptfilt_setup(void);
  void iem_ambi_setup(void);
  void iem_bin_ambi_setup(void);
  void iem_delay_setup(void);
  void iem_roomsim_setup(void);
  void iem_spec2_setup(void);
  void iem_tab_setup(void);
  void canvasargs_setup(void);
  void canvasconnections_setup(void);
  void canvasdelete_setup(void);
  void canvasdollarzero_setup(void);
  void canvaserror_setup(void);
  void canvasindex_setup(void);
  void canvasname_setup(void);
  void canvasobjectposition_setup(void);
  void canvasposition_setup(void);
  void canvasselect_setup(void);
  void classtest_setup(void);
  void findbrokenobjects_setup(void);
  void oreceive_setup(void);
  void propertybang_setup(void);
  void receivecanvas_setup(void);
  void savebangs_setup(void);
  void sendcanvas_setup(void);
  void try_setup(void);
  void IEM16_setup(void);
  void iemlib1_setup(void);
  void iemlib2_setup(void);
  void detox_setup(void);
  void memchr_setup(void);
  void strchr_setup(void);
  void strcut_setup(void);
  void strlen_setup(void);
  void strtok_setup(void);
  void __setup(void);
  void libdir_setup(void);
  void MarkEx_setup(void);
  void maxlib_setup(void);
  void mjLib_setup(void);
  void basedir_setup(void);
  void char2f_setup(void);
  void comma_setup(void);
  void dinlet_tilde_setup(void);
  void dispatch_setup(void);
  void dripchar_setup(void);
  void f2char_setup(void);
  void gamme_setup(void);
  void image_setup(void);
  void mknob_setup(void);
  void panvol_tilde_setup(void);
  void readsfv_tilde_setup(void);
  void s2f_setup(void);
  void sarray_setup(void);
  void sfread2_tilde_setup(void);
  void slist_setup(void);
  void ssaw_tilde_setup(void);
  void tabdump2_setup(void);
  void tabenv_setup(void);
  void tabreadl_setup(void);
  void tabsort_setup(void);
  void tabsort2_setup(void);
  void wac_setup(void);
  void getenv_setup(void);
  void ln_tilde_setup(void);
  void pan_tilde_setup(void);
  void pansig_tilde_setup(void);
  void pol2rec_tilde_setup(void);
  void polygate_tilde_setup(void);
  void rec2pol_tilde_setup(void);
  void shuffle_setup(void);
  void system_setup(void);
  void binfile_setup(void);
  void cd4000_setup(void);
  void cd4001_setup(void);
  void cd4002_setup(void);
  void cd4008_setup(void);
  void cd4011_setup(void);
  void cd4012_setup(void);
  void cd4013_setup(void);
  void cd4014_setup(void);
  void cd4015_setup(void);
  void cd4017_setup(void);
  void cd4023_setup(void);
  void cd4024_setup(void);
  void cd4025_setup(void);
  void cd4027_setup(void);
  void cd4070_setup(void);
  void cd4071_setup(void);
  void cd4072_setup(void);
  void cd4073_setup(void);
  void cd4075_setup(void);
  void cd4076_setup(void);
  void cd4081_setup(void);
  void cd4082_setup(void);
  void cd4094_setup(void);
  void cd4516_setup(void);
  void cd40193_setup(void);
  void flist2tab_setup(void);
  void life2x_setup(void);
  void midifile_setup(void);
  void httpreceive_setup(void);
  void httpreq_setup(void);
  void tcpclient_setup(void);
  void tcpreceive_setup(void);
  void tcpsend_setup(void);
  void tcpserver_setup(void);
  void udpreceive_setup(void);
  void udpreceive_tilde_setup(void);
  void udpsend_setup(void);
  void udpsend_tilde_setup(void);
  void udpsndrcv_setup(void);
  void op_tilde_setup(void);
  void packOSC_setup(void);
  void pipelist_setup(void);
  void routeOSC_setup(void);
  void unpackOSC_setup(void);
  void rc_tilde_setup(void);
  void rcosc_tilde_setup(void);
  void rojo_tilde_setup(void);
  void runningmean_setup(void);
  void b2f_setup(void);
  void f2b_setup(void);
  void slipdec_setup(void);
  void slipenc_setup(void);
  void sqosc_tilde_setup(void);
  void str_setup(void);
  void tab2flist_setup(void);
  void tabfind_setup(void);
  void packxbee_setup(void);
  void unpackxbee_setup(void);
  void helplink_setup(void);
  void pddplink_setup(void);
  void plugin_tilde_setup(void);
  void sigpack_setup(void);
  void SMLib_setup(void);
  void argument_setup(void);
  void arguments_setup(void);
  void breakpoints_setup(void);
  void breakpoints_tilde_setup(void);
  void common_tilde_setup(void);
  void crossfade_tilde_setup(void);
  void folderpanel_setup(void);
  void from_ascii_code_setup(void);
  void getdollarzero_setup(void);
  void imagebang_setup(void);
  void increment_setup(void);
  void iterate_setup(void);
  void list_accum_setup(void);
  void list_unfold_setup(void);
  void listUnfold_setup(void);
  void menubutton_setup(void);
  void onlyone_setup(void);
  void open_help_setup(void);
  void openHelp_setup(void);
  void param_setup(void);
  void path_setup(void);
  void phasorshot_tilde_setup(void);
  void pmenu_setup(void);
  void streamMinMax_setup(void);
  void to_ascii_code_setup(void);
  void define_loudspeakers_setup(void);
  void rvbap_setup(void);
  void vbap_setup(void);
  void bartlett_tilde_setup(void);
  void blackman_tilde_setup(void);
  void connes_tilde_setup(void);
  void cosine_tilde_setup(void);
  void gaussian_tilde_setup(void);
  void hamming_tilde_setup(void);
  void hanning_tilde_setup(void);
  void kaiser_tilde_setup(void);
  void lanczos_tilde_setup(void);
  void welch_tilde_setup(void);
  void zexy_setup(void);
#endif

static t_atom *s_argv = NULL;
static t_atom *s_curr = NULL;
static int s_argm = 0;
static int s_argc = 0;

static void *get_object(const char *s) {
  t_pd *x = gensym(s)->s_thing;
  return x;
}

static void call_external_setup(const char *prefix, void(*fun)(void)) {
  t_class *c = pd_objectmaker;
  int pre_nmethod = c->c_nmethod;
  (*fun)();
  int new_nmethod = c->c_nmethod;
  for (int i = pre_nmethod; i < new_nmethod; i++) {
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, "%s/%s", prefix,
             c->c_methods[i].me_name->s_name);
    class_addcreator((t_newmethod)c->c_methods[i].me_fun, gensym(buf),
                     c->c_methods[i].me_arg[0], c->c_methods[i].me_arg[1],
                     c->c_methods[i].me_arg[2], c->c_methods[i].me_arg[3],
                     c->c_methods[i].me_arg[4], c->c_methods[i].me_arg[5]);
  }
}

// this is called instead of sys_main() to start things
int libpd_init(void) {
  static int initialized = 0;
  if (initialized) return -1; // only allow init once (for now)
  initialized = 1;
#ifndef __EMSCRIPTEN__
  signal(SIGFPE, SIG_IGN);
#endif
  libpd_start_message(32); // allocate array for message assembly
  sys_externalschedlib = 0;
  sys_printtostderr = 0;
  sys_usestdpath = 0; // don't use pd_extrapath, only sys_searchpath
  sys_debuglevel = 0;
  sys_noloadbang = 0;
  sys_hipriority = 0;
  sys_nmidiin = 0;
  sys_nmidiout = 0;
#ifdef HAVE_SCHED_TICK_ARG
  sys_time = 0;
#endif
  pd_init();
  sys_soundin = NULL;
  sys_soundout = NULL;
  sys_schedblocksize = DEFDACBLKSIZE;
  libpdreceive_setup();
  sys_set_audio_api(API_DUMMY);
  sys_searchpath = NULL;
  sys_helppath = NULL;
  sys_libdir = gensym("");
  post("pd %d.%d.%d%s", PD_MAJOR_VERSION, PD_MINOR_VERSION,
    PD_BUGFIX_VERSION, PD_TEST_VERSION);
#ifdef LIBPD_EXTRA
  call_external_setup("bob~", bob_tilde_setup);
  call_external_setup("bonk~", bonk_tilde_setup);
  call_external_setup("choice", choice_setup);
  call_external_setup("fiddle~", fiddle_tilde_setup);
  call_external_setup("loop~", loop_tilde_setup);
  call_external_setup("lrshift~", lrshift_tilde_setup);
  call_external_setup("pique", pique_setup);
  call_external_setup("sigmund~", sigmund_tilde_setup);
  call_external_setup("stdout", stdout_setup);
  call_external_setup("adaptive", adaptive_setup);
  call_external_setup("arraysize", arraysize_setup);
  call_external_setup("autotune~", autotune_tilde_setup);
  call_external_setup("bassemu~", bassemu_tilde_setup);
  call_external_setup("bsaylor", aenv_tilde_setup);
  call_external_setup("bsaylor", susloop_tilde_setup);
  call_external_setup("bsaylor", svf_tilde_setup);
  call_external_setup("bsaylor", zhzxh_tilde_setup);
  call_external_setup("comport", comport_setup);
  call_external_setup("creb", abs_tilde_setup);
  call_external_setup("creb", bdiag_tilde_setup);
  call_external_setup("creb", bfft_tilde_setup);
  call_external_setup("creb", bitsplit_tilde_setup);
  call_external_setup("creb", blocknorm_tilde_setup);
  call_external_setup("creb", bmatrix_tilde_setup);
  call_external_setup("creb", bwin_tilde_setup);
  call_external_setup("creb", cheby_tilde_setup);
  call_external_setup("creb", clog_tilde_setup);
  call_external_setup("creb", diag_tilde_setup);
  call_external_setup("creb", dist_tilde_setup);
  call_external_setup("creb", dwt_tilde_setup);
  call_external_setup("creb", dynwav_tilde_setup);
  call_external_setup("creb", ead_tilde_setup);
  call_external_setup("creb", eadsr_tilde_setup);
  call_external_setup("creb", ear_tilde_setup);
  call_external_setup("creb", fdn_tilde_setup);
  call_external_setup("creb", ffpoly_setup);
  call_external_setup("creb", fwarp_setup);
  call_external_setup("creb", junction_tilde_setup);
  call_external_setup("creb", lattice_tilde_setup);
  call_external_setup("creb", permut_tilde_setup);
  call_external_setup("creb", qmult_tilde_setup);
  call_external_setup("creb", qnorm_tilde_setup);
  call_external_setup("creb", ramp_tilde_setup);
  call_external_setup("creb", ratio_setup);
  call_external_setup("creb", resofilt_tilde_setup);
  call_external_setup("creb", sbosc_tilde_setup);
  call_external_setup("creb", scrollgrid1D_tilde_setup);
  call_external_setup("creb", statwav_tilde_setup);
  call_external_setup("creb", tabreadmix_tilde_setup);
  call_external_setup("creb", xfm_tilde_setup);
  call_external_setup("cxc", cxc_setup);
  call_external_setup("disis", disis_munger_tilde_setup);
  call_external_setup("disis", disis_netreceive_setup);
  call_external_setup("disis", disis_netsend_setup);
  call_external_setup("disis", disis_phasor_tilde_setup);
  call_external_setup("earplug~", earplug_tilde_setup);
  call_external_setup("ekext", cup_setup);
  call_external_setup("ekext", cupd_setup);
  call_external_setup("ekext", doubledelta_setup);
  call_external_setup("ekext", framescore_tilde_setup);
  call_external_setup("ekext", framespect_tilde_setup);
  call_external_setup("ekext", hasc_tilde_setup);
  call_external_setup("ekext", hssc_tilde_setup);
  call_external_setup("ekext", list_sum_setup);
  call_external_setup("ekext", listmoses_setup);
  call_external_setup("ekext", lpc_tilde_setup);
  call_external_setup("ekext", lpreson_tilde_setup);
  call_external_setup("ekext", maskxor_setup);
  call_external_setup("ekext", peakit_tilde_setup);
  call_external_setup("ekext", polymap_setup);
  call_external_setup("ekext", polystat_setup);
  call_external_setup("ekext", sieve_setup);
  call_external_setup("ekext", simile_setup);
  call_external_setup("ekext", simile_tilde_setup);
  call_external_setup("ekext", steady_setup);
  call_external_setup("ekext", valve_setup);
  call_external_setup("ekext", voicing_detector_tilde_setup);
  call_external_setup("ekext", weightonset_setup);
  call_external_setup("ekext", zeroxpos_tilde_setup);
  call_external_setup("ext13", ext13_setup);
  call_external_setup("fftease", bthresher_tilde_setup);
  call_external_setup("fftease", burrow_tilde_setup);
  call_external_setup("fftease", cavoc_tilde_setup);
  call_external_setup("fftease", cavoc27_tilde_setup);
  call_external_setup("fftease", centerring_tilde_setup);
  call_external_setup("fftease", codepend_tilde_setup);
  call_external_setup("fftease", cross_tilde_setup);
  call_external_setup("fftease", dentist_tilde_setup);
  call_external_setup("fftease", disarrain_tilde_setup);
  call_external_setup("fftease", disarray_tilde_setup);
  call_external_setup("fftease", drown_tilde_setup);
  call_external_setup("fftease", enrich_tilde_setup);
  call_external_setup("fftease", ether_tilde_setup);
  call_external_setup("fftease", leaker_tilde_setup);
  call_external_setup("fftease", mindwarp_tilde_setup);
  call_external_setup("fftease", morphine_tilde_setup);
  call_external_setup("fftease", multyq_tilde_setup);
  call_external_setup("fftease", pileup_tilde_setup);
  call_external_setup("fftease", pvcompand_tilde_setup);
  call_external_setup("fftease", pvgrain_tilde_setup);
  call_external_setup("fftease", pvharm_tilde_setup);
  call_external_setup("fftease", pvoc_tilde_setup);
  call_external_setup("fftease", pvtuner_tilde_setup);
  call_external_setup("fftease", pvwarp_tilde_setup);
  call_external_setup("fftease", pvwarpb_tilde_setup);
  call_external_setup("fftease", reanimator_tilde_setup);
  call_external_setup("fftease", resent_tilde_setup);
  call_external_setup("fftease", residency_buffer_tilde_setup);
  call_external_setup("fftease", residency_tilde_setup);
  call_external_setup("fftease", schmear_tilde_setup);
  call_external_setup("fftease", scrape_tilde_setup);
  call_external_setup("fftease", shapee_tilde_setup);
  call_external_setup("fftease", swinger_tilde_setup);
  call_external_setup("fftease", taint_tilde_setup);
  call_external_setup("fftease", thresher_tilde_setup);
  call_external_setup("fftease", vacancy_tilde_setup);
  call_external_setup("fftease", xsyn_tilde_setup);
  call_external_setup("flatgui", knob_setup);
  call_external_setup("freeverb~", freeverb_tilde_setup);
  call_external_setup("ggee", constant_setup);
  call_external_setup("ggee", getdir_setup);
  call_external_setup("ggee", inv_tilde_setup);
  call_external_setup("ggee", inv_setup);
  call_external_setup("ggee", qread_setup);
  call_external_setup("ggee", rl_setup);
  call_external_setup("ggee", serial_bird_setup);
  call_external_setup("ggee", serial_ms_setup);
  call_external_setup("ggee", serial_mt_setup);
  call_external_setup("ggee", serialize_setup);
  call_external_setup("ggee", shell_setup);
  call_external_setup("ggee", sinh_setup);
  call_external_setup("ggee", sl_setup);
  call_external_setup("ggee", stripdir_setup);
  call_external_setup("ggee", unserialize_setup);
  call_external_setup("ggee", unwonk_setup);
  call_external_setup("ggee", fofsynth_tilde_setup);
  call_external_setup("ggee", shuffle_setup);
  call_external_setup("ggee", tabwrite4_tilde_setup);
  call_external_setup("ggee", bandpass_setup);
  call_external_setup("ggee", equalizer_setup);
  call_external_setup("ggee", highpass_setup);
  call_external_setup("ggee", highshelf_setup);
  call_external_setup("ggee", hlshelf_setup);
  call_external_setup("ggee", lowpass_setup);
  call_external_setup("ggee", lowshelf_setup);
  call_external_setup("ggee", moog_tilde_setup);
  call_external_setup("ggee", notch_setup);
  call_external_setup("ggee", atan2_tilde_setup);
  call_external_setup("ggee", mixer_tilde_setup);
  call_external_setup("ggee", sfwrite_tilde_setup);
  call_external_setup("ggee", streamin_tilde_setup);
  call_external_setup("ggee", streamout_tilde_setup);
  call_external_setup("hcs", canvas_name_setup);
  call_external_setup("hcs", ce_path_setup);
  call_external_setup("hcs", classpath_setup);
  call_external_setup("hcs", colorpanel_setup);
  call_external_setup("hcs", cursor_setup);
  call_external_setup("hcs", folder_list_setup);
  call_external_setup("hcs", helppath_setup);
  call_external_setup("hcs", ifeel_setup);
  call_external_setup("hcs", passwd_setup);
  call_external_setup("hcs", screensize_setup);
  call_external_setup("hcs", setenv_setup);
  call_external_setup("hcs", split_path_setup);
  call_external_setup("hcs", sql_query_setup);
  call_external_setup("hcs", stat_setup);
  call_external_setup("hcs", sys_gui_setup);
  call_external_setup("hcs", uname_setup);
  call_external_setup("hcs", unsetenv_setup);
  call_external_setup("hcs", version_setup);
  call_external_setup("hcs", window_name_setup);
  call_external_setup("iem", iem_adaptfilt_setup);
  call_external_setup("iem", iem_ambi_setup);
  call_external_setup("iem", iem_bin_ambi_setup);
  call_external_setup("iem", iem_delay_setup);
  call_external_setup("iem", iem_roomsim_setup);
  call_external_setup("iem", iem_spec2_setup);
  call_external_setup("iem", iem_tab_setup);
  call_external_setup("iem", canvasargs_setup);
  call_external_setup("iem", canvasconnections_setup);
  call_external_setup("iem", canvasdelete_setup);
  call_external_setup("iem", canvasdollarzero_setup);
  call_external_setup("iem", canvaserror_setup);
  call_external_setup("iem", canvasindex_setup);
  call_external_setup("iem", canvasname_setup);
  call_external_setup("iem", canvasobjectposition_setup);
  call_external_setup("iem", canvasposition_setup);
  call_external_setup("iem", canvasselect_setup);
  call_external_setup("iem", classtest_setup);
  call_external_setup("iem", findbrokenobjects_setup);
  call_external_setup("iem", oreceive_setup);
  call_external_setup("iem", propertybang_setup);
  call_external_setup("iem", receivecanvas_setup);
  call_external_setup("iem", savebangs_setup);
  call_external_setup("iem", sendcanvas_setup);
  call_external_setup("iem", try_setup);
  call_external_setup("iem16", IEM16_setup);
  call_external_setup("iemlib", iemlib1_setup);
  call_external_setup("iemlib", iemlib2_setup);
  call_external_setup("jasch_lib", detox_setup);
  call_external_setup("jasch_lib", memchr_setup);
  call_external_setup("jasch_lib", strchr_setup);
  call_external_setup("jasch_lib", strcut_setup);
  call_external_setup("jasch_lib", strlen_setup);
  call_external_setup("jasch_lib", strtok_setup);
  call_external_setup("jasch_lib", __setup);
  call_external_setup("libdir", libdir_setup);
  call_external_setup("markex", MarkEx_setup);
  call_external_setup("maxlib", maxlib_setup);
  call_external_setup("mjlib", mjLib_setup);
  call_external_setup("moonlib", basedir_setup);
  call_external_setup("moonlib", char2f_setup);
  call_external_setup("moonlib", comma_setup);
  call_external_setup("moonlib", dinlet_tilde_setup);
  call_external_setup("moonlib", dispatch_setup);
  call_external_setup("moonlib", dripchar_setup);
  call_external_setup("moonlib", f2char_setup);
  call_external_setup("moonlib", gamme_setup);
  call_external_setup("moonlib", image_setup);
  call_external_setup("moonlib", mknob_setup);
  call_external_setup("moonlib", panvol_tilde_setup);
  call_external_setup("moonlib", readsfv_tilde_setup);
  call_external_setup("moonlib", s2f_setup);
  call_external_setup("moonlib", sarray_setup);
  call_external_setup("moonlib", sfread2_tilde_setup);
  call_external_setup("moonlib", slist_setup);
  call_external_setup("moonlib", ssaw_tilde_setup);
  call_external_setup("moonlib", tabdump2_setup);
  call_external_setup("moonlib", tabenv_setup);
  call_external_setup("moonlib", tabreadl_setup);
  call_external_setup("moonlib", tabsort_setup);
  call_external_setup("moonlib", tabsort2_setup);
  call_external_setup("moonlib", wac_setup);
  call_external_setup("motex", getenv_setup);
  call_external_setup("motex", ln_tilde_setup);
  call_external_setup("motex", pan_tilde_setup);
  call_external_setup("motex", pansig_tilde_setup);
  call_external_setup("motex", pol2rec_tilde_setup);
  call_external_setup("motex", polygate_tilde_setup);
  call_external_setup("motex", rec2pol_tilde_setup);
  call_external_setup("motex", shuffle_setup);
  call_external_setup("motex", system_setup);
  call_external_setup("mrpeach", binfile_setup);
  call_external_setup("mrpeach", cd4000_setup);
  call_external_setup("mrpeach", cd4001_setup);
  call_external_setup("mrpeach", cd4002_setup);
  call_external_setup("mrpeach", cd4008_setup);
  call_external_setup("mrpeach", cd4011_setup);
  call_external_setup("mrpeach", cd4012_setup);
  call_external_setup("mrpeach", cd4013_setup);
  call_external_setup("mrpeach", cd4014_setup);
  call_external_setup("mrpeach", cd4015_setup);
  call_external_setup("mrpeach", cd4017_setup);
  call_external_setup("mrpeach", cd4023_setup);
  call_external_setup("mrpeach", cd4024_setup);
  call_external_setup("mrpeach", cd4025_setup);
  call_external_setup("mrpeach", cd4027_setup);
  call_external_setup("mrpeach", cd4070_setup);
  call_external_setup("mrpeach", cd4071_setup);
  call_external_setup("mrpeach", cd4072_setup);
  call_external_setup("mrpeach", cd4073_setup);
  call_external_setup("mrpeach", cd4075_setup);
  call_external_setup("mrpeach", cd4076_setup);
  call_external_setup("mrpeach", cd4081_setup);
  call_external_setup("mrpeach", cd4082_setup);
  call_external_setup("mrpeach", cd4094_setup);
  call_external_setup("mrpeach", cd4516_setup);
  call_external_setup("mrpeach", cd40193_setup);
  call_external_setup("mrpeach", flist2tab_setup);
  call_external_setup("mrpeach", life2x_setup);
  call_external_setup("mrpeach", midifile_setup);
  call_external_setup("mrpeach", httpreceive_setup);
  call_external_setup("mrpeach", httpreq_setup);
  call_external_setup("mrpeach", tcpclient_setup);
  call_external_setup("mrpeach", tcpreceive_setup);
  call_external_setup("mrpeach", tcpsend_setup);
  call_external_setup("mrpeach", tcpserver_setup);
  call_external_setup("mrpeach", udpreceive_setup);
  call_external_setup("mrpeach", udpreceive_tilde_setup);
  call_external_setup("mrpeach", udpsend_setup);
  call_external_setup("mrpeach", udpsend_tilde_setup);
  call_external_setup("mrpeach", udpsndrcv_setup);
  call_external_setup("mrpeach", op_tilde_setup);
  call_external_setup("mrpeach", packOSC_setup);
  call_external_setup("mrpeach", pipelist_setup);
  call_external_setup("mrpeach", routeOSC_setup);
  call_external_setup("mrpeach", unpackOSC_setup);
  call_external_setup("mrpeach", rc_tilde_setup);
  call_external_setup("mrpeach", rcosc_tilde_setup);
  call_external_setup("mrpeach", rojo_tilde_setup);
  call_external_setup("mrpeach", runningmean_setup);
  call_external_setup("mrpeach", b2f_setup);
  call_external_setup("mrpeach", f2b_setup);
  call_external_setup("mrpeach", slipdec_setup);
  call_external_setup("mrpeach", slipenc_setup);
  call_external_setup("mrpeach", sqosc_tilde_setup);
  call_external_setup("mrpeach", str_setup);
  call_external_setup("mrpeach", tab2flist_setup);
  call_external_setup("mrpeach", tabfind_setup);
  call_external_setup("mrpeach", packxbee_setup);
  call_external_setup("mrpeach", unpackxbee_setup);
  call_external_setup("pddp", helplink_setup);
  call_external_setup("pddp", pddplink_setup);
  call_external_setup("plugin~", plugin_tilde_setup);
  call_external_setup("sigpack", sigpack_setup);
  call_external_setup("smlib", SMLib_setup);
  call_external_setup("tof", argument_setup);
  call_external_setup("tof", arguments_setup);
  call_external_setup("tof", breakpoints_setup);
  call_external_setup("tof", breakpoints_tilde_setup);
  call_external_setup("tof", common_tilde_setup);
  call_external_setup("tof", crossfade_tilde_setup);
  call_external_setup("tof", folderpanel_setup);
  call_external_setup("tof", from_ascii_code_setup);
  call_external_setup("tof", getdollarzero_setup);
  call_external_setup("tof", imagebang_setup);
  call_external_setup("tof", increment_setup);
  call_external_setup("tof", iterate_setup);
  call_external_setup("tof", list_accum_setup);
  call_external_setup("tof", list_unfold_setup);
  call_external_setup("tof", listUnfold_setup);
  call_external_setup("tof", menubutton_setup);
  call_external_setup("tof", onlyone_setup);
  call_external_setup("tof", open_help_setup);
  call_external_setup("tof", openHelp_setup);
  call_external_setup("tof", param_setup);
  call_external_setup("tof", path_setup);
  call_external_setup("tof", phasorshot_tilde_setup);
  call_external_setup("tof", pmenu_setup);
  call_external_setup("tof", streamMinMax_setup);
  call_external_setup("tof", to_ascii_code_setup);
  call_external_setup("vbap", define_loudspeakers_setup);
  call_external_setup("vbap", rvbap_setup);
  call_external_setup("vbap", vbap_setup);
  call_external_setup("windowing", bartlett_tilde_setup);
  call_external_setup("windowing", blackman_tilde_setup);
  call_external_setup("windowing", connes_tilde_setup);
  call_external_setup("windowing", cosine_tilde_setup);
  call_external_setup("windowing", gaussian_tilde_setup);
  call_external_setup("windowing", hamming_tilde_setup);
  call_external_setup("windowing", hanning_tilde_setup);
  call_external_setup("windowing", kaiser_tilde_setup);
  call_external_setup("windowing", lanczos_tilde_setup);
  call_external_setup("windowing", welch_tilde_setup);
  call_external_setup("zexy", zexy_setup);
#endif
#ifndef LIBPD_NO_NUMERIC
  setlocale(LC_NUMERIC, "C");
#endif
  return 0;
}

void libpd_clear_search_path(void) {
  sys_lock();
  namelist_free(sys_searchpath);
  sys_searchpath = NULL;
  sys_unlock();
}

void libpd_add_to_search_path(const char *path) {
  sys_lock();
  sys_searchpath = namelist_append(sys_searchpath, path, 0);
  sys_unlock();
}

void libpd_clear_help_path(void) {
  sys_lock();
  namelist_free(sys_helppath);
  sys_helppath = NULL;
  sys_unlock();
}

void libpd_add_to_help_path(const char *path) {
  sys_lock();
  sys_helppath = namelist_append(sys_helppath, path, 0);
  sys_unlock();
}
  
void libpd_openfile(const char *name, const char *dir) {
  sys_lock();
  glob_evalfile(NULL, gensym(name), gensym(dir));
  sys_unlock();
}

void libpd_closefile(void *p) {
  sys_lock();
  pd_free((t_pd *)p);
  sys_unlock();
}

int libpd_blocksize(void) {
  return DEFDACBLKSIZE;
}

int libpd_init_audio(int inChannels, int outChannels, int sampleRate) {
  int indev[MAXAUDIOINDEV], inch[MAXAUDIOINDEV],
       outdev[MAXAUDIOOUTDEV], outch[MAXAUDIOOUTDEV];
  indev[0] = outdev[0] = DEFAULTAUDIODEV;
  inch[0] = inChannels;
  outch[0] = outChannels;
  sys_lock();
  sys_set_audio_settings(1, indev, 1, inch,
         1, outdev, 1, outch, sampleRate, -1, 1, DEFDACBLKSIZE);
  sched_set_using_audio(SCHED_AUDIO_CALLBACK);
  sys_reopen_audio();
  sys_unlock();
  return 0;
}

static const t_sample sample_to_short = SHRT_MAX,
                      short_to_sample = 1.0 / (t_sample) SHRT_MAX;

#define PROCESS(_x, _y) \
  int i, j, k; \
  t_sample *p0, *p1; \
  sys_lock(); \
  sys_pollgui(); \
  for (i = 0; i < ticks; i++) { \
    for (j = 0, p0 = sys_soundin; j < DEFDACBLKSIZE; j++, p0++) { \
      for (k = 0, p1 = p0; k < sys_inchannels; k++, p1 += DEFDACBLKSIZE) \
        { \
        *p1 = *inBuffer++ _x; \
      } \
    } \
    memset(sys_soundout, 0, \
        sys_outchannels*DEFDACBLKSIZE*sizeof(t_sample)); \
    SCHED_TICK(pd_this->pd_systime + sys_time_per_dsp_tick); \
    for (j = 0, p0 = sys_soundout; j < DEFDACBLKSIZE; j++, p0++) { \
      for (k = 0, p1 = p0; k < sys_outchannels; k++, p1 += DEFDACBLKSIZE) \
        { \
        *outBuffer++ = *p1 _y; \
      } \
    } \
  } \
  sys_unlock(); \
  return 0;

int libpd_process_short(const int ticks, const short *inBuffer, short *outBuffer) {
  PROCESS(* short_to_sample, * sample_to_short)
}

int libpd_process_float(const int ticks, const float *inBuffer, float *outBuffer) {
  PROCESS(,)
}

int libpd_process_double(const int ticks, const double *inBuffer, double *outBuffer) {
  PROCESS(,)
}

#define PROCESS_RAW(_x, _y) \
  size_t n_in = sys_inchannels * DEFDACBLKSIZE; \
  size_t n_out = sys_outchannels * DEFDACBLKSIZE; \
  t_sample *p; \
  size_t i; \
  sys_lock(); \
  sys_pollgui(); \
  for (p = sys_soundin, i = 0; i < n_in; i++) { \
    *p++ = *inBuffer++ _x; \
  } \
  memset(sys_soundout, 0, n_out * sizeof(t_sample)); \
  SCHED_TICK(pd_this->pd_systime + sys_time_per_dsp_tick); \
  for (p = sys_soundout, i = 0; i < n_out; i++) { \
    *outBuffer++ = *p++ _y; \
  } \
  sys_unlock(); \
  return 0; 

int libpd_process_raw(const float *inBuffer, float *outBuffer) {
  PROCESS_RAW(,)
}

int libpd_process_raw_short(const short *inBuffer, short *outBuffer) {
  PROCESS_RAW(* short_to_sample, * sample_to_short)
}

int libpd_process_raw_double(const double *inBuffer, double *outBuffer) {
  PROCESS_RAW(,)
}
 
#define GETARRAY \
  t_garray *garray = (t_garray *) pd_findbyclass(gensym(name), garray_class); \
  if (!garray) {sys_unlock(); return -1;} \

int libpd_arraysize(const char *name) {
  int retval;
  sys_lock();
  GETARRAY
  retval = garray_npoints(garray);
  sys_unlock();
  return retval;
}

int libpd_resize_array(const char *name, long size) {
  sys_lock();
  GETARRAY
  garray_resize_long(garray, size);
  sys_unlock();
  return 0;
}

#define MEMCPY(_x, _y) \
  GETARRAY \
  if (n < 0 || offset < 0 || offset + n > garray_npoints(garray)) return -2; \
  t_word *vec = ((t_word *) garray_vec(garray)) + offset; \
  int i; \
  for (i = 0; i < n; i++) _x = _y;

int libpd_read_array(float *dest, const char *name, int offset, int n) {
  sys_lock();
  MEMCPY(*dest++, (vec++)->w_float)
  sys_unlock();
  return 0;
}

int libpd_write_array(const char *name, int offset, const float *src, int n) {
  sys_lock();
  MEMCPY((vec++)->w_float, *src++)
  sys_unlock();
  return 0;
}

int libpd_bang(const char *recv) {
  void *obj;
  sys_lock();
  obj = get_object(recv);
  if (obj == NULL)
  {
    sys_unlock();
    return -1;
  }
  pd_bang(obj);
  sys_unlock();
  return 0;
}

int libpd_float(const char *recv, float x) {
  void *obj;
  sys_lock();
  obj = get_object(recv);
  if (obj == NULL)
  {
    sys_unlock();
    return -1;
  }
  pd_float(obj, x);
  sys_unlock();
  return 0;
}

int libpd_symbol(const char *recv, const char *symbol) {
  void *obj;
  sys_lock();
  obj = get_object(recv);
  if (obj == NULL)
  {
    sys_unlock();
    return -1;
  }
  pd_symbol(obj, gensym(symbol));
  sys_unlock();
  return 0;
}

int libpd_start_message(int maxlen) {
  if (maxlen > s_argm) {
    t_atom *v = realloc(s_argv, maxlen * sizeof(t_atom));
    if (v) {
      s_argv = v;
      s_argm = maxlen;
    } else {
      return -1;
    }
  }
  s_argc = 0;
  s_curr = s_argv;
  return 0;
}

#define ADD_ARG(f) f(s_curr, x); s_curr++; s_argc++;

void libpd_add_float(float x) {
  ADD_ARG(SETFLOAT);
}

void libpd_add_symbol(const char *symbol) {
  t_symbol *x;
  sys_lock();
  x = gensym(symbol);
  sys_unlock();
  ADD_ARG(SETSYMBOL);
}

int libpd_finish_list(const char *recv) {
  return libpd_list(recv, s_argc, s_argv);
}

int libpd_finish_message(const char *recv, const char *msg) {
  return libpd_message(recv, msg, s_argc, s_argv);
}

void libpd_set_float(t_atom *a, float x) {
  SETFLOAT(a, x);
}

void libpd_set_symbol(t_atom *a, const char *symbol) {
  SETSYMBOL(a, gensym(symbol));
}

int libpd_list(const char *recv, int argc, t_atom *argv) {
  t_pd *obj;
  sys_lock();
  obj = get_object(recv);
  if (obj == NULL)
  {
    sys_unlock();
    return -1;
  }
  pd_list(obj, &s_list, argc, argv);
  sys_unlock();
  return 0;
}

int libpd_message(const char *recv, const char *msg, int argc, t_atom *argv) {
  t_pd *obj;
  sys_lock();
  obj = get_object(recv);
  if (obj == NULL)
  {
    sys_unlock();
    return -1;
  }
  pd_typedmess(obj, gensym(msg), argc, argv);
  sys_unlock();
  return 0;
}

void *libpd_bind(const char *recv) {
  t_symbol *x;
  sys_lock();
  x = gensym(recv);
  sys_unlock();
  return libpdreceive_new(x);
}

void libpd_unbind(void *p) {
  sys_lock();
  pd_free((t_pd *)p);
  sys_unlock();
}

int libpd_exists(const char *recv) {
  int retval;
  sys_lock();
  retval = (get_object(recv) != NULL);
  sys_unlock();
  return retval;
}

void libpd_set_printhook(const t_libpd_printhook hook) {
  sys_printhook = (t_printhook) hook;
}

void libpd_set_banghook(const t_libpd_banghook hook) {
  libpd_banghook = hook;
}

void libpd_set_floathook(const t_libpd_floathook hook) {
  libpd_floathook = hook;
}

void libpd_set_symbolhook(const t_libpd_symbolhook hook) {
  libpd_symbolhook = hook;
}

void libpd_set_listhook(const t_libpd_listhook hook) {
  libpd_listhook = hook;
}

void libpd_set_messagehook(const t_libpd_messagehook hook) {
  libpd_messagehook = hook;
}

int libpd_is_float(t_atom *a) {
  return (a)->a_type == A_FLOAT;
}

int libpd_is_symbol(t_atom *a) {
  return (a)->a_type == A_SYMBOL;
}

float libpd_get_float(t_atom *a) {
  return (a)->a_w.w_float;
}

const char *libpd_get_symbol(t_atom *a) {
  return (a)->a_w.w_symbol->s_name;
}

t_atom *libpd_next_atom(t_atom *a) {
  return a + 1;
}

#define CHECK_CHANNEL if (channel < 0) return -1;
#define CHECK_PORT if (port < 0 || port > 0x0fff) return -1;
#define CHECK_RANGE_7BIT(v) if (v < 0 || v > 0x7f) return -1;
#define CHECK_RANGE_8BIT(v) if (v < 0 || v > 0xff) return -1;
#define PORT (channel >> 4)
#define CHANNEL (channel & 0x0f)

int libpd_noteon(int channel, int pitch, int velocity) {
  CHECK_CHANNEL
  CHECK_RANGE_7BIT(pitch)
  CHECK_RANGE_7BIT(velocity)
  sys_lock();
  inmidi_noteon(PORT, CHANNEL, pitch, velocity);
  sys_unlock();
  return 0;
}

int libpd_controlchange(int channel, int controller, int value) {
  CHECK_CHANNEL
  CHECK_RANGE_7BIT(controller)
  CHECK_RANGE_7BIT(value)
  sys_lock();
  inmidi_controlchange(PORT, CHANNEL, controller, value);
  sys_unlock();
  return 0;
}

int libpd_programchange(int channel, int value) {
  CHECK_CHANNEL
  CHECK_RANGE_7BIT(value)
  sys_lock();
  inmidi_programchange(PORT, CHANNEL, value);
  sys_unlock();
  return 0;
}

// note: for consistency with Pd, we center the output of [pitchin] at 8192
int libpd_pitchbend(int channel, int value) {
  CHECK_CHANNEL
  if (value < -8192 || value > 8191) return -1;
  sys_lock();
  inmidi_pitchbend(PORT, CHANNEL, value + 8192);
  sys_unlock();
  return 0;
}

int libpd_aftertouch(int channel, int value) {
  CHECK_CHANNEL
  CHECK_RANGE_7BIT(value)
  sys_lock();
  inmidi_aftertouch(PORT, CHANNEL, value);
  sys_unlock();
  return 0;
}

int libpd_polyaftertouch(int channel, int pitch, int value) {
  CHECK_CHANNEL
  CHECK_RANGE_7BIT(pitch)
  CHECK_RANGE_7BIT(value)
  sys_lock();
  inmidi_polyaftertouch(PORT, CHANNEL, pitch, value);
  sys_unlock();
  return 0;
}

void libpd_set_noteonhook(const t_libpd_noteonhook hook) {
  libpd_noteonhook = hook;
}

void libpd_set_controlchangehook(const t_libpd_controlchangehook hook) {
  libpd_controlchangehook = hook;
}

void libpd_set_programchangehook(const t_libpd_programchangehook hook) {
  libpd_programchangehook = hook;
}

void libpd_set_pitchbendhook(const t_libpd_pitchbendhook hook) {
  libpd_pitchbendhook = hook;
}

void libpd_set_aftertouchhook(const t_libpd_aftertouchhook hook) {
  libpd_aftertouchhook = hook;
}

void libpd_set_polyaftertouchhook(const t_libpd_polyaftertouchhook hook) {
  libpd_polyaftertouchhook = hook;
}

void libpd_set_midibytehook(const t_libpd_midibytehook hook) {
  libpd_midibytehook = hook;
}

int libpd_start_gui(char *path) {
  int retval;
  sys_lock();
  retval = sys_startgui(path);
  sys_unlock();
  return retval;
}

void libpd_stop_gui(void) {
  sys_lock();
  sys_stopgui();
  sys_unlock();
}

void libpd_poll_gui(void) {
  sys_lock();
  sys_pollgui();
  sys_unlock();
}

void libpd_set_verbose(int verbose) {
  if (verbose < 0) verbose = 0;
  sys_verbose = verbose;
}

int libpd_get_verbose(void) {
  return sys_verbose;
}

// dummy routines needed because we don't use s_file.c
void glob_savepreferences(t_pd *dummy) {}
void glob_recent_files(t_pd *dummy) {}
void glob_add_recent_file(t_pd *dummy, t_symbol *s) {}
void glob_clear_recent_files(t_pd *dummy) {}
int sys_defeatrt, sys_autopatch_yoffset, sys_zoom, sys_browser_doc = 1,
    sys_browser_path, sys_browser_init;
t_symbol *sys_flags = &s_;

// dummy routines needed because we don't use s_midi.c
void sys_get_midi_apis2(t_binbuf *buf) {}
void sys_get_midi_devs(char *indevlist, int *nindevs, char *outdevlist, int *noutdevs, int maxndev, int devdescsize) {}
