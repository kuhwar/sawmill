/* App icons from the catalog, and cairo-drawn glyphs for categories. */

#include <math.h>
#include "store-art.h"


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

/* ---------- App icon (icon.png from the catalog) ---------- */

GtkWidget *
store_app_icon_new (StoreApp *app, int size)
{
  GdkTexture *tex = store_app_get_icon (app);
  GtkWidget *img = tex ? gtk_image_new_from_paintable (GDK_PAINTABLE (tex))
                       : gtk_image_new_from_icon_name ("application-x-executable");

  gtk_image_set_pixel_size (GTK_IMAGE (img), size);
  gtk_widget_add_css_class (img, size >= 96 ? "icon-dropshadow" : "lowres-icon");
  gtk_widget_set_halign (img, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (img, GTK_ALIGN_CENTER);
  gtk_accessible_update_property (GTK_ACCESSIBLE (img), GTK_ACCESSIBLE_PROPERTY_LABEL, app->title, -1);
  return img;
}

StoreGlyph
store_category_glyph (const char *category_id)
{
  static const struct { const char *id; StoreGlyph glyph; } map[] = {
    { "create", GLYPH_BRUSH }, { "work", GLYPH_CHART }, { "play", GLYPH_GAME }, { "social", GLYPH_CHAT },
    { "learn", GLYPH_BOOK }, { "develop", GLYPH_CODE }, { "audio", GLYPH_NOTE }, { "utils", GLYPH_TILES },
  };
  for (guint i = 0; i < G_N_ELEMENTS (map); i++)
    if (g_str_equal (map[i].id, category_id))
      return map[i].glyph;
  return GLYPH_TILES;
}
