// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/ui/slm_fonts.cpp
 * @brief Compact French-accent wrapper around LVGL built-in Montserrat fonts.
 */

#include "src/ui/slm_fonts.h"

#include <string.h>

namespace {

enum accent_kind_t : uint8_t {
  ACCENT_NONE = 0,
  ACCENT_ACUTE,
  ACCENT_GRAVE,
  ACCENT_CIRCUMFLEX,
  ACCENT_CEDILLA,
};

typedef struct {
  const lv_font_t *base;
  uint8_t nominal_size;
} accent_font_config_t;

static accent_font_config_t s_cfg_18 = { &lv_font_montserrat_18, 18u };
static accent_font_config_t s_cfg_24 = { &lv_font_montserrat_24, 24u };
static accent_font_config_t s_cfg_30 = { &lv_font_montserrat_30, 30u };
static accent_font_config_t s_cfg_32 = { &lv_font_montserrat_32, 32u };
static accent_font_config_t s_cfg_34 = { &lv_font_montserrat_34, 34u };
static accent_font_config_t s_cfg_36 = { &lv_font_montserrat_36, 36u };
static accent_font_config_t s_cfg_48 = { &lv_font_montserrat_48, 48u };

static bool map_accented_letter_(uint32_t letter, uint32_t *base_letter, accent_kind_t *accent)
{
  if(base_letter == nullptr || accent == nullptr) return false;

  switch(letter) {
    case 0x00C0u: *base_letter = 'A'; *accent = ACCENT_GRAVE; return true;       // À
    case 0x00C7u: *base_letter = 'C'; *accent = ACCENT_CEDILLA; return true;     // Ç
    case 0x00C9u: *base_letter = 'E'; *accent = ACCENT_ACUTE; return true;       // É
    case 0x00CAu: *base_letter = 'E'; *accent = ACCENT_CIRCUMFLEX; return true;  // Ê
    case 0x00E0u: *base_letter = 'a'; *accent = ACCENT_GRAVE; return true;       // à
    case 0x00E7u: *base_letter = 'c'; *accent = ACCENT_CEDILLA; return true;     // ç
    case 0x00E8u: *base_letter = 'e'; *accent = ACCENT_GRAVE; return true;       // è
    case 0x00E9u: *base_letter = 'e'; *accent = ACCENT_ACUTE; return true;       // é
    case 0x00EAu: *base_letter = 'e'; *accent = ACCENT_CIRCUMFLEX; return true;  // ê
    default: break;
  }

  return false;
}

static uint32_t map_next_letter_(uint32_t letter)
{
  uint32_t base = 0u;
  accent_kind_t accent = ACCENT_NONE;
  return map_accented_letter_(letter, &base, &accent) ? base : letter;
}

static uint8_t desired_mark_height_(uint8_t nominal_size)
{
  uint8_t h = (uint8_t)(nominal_size / 8u);
  if(h < 2u) h = 2u;
  if(h > 6u) h = 6u;
  return h;
}

static uint8_t top_extra_rows_(const lv_font_t *font,
                               const lv_font_glyph_dsc_t *base_dsc,
                               uint8_t nominal_size)
{
  if(font == nullptr || base_dsc == nullptr) return 0u;

  const int32_t ascent = font->line_height - font->base_line;
  const int32_t glyph_top = (int32_t)base_dsc->ofs_y + (int32_t)base_dsc->box_h;
  int32_t available = ascent - glyph_top;
  if(available <= 0) return 0u;

  int32_t desired = (int32_t)desired_mark_height_(nominal_size) + 1; // one blank row above letter
  if(desired > available) desired = available;
  return (uint8_t)desired;
}

static uint8_t bottom_extra_rows_(const lv_font_t *font,
                                  const lv_font_glyph_dsc_t *base_dsc,
                                  uint8_t nominal_size)
{
  if(font == nullptr || base_dsc == nullptr) return 0u;

  int32_t available = font->base_line + (int32_t)base_dsc->ofs_y;
  if(available <= 0) return 0u;

  int32_t desired = (int32_t)desired_mark_height_(nominal_size) + 1;
  if(desired > available) desired = available;
  return (uint8_t)desired;
}

static bool accent_get_glyph_dsc_(const lv_font_t *font,
                                  lv_font_glyph_dsc_t *dsc_out,
                                  uint32_t letter,
                                  uint32_t letter_next)
{
  if(font == nullptr || dsc_out == nullptr || font->user_data == nullptr) return false;

  uint32_t base_letter = 0u;
  accent_kind_t accent = ACCENT_NONE;
  if(!map_accented_letter_(letter, &base_letter, &accent)) return false;

  const accent_font_config_t *cfg = static_cast<const accent_font_config_t *>(font->user_data);
  lv_font_glyph_dsc_t base_dsc;
  if(!lv_font_get_glyph_dsc(cfg->base, &base_dsc, base_letter, map_next_letter_(letter_next))) return false;

  *dsc_out = base_dsc;
  dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
  dsc_out->stride = 0u;
  dsc_out->is_placeholder = false;
  dsc_out->gid.index = letter;

  if(accent == ACCENT_CEDILLA) {
    const uint8_t extra = bottom_extra_rows_(font, &base_dsc, cfg->nominal_size);
    dsc_out->box_h = (uint16_t)(base_dsc.box_h + extra);
    dsc_out->ofs_y = (int16_t)(base_dsc.ofs_y - (int16_t)extra);
  }
  else {
    const uint8_t extra = top_extra_rows_(font, &base_dsc, cfg->nominal_size);
    dsc_out->box_h = (uint16_t)(base_dsc.box_h + extra);
  }

  return true;
}

static void put_alpha_(uint8_t *data, uint32_t stride, uint16_t w, uint16_t h,
                       int32_t x, int32_t y, uint8_t alpha)
{
  if(data == nullptr || x < 0 || y < 0 || x >= (int32_t)w || y >= (int32_t)h) return;
  uint8_t *p = data + ((uint32_t)y * stride) + (uint32_t)x;
  if(*p < alpha) *p = alpha;
}

static void draw_thick_pixel_(uint8_t *data, uint32_t stride, uint16_t w, uint16_t h,
                              int32_t x, int32_t y, uint8_t thickness)
{
  const int32_t before = (int32_t)(thickness - 1u) / 2;
  const int32_t after = (int32_t)thickness / 2;
  for(int32_t dy = -before; dy <= after; ++dy) {
    for(int32_t dx = -before; dx <= after; ++dx) {
      put_alpha_(data, stride, w, h, x + dx, y + dy, 255u);
    }
  }
}

static void draw_top_accent_(uint8_t *data, uint32_t stride, uint16_t w, uint16_t h,
                             uint8_t top_extra, uint8_t nominal_size, accent_kind_t accent)
{
  if(data == nullptr || top_extra < 2u || w < 3u) return;

  const uint8_t mark_h = (uint8_t)(top_extra - 1u); // final row is the gap to the base glyph
  if(mark_h == 0u) return;

  uint8_t mark_w = (uint8_t)(nominal_size / 4u);
  if(mark_w < 3u) mark_w = 3u;
  if(mark_w > (uint8_t)(w - 1u)) mark_w = (uint8_t)(w - 1u);

  uint8_t thickness = (uint8_t)(nominal_size / 24u);
  if(thickness < 1u) thickness = 1u;
  if(thickness > 2u) thickness = 2u;

  const int32_t cx = (int32_t)(w - 1u) / 2;

  if(accent == ACCENT_CIRCUMFLEX) {
    const int32_t half = (int32_t)mark_w / 2;
    for(uint8_t y = 0u; y < mark_h; ++y) {
      const int32_t spread = (mark_h <= 1u) ? half : ((int32_t)y * half) / (int32_t)(mark_h - 1u);
      draw_thick_pixel_(data, stride, w, h, cx - spread, (int32_t)y, thickness);
      draw_thick_pixel_(data, stride, w, h, cx + spread, (int32_t)y, thickness);
    }
    return;
  }

  const int32_t x_left = cx - ((int32_t)mark_w - 1) / 2;
  const int32_t x_right = x_left + (int32_t)mark_w - 1;
  for(uint8_t y = 0u; y < mark_h; ++y) {
    int32_t x;
    if(mark_h <= 1u) {
      x = cx;
    }
    else if(accent == ACCENT_ACUTE) {
      // Acute rises toward the right: /
      x = x_right - ((int32_t)y * (x_right - x_left)) / (int32_t)(mark_h - 1u);
    }
    else {
      // Grave accent slopes down toward the right.
      x = x_left + ((int32_t)y * (x_right - x_left)) / (int32_t)(mark_h - 1u);
    }
    draw_thick_pixel_(data, stride, w, h, x, (int32_t)y, thickness);
  }
}

static void draw_cedilla_(uint8_t *data, uint32_t stride, uint16_t w, uint16_t h,
                          uint16_t base_h, uint8_t bottom_extra, uint8_t nominal_size)
{
  if(data == nullptr || bottom_extra < 2u || w < 3u) return;

  const uint8_t mark_h = (uint8_t)(bottom_extra - 1u);
  uint8_t thickness = (uint8_t)(nominal_size / 24u);
  if(thickness < 1u) thickness = 1u;
  if(thickness > 2u) thickness = 2u;

  const int32_t cx = (int32_t)(w - 1u) / 2;
  for(uint8_t y = 0u; y < mark_h; ++y) {
    // Start below the centre of C/c and curl gently to the left.
    const int32_t shift = (mark_h <= 1u) ? 0 : ((int32_t)y * 2) / (int32_t)(mark_h - 1u);
    draw_thick_pixel_(data, stride, w, h, cx - shift, (int32_t)base_h + (int32_t)y + 1, thickness);
  }
}

static const void *accent_get_glyph_bitmap_(lv_font_glyph_dsc_t *g_dsc, lv_draw_buf_t *draw_buf)
{
  if(g_dsc == nullptr || draw_buf == nullptr || g_dsc->resolved_font == nullptr) return nullptr;

  const lv_font_t *font = g_dsc->resolved_font;
  if(font->user_data == nullptr) return nullptr;
  const accent_font_config_t *cfg = static_cast<const accent_font_config_t *>(font->user_data);

  uint32_t base_letter = 0u;
  accent_kind_t accent = ACCENT_NONE;
  if(!map_accented_letter_(g_dsc->gid.index, &base_letter, &accent)) return nullptr;

  lv_font_glyph_dsc_t base_dsc;
  if(!lv_font_get_glyph_dsc(cfg->base, &base_dsc, base_letter, 0u)) return nullptr;

  const uint8_t top_extra = (accent == ACCENT_CEDILLA) ? 0u : top_extra_rows_(font, &base_dsc, cfg->nominal_size);
  const uint8_t bottom_extra = (accent == ACCENT_CEDILLA) ? bottom_extra_rows_(font, &base_dsc, cfg->nominal_size) : 0u;

  const uint16_t out_w = g_dsc->box_w;
  const uint16_t out_h = g_dsc->box_h;
  const uint32_t stride = lv_draw_buf_width_to_stride(out_w, LV_COLOR_FORMAT_A8);
  uint8_t *data = draw_buf->data;
  if(data == nullptr) return nullptr;

  memset(data, 0, stride * (uint32_t)out_h);

  // Render the exact built-in Montserrat base glyph into the caller's A8 buffer.
  if(lv_font_get_glyph_bitmap(&base_dsc, draw_buf) == nullptr) return nullptr;

  if(top_extra > 0u) {
    // Make room above the base glyph without changing its baseline position.
    for(int32_t y = (int32_t)base_dsc.box_h - 1; y >= 0; --y) {
      memmove(data + ((uint32_t)y + top_extra) * stride,
              data + (uint32_t)y * stride,
              stride);
    }
    memset(data, 0, stride * (uint32_t)top_extra);
    draw_top_accent_(data, stride, out_w, out_h, top_extra, cfg->nominal_size, accent);
  }
  else if(bottom_extra > 0u) {
    memset(data + ((uint32_t)base_dsc.box_h * stride), 0, (uint32_t)bottom_extra * stride);
    draw_cedilla_(data, stride, out_w, out_h, base_dsc.box_h, bottom_extra, cfg->nominal_size);
  }

  lv_draw_buf_flush_cache(draw_buf, nullptr);
  return draw_buf;
}

static void init_font_(lv_font_t *out, const lv_font_t *base, accent_font_config_t *cfg)
{
  if(out == nullptr || base == nullptr || cfg == nullptr) return;

  *out = *base;
  out->get_glyph_dsc = accent_get_glyph_dsc_;
  out->get_glyph_bitmap = accent_get_glyph_bitmap_;
  out->dsc = nullptr;
  out->fallback = base;
  out->user_data = cfg;
  out->static_bitmap = 0u;
}

} // namespace

lv_font_t slm_font_montserrat_18;
lv_font_t slm_font_montserrat_24;
lv_font_t slm_font_montserrat_30;
lv_font_t slm_font_montserrat_32;
lv_font_t slm_font_montserrat_34;
lv_font_t slm_font_montserrat_36;
lv_font_t slm_font_montserrat_48;

void slm_fonts_init(void)
{
  static bool initialized = false;
  if(initialized) return;

  init_font_(&slm_font_montserrat_18, &lv_font_montserrat_18, &s_cfg_18);
  init_font_(&slm_font_montserrat_24, &lv_font_montserrat_24, &s_cfg_24);
  init_font_(&slm_font_montserrat_30, &lv_font_montserrat_30, &s_cfg_30);
  init_font_(&slm_font_montserrat_32, &lv_font_montserrat_32, &s_cfg_32);
  init_font_(&slm_font_montserrat_34, &lv_font_montserrat_34, &s_cfg_34);
  init_font_(&slm_font_montserrat_36, &lv_font_montserrat_36, &s_cfg_36);
  init_font_(&slm_font_montserrat_48, &lv_font_montserrat_48, &s_cfg_48);

  initialized = true;
}
