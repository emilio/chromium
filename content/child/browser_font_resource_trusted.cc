// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/browser_font_resource_trusted.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/web_preferences.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebFont.h"
#include "third_party/WebKit/public/platform/WebFontDescription.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebTextRun.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/skia/include/core/SkRect.h"

using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;
using blink::WebFloatPoint;
using blink::WebFloatRect;
using blink::WebFont;
using blink::WebFontDescription;
using blink::WebRect;
using blink::WebTextRun;
using blink::WebCanvas;

namespace content {

namespace {

// Same as WebPreferences::kCommonScript. I'd use that directly here, but get an
// undefined reference linker error.
const char kCommonScript[] = "Zyyy";

base::string16 GetFontFromMap(const ScriptFontFamilyMap& map,
                              const std::string& script) {
  ScriptFontFamilyMap::const_iterator it = map.find(script);
  if (it != map.end())
    return it->second;
  return base::string16();
}

// Splits a PP_BrowserFont_Trusted_TextRun into a sequence or LTR and RTL
// WebTextRuns that can be used for WebKit. Normally WebKit does this for us,
// but the font drawing and measurement routines we call happen after this
// step. So for correct rendering of RTL content, we need to do it ourselves.
class TextRunCollection {
 public:
  explicit TextRunCollection(const PP_BrowserFont_Trusted_TextRun& run)
      : bidi_(NULL),
        num_runs_(0) {
    StringVar* text_string = StringVar::FromPPVar(run.text);
    if (!text_string)
      return;  // Leave num_runs_ = 0 so we'll do nothing.
    text_ = base::UTF8ToUTF16(text_string->value());

    if (run.override_direction) {
      // Skip autodetection.
      num_runs_ = 1;
      override_run_ = WebTextRun(blink::WebString::fromUTF16(text_),
                                 PP_ToBool(run.rtl), true);
    } else {
      bidi_ = ubidi_open();
      UErrorCode uerror = U_ZERO_ERROR;
      ubidi_setPara(bidi_, text_.data(), text_.size(), run.rtl, NULL, &uerror);
      if (U_SUCCESS(uerror))
        num_runs_ = ubidi_countRuns(bidi_, &uerror);
    }
  }

  ~TextRunCollection() {
    if (bidi_)
      ubidi_close(bidi_);
  }

  const base::string16& text() const { return text_; }
  int num_runs() const { return num_runs_; }

  // Returns a WebTextRun with the info for the run at the given index.
  // The range covered by the run is in the two output params.
  WebTextRun GetRunAt(int index, int32_t* run_start, int32_t* run_len) const {
    DCHECK(index < num_runs_);
    if (bidi_) {
      bool run_rtl = !!ubidi_getVisualRun(bidi_, index, run_start, run_len);
      return WebTextRun(blink::WebString::fromUTF16(
                            base::string16(&text_[*run_start], *run_len)),
                        run_rtl, true);
    }

    // Override run, return the single one.
    DCHECK_EQ(0, index);
    *run_start = 0;
    *run_len = static_cast<int32_t>(text_.size());
    return override_run_;
  }

 private:
  // Will be null if we skipped autodetection.
  UBiDi* bidi_;

  // Text of all the runs.
  base::string16 text_;

  int num_runs_;

  // When the content specifies override_direction (bidi_ is null) then this
  // will contain the single text run for WebKit.
  WebTextRun override_run_;

  DISALLOW_COPY_AND_ASSIGN(TextRunCollection);
};

bool PPTextRunToWebTextRun(const PP_BrowserFont_Trusted_TextRun& text,
                           WebTextRun* run) {
  StringVar* text_string = StringVar::FromPPVar(text.text);
  if (!text_string)
    return false;

  *run = WebTextRun(blink::WebString::fromUTF8(text_string->value()),
                    PP_ToBool(text.rtl), PP_ToBool(text.override_direction));
  return true;
}

// The PP_* version lacks "None", so is just one value shifted from the
// WebFontDescription version. These values are checked in
// PPFontDescToWebFontDesc to make sure the conversion is correct. This is a
// macro so it can also be used in the static_asserts.
#define PP_FAMILY_TO_WEB_FAMILY(f) \
  static_cast<WebFontDescription::GenericFamily>(f + 1)

// Assumes the given PP_FontDescription has been validated.
WebFontDescription PPFontDescToWebFontDesc(
    const PP_BrowserFont_Trusted_Description& font,
    const ppapi::Preferences& prefs) {
  // Verify that the enums match so we can just static cast.
  static_assert(static_cast<int>(WebFontDescription::Weight100) ==
                static_cast<int>(PP_BROWSERFONT_TRUSTED_WEIGHT_100),
                "font Weight100");
  static_assert(static_cast<int>(WebFontDescription::Weight900) ==
                static_cast<int>(PP_BROWSERFONT_TRUSTED_WEIGHT_900),
                "font Weight900");
  static_assert(
      WebFontDescription::GenericFamilyStandard ==
      PP_FAMILY_TO_WEB_FAMILY(PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT),
      "FamilyStandard");
  static_assert(
      WebFontDescription::GenericFamilySerif ==
      PP_FAMILY_TO_WEB_FAMILY(PP_BROWSERFONT_TRUSTED_FAMILY_SERIF),
      "FamilySerif");
  static_assert(
      WebFontDescription::GenericFamilySansSerif ==
      PP_FAMILY_TO_WEB_FAMILY(PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF),
      "FamilySansSerif");
  static_assert(
      WebFontDescription::GenericFamilyMonospace ==
      PP_FAMILY_TO_WEB_FAMILY(PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE),
      "FamilyMonospace");

  StringVar* face_name = StringVar::FromPPVar(font.face);  // Possibly null.

  WebFontDescription result;
  base::string16 resolved_family;
  if (!face_name || face_name->value().empty()) {
    // Resolve the generic family.
    switch (font.family) {
      case PP_BROWSERFONT_TRUSTED_FAMILY_SERIF:
        resolved_family = GetFontFromMap(prefs.serif_font_family_map,
                                         kCommonScript);
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF:
        resolved_family = GetFontFromMap(prefs.sans_serif_font_family_map,
                                         kCommonScript);
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE:
        resolved_family = GetFontFromMap(prefs.fixed_font_family_map,
                                         kCommonScript);
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT:
      default:
        resolved_family = GetFontFromMap(prefs.standard_font_family_map,
                                         kCommonScript);
        break;
    }
  } else {
    // Use the exact font.
    resolved_family = base::UTF8ToUTF16(face_name->value());
  }
  result.family = blink::WebString::fromUTF16(resolved_family);

  result.genericFamily = PP_FAMILY_TO_WEB_FAMILY(font.family);

  if (font.size == 0) {
    // Resolve the default font size, using the resolved family to see if
    // we should use the fixed or regular font size. It's difficult at this
    // level to detect if the requested font is fixed width, so we only apply
    // the alternate font size to the default fixed font family.
    if (base::ToLowerASCII(resolved_family) ==
        base::ToLowerASCII(GetFontFromMap(prefs.fixed_font_family_map,
                                          kCommonScript)))
      result.size = static_cast<float>(prefs.default_fixed_font_size);
    else
      result.size = static_cast<float>(prefs.default_font_size);
  } else {
    // Use the exact size.
    result.size = static_cast<float>(font.size);
  }

  result.italic = font.italic != PP_FALSE;
  result.smallCaps = font.small_caps != PP_FALSE;
  result.weight = static_cast<WebFontDescription::Weight>(font.weight);
  result.letterSpacing = static_cast<short>(font.letter_spacing);
  result.wordSpacing = static_cast<short>(font.word_spacing);
  return result;
}

}  // namespace

// static
bool BrowserFontResource_Trusted::IsPPFontDescriptionValid(
    const PP_BrowserFont_Trusted_Description& desc) {
  // Check validity of string. We can't check the actual text since we could
  // be on the wrong thread and don't know if we're in the plugin or the host.
  if (desc.face.type != PP_VARTYPE_STRING &&
      desc.face.type != PP_VARTYPE_UNDEFINED)
    return false;

  // Check enum ranges.
  if (static_cast<int>(desc.family) < PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT ||
      static_cast<int>(desc.family) > PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE)
    return false;
  if (static_cast<int>(desc.weight) < PP_BROWSERFONT_TRUSTED_WEIGHT_100 ||
      static_cast<int>(desc.weight) > PP_BROWSERFONT_TRUSTED_WEIGHT_900)
    return false;

  // Check for excessive sizes which may cause layout to get confused.
  if (desc.size > 200)
    return false;

  return true;
}

BrowserFontResource_Trusted::BrowserFontResource_Trusted(
    ppapi::proxy::Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const ppapi::Preferences& prefs)
    : PluginResource(connection, instance),
      font_(WebFont::create(PPFontDescToWebFontDesc(desc, prefs))) {
}

BrowserFontResource_Trusted::~BrowserFontResource_Trusted() {
}

ppapi::thunk::PPB_BrowserFont_Trusted_API*
BrowserFontResource_Trusted::AsPPB_BrowserFont_Trusted_API() {
  return this;
}

PP_Bool BrowserFontResource_Trusted::Describe(
    PP_BrowserFont_Trusted_Description* description,
    PP_BrowserFont_Trusted_Metrics* metrics) {
  if (description->face.type != PP_VARTYPE_UNDEFINED)
    return PP_FALSE;

  // While converting the other way in PPFontDescToWebFontDesc we validated
  // that the enums can be casted.
  WebFontDescription web_desc = font_->getFontDescription();
  description->face = StringVar::StringToPPVar(web_desc.family.utf8());
  description->family =
      static_cast<PP_BrowserFont_Trusted_Family>(web_desc.genericFamily);
  description->size = static_cast<uint32_t>(web_desc.size);
  description->weight = static_cast<PP_BrowserFont_Trusted_Weight>(
      web_desc.weight);
  description->italic = web_desc.italic ? PP_TRUE : PP_FALSE;
  description->small_caps = web_desc.smallCaps ? PP_TRUE : PP_FALSE;
  description->letter_spacing = static_cast<int32_t>(web_desc.letterSpacing);
  description->word_spacing = static_cast<int32_t>(web_desc.wordSpacing);

  metrics->height = font_->height();
  metrics->ascent = font_->ascent();
  metrics->descent = font_->descent();
  metrics->line_spacing = font_->lineSpacing();
  metrics->x_height = static_cast<int32_t>(font_->xHeight());

  // Convert the string.
  return PP_TRUE;
}

PP_Bool BrowserFontResource_Trusted::DrawTextAt(
    PP_Resource image_data,
    const PP_BrowserFont_Trusted_TextRun* text,
    const PP_Point* position,
    uint32_t color,
    const PP_Rect* clip,
    PP_Bool image_data_is_opaque) {
  PP_Bool result = PP_FALSE;
  // Get and map the image data we're painting to.
  EnterResourceNoLock<PPB_ImageData_API> enter(image_data, true);
  if (enter.failed())
    return result;

  PPB_ImageData_API* image = static_cast<PPB_ImageData_API*>(
      enter.object());
  SkCanvas* canvas = image->GetCanvas();
  bool needs_unmapping = false;
  if (!canvas) {
    needs_unmapping = true;
    image->Map();
    canvas = image->GetCanvas();
    if (!canvas)
      return result;  // Failure mapping.
  }

  if (!PP_ToBool(image_data_is_opaque)) {
    // Ideally, LCD text should be configured at canvas creation time using
    // SkSurfaceProps. But because the API exposes image_data_is_opaque per
    // draw text call (allowing clients to essentially change their mind),
    // we have to handle it here.
    SkImageInfo info;
    size_t row_bytes;
    void* pixels = canvas->accessTopLayerPixels(&info, &row_bytes);
    if (!pixels)
      return result;

    SkBitmap bm;
    if (!bm.installPixels(info, pixels, row_bytes))
      return result;

    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
    cc::PaintCanvas temp_canvas(bm, props);

    DrawTextToCanvas(&temp_canvas, *text, position, color, clip);
  } else {
    cc::PaintCanvas temp_canvas(canvas);
    DrawTextToCanvas(&temp_canvas, *text, position, color, clip);
  }

  if (needs_unmapping)
    image->Unmap();
  return PP_TRUE;
}

int32_t BrowserFontResource_Trusted::MeasureText(
    const PP_BrowserFont_Trusted_TextRun* text) {
  WebTextRun run;
  if (!PPTextRunToWebTextRun(*text, &run))
    return -1;
  return font_->calculateWidth(run);
}

uint32_t BrowserFontResource_Trusted::CharacterOffsetForPixel(
    const PP_BrowserFont_Trusted_TextRun* text,
    int32_t pixel_position) {
  TextRunCollection runs(*text);
  int32_t cur_pixel_offset = 0;
  for (int i = 0; i < runs.num_runs(); i++) {
    int32_t run_begin = 0;
    int32_t run_len = 0;
    WebTextRun run = runs.GetRunAt(i, &run_begin, &run_len);
    int run_width = font_->calculateWidth(run);
    if (pixel_position < cur_pixel_offset + run_width) {
      // Offset is in this run.
      return static_cast<uint32_t>(font_->offsetForPosition(
              run, static_cast<float>(pixel_position - cur_pixel_offset))) +
          run_begin;
    }
    cur_pixel_offset += run_width;
  }
  return runs.text().size();
}

int32_t BrowserFontResource_Trusted::PixelOffsetForCharacter(
    const PP_BrowserFont_Trusted_TextRun* text,
    uint32_t char_offset) {
  TextRunCollection runs(*text);
  int32_t cur_pixel_offset = 0;
  for (int i = 0; i < runs.num_runs(); i++) {
    int32_t run_begin = 0;
    int32_t run_len = 0;
    WebTextRun run = runs.GetRunAt(i, &run_begin, &run_len);
    if (char_offset >= static_cast<uint32_t>(run_begin) &&
        char_offset < static_cast<uint32_t>(run_begin + run_len)) {
      // Character we're looking for is in this run.
      //
      // Here we ask WebKit to give us the rectangle around the character in
      // question, and then return the left edge. If we asked for a range of
      // 0 characters starting at the character in question, it would give us
      // a 0-width rect around the insertion point. But that will be on the
      // right side of the character for an RTL run, which would be wrong.
      WebFloatRect rect = font_->selectionRectForText(
          run, WebFloatPoint(0.0f, 0.0f), font_->height(),
          char_offset - run_begin, char_offset - run_begin + 1);
      return cur_pixel_offset + static_cast<int>(rect.x);
    } else {
      // Character is past this run, account for the pixels and continue
      // looking.
      cur_pixel_offset += font_->calculateWidth(run);
    }
  }
  return -1;  // Requested a char beyond the end.
}

void BrowserFontResource_Trusted::DrawTextToCanvas(
    cc::PaintCanvas* destination,
    const PP_BrowserFont_Trusted_TextRun& text,
    const PP_Point* position,
    uint32_t color,
    const PP_Rect* clip) {
  // Convert position and clip.
  WebFloatPoint web_position(static_cast<float>(position->x),
                             static_cast<float>(position->y));
  WebRect web_clip;
  if (!clip) {
    // Use entire canvas. PaintCanvas doesn't have a size on it, so we just use
    // the current clip bounds.
    SkRect skclip = destination->getLocalClipBounds();
    web_clip = WebRect(skclip.fLeft, skclip.fTop, skclip.fRight - skclip.fLeft,
                       skclip.fBottom - skclip.fTop);
  } else {
    web_clip = WebRect(clip->point.x, clip->point.y,
                       clip->size.width, clip->size.height);
  }

  TextRunCollection runs(text);
  for (int i = 0; i < runs.num_runs(); i++) {
    int32_t run_begin = 0;
    int32_t run_len = 0;
    WebTextRun run = runs.GetRunAt(i, &run_begin, &run_len);
    font_->drawText(destination, run, web_position, color, web_clip);

    // Advance to the next run. Note that we avoid doing this for the last run
    // since it's unnecessary, measuring text is slow, and most of the time
    // there will be only one run anyway.
    if (i != runs.num_runs() - 1)
      web_position.x += font_->calculateWidth(run);
  }
}

}  // namespace content
