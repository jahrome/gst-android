/*
 * =====================================================================================
 *
 *       Filename:  GstMetadataRetrieverDriver.cpp
 *
 *	Copyright ST-Ericsson 2009
 *	Copyright 2011 Reynaldo H. Verdejo Pinochet <reynaldo@collabora.co.uk>
 * =====================================================================================
 */
/* 
Copyright (c) 2010, ST-Ericsson SA
All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
- Neither the name of the ST-Ericsson nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "GstMetadataRetrieverDriver"

#include "GstMetadataRetrieverDriver.h"
#include <gst/video/video.h>
#include <fcntl.h>

#include <utils/Log.h>
#include <cutils/properties.h>

#define UNUSED(x) (void)x

using namespace android;

static GstStaticCaps static_audio_caps =
    GST_STATIC_CAPS
    ("audio/mpeg,framed=true;audio/mpeg,parsed=true;audio/AMR;audio/AMR-WB;audio/x-wma;audio/midi;audio/mobile-xmf");
static GstStaticCaps static_video_caps =
    GST_STATIC_CAPS
    ("video/mpeg;video/x-h263;video/x-h264;video/x-divx;video/x-wmv");

static GstStaticCaps static_have_video_caps =
    GST_STATIC_CAPS ("video/x-raw-yuv;video/x-raw-rgb");


GstMetadataRetrieverDriver::GstMetadataRetrieverDriver ():
mPipeline (NULL),
mAppsrc (NULL),
mColorTransform (NULL),
mScaler (NULL),
mUriDecodeBin (NULL),
mAppSink (NULL),
mAudioSink (NULL),
mUri (NULL),
mTag_list (NULL),
mFdSrcOffset_min (0), mFdSrcOffset_max (0),
mFdSrcOffset_current (0), mFd (-1), mState (GST_STATE_NULL), mAlbumArt (NULL)
{
  LOGV ("constructor");

  mHaveStreamVideo = false;

  init_gstreamer ();
}

GstMetadataRetrieverDriver::~GstMetadataRetrieverDriver ()
{
  LOGV ("destructor");

  if (mTag_list) {
    LOGV ("free tag list");
    gst_tag_list_free (mTag_list);
    mTag_list = NULL;
  }

  if (mAlbumArt) {
    gst_buffer_unref (mAlbumArt);
    mAlbumArt = NULL;
  }

  if (mPipeline) {
    LOGV ("free pipeline %s", GST_ELEMENT_NAME (mPipeline));
    gst_element_set_state (mPipeline, GST_STATE_NULL);
    gst_object_unref (mPipeline);
    mPipeline = NULL;
  }

  if (mFd != -1) {
    close (mFd);
  }

  if (mUri) {
    g_free (mUri);
  }
}

void
GstMetadataRetrieverDriver::cb_newpad (GstElement * mUriDecodeBin, GstPad * pad,
    GstMetadataRetrieverDriver * data)
{
  GstCaps *caps;
  GstStructure *str;
  gboolean err = true;

  caps = gst_pad_get_caps (pad);
  str = gst_caps_get_structure (caps, 0);
  if (g_strrstr (gst_structure_get_name (str), "audio")) {
    LOGI ("Got an audio pad");
    err = gst_element_link (data->mUriDecodeBin, data->mAudioSink);
  } else if (g_strrstr (gst_structure_get_name (str), "video")) {
    LOGI ("Got a video pad");
    err = gst_element_link (data->mUriDecodeBin, data->mColorTransform);
  } else {
    LOGW ("Got a pad we don't know how to handle");
    return;
  }

  if (!err)
    LOGE ("Could not link %s with %s", GST_ELEMENT_NAME (data->mUriDecodeBin),
          GST_ELEMENT_NAME (data->mAudioSink ?
          data->mAudioSink : data->mColorTransform));

  gst_caps_unref (caps);

  return;
}

void
GstMetadataRetrieverDriver::setup (int mode)
{
  gchar *description = NULL;
  GError *error = NULL;
  GstCaps *caps_filter = NULL;
  mMode = mode;


  LOGI ("Called For URI:%s", mUri);
  if (mMode & METADATA_MODE_FRAME_CAPTURE_ONLY) {
    LOGI ("Called in METADATA_MODE_FRAME_CAPTURE_ONLY mode");

    mPipeline       = gst_pipeline_new ("pipeline");
    mColorTransform = gst_element_factory_make ("ffmpegcolorspace", NULL);
    mScaler         = gst_element_factory_make ("videoscale", NULL);
    mUriDecodeBin   = gst_element_factory_make ("uridecodebin", "src");
    mAppSink        = gst_element_factory_make ("appsink", "sink");
    mAudioSink      = gst_element_factory_make ("fakesink", NULL);

    g_object_set (G_OBJECT (mUriDecodeBin), "uri", mUri, NULL);
    g_object_set (G_OBJECT (mAppSink), "enable-last-buffer", "true", NULL);

    gst_bin_add_many (GST_BIN (mPipeline), mUriDecodeBin, mColorTransform,
                      mAudioSink, mScaler, mAppSink, NULL);

    caps_filter = gst_caps_new_simple ("video/x-raw-rgb", "bpp", G_TYPE_INT, 16,
                                       NULL);

    if (!gst_element_link_filtered (mColorTransform, mScaler, caps_filter))
      LOGE ("Failed to link %s to %s", GST_ELEMENT_NAME (mColorTransform),
            GST_ELEMENT_NAME (mScaler));

    gst_caps_unref (caps_filter);

    if (!gst_element_link (mScaler, mAppSink))
      LOGE ("Failed to link %s to %s", GST_ELEMENT_NAME (mScaler),
            GST_ELEMENT_NAME (mAppSink));

    g_signal_connect (mUriDecodeBin, "pad-added", G_CALLBACK (cb_newpad), this);
  } else {
    LOGI ("Called in mode %d", mMode);
    description =
        g_strdup_printf ("uridecodebin uri=%s name=src ! fakesink name=sink",
                         mUri);
    mPipeline = gst_parse_launch (description, &error);
  }

  if (!mPipeline) {
    LOGE ("can't create pipeline");
    return;
  }
  LOGV ("pipeline creation: %s", GST_ELEMENT_NAME (mPipeline));

  // verbose info (as gst-launch -v)
  // Activate the trace with the command: "setprop persist.gst.verbose 1"
  char value[PROPERTY_VALUE_MAX];
  property_get ("persist.gst.verbose", value, "0");
  LOGV ("persist.gst.verbose property = %s", value);
  if (value[0] == '1') {
    LOGV ("Activate deep_notify");
    g_signal_connect (mPipeline, "deep_notify",
        G_CALLBACK (gst_object_default_deep_notify), NULL);
  }

  mState = GST_STATE_NULL;
}


void
GstMetadataRetrieverDriver::setDataSource (const char *url)
{
  LOGI ("create source from uri %s", url);

  if (!gst_uri_is_valid (url)) {
    gchar *uri_file = g_filename_to_uri (url, NULL, NULL);
    mUri = g_strdup (uri_file);
    g_free (uri_file);
  } else {
    mUri = g_strdup (url);
  }

  LOGV ("set uri %s to src", mUri);
}

/*static*/ gboolean
GstMetadataRetrieverDriver::have_video_caps (GstElement * uridecodebin,
    GstCaps * caps)
{
  GstCaps *video_caps;
  gboolean res;

  video_caps = gst_static_caps_get (&static_have_video_caps);
  GST_OBJECT_LOCK (uridecodebin);
  res = gst_caps_can_intersect (caps, video_caps);
  GST_OBJECT_UNLOCK (uridecodebin);

  gst_caps_unref (video_caps);
  return res;
}

/*static*/ gboolean
GstMetadataRetrieverDriver::are_audio_caps (GstElement * uridecodebin,
    GstCaps * caps)
{
  GstCaps *end_caps;
  gboolean res;

  end_caps = gst_static_caps_get (&static_audio_caps);
  GST_OBJECT_LOCK (uridecodebin);
  res = gst_caps_can_intersect (caps, end_caps);
  GST_OBJECT_UNLOCK (uridecodebin);

  gst_caps_unref (end_caps);
  return res;
}

/* static*/
gboolean
GstMetadataRetrieverDriver::are_video_caps (GstElement * uridecodebin,
    GstCaps * caps)
{
  GstCaps *end_caps;
  gboolean res;

  end_caps = gst_static_caps_get (&static_video_caps);
  GST_OBJECT_LOCK (uridecodebin);
  res = gst_caps_can_intersect (caps, end_caps);
  GST_OBJECT_UNLOCK (uridecodebin);

  gst_caps_unref (end_caps);
  return res;
}


/* return TRUE if we continue to build the graph, FALSE either */
/*static */
gboolean
GstMetadataRetrieverDriver::autoplug_continue (GstElement * object,
    GstPad * pad, GstCaps * caps, GstMetadataRetrieverDriver * ed)
{
  GstStructure *structure = NULL;
  structure = gst_caps_get_structure (caps, 0);
  gboolean res;

  UNUSED (pad);

  //LOGV("autoplug_continue %s" ,gst_structure_get_name(structure));
  if (are_video_caps (object, caps)) {
    //LOGV("\nfound video caps %" GST_PTR_FORMAT, caps);
    ed->mHaveStreamVideo = TRUE;
  }

  res = are_audio_caps (object, caps);

  if (res && (ed->mMode & METADATA_MODE_METADATA_RETRIEVAL_ONLY)) {
    res &= are_video_caps (object, caps);
  }

  return res;
}

/*static*/ void
GstMetadataRetrieverDriver::need_data (GstElement * object, guint size,
    GstMetadataRetrieverDriver * ed)
{
  GstFlowReturn ret;
  GstBuffer *buffer;
  UNUSED (object);

  if (ed->mFdSrcOffset_current >= ed->mFdSrcOffset_max) {
    LOGV ("appsrc send eos");
    g_signal_emit_by_name (ed->mAppsrc, "end-of-stream", &ret);
    return;
  }

  if ((ed->mFdSrcOffset_current + size) > ed->mFdSrcOffset_max) {
    size = ed->mFdSrcOffset_max - ed->mFdSrcOffset_current;
  }

  buffer = gst_buffer_new_and_alloc (size);

  if (buffer == NULL) {
    LOGV ("appsrc can't allocate buffer of size %d", size);
    return;
  }
  size = read (ed->mFd, GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

  GST_BUFFER_SIZE (buffer) = size;
  /* we need to set an offset for random access */
  GST_BUFFER_OFFSET (buffer) = ed->mFdSrcOffset_current - ed->mFdSrcOffset_min;
  ed->mFdSrcOffset_current += size;
  GST_BUFFER_OFFSET_END (buffer) =
      ed->mFdSrcOffset_current - ed->mFdSrcOffset_min;

  g_signal_emit_by_name (ed->mAppsrc, "push-buffer", buffer, &ret);
  gst_buffer_unref (buffer);
}

/*static*/ gboolean
GstMetadataRetrieverDriver::seek_data (GstElement * object, guint64 offset,
    GstMetadataRetrieverDriver * ed)
{
  UNUSED (object);

  if ((ed->mFdSrcOffset_min + offset) <= ed->mFdSrcOffset_max) {
    lseek (ed->mFd, ed->mFdSrcOffset_min + offset, SEEK_SET);
    ed->mFdSrcOffset_current = ed->mFdSrcOffset_min + offset;
  }

  return TRUE;
}

/*static*/ void
GstMetadataRetrieverDriver::source_changed_cb (GObject * obj,
    GParamSpec * pspec, GstMetadataRetrieverDriver * ed)
{
  UNUSED (pspec);

  // get the newly created source element
  g_object_get (obj, "source", &(ed->mAppsrc), (gchar *) NULL);

  if (ed->mAppsrc != NULL) {
    lseek (ed->mFd, ed->mFdSrcOffset_min, SEEK_SET);

    g_object_set (ed->mAppsrc, "format", GST_FORMAT_BYTES, NULL);
    g_object_set (ed->mAppsrc, "stream-type", 2 /*"random-access" */ , NULL);
    g_object_set (ed->mAppsrc, "size",
                 (gint64) (ed->mFdSrcOffset_max - ed->mFdSrcOffset_min), NULL);
    g_signal_connect (ed->mAppsrc, "need-data", G_CALLBACK (need_data), ed);
    g_signal_connect (ed->mAppsrc, "seek-data", G_CALLBACK (seek_data), ed);
  }
}


void
GstMetadataRetrieverDriver::setFdDataSource (int fd, gint64 offset,
    gint64 length)
{
  LOGI ("create source from fd %d offset %lld lenght %lld", fd, offset, length);

  /* duplicate the fd because it should be closed in java layers
   * before we can use it */
  mFd = dup (fd);
  LOGV ("dup(fd) old %d new %d", fd, mFd);
  // create the uri string with the new fd
  mUri = g_strdup_printf ("appsrc://");
  mFdSrcOffset_min = offset;
  mFdSrcOffset_current = mFdSrcOffset_min;
  mFdSrcOffset_max = mFdSrcOffset_min + length;
}

void
GstMetadataRetrieverDriver::getVideoSize (int *width, int *height)
{
  *width = 0;
  *height = 0;

  if (mHaveStreamVideo) {
    GstElement *sink = gst_bin_get_by_name (GST_BIN (mPipeline), "sink");
    if (GstPad * pad = gst_element_get_static_pad (sink, "sink")) {
      gst_video_get_size (GST_PAD (pad), width, height);
      gst_object_unref (GST_OBJECT (pad));
    }
    gst_object_unref (GST_OBJECT (sink));
    LOGV ("video width %d height %d", *width, *height);
  }
}


void
GstMetadataRetrieverDriver::getFrameRate (int *framerate)
{
  *framerate = 0;

  if (mHaveStreamVideo) {
    const GValue *fps = NULL;
    GstElement *sink = gst_bin_get_by_name (GST_BIN (mPipeline), "sink");
    if (GstPad * pad = gst_element_get_static_pad (sink, "sink")) {
      fps = gst_video_frame_rate (GST_PAD (pad));
      if (fps != NULL && GST_VALUE_HOLDS_FRACTION (fps)) {
        *framerate =
            gst_value_get_fraction_numerator (fps) /
            gst_value_get_fraction_denominator (fps);
      }
      gst_object_unref (GST_OBJECT (pad));
    }
    gst_object_unref (GST_OBJECT (sink));
    LOGV ("framerate %d", *framerate);
  }
}

#define PREPARE_SYNC_TIMEOUT 5000 * GST_MSECOND

void
GstMetadataRetrieverDriver::prepareSync ()
{
  GstBus *bus = NULL;
  GstMessage *message = NULL;
  GstElement *src = NULL;
  GstMessageType message_filter =
      (GstMessageType) (GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE);

  if (mMode & METADATA_MODE_METADATA_RETRIEVAL_ONLY) {
    message_filter =
        (GstMessageType) (GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE |
        GST_MESSAGE_TAG);
  }

  LOGV ("prepareSync");
  src = gst_bin_get_by_name (GST_BIN (mPipeline), "src");

  if (src == NULL) {
    LOGV ("prepareSync no src found");
    mState = GST_STATE_NULL;
    return;
  }

  g_signal_connect (src, "autoplug-continue", G_CALLBACK (autoplug_continue),
                    this);

  if (mFdSrcOffset_max) {
    g_signal_connect (src, "notify::source", G_CALLBACK (source_changed_cb),
                      this);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (mPipeline));
  gst_element_set_state (mPipeline, GST_STATE_PAUSED);

  message =
      gst_bus_timed_pop_filtered (bus, PREPARE_SYNC_TIMEOUT, message_filter);

  mState = GST_STATE_PAUSED;

  while (message != NULL) {
    switch (GST_MESSAGE_TYPE (message)) {
      case GST_MESSAGE_TAG:
      {
        GstTagList *tag_list, *result;

        LOGV ("receive TAGS from the stream");
        gst_message_parse_tag (message, &tag_list);

        /* all tags (replace previous tags, title/artist/etc. might change
         * in the middle of a stream, e.g. with radio streams) */
        result =
            gst_tag_list_merge (mTag_list, tag_list, GST_TAG_MERGE_REPLACE);
        if (mTag_list)
          gst_tag_list_free (mTag_list);
        mTag_list = result;

        /* clean up */
        gst_tag_list_free (tag_list);
        gst_message_unref (message);
        break;
      }

      case GST_MESSAGE_ASYNC_DONE:
      {
        mState = GST_STATE_PAUSED;
        LOGV ("receive GST_MESSAGE_ASYNC_DONE");
        gst_message_unref (message);
        goto bail;
      }

      case GST_MESSAGE_ERROR:
      {
        GError *err;
        gchar *debug;

        mState = GST_STATE_NULL;
        gst_message_parse_error (message, &err, &debug);
        LOGV ("receive GST_MESSAGE_ERROR : %d, %s (EXTRA INFO=%s)",
            err->code, err->message, (debug != NULL) ? debug : "none");
        gst_message_unref (message);
        if (debug) {
          g_free (debug);
        }
        goto bail;
      }

      default:
        // do nothing
        break;
    }
    message = gst_bus_timed_pop_filtered (bus, 50 * GST_MSECOND, message_filter);
  }

bail:
  gst_object_unref (GST_OBJECT (src));
  gst_object_unref (GST_OBJECT (bus));
}


void
GstMetadataRetrieverDriver::seekSync (gint64 p)
{
  gint64 position;
  GstMessage *message = NULL;
  GstBus *bus = NULL;

  if (!mPipeline)
    return;

  position = ((gint64) p) * 1000000;
  LOGV ("Seek to %lld ms (%lld ns)", p, position);
  if (!gst_element_seek_simple (mPipeline, GST_FORMAT_TIME,
          (GstSeekFlags) ((int) GST_SEEK_FLAG_FLUSH | (int)
              GST_SEEK_FLAG_KEY_UNIT), position)) {
    LOGE ("Can't perfom seek for %lld ms", p);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (mPipeline));

  message =
      gst_bus_timed_pop_filtered (bus, PREPARE_SYNC_TIMEOUT,
      (GstMessageType) (GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE));

  if (message != NULL) {
    switch (GST_MESSAGE_TYPE (message)) {

      case GST_MESSAGE_ASYNC_DONE:
      {
        mState = GST_STATE_PAUSED;
        break;
      }

      case GST_MESSAGE_ERROR:
      {
        mState = GST_STATE_NULL;
        break;
      }
      default:
        // do nothing
        break;
    }
    gst_message_unref (message);
  }
  gst_object_unref (bus);
}


gint64
GstMetadataRetrieverDriver::getPosition ()
{
  GstFormat fmt = GST_FORMAT_TIME;
  gint64 pos = 0;

  if (!mPipeline) {
    LOGV ("get postion but pipeline has not been created yet");
    return 0;
  }

  LOGV ("getPosition");
  gst_element_query_position (mPipeline, &fmt, &pos);
  LOGV ("Stream position %lld ms", pos / 1000000);
  return (pos / 1000000);
}

int
GstMetadataRetrieverDriver::getStatus ()
{
  return mState;
}

gint64
GstMetadataRetrieverDriver::getDuration ()
{

  GstFormat fmt = GST_FORMAT_TIME;
  gint64 len;

  if (!mPipeline) {
    LOGV ("get duration but pipeline has not been created yet");
    return 0;
  }
  /* the duration given by gstreamer is in nanosecond
   * so we need to transform it to millisecond */
  LOGV ("getDuration");
  if (gst_element_query_duration (mPipeline, &fmt, &len))
    LOGE ("Stream duration %lld ms", len / 1000000);
  else {
    LOGV ("Query duration failed");
    len = 0;
  }

  if ((GstClockTime) len == GST_CLOCK_TIME_NONE) {
    LOGV ("Query duration return GST_CLOCK_TIME_NONE");
    len = 0;
  }

  return (len / 1000000);
}

void
GstMetadataRetrieverDriver::quit ()
{
  int state = -1;

  LOGV ("quit");


  if (mTag_list) {
    LOGV ("free tag list");
    gst_tag_list_free (mTag_list);
    mTag_list = NULL;
  }

  if (mPipeline) {
    GstBus *bus;
    bus = gst_pipeline_get_bus (GST_PIPELINE (mPipeline));
    LOGV ("flush bus messages");
    if (bus != NULL) {
      gst_bus_set_flushing (bus, TRUE);
      gst_object_unref (bus);
    }
    LOGV ("free pipeline %s", GST_ELEMENT_NAME (mPipeline));
    state = gst_element_set_state (mPipeline, GST_STATE_NULL);
    LOGV ("set pipeline state to NULL: %d (0:Failure, 1:Success, 2:Async, 3:NO_PREROLL)", state);
    gst_object_unref (mPipeline);
    mPipeline = NULL;
  }

  mState = GST_STATE_NULL;
}

void
GstMetadataRetrieverDriver::getCaptureFrame (guint8 ** data)
{
  LOGV ("getCaptureFrame");

  if (mPipeline != NULL) {
    GstBuffer *frame = NULL;
    GstElement *sink = gst_bin_get_by_name (GST_BIN (mPipeline), "sink");

    g_object_get (G_OBJECT (sink), "last-buffer", &frame, NULL);

    if (frame != NULL) {
      if (*data) delete[] * data;
      *data = new guint8[GST_BUFFER_SIZE (frame)];
      memcpy (*data, GST_BUFFER_DATA (frame), GST_BUFFER_SIZE (frame));
      gst_object_unref (frame);
    }
    gst_object_unref (GST_OBJECT (sink));
  }
}


gchar *
GstMetadataRetrieverDriver::getMetadata (gchar * tag)
{
  LOGV ("get metadata tag %s", tag);

  gchar *str;
  gint count;

  if (!mTag_list)               // no tag list nothing do to
  {
    LOGV ("No taglist => Nothing to do");
    return NULL;
  }

  count = gst_tag_list_get_tag_size (mTag_list, tag);
  if (count) {

    if (gst_tag_get_type (tag) == G_TYPE_STRING) {
      if (!gst_tag_list_get_string_index (mTag_list, tag, 0, &str))
        g_assert_not_reached ();
    } else
      str =
          g_strdup_value_contents (gst_tag_list_get_value_index (mTag_list, tag,
              0));

    LOGV ("for tag %s have metadata %s", tag, str);
    return str;
  } else
    LOGV (" No Tag : %s ! ", tag);

  return NULL;
}

void
GstMetadataRetrieverDriver::getAlbumArt (guint8 ** data, guint64 * size)
{
  if (mAlbumArt == NULL) {
    LOGV ("getAlbumArt try to get image from tags");
    if (mTag_list) {
      gboolean res = FALSE;
      res = gst_tag_list_get_buffer (mTag_list, GST_TAG_IMAGE, &mAlbumArt);
      if (!res)
        res =
            gst_tag_list_get_buffer (mTag_list, GST_TAG_PREVIEW_IMAGE,
            &mAlbumArt);

      if (!res)
        LOGV ("no album art found");
    }
  }

  if (mAlbumArt) {
    *data = GST_BUFFER_DATA (mAlbumArt);
    *size = GST_BUFFER_SIZE (mAlbumArt);
  }
}

/*static*/ void
GstMetadataRetrieverDriver::debug_log (GstDebugCategory * category,
    GstDebugLevel level, const gchar * file, const gchar * function, gint line,
    GObject * object, GstDebugMessage * message, gpointer data)
{
  gint pid;
  GstClockTime elapsed;
  GstMetadataRetrieverDriver *ed = (GstMetadataRetrieverDriver *) data;

  UNUSED (file);
  UNUSED (object);

  if (level > gst_debug_category_get_threshold (category))
    return;

  pid = getpid ();

  elapsed = GST_CLOCK_DIFF (ed->mGst_info_start_time, gst_util_get_timestamp ());

  g_printerr ("%" GST_TIME_FORMAT " %5d %s %s %s:%d %s\r\n",
              GST_TIME_ARGS (elapsed), pid, gst_debug_level_get_name (level),
              gst_debug_category_get_name (category), function, line,
              gst_debug_message_get (message));
}


void
GstMetadataRetrieverDriver::init_gstreamer ()
{
  GError *err = NULL;
  char debug[PROPERTY_VALUE_MAX];
  char trace[PROPERTY_VALUE_MAX];

  property_get ("persist.gst.debug", debug, "0");
  LOGV ("persist.gst.debug property %s", debug);
  setenv ("GST_DEBUG", debug, true);

  property_get ("persist.gst.trace", trace, "/dev/console");
  LOGV ("persist.gst.trace property %s", trace);
  LOGV ("route the trace to %s", trace);
  setenv ("GST_DEBUG_FILE", trace, true);

  setenv ("GST_REGISTRY", "/data/data/gstreamer/registry.bin", 0);
  LOGV ("gstreamer init check");

  if (!gst_init_check (NULL, NULL, &err)) {
    LOGE ("Could not initialize GStreamer: %s\n",
          err ? err->message : "unknown error occurred");
    if (err)
      g_error_free (err);
  }

}
