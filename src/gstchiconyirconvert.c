/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2017 James Henstridge <<user@hostname.org>>
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

#include "gstchiconyirconvert.h"

GST_DEBUG_CATEGORY_STATIC (gst_chicony_ir_convert_debug);
#define GST_CAT_DEFAULT gst_chicony_ir_convert_debug

static GQuark colorspace_quark;

struct _GstChiconyIrConvert
{
  GstVideoFilter element;

  GstPad *sinkpad, *srcpad;

  gboolean silent;
};

struct _GstChiconyIrConvertClass
{
  GstVideoFilterClass parent_class;
};

G_DEFINE_TYPE(GstChiconyIrConvert, gst_chicony_ir_convert, GST_TYPE_VIDEO_FILTER);

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */

#define INPUT_FORMAT "YUYV2"
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

static GstCaps *
ir_convert_transform_caps (GstBaseTransform *trans,
                           GstPadDirection direction,
                           GstCaps *caps,
                           GstCaps *filter)
{
  GstCaps *result;
  int i, n;

  result = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstStructure *st = gst_caps_get_structure (caps, i);
    GstCapsFeatures *f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (result, st, f))
      continue;

    st = gst_structure_copy (st);

    switch (direction) {
    case GST_PAD_SRC:
      gst_structure_set (st, "format", G_TYPE_STRING, INPUT_FORMAT, NULL);
      break;
    case GST_PAD_SINK:
      gst_structure_set (st, "format", G_TYPE_STRING, OUTPUT_FORMAT, NULL);
      break;
    case GST_PAD_UNKNOWN:
      break;
    }

    gst_caps_append_structure_full (result, st, gst_caps_features_copy (f));
  }
  return result;
}

static GstCaps *
ir_convert_fixate_caps (GstBaseTransform *trans,
                        GstPadDirection direction,
                        GstCaps *caps,
                        GstCaps *other_caps)
{
  GstCaps *result = NULL;

  return result;
}

static gboolean
ir_convert_filter_meta (GstBaseTransform *trans,
                        GstQuery *query,
                        GType api,
                        const GstStructure *params)
{
  /* propose all metadata upstream */
  return TRUE;
}

static gboolean
ir_convert_transform_meta (GstBaseTransform *trans,
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
ir_convert_set_info (GstVideoFilter *filter,
                     GstCaps *in_caps,
                     GstVideoInfo *in_info,
                     GstCaps *out_caps,
                     GstVideoInfo *out_info)
{
}

static GstFlowReturn
ir_convert_transform_frame (GstVideoFilter *filter,
                            GstVideoFrame *in_frame,
                            GstVideoFrame *out_frame)
{
}


/* initialize the chiconyirconvert's class */
static void
gst_chicony_ir_convert_class_init (GstChiconyIrConvertClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstElementClass *gstelement_class = (GstElementClass *)klass;
  GstBaseTransformClass *basetransform_class = (GstBaseTransformClass *)klass;
  GstVideoFilterClass *videofilter_class = (GstVideoFilterClass *)klass;

  gst_element_class_set_details_simple(gstelement_class,
    "ChiconyIrConvert",
    "Filter/Video/Converter",
    "Decode video from Chicony IR camera",
    "James Henstridge <james@jamesh.id.au>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  basetransform_class->transform_caps = ir_convert_transform_caps;
  basetransform_class->fixate_caps = ir_convert_fixate_caps;
  basetransform_class->filter_meta = ir_convert_filter_meta;
  basetransform_class->transform_meta = ir_convert_transform_meta;

  videofilter_class->set_info = ir_convert_set_info;
  videofilter_class->transform_frame = ir_convert_transform_frame;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_chicony_ir_convert_init (GstChiconyIrConvert * filter)
{
}

static gboolean
plugin_init (GstPlugin * chiconyirconvert)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template chiconyirconvert' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_chicony_ir_convert_debug, "chiconyirconvert",
      0, "Template chiconyirconvert");

  colorspace_quark = g_quark_from_static_string (
    GST_META_TAG_VIDEO_COLORSPACE_STR);

  return gst_element_register (chiconyirconvert, "chiconyirconvert", GST_RANK_NONE,
      GST_TYPE_CHICONY_IR_CONVERT);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstchiconyirconvert"
#endif

/* gstreamer looks for this structure to register chiconyirconverts
 *
 * exchange the string 'Template chiconyirconvert' with your chiconyirconvert description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    chiconyirconvert,
    "Infrared convert",
    plugin_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
