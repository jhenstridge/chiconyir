/*
 * GStreamer
 * Copyright (C) 2017 James Henstridge <james@jamesh.id.au>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-chiconyirconvert
 *
 * FIXME:Describe chiconyirconvert here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! chiconyirconvert ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstchiconyirdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_chicony_ir_dec_debug);
#define GST_CAT_DEFAULT gst_chicony_ir_dec_debug

static GQuark colorspace_quark;

struct _GstChiconyIrDec
{
  GstVideoFilter element;

  gboolean auto_gain;
};

struct _GstChiconyIrDecClass
{
  GstVideoFilterClass parent_class;
};

G_DEFINE_TYPE(GstChiconyIrDec, gst_chicony_ir_dec, GST_TYPE_VIDEO_FILTER);

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */

#define INPUT_FORMAT "YUY2"
#define OUTPUT_FORMAT "GRAY16_LE"

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (INPUT_FORMAT))
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (OUTPUT_FORMAT))
    );

static int
transform_width (GstPadDirection direction, int width)
{
  gint64 new_val = width;

  if (direction == GST_PAD_SRC) {
    new_val = new_val / 8 * 5;
  } else {
    new_val = new_val / 5 * 8;
  }
  return (int) CLAMP(new_val, 1, G_MAXINT);
}

static gboolean
transform_width_value (GstPadDirection direction,
                       const GValue *src_val,
                       GValue *dest_val)
{
  gboolean ret = TRUE;

  g_value_init (dest_val, G_VALUE_TYPE (src_val));
  if (G_VALUE_HOLDS_INT (src_val)) {
    gint ival = g_value_get_int (src_val);

    ival = transform_width (direction, ival);
    g_value_set_int (dest_val, ival);
  } else if (GST_VALUE_HOLDS_INT_RANGE (src_val)) {
    gint min = gst_value_get_int_range_min (src_val);
    gint max = gst_value_get_int_range_max (src_val);

    min = transform_width (direction, min);
    max = transform_width (direction, max);
    if (min >= max) {
      ret = FALSE;
      g_value_unset (dest_val);
    } else {
      gst_value_set_int_range (dest_val, min, max);
    }
  } else if (GST_VALUE_HOLDS_LIST (src_val)) {
    gint i;

    for (i = 0; i < gst_value_list_get_size (src_val); ++i) {
      const GValue *list_val;
      GValue newval = { 0, };

      list_val = gst_value_list_get_value (src_val, i);
      if (transform_width_value (direction, list_val, &newval))
        gst_value_list_append_value (dest_val, &newval);
      g_value_unset (&newval);
    }

    if (gst_value_list_get_size (dest_val) == 0) {
      g_value_unset (dest_val);
      ret = FALSE;
    }
  } else {
    g_value_unset (dest_val);
    ret = FALSE;
  }

  return ret;
}

static GstCaps *
ir_dec_transform_caps (GstBaseTransform *trans,
                       GstPadDirection direction,
                       GstCaps *caps,
                       GstCaps *filter)
{
  GstCaps *to, *result;
  GstCaps *templ;
  GstPad *other;
  int i, n;

  to = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstStructure *st = gst_caps_get_structure (caps, i);
    GstCapsFeatures *f = gst_caps_get_features (caps, i);
    const GValue *old_width;
    GValue new_width = { 0, };

    st = gst_structure_copy (st);

    if (direction == GST_PAD_SRC) {
      gst_structure_set (st, "format", G_TYPE_STRING, INPUT_FORMAT, NULL);
    } else {
      gst_structure_set (st, "format", G_TYPE_STRING, OUTPUT_FORMAT, NULL);
    }

    old_width = gst_structure_get_value (st, "width");
    if (old_width) {
      if (!transform_width_value (direction, old_width, &new_width)) {
        GST_WARNING_OBJECT (trans, "could not transform width");
        goto bail;
      }
      gst_structure_set_value (st, "width", &new_width);
      g_value_unset (&new_width);
    }

    gst_structure_remove_field (st, "colorimetry");
    gst_structure_remove_field (st, "chroma-site");

    gst_caps_append_structure_full (to, st, gst_caps_features_copy (f));
  }

  /* filter against set allowed caps on the pad */
  other = (direction == GST_PAD_SINK) ? trans->srcpad : trans->sinkpad;
  templ = gst_pad_get_pad_template_caps (other);
  result = gst_caps_intersect (to, templ);
  gst_caps_unref (to);
  gst_caps_unref (templ);

  if (result && filter) {
    GstCaps *intersection;

    GST_DEBUG_OBJECT (trans, "Using filter caps %" GST_PTR_FORMAT, filter);
    intersection =
        gst_caps_intersect_full (filter, result, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (result);
    result = intersection;
    GST_DEBUG_OBJECT (trans, "Intersection %" GST_PTR_FORMAT, result);
  }

  return result;

bail:
  gst_caps_unref (to);
  to = gst_caps_new_empty ();
  return to;
}

static GstCaps *
ir_dec_fixate_caps (GstBaseTransform *trans,
                    GstPadDirection direction,
                    GstCaps *caps,
                    GstCaps *other_caps)
{
  GstCaps *result = NULL;
  GstStructure *ins, *outs;
  int width = 0;

  result = gst_caps_intersect (other_caps, caps);
  if (gst_caps_is_empty (result)) {
    gst_caps_unref (result);
    result = other_caps;
  } else {
    gst_caps_unref (other_caps);
  }
  result = gst_caps_make_writable (result);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (result, 0);

  if (direction == GST_PAD_SRC) {
    gst_structure_set (outs, "format", G_TYPE_STRING, INPUT_FORMAT, NULL);
  } else {
    gst_structure_set (outs, "format", G_TYPE_STRING, OUTPUT_FORMAT, NULL);
  }

  gst_structure_get_int (ins, "width", &width);
  gst_structure_set (outs,
                     "width", G_TYPE_INT, transform_width (direction, width),
                     NULL);

  return result;
}

static gboolean
ir_dec_filter_meta (GstBaseTransform *trans,
                    GstQuery *query,
                    GType api,
                    const GstStructure *params)
{
  /* propose all metadata upstream */
  return TRUE;
}

static gboolean
ir_dec_transform_meta (GstBaseTransform *trans,
                       GstBuffer *out_buf,
                       GstMeta *meta,
                       GstBuffer *in_buf)
{
  const GstMetaInfo *info = meta->info;

  if (gst_meta_api_type_has_tag (info->api, colorspace_quark)) {
    /* don't copy colorspace specific metadata. */
    return FALSE;
  }
  return TRUE;
}

static gboolean
ir_dec_set_info (GstVideoFilter *filter,
                 GstCaps *in_caps,
                 GstVideoInfo *in_info,
                 GstCaps *out_caps,
                 GstVideoInfo *out_info)
{
  return TRUE;
}

static GstFlowReturn
ir_dec_transform_frame (GstVideoFilter *filter,
                        GstVideoFrame *src_frame,
                        GstVideoFrame *dest_frame)
{
  GstChiconyIrDec *self = GST_CHICONY_IR_DEC (filter);
  int i, j;
  int width, height;
  int src_stride, dest_stride;
  const guint8 *src;
  guint8 *dest;
  guint16 max = 1;

  width = GST_VIDEO_FRAME_WIDTH (src_frame);
  height = GST_VIDEO_FRAME_HEIGHT (src_frame);
  src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (src_frame, 0);
  dest_stride = GST_VIDEO_FRAME_PLANE_STRIDE (dest_frame, 0);

  src = GST_VIDEO_FRAME_PLANE_DATA (src_frame, 0);
  dest = GST_VIDEO_FRAME_PLANE_DATA (dest_frame, 0);

  // Calculate maximum pixel value in frame if in auto gain mode
  if (self->auto_gain) {
    for (j = 0; j < height; j++) {
      for (i = 0; i < width*2; i += 5) {
        guint16 p1 = src[i] | ((guint)(src[i+1] & 0x03) << 8);
        guint16 p2 = (src[i+1] >> 2) | ((guint)(src[i+2] & 0x0f) << 6);
        guint16 p3 = (src[i+2] >> 4) | ((guint)(src[i+3] & 0x3f) << 4);
        guint16 p4 = (src[i+3] >> 6) | ((guint)src[i+4] << 2);

        max = MAX(max, p1);
        max = MAX(max, p2);
        max = MAX(max, p3);
        max = MAX(max, p4);
      }
      src += src_stride;
    }
    src = GST_VIDEO_FRAME_PLANE_DATA (src_frame, 0);
  }

  for (j = 0; j < height; j++) {
    int x = 0;
    for (i = 0; i < width*2; i += 5) {
      guint16 p1 = src[i] | ((guint)(src[i+1] & 0x03) << 8);
      guint16 p2 = (src[i+1] >> 2) | ((guint)(src[i+2] & 0x0f) << 6);
      guint16 p3 = (src[i+2] >> 4) | ((guint)(src[i+3] & 0x3f) << 4);
      guint16 p4 = (src[i+3] >> 6) | ((guint)src[i+4] << 2);

      if (self->auto_gain) {
        p1 = (float)p1 / max * 0x3ff;
        p2 = (float)p2 / max * 0x3ff;
        p3 = (float)p3 / max * 0x3ff;
        p4 = (float)p4 / max * 0x3ff;
      }

      dest[x++] = (guint8)((p1 & 0x03) << 6 | (p1 >> 4));
      dest[x++] = (guint8)(p1 >> 2);
      dest[x++] = (guint8)((p2 & 0x03) << 6 | (p2 >> 4));
      dest[x++] = (guint8)(p2 >> 2);
      dest[x++] = (guint8)((p3 & 0x03) << 6 | (p3 >> 4));
      dest[x++] = (guint8)(p3 >> 2);
      dest[x++] = (guint8)((p4 & 0x03) << 6 | (p4 >> 4));
      dest[x++] = (guint8)(p4 >> 2);
    }
    src += src_stride;
    dest += dest_stride;
  }
  return GST_FLOW_OK;
}

enum
{
  PROP_0,
  PROP_AUTO_GAIN,
};

static void
ir_dec_get_property (GObject *object,
                     guint prop_id,
                     GValue *value,
                     GParamSpec *pspec)
{
  GstChiconyIrDec *self = GST_CHICONY_IR_DEC (object);
  switch (prop_id) {
  case PROP_AUTO_GAIN:
    g_value_set_boolean (value, self->auto_gain);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ir_dec_set_property (GObject *object,
                     guint prop_id,
                     const GValue *value,
                     GParamSpec *pspec)
{
  GstChiconyIrDec *self = GST_CHICONY_IR_DEC (object);
  switch (prop_id) {
  case PROP_AUTO_GAIN:
    self->auto_gain = g_value_get_boolean (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gst_chicony_ir_dec_class_init (GstChiconyIrDecClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstElementClass *gstelement_class = (GstElementClass *)klass;
  GstBaseTransformClass *basetransform_class = (GstBaseTransformClass *)klass;
  GstVideoFilterClass *videofilter_class = (GstVideoFilterClass *)klass;

  gobject_class->get_property = ir_dec_get_property;
  gobject_class->set_property = ir_dec_set_property;

  basetransform_class->transform_caps = ir_dec_transform_caps;
  basetransform_class->fixate_caps = ir_dec_fixate_caps;
  basetransform_class->filter_meta = ir_dec_filter_meta;
  basetransform_class->transform_meta = ir_dec_transform_meta;

  videofilter_class->set_info = ir_dec_set_info;
  videofilter_class->transform_frame = ir_dec_transform_frame;

  g_object_class_install_property (gobject_class, PROP_AUTO_GAIN,
      g_param_spec_boolean ("auto-gain", "Auto Gain",
          "Automatically increase gain", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple(gstelement_class,
    "ChiconyIrDec",
    "Filter/Video/Converter",
    "Decode video from Chicony IR camera",
    "James Henstridge <james@jamesh.id.au>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_chicony_ir_dec_init (GstChiconyIrDec * filter)
{
}

static gboolean
plugin_init (GstPlugin * chiconyirdec)
{
  GST_DEBUG_CATEGORY_INIT (gst_chicony_ir_dec_debug, "chiconyirdec",
      0, "Chicony IR decoder");

  colorspace_quark = g_quark_from_static_string (
    GST_META_TAG_VIDEO_COLORSPACE_STR);

  return gst_element_register (chiconyirdec, "chiconyirdec", GST_RANK_NONE,
      GST_TYPE_CHICONY_IR_DEC);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    chiconyirdec,
    "Infrared decoder",
    plugin_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
