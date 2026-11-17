/* Cairo-drawn app icons, category glyphs and mock screenshots. */

#include "store-art.h"
#include <math.h>

static const GdkRGBA WHITE = { 1, 1, 1, 1 };

static void
rrect (cairo_t *cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r,     r, -G_PI / 2, 0);
  cairo_arc (cr, x + w - r, y + h - r, r, 0, G_PI / 2);
  cairo_arc (cr, x + r,     y + h - r, r, G_PI / 2, G_PI);
  cairo_arc (cr, x + r,     y + r,     r, G_PI, 3 * G_PI / 2);
  cairo_close_path (cr);
}

static void
circle (cairo_t *cr, double x, double y, double r)
{
  cairo_new_sub_path (cr);
  cairo_arc (cr, x, y, r, 0, 2 * G_PI);
}

static void
rgba (cairo_t *cr, const GdkRGBA *c, double alpha)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, c->red, c->green, c->blue, c->alpha * alpha);
}

static void
hex (cairo_t *cr, guint32 v, double alpha)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, ((v >> 16) & 0xff) / 255.0, ((v >> 8) & 0xff) / 255.0, (v & 0xff) / 255.0, alpha);
}

/* "cut" parts of a glyph: painted in a contrast color, or erased when NULL */
static void
cut (cairo_t *cr, const GdkRGBA *c)
{
  if (c)
    rgba (cr, c, 1);
  else
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_set_source_rgba (cr, 0, 0, 0, 1);
    }
}

/* Glyphs live on a 24×24 grid. */
static void
draw_glyph (cairo_t *cr, StoreGlyph g, const GdkRGBA *fg, const GdkRGBA *cc)
{
  cairo_save (cr);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  rgba (cr, fg, 1);

  switch (g)
    {
    case GLYPH_NOTE:
      cairo_set_line_width (cr, 2.2);
      cairo_move_to (cr, 9.6, 17); cairo_line_to (cr, 9.6, 6); cairo_line_to (cr, 19.6, 4); cairo_line_to (cr, 19.6, 15);
      cairo_stroke (cr);
      circle (cr, 7, 17, 2.7); circle (cr, 17, 15, 2.7); cairo_fill (cr);
      break;

    case GLYPH_LEAF:
      cairo_move_to (cr, 5, 19);
      cairo_curve_to (cr, 5, 10, 11, 5, 19, 5);
      cairo_curve_to (cr, 19, 13, 14, 19, 5, 19);
      cairo_close_path (cr); cairo_fill (cr);
      cut (cr, cc); cairo_set_line_width (cr, 1.6);
      cairo_move_to (cr, 6.5, 17.5); cairo_line_to (cr, 14, 10); cairo_stroke (cr);
      break;

    case GLYPH_CAMERA:
      rrect (cr, 3, 7, 18, 12, 2.5); rrect (cr, 8, 4.5, 8, 3.5, 1); cairo_fill (cr);
      cut (cr, cc); circle (cr, 12, 13, 3.8); cairo_fill (cr);
      rgba (cr, fg, .55); circle (cr, 12, 13, 2); cairo_fill (cr);
      break;

    case GLYPH_CODE:
      cairo_set_line_width (cr, 2.6);
      cairo_move_to (cr, 9, 7); cairo_line_to (cr, 4, 12); cairo_line_to (cr, 9, 17);
      cairo_move_to (cr, 15, 7); cairo_line_to (cr, 20, 12); cairo_line_to (cr, 15, 17);
      cairo_stroke (cr);
      break;

    case GLYPH_CHAT:
      rrect (cr, 4, 4, 16, 12, 2.2); cairo_fill (cr);
      cairo_move_to (cr, 6, 15); cairo_line_to (cr, 6, 20); cairo_line_to (cr, 11, 15); cairo_close_path (cr); cairo_fill (cr);
      cut (cr, cc); circle (cr, 8.5, 10, 1.2); circle (cr, 12, 10, 1.2); circle (cr, 15.5, 10, 1.2); cairo_fill (cr);
      break;

    case GLYPH_GLOBE:
      cairo_set_line_width (cr, 2.2); circle (cr, 12, 12, 8); cairo_stroke (cr);
      cairo_set_line_width (cr, 1.7);
      cairo_save (cr); cairo_translate (cr, 12, 12); cairo_scale (cr, 3.4 / 8.0, 1); circle (cr, 0, 0, 8); cairo_restore (cr); cairo_stroke (cr);
      cairo_move_to (cr, 4, 12); cairo_line_to (cr, 20, 12); cairo_stroke (cr);
      break;

    case GLYPH_BRUSH:
      cairo_move_to (cr, 15, 3.5); cairo_line_to (cr, 20.5, 9); cairo_line_to (cr, 10, 19.5);
      cairo_line_to (cr, 4, 21); cairo_line_to (cr, 5.5, 15); cairo_close_path (cr); cairo_fill (cr);
      cut (cr, cc); cairo_set_line_width (cr, 1.6);
      cairo_move_to (cr, 13.5, 5.5); cairo_line_to (cr, 18.5, 10.5); cairo_stroke (cr);
      break;

    case GLYPH_CHART:
      rrect (cr, 4, 12, 3.6, 8, 1); rrect (cr, 10.2, 7, 3.6, 13, 1); rrect (cr, 16.4, 4, 3.6, 16, 1); cairo_fill (cr);
      break;

    case GLYPH_PIN:
      cairo_arc (cr, 12, 10, 6.5, G_PI * 0.82, G_PI * 0.18);
      cairo_line_to (cr, 12, 21.5); cairo_close_path (cr); cairo_fill (cr);
      cut (cr, cc); circle (cr, 12, 10, 2.5); cairo_fill (cr);
      break;

    case GLYPH_BOOK:
      cairo_move_to (cr, 3.5, 5.5); cairo_curve_to (cr, 6.7, 4.4, 9.3, 4.9, 11.3, 6.6);
      cairo_line_to (cr, 11.3, 20); cairo_curve_to (cr, 9.3, 18.4, 6.7, 17.9, 3.5, 19); cairo_close_path (cr);
      cairo_move_to (cr, 20.5, 5.5); cairo_curve_to (cr, 17.3, 4.4, 14.7, 4.9, 12.7, 6.6);
      cairo_line_to (cr, 12.7, 20); cairo_curve_to (cr, 14.7, 18.4, 17.3, 17.9, 20.5, 19); cairo_close_path (cr);
      cairo_fill (cr);
      break;

    case GLYPH_GAME:
      rrect (cr, 2.5, 7, 19, 11, 5.5); cairo_fill (cr);
      cut (cr, cc); cairo_set_line_width (cr, 1.9);
      cairo_move_to (cr, 7.5, 10.3); cairo_line_to (cr, 7.5, 14.7);
      cairo_move_to (cr, 5.3, 12.5); cairo_line_to (cr, 9.7, 12.5); cairo_stroke (cr);
      circle (cr, 16, 11.3, 1.3); circle (cr, 18.2, 13.8, 1.3); cairo_fill (cr);
      break;

    case GLYPH_TERM:
      rrect (cr, 3, 4, 18, 16, 2.5); cairo_fill (cr);
      cut (cr, cc); cairo_set_line_width (cr, 1.9);
      cairo_move_to (cr, 7, 9); cairo_line_to (cr, 10, 12); cairo_line_to (cr, 7, 15);
      cairo_move_to (cr, 12.5, 15); cairo_line_to (cr, 17, 15); cairo_stroke (cr);
      break;

    case GLYPH_CLOUD:
      hex (cr, 0xffd54a, 1); circle (cr, 9.5, 9, 4.2); cairo_fill (cr);
      rgba (cr, fg, 1);
      rrect (cr, 4.5, 14.2, 16.5, 5.3, 2.65); circle (cr, 13.2, 13.6, 4.6); circle (cr, 8.4, 15.2, 2.9); cairo_fill (cr);
      break;

    case GLYPH_MIC:
      rrect (cr, 9, 3, 6, 11, 3); cairo_fill (cr);
      cairo_set_line_width (cr, 2.1);
      cairo_arc (cr, 12, 11, 6, 0, G_PI); cairo_stroke (cr);
      cairo_move_to (cr, 12, 17); cairo_line_to (cr, 12, 21); cairo_stroke (cr);
      break;

    case GLYPH_TILES:
      rrect (cr, 4, 4, 7, 7, 1.6); rrect (cr, 13, 13, 7, 7, 1.6); cairo_fill (cr);
      rgba (cr, fg, .65); rrect (cr, 13, 4, 7, 7, 1.6); rrect (cr, 4, 13, 7, 7, 1.6); cairo_fill (cr);
      break;

    case GLYPH_LOCK:
      rrect (cr, 5, 10, 14, 10.5, 2.5); cairo_fill (cr);
      cairo_set_line_width (cr, 2.3);
      cairo_move_to (cr, 8, 10.5); cairo_line_to (cr, 8, 8); cairo_arc (cr, 12, 8, 4, G_PI, 2 * G_PI); cairo_line_to (cr, 16, 10.5);
      cairo_stroke (cr);
      cut (cr, cc); circle (cr, 12, 15.2, 1.7); cairo_fill (cr);
      break;

    default:
      break;
    }

  cairo_restore (cr);
}

/* ---------- App icon ---------- */

static void
icon_draw (GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer data)
{
  StoreApp *a = data;
  double s = MIN (w, h) / 64.0;
  cairo_pattern_t *p;

  cairo_translate (cr, (w - 64 * s) / 2, (h - 64 * s) / 2);
  cairo_scale (cr, s, s);

  /* base thickness, like Adwaita app icons */
  rgba (cr, &a->c2, 1);
  rrect (cr, 5, 8, 54, 52, 13);
  cairo_fill (cr);
  rgba (cr, &WHITE, 0); /* reset */
  hex (cr, 0x000000, .18);
  rrect (cr, 5, 8, 54, 52, 13);
  cairo_fill (cr);

  p = cairo_pattern_create_linear (0, 4, 16, 56);
  cairo_pattern_add_color_stop_rgb (p, 0, a->c1.red, a->c1.green, a->c1.blue);
  cairo_pattern_add_color_stop_rgb (p, 1, a->c2.red, a->c2.green, a->c2.blue);
  rrect (cr, 5, 4, 54, 52, 13);
  cairo_set_source (cr, p);
  cairo_fill (cr);
  cairo_pattern_destroy (p);

  cairo_save (cr);
  rrect (cr, 5, 4, 54, 52, 13);
  cairo_clip (cr);
  hex (cr, 0xffffff, .09);
  cairo_rectangle (cr, 5, 4, 54, 24);
  cairo_fill (cr);
  cairo_restore (cr);

  cairo_translate (cr, 14, 12);
  cairo_scale (cr, 1.5, 1.5);
  draw_glyph (cr, a->glyph, &WHITE, &a->c2);
}

GtkWidget *
store_app_icon_new (StoreApp *app, int size)
{
  GtkWidget *da = gtk_drawing_area_new ();

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), size);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), size);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), icon_draw, app, NULL);
  gtk_widget_set_halign (da, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (da, GTK_ALIGN_CENTER);
  gtk_accessible_update_property (GTK_ACCESSIBLE (da), GTK_ACCESSIBLE_PROPERTY_LABEL, app->name, -1);
  return da;
}

/* ---------- Symbolic glyph in the widget's CSS color ---------- */

static void
glyph_draw (GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer data)
{
  StoreGlyph g = GPOINTER_TO_INT (data);
  GdkRGBA color;
  double s = MIN (w, h) / 24.0;

  gtk_widget_get_color (GTK_WIDGET (da), &color);
  cairo_push_group (cr);
  cairo_translate (cr, (w - 24 * s) / 2, (h - 24 * s) / 2);
  cairo_scale (cr, s, s);
  draw_glyph (cr, g, &color, NULL);
  cairo_pop_group_to_source (cr);
  cairo_paint (cr);
}

GtkWidget *
store_glyph_icon_new (StoreGlyph glyph, int size)
{
  GtkWidget *da = gtk_drawing_area_new ();

  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), size);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), size);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), glyph_draw, GINT_TO_POINTER (glyph), NULL);
  gtk_widget_set_valign (da, GTK_ALIGN_CENTER);
  return da;
}

/* ---------- Screenshots (800×500 reference canvas) ---------- */

#define SW 800.0
#define SH 500.0
#define INK  0x2e2e33
#define SOFT 0xe9e9ee
#define LINE 0xd7d7de

typedef struct {
  StoreApp *app;
  StoreShot layout;
  double    xalign, yalign;
} ShotData;

static cairo_pattern_t *
app_gradient (StoreApp *a, double x0, double y0, double x1, double y1)
{
  cairo_pattern_t *p = cairo_pattern_create_linear (x0, y0, x1, y1);
  cairo_pattern_add_color_stop_rgb (p, 0, a->c1.red, a->c1.green, a->c1.blue);
  cairo_pattern_add_color_stop_rgb (p, 1, a->c2.red, a->c2.green, a->c2.blue);
  return p;
}

static void
bar (cairo_t *cr, double x, double y, double w, double h, guint32 color, double alpha)
{
  hex (cr, color, alpha);
  rrect (cr, x, y, w, h, h / 2);
  cairo_fill (cr);
}

static void
card (cairo_t *cr, double x, double y, double w, double h, double r, guint32 fill)
{
  hex (cr, fill, 1);
  rrect (cr, x, y, w, h, r);
  cairo_fill_preserve (cr);
  hex (cr, LINE, 1);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
}

static void
sidebar (cairo_t *cr, StoreApp *a)
{
  hex (cr, 0xf3f3f6, 1);
  cairo_rectangle (cr, 0, 46, 190, SH - 46);
  cairo_fill (cr);
  for (int i = 0; i < 7; i++)
    {
      double y = 70 + i * 40;
      if (i == 1)
        {
          rgba (cr, &a->c1, .22);
          rrect (cr, 10, y - 12, 170, 32, 7);
          cairo_fill (cr);
        }
      if (i == 1) rgba (cr, &a->c2, 1); else hex (cr, 0xb9b9c3, 1);
      rrect (cr, 24, y - 2, 14, 14, 3);
      cairo_fill (cr);
      if (i == 1) rgba (cr, &a->c2, 1); else hex (cr, 0xc5c5cd, 1);
      rrect (cr, 48, y + 1, 70 + (i * 23) % 60, 8, 4);
      cairo_fill (cr);
    }
}

static void
shot_grid (cairo_t *cr, StoreApp *a)
{
  sidebar (cr, a);
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 4; c++)
      {
        double x = 214 + c * 144, y = 66 + r * 140;
        int k = r * 4 + c;
        if (k % 3 == 0) rgba (cr, &a->c1, .35 + (k % 4) * .15);
        else if (k % 3 == 1) rgba (cr, &a->c2, .35 + (k % 4) * .15);
        else hex (cr, SOFT, 1);
        rrect (cr, x, y, 128, 96, 8);
        cairo_fill (cr);
        hex (cr, 0xffffff, .55);
        circle (cr, x + 34 + (k * 17) % 50, y + 40, 12 + (k % 3) * 5);
        cairo_fill (cr);
        bar (cr, x, y + 106, 70 + (k * 13) % 50, 8, 0xc5c5cd, 1);
      }
}

static void
shot_list (cairo_t *cr, StoreApp *a)
{
  sidebar (cr, a);
  for (int i = 0; i < 8; i++)
    {
      double y = 66 + i * 52;
      card (cr, 210, y, 570, 44, 8, i == 2 ? SOFT : 0xffffff);
      rgba (cr, i % 2 ? &a->c1 : &a->c2, .8);
      circle (cr, 236, y + 22, 13);
      cairo_fill (cr);
      bar (cr, 260, y + 11, 120 + (i * 37) % 140, 9, INK, .7);
      bar (cr, 260, y + 26, 200 + (i * 53) % 180, 7, 0xb9b9c3, 1);
      bar (cr, 730, y + 17, 34, 10, 0xc5c5cd, 1);
    }
}

static void
shot_doc (cairo_t *cr, StoreApp *a)
{
  static const int widths[] = { 520, 480, 540, 300, 0, 500, 460, 520, 410, 0, 380, 520, 490, 260 };

  sidebar (cr, a);
  hex (cr, INK, .85);
  rrect (cr, 214, 70, 260, 18, 5);
  cairo_fill (cr);
  for (int i = 0; i < 14; i++)
    {
      double y = 110 + i * 26;
      if (!widths[i])
        continue;
      if (i == 6)
        {
          rgba (cr, &a->c1, .25);
          rrect (cr, 210, y - 6, widths[i] + 8, 20, 4);
          cairo_fill (cr);
        }
      if (i % 5 == 0) rgba (cr, &a->c2, .8); else hex (cr, 0xb0b0ba, 1);
      rrect (cr, 214, y, widths[i], 8, 4);
      cairo_fill (cr);
    }
}

static void
shot_chart (cairo_t *cr, StoreApp *a)
{
  double px[11], py[11];

  sidebar (cr, a);
  card (cr, 214, 66, 566, 250, 10, 0xffffff);
  hex (cr, SOFT, 1);
  cairo_set_line_width (cr, 1);
  for (int g = 0; g < 4; g++)
    {
      cairo_move_to (cr, 236, 110 + g * 50);
      cairo_line_to (cr, 760, 110 + g * 50);
    }
  cairo_stroke (cr);

  for (int i = 0; i < 11; i++)
    {
      px[i] = 236 + i * 52.4;
      py[i] = 260 - (sin (i * .8 + strlen (a->name)) * 50 + i * 9 + 30);
    }
  cairo_move_to (cr, px[0], py[0]);
  for (int i = 1; i < 11; i++)
    cairo_line_to (cr, px[i], py[i]);
  cairo_line_to (cr, 760, 296);
  cairo_line_to (cr, 236, 296);
  cairo_close_path (cr);
  rgba (cr, &a->c1, .25);
  cairo_fill (cr);

  cairo_move_to (cr, px[0], py[0]);
  for (int i = 1; i < 11; i++)
    cairo_line_to (cr, px[i], py[i]);
  rgba (cr, &a->c2, 1);
  cairo_set_line_width (cr, 3);
  cairo_stroke (cr);
  circle (cr, px[10], py[10], 5);
  cairo_fill (cr);

  for (int k = 0; k < 3; k++)
    {
      double x = 214 + k * 194;
      card (cr, x, 336, 178, 140, 10, 0xffffff);
      bar (cr, x + 18, 356, 70, 8, 0xb9b9c3, 1);
      if (k == 1) rgba (cr, &a->c2, .8); else hex (cr, INK, .8);
      rrect (cr, x + 18, 378, 110, 22, 5);
      cairo_fill (cr);
      bar (cr, x + 18, 420, 130, 6, SOFT, 1);
      bar (cr, x + 18, 436, 90, 6, SOFT, 1);
    }
}

static void
shot_player (cairo_t *cr, StoreApp *a)
{
  cairo_pattern_t *p = app_gradient (a, 0, 46, SW, SH);

  cairo_save (cr);
  cairo_rectangle (cr, 0, 46, SW, SH - 46);
  cairo_clip (cr);
  cairo_set_source (cr, p);
  cairo_paint_with_alpha (cr, .18);
  cairo_restore (cr);

  rrect (cr, 80, 90, 280, 280, 16);
  cairo_set_source (cr, p);
  cairo_fill (cr);
  cairo_pattern_destroy (p);

  cairo_save (cr);
  cairo_translate (cr, 150, 160);
  cairo_scale (cr, 5.8, 5.8);
  draw_glyph (cr, a->glyph, &WHITE, &a->c2);
  cairo_restore (cr);

  hex (cr, INK, .85); rrect (cr, 410, 140, 250, 20, 6); cairo_fill (cr);
  bar (cr, 410, 174, 160, 12, 0x9a9aa5, 1);
  bar (cr, 410, 250, 320, 6, 0xcfcfd6, 1);
  rgba (cr, &a->c2, 1); rrect (cr, 410, 250, 130, 6, 3); cairo_fill (cr);
  circle (cr, 540, 253, 8); cairo_fill (cr);
  circle (cr, 570, 320, 28); cairo_fill (cr);
  hex (cr, 0xffffff, 1);
  cairo_move_to (cr, 562, 306); cairo_line_to (cr, 562, 334); cairo_line_to (cr, 584, 320); cairo_close_path (cr); cairo_fill (cr);
  hex (cr, 0x77777f, 1);
  cairo_move_to (cr, 490, 308); cairo_line_to (cr, 490, 332); cairo_line_to (cr, 472, 320); cairo_close_path (cr);
  cairo_move_to (cr, 470, 308); cairo_line_to (cr, 470, 332); cairo_line_to (cr, 452, 320); cairo_close_path (cr);
  cairo_move_to (cr, 650, 308); cairo_line_to (cr, 650, 332); cairo_line_to (cr, 668, 320); cairo_close_path (cr);
  cairo_move_to (cr, 670, 308); cairo_line_to (cr, 670, 332); cairo_line_to (cr, 688, 320); cairo_close_path (cr);
  cairo_fill (cr);

  hex (cr, 0xf3f3f6, 1);
  cairo_rectangle (cr, 0, 430, SW, 70);
  cairo_fill (cr);
  for (int i = 0; i < 6; i++)
    {
      rgba (cr, i % 2 ? &a->c1 : &a->c2, .6);
      rrect (cr, 30 + i * 128, 445, 40, 40, 6);
      cairo_fill (cr);
      bar (cr, 78 + i * 128, 458, 40, 7, 0xb9b9c3, 1);
    }
}

static void
shot_map (cairo_t *cr, StoreApp *a)
{
  hex (cr, 0xe8efe2, 1);
  cairo_rectangle (cr, 0, 46, SW, SH - 46);
  cairo_fill (cr);

  hex (cr, 0xcfdcc4, 1);
  cairo_set_line_width (cr, 2);
  for (int i = 0; i < 9; i++)
    {
      cairo_move_to (cr, -20, 90 + i * 48);
      cairo_curve_to (cr, 200, 40 + i * 52, 420, 150 + i * 40, 820, 70 + i * 50);
    }
  cairo_stroke (cr);

  hex (cr, 0xbcd9f0, 1);
  cairo_move_to (cr, 0, 380);
  cairo_curve_to (cr, 160, 340, 260, 420, 420, 380);
  cairo_curve_to (cr, 580, 340, 700, 330, 800, 360);
  cairo_line_to (cr, 800, 500); cairo_line_to (cr, 0, 500); cairo_close_path (cr);
  cairo_fill (cr);

  rgba (cr, &a->c2, 1);
  cairo_set_line_width (cr, 4);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_move_to (cr, 110, 140);
  cairo_curve_to (cr, 220, 200, 260, 260, 360, 250);
  cairo_curve_to (cr, 460, 240, 520, 170, 640, 300);
  cairo_stroke (cr);

  hex (cr, 0xffffff, 1); circle (cr, 110, 140, 9); cairo_fill_preserve (cr);
  rgba (cr, &a->c2, 1); cairo_stroke (cr);

  cairo_save (cr);
  cairo_translate (cr, 622, 262);
  cairo_scale (cr, 1.6, 1.6);
  cairo_arc (cr, 12, 10, 6.5, G_PI * 0.82, G_PI * 0.18);
  cairo_line_to (cr, 12, 21.5);
  cairo_close_path (cr);
  rgba (cr, &a->c2, 1); cairo_fill (cr);
  hex (cr, 0xffffff, 1); circle (cr, 12, 10, 2.5); cairo_fill (cr);
  cairo_restore (cr);

  card (cr, 20, 62, 250, 40, 20, 0xffffff);
  bar (cr, 44, 78, 140, 8, 0xb9b9c3, 1);

  card (cr, 560, 390, 220, 92, 12, 0xffffff);
  bar (cr, 578, 406, 100, 10, INK, .8);
  rgba (cr, &a->c2, 1);
  cairo_set_line_width (cr, 3);
  cairo_move_to (cr, 578, 466); cairo_line_to (cr, 610, 446); cairo_line_to (cr, 640, 456);
  cairo_line_to (cr, 680, 428); cairo_line_to (cr, 720, 440); cairo_line_to (cr, 760, 420);
  cairo_stroke (cr);
}

static void
shot_game (cairo_t *cr, StoreApp *a)
{
  static const double alphas[] = { .12, .3, .5, .2, .7 };
  cairo_pattern_t *p = app_gradient (a, 0, 46, SW, SH);

  cairo_rectangle (cr, 0, 46, SW, SH - 46);
  cairo_set_source (cr, p);
  cairo_fill (cr);
  cairo_pattern_destroy (p);

  for (int r = 0; r < 6; r++)
    for (int c = 0; c < 10; c++)
      {
        hex (cr, 0xffffff, alphas[(r * 7 + c * 3) % 5]);
        rrect (cr, 60 + c * 70, 80 + r * 64, 62, 56, 8);
        cairo_fill (cr);
      }

  hex (cr, 0x000000, .3); rrect (cr, 20, 62, 150, 34, 17); cairo_fill (cr);
  hex (cr, 0xffd54a, 1); circle (cr, 40, 79, 8); cairo_fill (cr);
  bar (cr, 56, 74, 80, 10, 0xffffff, 1);

  cairo_save (cr);
  cairo_translate (cr, 346, 210);
  cairo_scale (cr, 4.5, 4.5);
  draw_glyph (cr, a->glyph, &WHITE, &a->c2);
  cairo_restore (cr);
}

static void
shot_draw (GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer data)
{
  ShotData *d = data;
  double s = MIN (w / SW, h / SH);

  cairo_translate (cr, (w - SW * s) * d->xalign, (h - SH * s) * d->yalign);
  cairo_scale (cr, s, s);

  /* soft drop shadow */
  for (int i = 1; i <= 6; i++)
    {
      hex (cr, 0x000000, .035);
      rrect (cr, -i, -i + 3, SW + 2 * i, SH + 2 * i, 12 + i);
      cairo_fill (cr);
    }

  rrect (cr, 0, 0, SW, SH, 12);
  cairo_clip (cr);
  hex (cr, 0xffffff, 1);
  cairo_paint (cr);

  switch (d->layout)
    {
    case SHOT_GRID:   shot_grid (cr, d->app);   break;
    case SHOT_LIST:   shot_list (cr, d->app);   break;
    case SHOT_DOC:    shot_doc (cr, d->app);    break;
    case SHOT_CHART:  shot_chart (cr, d->app);  break;
    case SHOT_PLAYER: shot_player (cr, d->app); break;
    case SHOT_MAP:    shot_map (cr, d->app);    break;
    case SHOT_GAME:   shot_game (cr, d->app);   break;
    default: break;
    }

  /* header bar */
  hex (cr, 0xebebed, 1);
  cairo_rectangle (cr, 0, 0, SW, 46);
  cairo_fill (cr);
  hex (cr, 0xdcdce0, 1);
  rrect (cr, 14, 10, 26, 26, 6);
  cairo_fill (cr);
  bar (cr, SW / 2 - 60, 18, 120, 10, 0x5e5e66, 1);
  hex (cr, 0xdcdce0, 1);
  circle (cr, SW - 26, 23, 11);
  cairo_fill (cr);
  hex (cr, INK, 1);
  cairo_set_line_width (cr, 2);
  cairo_move_to (cr, SW - 30, 19); cairo_line_to (cr, SW - 22, 27);
  cairo_move_to (cr, SW - 22, 19); cairo_line_to (cr, SW - 30, 27);
  cairo_stroke (cr);
}

GtkWidget *
store_screenshot_new (StoreApp *app, StoreShot layout, int height, double xalign, double yalign)
{
  GtkWidget *da = gtk_drawing_area_new ();
  ShotData *d = g_new0 (ShotData, 1);

  d->app = app;
  d->layout = layout;
  d->xalign = xalign;
  d->yalign = yalign;
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), height);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), shot_draw, d, g_free);
  gtk_widget_set_hexpand (da, TRUE);
  return da;
}
