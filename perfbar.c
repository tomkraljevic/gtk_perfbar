/*
 * Written by Doug and released to the public domain, as explained at
 * http://creativecommons.org/licenses/publicdomain
 *
 * A gtk look-alike of the original Solaris perfbar
 * 
 * Last update Wed Aug  3 18:51:15 2011  Doug Lea  (dl at altair)
*/

#include <unistd.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/sysinfo.h>

#ifdef CUDA
#include <assert.h>
#include "nvml.h"
#endif

extern void abort(void);

#ifdef SOLARIS
#include <kstat.h>
#endif

struct cpu_times {
  guint64 idle;
  guint64 user;
  guint64 sys;
  guint64 other;
};

typedef struct {
  gboolean ready;
  int ncpus;
  struct cpu_times* current;
  struct cpu_times* prev;
  struct cpu_times* diff;

  /* widgets */
  GtkWidget *app;
  GtkWidget *frame;
  GtkWidget *drawing_area;

  /* graphics */
  GdkPixmap *d_buffer;
  GdkGC *user_pen;
  GdkGC *other_pen;
  GdkGC *sys_pen;
  GdkGC *spacer_pen;
  GdkGC *idle_pen;
  GdkColor user_color;
  GdkColor other_color;
  GdkColor sys_color;
  GdkColor idle_color;
  GdkColor spacer_color;
  gint spacer_width;

} perfbar_panel; 

static perfbar_panel *create_panel(GtkWidget *applet, int n);
static void get_times(perfbar_panel* panel);
static void make_diffs(perfbar_panel* panel);
static void initialize_pens(perfbar_panel *panel);
static gint update_cb(gpointer data);
static gint resize_cb(GtkWidget *w, GdkEventConfigure *e, gpointer data);
static gint expose_cb(GtkWidget *w, GdkEventExpose* e, gpointer data);
static void redraw_app(perfbar_panel *panel);
static void delete_cb(GtkWidget *w, gpointer data);
static guint64 smooth(guint64 current, guint64 prev, guint64 diff);

/* Default dimensions -- scaled (a bit) by ncpus below */
#define DEFAULT_SPACER      1
#define DEFAULT_HEIGHT    128
#define DEFAULT_BAR_WIDTH   7

/* Interval (millisecs) for gtk time-based update */
#define UPDATE_INTERVAL 200

/* Hostname for title bar */
#define MAX_HOSTNAME_LENGTH 256
static char hostname[MAX_HOSTNAME_LENGTH];


int main(int argc, char **argv) {
  GtkWidget *window;
  perfbar_panel *panel;

#ifdef CUDA
  nvmlReturn_t rv;
  rv = nvmlInit();
  assert(rv == NVML_SUCCESS);
  unsigned int n;
  rv = nvmlDeviceGetCount(&n);
  assert(rv == NVML_SUCCESS);
#else
  int n = (int)(sysconf(_SC_NPROCESSORS_CONF));
#endif
    
  gtk_init(&argc, &argv);
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gethostname(hostname, MAX_HOSTNAME_LENGTH);
  gtk_window_set_title (GTK_WINDOW(window), hostname);

  panel = create_panel(window, n);
  if (!panel)
    g_error("Can't create widgets!\n");
  get_times(panel);

  gtk_container_add(GTK_CONTAINER(window), panel->frame);

  gtk_signal_connect(GTK_OBJECT(window), "destroy",
                     GTK_SIGNAL_FUNC(delete_cb),
                     panel);

  /* I don't know why this is done. Just an incantation */
  while (gtk_events_pending()) gtk_main_iteration()
     ;

  gtk_widget_show(window);
  panel->ready = TRUE;

  gtk_timeout_add(UPDATE_INTERVAL, update_cb, panel);
  update_cb(panel);
  gtk_main();
  return 0;
}

static guint64 smooth(guint64 current, guint64 prev, guint64 diff) {
  //  guint64 d = ((current - prev) + diff) / 2;
  guint64 d = ((current - prev) + 3 * diff) / 4;
  if (d < 0) d = 0;
  return d;
}

static void make_diffs(perfbar_panel* panel) {
  struct cpu_times* tmp;
  int i;
  for (i = 0; i < panel->ncpus; ++i) {
    panel->diff[i].idle  = smooth(panel->current[i].idle,
                                  panel->prev[i].idle,
                                  panel->diff[i].idle);
    panel->diff[i].user  = smooth(panel->current[i].user,
                                  panel->prev[i].user,
                                  panel->diff[i].user);
    panel->diff[i].sys   = smooth(panel->current[i].sys,
                                  panel->prev[i].sys,
                                  panel->diff[i].sys);
    panel->diff[i].other = smooth(panel->current[i].other,
                                  panel->prev[i].other,
                                  panel->diff[i].other);
  }
  
  /* Swap for next time */
  tmp = panel->current;
  panel->current = panel->prev;
  panel->prev = tmp;
}

static perfbar_panel *create_panel(GtkWidget *window, int n) {
  struct cpu_times* tmp;
  gint width;
  gint width_scale;

  perfbar_panel *panel = (perfbar_panel*) g_malloc(sizeof(perfbar_panel));
  panel->app = window;
  panel->ready = FALSE;
  panel->ncpus = n;

  panel->current = (struct cpu_times*)(calloc(n, sizeof (struct cpu_times)));
  panel->prev = (struct cpu_times*)(calloc(n, sizeof (struct cpu_times)));
  panel->diff = (struct cpu_times*)(calloc(n, sizeof (struct cpu_times)));

  get_times(panel);
  tmp = panel->current;
  panel->current = panel->prev;
  panel->prev = tmp;
  get_times(panel);

  panel->d_buffer = NULL;
  panel->user_pen = NULL;
  panel->other_pen = NULL;
  panel->sys_pen = NULL;
  panel->idle_pen = NULL;
  panel->spacer_pen = NULL;

  panel->user_color.red = 0;
  panel->user_color.green = 65535;
  panel->user_color.blue = 0;
  gdk_color_alloc(gdk_colormap_get_system(), &panel->user_color);

  panel->other_color.red = 58980;
  panel->other_color.green = 58980;
  panel->other_color.blue = 58980;
  gdk_color_alloc(gdk_colormap_get_system(), &panel->other_color);

  panel->sys_color.red = 65535;
  panel->sys_color.green = 0;
  panel->sys_color.blue = 0;
  gdk_color_alloc(gdk_colormap_get_system(), &panel->sys_color);

  panel->spacer_color.red = 65535;
  panel->spacer_color.green = 65535;
  panel->spacer_color.blue = 65535;
  gdk_color_alloc(gdk_colormap_get_system(), &panel->spacer_color);

  panel->idle_color.red = 0;
  panel->idle_color.green = 0;
  panel->idle_color.blue = 65535;
  gdk_color_alloc(gdk_colormap_get_system(), &panel->idle_color);

  width_scale = (n <= 2? 8 : n <= 4? 4 : n <= 8? 2 : 1);
  width = (2 * DEFAULT_SPACER + (DEFAULT_SPACER + DEFAULT_BAR_WIDTH) * n) *
    width_scale;
  panel->spacer_width = DEFAULT_SPACER * width_scale;

  /* create widgets */
  panel->frame = gtk_frame_new(NULL);
  panel->drawing_area = gtk_drawing_area_new();
  gtk_drawing_area_size(GTK_DRAWING_AREA(panel->drawing_area), 
                        width, DEFAULT_HEIGHT);

  gtk_container_add(GTK_CONTAINER(panel->frame), panel->drawing_area);
  gtk_widget_show_all(panel->frame);

  gtk_signal_connect(GTK_OBJECT(panel->drawing_area), "configure_event",
                     GTK_SIGNAL_FUNC(resize_cb), panel);
  gtk_signal_connect(GTK_OBJECT(panel->drawing_area), "expose_event", 
                     GTK_SIGNAL_FUNC(expose_cb), panel); 

  return panel;
}

static void initialize_pens(perfbar_panel *panel) {
  panel->user_pen = gdk_gc_new(panel->drawing_area->window);
  gdk_gc_set_foreground(panel->user_pen, &panel->user_color);

  panel->other_pen = gdk_gc_new(panel->drawing_area->window);
  gdk_gc_set_foreground(panel->other_pen, &panel->other_color);

  panel->sys_pen = gdk_gc_new(panel->drawing_area->window);
  gdk_gc_set_foreground(panel->sys_pen, &panel->sys_color);

  panel->idle_pen = gdk_gc_new(panel->drawing_area->window);
  gdk_gc_set_foreground(panel->idle_pen, &panel->idle_color);

  panel->spacer_pen = gdk_gc_new(panel->drawing_area->window);
  gdk_gc_set_foreground(panel->spacer_pen, &panel->spacer_color);
}

static void redraw_app(perfbar_panel *panel) {
  gint x, i;
  gint height, width, bar_width, spacer;
  gint h_total;
  int n = panel->ncpus;

  if (!panel->ready) return;
  if (!panel->user_pen) 
    initialize_pens(panel);

  width = panel->drawing_area->allocation.width;
  height = panel->drawing_area->allocation.height;
  spacer = panel->spacer_width;
  h_total = height - spacer * 2;
  bar_width = (width - n * spacer) / n;
  if (bar_width <= 0)
    bar_width = 1;

  /* clear top and bottom*/
  gdk_draw_rectangle(panel->d_buffer, panel->spacer_pen, TRUE, 0, 0, width, spacer);
  gdk_draw_rectangle(panel->d_buffer, panel->spacer_pen, TRUE, 0, height-spacer, width, spacer);

  get_times(panel);
  make_diffs(panel);

  x = 0;
  for (i = 0; i < n; i++) {
    guint64 d_idle = panel->diff[i].idle;
    guint64 d_user = panel->diff[i].user;
    guint64 d_sys = panel->diff[i].sys;
    guint64 d_other = panel->diff[i].other;
    guint64 d_total = d_user + d_other + d_sys + d_idle;
    if (d_total == 0) /* assume idle if all 0 */
      d_idle = d_total = 1;
    
    int spc = spacer;
    if (i == 0) {
      spc = (width - n * bar_width - (n - 1) * spacer) / 2;
      if (spc < 0) spc = 0;
    }
    gdk_draw_rectangle(panel->d_buffer, panel->spacer_pen, TRUE,
                       x, 0, spc, height);
    x += spc;

    gint y_user, y_other, y_sys, y_idle;
    gint h_user, h_other, h_sys, h_idle, h_sum;
    double scale = (double) h_total / d_total;
    
    h_user = scale * d_user;
    h_sum = h_user;
    
    h_other = scale * d_other;
    h_sum += h_other;
    if (h_sum >= h_total) { /* max out at 100% */
      h_other = h_total - h_user;
      h_sum = h_total;
    }
    
    h_sys = scale * d_sys;
    h_sum += h_sys;
    if (h_sum >= h_total) {
      h_sys = h_total - h_user - h_other;
      h_sum = h_total;
    }
    
    h_idle = scale * d_idle;
    h_sum += h_idle;
    if (h_sum >= h_total) {
      h_sys = h_total - h_user - h_other - h_sys;
      h_sum = h_total;
    }
    
    y_user = h_total - h_user + spacer;
    y_other = y_user - h_other;
    y_sys = y_other - h_sys;
    y_idle = spacer;
    
    if (h_user > 0) 
      gdk_draw_rectangle(panel->d_buffer, panel->user_pen, TRUE,
                         x, y_user, bar_width, h_user);
    if (h_other > 0) 
      gdk_draw_rectangle(panel->d_buffer, panel->other_pen, TRUE,
                         x, y_other, bar_width, h_other);
    if (h_sys > 0) 
      gdk_draw_rectangle(panel->d_buffer, panel->sys_pen, TRUE,
                         x, y_sys, bar_width, h_sys);
    if (h_idle > 0) 
      gdk_draw_rectangle(panel->d_buffer, panel->idle_pen, TRUE,
                         x, y_idle, bar_width, h_idle);
    x += bar_width;
  }

  /* clear rightmost side */
  if (width-x > 0) {
    gdk_draw_rectangle(panel->d_buffer, panel->spacer_pen, TRUE,
                       x, 0, width-x, height);
  }

  /* copy to screen */
  gdk_draw_pixmap(panel->drawing_area->window, 
                  panel->drawing_area->style->fg_gc[GTK_WIDGET_STATE(panel->drawing_area)],
                  panel->d_buffer, 0, 0, 0, 0, 
                  width,
                  height);
}


static void delete_cb(GtkWidget *w, gpointer data) {
  gtk_main_quit();
}

gint update_cb(gpointer data) {
  redraw_app((perfbar_panel*)data);
  return TRUE;
}

static gint resize_cb(GtkWidget *w, GdkEventConfigure *e, gpointer data)
{
  perfbar_panel *panel = (perfbar_panel*)data;
  if (panel->d_buffer)
    gdk_pixmap_unref(panel->d_buffer);
  panel->d_buffer = gdk_pixmap_new(w->window, w->allocation.width,
                                 w->allocation.height, -1);
  redraw_app(panel);
  return TRUE;
}

static gint expose_cb(GtkWidget *w, GdkEventExpose* e, gpointer data)
{
  perfbar_panel *panel = (perfbar_panel*)data;
  gdk_draw_pixmap(panel->drawing_area->window, 
                  panel->drawing_area->style->fg_gc[GTK_WIDGET_STATE(panel->drawing_area)],
                  panel->d_buffer, e->area.x, e->area.y, e->area.x, e->area.y,
                  e->area.width, e->area.height);
  return FALSE;
}

#ifdef CUDA
static void get_times(perfbar_panel* panel) {
  for (int i = 0; i < panel->ncpus; ++i) {
    nvmlDevice_t device;
    nvmlReturn_t rv;
    rv = nvmlDeviceGetHandleByIndex(i, &device);
    assert(rv == NVML_SUCCESS);
    nvmlUtilization_t utilization;
    rv = nvmlDeviceGetUtilizationRates(device, &utilization);
    assert(rv == NVML_SUCCESS);
    int gpu = utilization.gpu;
    printf("%d : %d\n", i, gpu);
  }
}
#endif

#ifdef LINUX
/* PBSIZE should be much more than enough to read lines from proc */
#define PBSIZE 65536
static char pstat_buf[PBSIZE];

static void get_times(perfbar_panel* panel) {
  int i;
  int len;
  char* p;
  int fd;

  fd = open("/proc/stat", O_RDONLY);
  if (fd < 0)
    abort();

  len = read(fd, pstat_buf, PBSIZE-1);
  pstat_buf[len] = 0;
  p = &(pstat_buf[0]);

  if (strncmp(p, "cpu ", 4) == 0) {
    /* skip first line, which is an aggregate of all cpus. */
    while (*p != '\n') p++;
    p++;
  }
  
  for (i = 0; i < panel->ncpus; ++i) {
    while (*p != ' ' && *p != 0) ++p;
    panel->current[i].user  = strtoull(p, &p, 0);
    panel->current[i].other = strtoull(p, &p, 0);
    panel->current[i].sys   = strtoull(p, &p, 0);
    panel->current[i].idle  = strtoull(p, &p, 0);
    while (*p != '\n' && *p != 0) {
      if (isdigit(*p)) { /* assume 2.6 kernel, with extra values */
        panel->current[i].other += strtoull(p, &p, 0);
        panel->current[i].sys   += strtoull(p, &p, 0); 
        panel->current[i].sys   += strtoull(p, &p, 0); 
      }
      else
        ++p;
    }
  }
  close(fd);
}  

#endif

#ifdef SOLARIS
static void get_times(perfbar_panel* panel) {
  kstat_ctl_t* kstat_control;
  cpu_stat_t stat;
  kstat_t *p;
  int i;

  kstat_control = kstat_open();
  if (kstat_control == NULL)
    abort();

  for (p = kstat_control->kc_chain; p != NULL; p = p->ks_next) {
    if ((strcmp(p->ks_module, "cpu_stat")) == 0 &&
        kstat_read(kstat_control, p, &stat) != -1) {
      i = p->ks_instance;
      panel->current[i].idle = stat.cpu_sysinfo.cpu[CPU_IDLE];
      panel->current[i].user = stat.cpu_sysinfo.cpu[CPU_USER];
      panel->current[i].sys = stat.cpu_sysinfo.cpu[CPU_KERNEL];
      panel->current[i].other = stat.cpu_sysinfo.cpu[CPU_WAIT];
    }
  }
  kstat_close(kstat_control);
}  

#endif
