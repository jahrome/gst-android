/*
 * =====================================================================================
 *
 *       Filename:  GstDriver.cpp
 *	Copyright ST-Ericsson 2009
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
#define LOG_TAG "GstDriver"

#include "GstDriver.h"
#include "GstPlayer.h"
#include <gst/video/video.h>
#include <gst/pbutils/pbutils.h>
#include <fcntl.h>
#include <media/Metadata.h>

#include <utils/Log.h>
#include <cutils/properties.h>

#define UNUSED(x) (void)x

using namespace android;

GstDriver::GstDriver (MediaPlayerInterface * parent):
mparent (parent),
mVideoBin (NULL),
mPlaybin (NULL),
mFdSrcOffset_min (0), mFdSrcOffset_max (0),
mFdSrcOffset_current (0), mFd (-1),
mLastValidPosition (0),
mState (GSTDRIVER_STATE_IDLE),
mEos (FALSE),
mLoop (FALSE),
mAudioStreamType (0), mAudioFlingerGstId (0), mAudioFlinger (0),
mAudioLeftVolume (1.0f), mAudioRightVolume (1.0f),
mNbAudioStream (0), mNbAudioStreamError (0),
mDuration (0), mPlaybackType (GSTDRIVER_PLAYBACK_TYPE_UNKNOWN),
mMainCtx(NULL), mMainLoop(NULL), mMainThread(NULL), mBusWatch(NULL)
{
  LOGV ("constructor");

  mHaveStreamInfo = false;
  mHaveStreamAudio = false;
  mHaveStreamVideo = false;

  init_gstreamer ();
  mState = GSTDRIVER_STATE_IDLE;

  mPausedByUser = FALSE;
  //LOGV("exit from GstDriver constructor");
}

GstDriver::~GstDriver ()
{
  LOGV ("destructor");

  if (mPlaybin) {
    LOGV ("free pipeline %s", gst_element_get_name (mPlaybin));
    gst_element_set_state (mPlaybin, GST_STATE_NULL);
    gst_object_unref (mPlaybin);
    mPlaybin = NULL;
  }

  if (mFd != -1) {
    close (mFd);
  }
  mState = GSTDRIVER_STATE_END;
}

GstStateChangeReturn
GstDriver::wait_for_set_state (int timeout_msec)
{
  GstMessage *msg;
  GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

  /* Wait for state change */
  msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (mPlaybin), timeout_msec * GST_MSECOND,     /* in nanosec */
      (GstMessageType) (GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE));

  if (msg) {
    if ((GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ASYNC_DONE))
      ret = GST_STATE_CHANGE_SUCCESS;

    gst_message_unref (msg);
  }

  return ret;
}

gpointer
GstDriver::do_loop (GstDriver * ed)
{
  LOGV ("enter main loop");
  g_main_loop_run (ed->mMainLoop);
  LOGV ("exit main loop");

  return NULL;
}

void
GstDriver::setup ()
{

  GstBus *bus;
  GError *error = NULL;

  LOGV ("create playbin2");
  mPlaybin = gst_element_factory_make ("playbin2", NULL);
  if (!mPlaybin) {
    LOGE ("can't create playbin2");
  }
  LOGV ("playbin2 creation: %s", gst_element_get_name (mPlaybin));

  // verbose info (as gst-launch -v)
  // Activate the trace with the command: "setprop persist.gst.verbose 1"
  char value[PROPERTY_VALUE_MAX];
  property_get ("persist.gst.verbose", value, "0");
  LOGV ("persist.gst.verbose property = %s", value);
  if (value[0] == '1') {
    LOGV ("Activate deep_notify");
    g_signal_connect (mPlaybin, "deep_notify",
        G_CALLBACK (gst_object_default_deep_notify), NULL);
  }

  /* create and start mainloop */
  LOGV ("set up mainloop");
  mMainCtx = g_main_context_new ();
  mMainLoop = g_main_loop_new (mMainCtx, FALSE);
  mMainThread = g_thread_create ((GThreadFunc) do_loop, this, TRUE, &error);
  if (error != NULL) {
    g_critical ("could not start bus thread: %s", error->message);
  }

  LOGV ("register bus callback");
  // register bus callback        
  bus = gst_pipeline_get_bus (GST_PIPELINE (mPlaybin));
  // add the pipeline bus to our custom mainloop
  mBusWatch = gst_bus_create_watch (bus);
  gst_object_unref (bus);
  g_source_set_callback (mBusWatch, (GSourceFunc) bus_message, this, NULL);
  g_source_attach (mBusWatch, mMainCtx);

  if (mparent) {
    mVideoBin = gst_element_factory_make ("surfaceflingersink", NULL);
    if (!mVideoBin) {
      LOGE ("Can't create surfaceflingersink");
    }
    LOGV ("add surfaceflingersink to playbin");
    mAudioBin = gst_element_factory_make ("audioflingersink", NULL);
    if (!mAudioBin) {
      LOGE ("Can't create audioflingersink");
    }
  } else {
    LOGV ("no parent, maybe call by metadataretriever");

    mVideoBin = gst_element_factory_make ("appsink", NULL);
    if (mVideoBin) {
      LOGV ("set caps video/x-raw-rgb,bpp=16 to appsink");
      g_object_set (G_OBJECT (mVideoBin), "caps",
          gst_caps_new_simple ("video/x-raw-rgb", "bpp", G_TYPE_INT, 16, NULL),
          (gchar *) NULL);
    }
    mAudioBin = gst_element_factory_make ("fakesink", NULL);
    if (!mAudioBin) {
      LOGE ("Can't create fakesink");
    }
  }
  g_object_set (G_OBJECT (mPlaybin), "video-sink", mVideoBin, (gchar *) NULL);

  /*Audio sink */
  LOGV ("add audioflingersink to playbin");
  g_object_set (G_OBJECT (mPlaybin), "audio-sink", mAudioBin, (gchar *) NULL);

  gst_element_set_state (mPlaybin, GST_STATE_NULL);
  mState = GSTDRIVER_STATE_INITIALIZED;
}

void
GstDriver::setDataSource (const char *url)
{
  LOGI ("create source from uri %s", url);

  if (!gst_uri_is_valid (url)) {
    char uri[512] = "file://";
    strcat (uri, url);
    LOGV ("set uri to playbin %s", uri);
    g_object_set (G_OBJECT (mPlaybin), "uri", uri, (gchar *) NULL);
	mPlaybackType = GSTDRIVER_PLAYBACK_TYPE_LOCAL_FILE;

  } else {
    LOGV ("set uri to playbin %s", url);
    g_object_set (G_OBJECT (mPlaybin), "uri", url, (gchar *) NULL);
    if (strncasecmp (url, "http", 4) == 0) {
      g_object_set (G_OBJECT (mPlaybin), "buffer-duration", 30 * GST_SECOND,
          (gchar *) NULL);
      g_object_set (G_OBJECT (mPlaybin), "buffer-size", (gint) 500 * 1024,
          (gchar *) NULL);
	  mPlaybackType = GSTDRIVER_PLAYBACK_TYPE_HTTP;
    }
	if (strncasecmp (url, "rtsp", 4) == 0) {
		mPlaybackType = GSTDRIVER_PLAYBACK_TYPE_RTSP;
	}
  }
}

/*static*/ void
GstDriver::need_data (GstAppSrc * src, guint length, gpointer user_data)
{
  GstBuffer *buffer;
  GstDriver *ed = (GstDriver *) user_data;

  if (ed->mFdSrcOffset_current >= ed->mFdSrcOffset_max) {
    LOGV ("appsrc send eos");
    gst_app_src_end_of_stream (src);
    return;
  }

  if ((ed->mFdSrcOffset_current + length) > ed->mFdSrcOffset_max) {
    length = ed->mFdSrcOffset_max - ed->mFdSrcOffset_current;
  }

  buffer = gst_buffer_new_and_alloc (length);

  if (buffer == NULL) {
    LOGV ("appsrc can't allocate buffer of size %d", length);
    return;
  }
  length = read (ed->mFd, GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

  GST_BUFFER_SIZE (buffer) = length;
  /* we need to set an offset for random access */
  GST_BUFFER_OFFSET (buffer) = ed->mFdSrcOffset_current - ed->mFdSrcOffset_min;
  ed->mFdSrcOffset_current += length;
  GST_BUFFER_OFFSET_END (buffer) =
      ed->mFdSrcOffset_current - ed->mFdSrcOffset_min;

  gst_app_src_push_buffer (src, buffer);
}

/*static*/ gboolean
GstDriver::seek_data (GstAppSrc * src, guint64 offset, gpointer user_data)
{
  UNUSED (src);

  GstDriver *ed = (GstDriver *) user_data;

  if ((ed->mFdSrcOffset_min + offset) <= ed->mFdSrcOffset_max) {
    lseek (ed->mFd, ed->mFdSrcOffset_min + offset, SEEK_SET);
    ed->mFdSrcOffset_current = ed->mFdSrcOffset_min + offset;
  }

  return TRUE;
}

/*static*/ void
GstDriver::source_changed_cb (GObject * obj, GParamSpec * pspec, GstDriver * ed)
{
  UNUSED (pspec);

  // get the newly created source element 
  g_object_get (obj, "source", &(ed->mAppsrc), (gchar *) NULL);

  if (ed->mAppsrc != NULL) {
    GstAppSrcCallbacks callbacks = { need_data, NULL, seek_data, {NULL,}
    };
    LOGV ("nd %p : sd %p", need_data, seek_data);

    lseek (ed->mFd, ed->mFdSrcOffset_min, SEEK_SET);

    g_object_set (ed->mAppsrc, "format", GST_FORMAT_BYTES, NULL);
    g_object_set (ed->mAppsrc, "stream-type", 2 /*"random-access" */ , NULL);
    g_object_set (ed->mAppsrc, "size",
        (gint64) (ed->mFdSrcOffset_max - ed->mFdSrcOffset_min), NULL);
    LOGV ("create and register appsrc callbacks");
    gst_app_src_set_callbacks (GST_APP_SRC (ed->mAppsrc), &callbacks, ed, NULL);
  }
}


void
GstDriver::setFdDataSource (int fd, gint64 offset, gint64 length)
{
  LOGI ("create source from fd %d offset %lld lenght %lld", fd, offset, length);

  // duplicate the fd because it should be close in java layers before we can use it
  mFd = dup (fd);
  LOGV ("dup(fd) old %d new %d", fd, mFd);
  // create the uri string with the new fd
  gchar *uri = g_strdup_printf ("appsrc://");
  mFdSrcOffset_min = offset;
  mFdSrcOffset_current = mFdSrcOffset_min;
  mFdSrcOffset_max = mFdSrcOffset_min + length;

  // the source element isn't created yet, so ask to be notify when created 
  g_signal_connect (mPlaybin, "notify::source", G_CALLBACK (source_changed_cb),
      this);
  g_object_set (G_OBJECT (mPlaybin), "uri", uri, (gchar *) NULL);
  g_free (uri);
  mPlaybackType = GSTDRIVER_PLAYBACK_TYPE_LOCAL_FILE;
}

void
GstDriver::getVideoSize (int *width, int *height)
{
  *width = 0;
  *height = 0;

  if (mVideoBin == NULL) {
    LOGV ("mVideoBin not set return 0,0");
    return;
  }
  switch (mState) {
    case GSTDRIVER_STATE_IDLE:
    case GSTDRIVER_STATE_INITIALIZED:
    case GSTDRIVER_STATE_STOPPED:
    case GSTDRIVER_STATE_ERROR:
    case GSTDRIVER_STATE_END:
      return;
    case GSTDRIVER_STATE_COMPLETED:
    case GSTDRIVER_STATE_PREPARED:
    case GSTDRIVER_STATE_STARTED:
    case GSTDRIVER_STATE_PAUSED:
      if (mHaveStreamVideo) {
        if (GstPad * pad = gst_element_get_static_pad (mVideoBin, "sink")) {
          gst_video_get_size (GST_PAD (pad), width, height);
          gst_object_unref (GST_OBJECT (pad));
        }
        LOGV ("video width %d height %d", *width, *height);
      }
      break;
  }
}

void
GstDriver::setVolume (float left, float right)
{
  LOGV ("set volume left %f right %f", left, right);
  if (!mPlaybin)
    return;

  if (mAudioFlinger != 0) {
    mAudioLeftVolume = left;
    mAudioRightVolume = right;
  }
}

void
GstDriver::setAudioStreamType (int streamType)
{
  LOGV ("set audio stream type %d", streamType);
  mAudioStreamType = streamType;
}

void
GstDriver::setVideoSurface (const sp < ISurface > &surface)
{
  LOGV ("set surface to videosink");
  mSurface = surface;
  g_object_set (G_OBJECT (mVideoBin), "surface", surface.get(), (gchar *) NULL);
}

bool
GstDriver::setAudioSink (sp < MediaPlayerInterface::AudioSink > audiosink)
{
  if (audiosink == 0) {
    LOGE ("Error audio sink %p", audiosink.get ());
    return false;
  }

  if (!mAudioBin) {
    LOGE ("Pipeline not initialized\n");
    return false;
  }

  if (mAudioOut != 0) {
    mAudioOut.clear ();
    mAudioOut = 0;
  }
  mAudioOut = audiosink;

  // set AudioSink
  LOGD ("GstDriver::setAudioSink: %p\n", mAudioOut.get ());
  g_object_set (mAudioBin, "audiosink", mAudioOut.get (), NULL);

  return true;
}

#define PREPARE_SYNC_TIMEOUT 3000       /* 3 sec */

void
GstDriver::prepareSync ()
{
  GstStateChangeReturn ret;
  LOGV ("prepareSync");

  gst_element_set_state (mPlaybin, GST_STATE_READY);

  ret = gst_element_set_state (mPlaybin, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_ASYNC) {
    mState = GSTDRIVER_STATE_INITIALIZED;
    LOGV ("wait for completion of state change for prepare synchronous");

    ret = wait_for_set_state (PREPARE_SYNC_TIMEOUT);
  }

  if (ret == GST_STATE_CHANGE_SUCCESS) {
    mState = GSTDRIVER_STATE_PREPARED;
  }
}

void
GstDriver::prepareAsync ()
{
  LOGV ("prepareAsync");
  GstStateChangeReturn ret;
  ret = gst_element_set_state (mPlaybin, GST_STATE_PAUSED);
}

void
GstDriver::start ()
{

  switch (mState) {
    case GSTDRIVER_STATE_IDLE:
    case GSTDRIVER_STATE_INITIALIZED:
    case GSTDRIVER_STATE_STOPPED:
    case GSTDRIVER_STATE_ERROR:
    case GSTDRIVER_STATE_END:
    {
      LOGD("We are in IDLE/INITIALIZED/STOPPPED/ERROR/END: %d", mState);
      GstPlayer *parent = (GstPlayer *) mparent;
      if (parent) {
        parent->sendEvent (MEDIA_ERROR, 0);
      }
      mState = GSTDRIVER_STATE_ERROR;
    }
      break;

    case GSTDRIVER_STATE_COMPLETED:
    {
      LOGD("We are in GSTDRIVER_STATE_COMPLETED");
      gint64 duration, position;
      duration = getDuration ();
      position = getPosition ();

      if ((duration - position) <= 0) {
        seek (0);
      }
   }
    case GSTDRIVER_STATE_PREPARED:
    case GSTDRIVER_STATE_STARTED:
    case GSTDRIVER_STATE_PAUSED:
    LOGD("We are in PREPARED/STARTED/PAUSED: %d", mState);
      /* FIXME, twi says NOT 
         HAVING THIS MAKES GENERATING THUMBNAILS DOGSLOW
         no track means the sink is an AudioCache instance and the player is
         being used to decode in memory
      */ 

      if (mAudioOut->getTrack() == NULL) {
        g_object_set (mAudioBin, "sync", FALSE, NULL);
      } else {
        g_object_set (mAudioBin, "sync", TRUE, NULL);
      }
 
      mEos = false;
      gst_element_set_state (mPlaybin, GST_STATE_PLAYING);
      mState = GSTDRIVER_STATE_STARTED;
      mPausedByUser = FALSE;
      break;
    default:
      break;
  }
  LOGV ("start");
}

void
GstDriver::seek (gint64 p)
{
  gint64 position;
  GstPlayer *parent = (GstPlayer *) mparent;


  if (!mPlaybin)
    goto bail;
	

  if(p < 0) //don't seek to negative time 
	goto bail;

  mLastValidPosition = p;

  position = ((gint64) p) * 1000000;
  LOGV ("Seek to %lld ms (%lld ns)", p, position);
  if (!gst_element_seek_simple (mPlaybin, GST_FORMAT_TIME,
          (GstSeekFlags) ((int) GST_SEEK_FLAG_FLUSH | (int)
              GST_SEEK_FLAG_KEY_UNIT), position)) {
    LOGE ("Can't perfom seek for %lld ms", p);
  }

bail:
  if (parent) {
    parent->sendEvent (MEDIA_SEEK_COMPLETE);
    LOGV ("Send MEDIA_SEEK_COMPLETE");
  }
}

gint64
GstDriver::getPosition ()
{
  GstFormat fmt = GST_FORMAT_TIME;
  gint64 pos = 0;

  if (!mPlaybin) {
    LOGV ("get postion but pipeline has not been created yet");
    return 0;
  }

  LOGV ("getPosition eos %d", mEos);
  if (!mEos && gst_element_query_position (mPlaybin, &fmt, &pos)) {
    LOGV ("got position from query");
    mLastValidPosition = pos / 1000000;
  }

  LOGV ("Stream position %lld ms", mLastValidPosition);
  return mLastValidPosition;
}

int
GstDriver::getStatus ()
{
  return mState;
}

gint64
GstDriver::getDuration ()
{

  GstFormat fmt = GST_FORMAT_TIME;
  gint64 len;

  if (!mPlaybin) {
    LOGV ("get duration but pipeline has not been created yet");
    return 0;
  }
  // the duration given by gstreamer is in nanosecond 
  // so we need to transform it in millisecond
  LOGV ("getDuration");
  if (gst_element_query_duration (mPlaybin, &fmt, &len)) {
    LOGI ("Stream duration %lld ms", len / 1000000);
  } else {
    LOGE ("Query duration failed use message duration value %lld ms",
        mDuration / 1000000);
    len = mDuration;
  }

  if ((GstClockTime) len == GST_CLOCK_TIME_NONE) {
    LOGV ("Query duration return GST_CLOCK_TIME_NONE");
    len = 0;
  }
  return (len / 1000000);
}

void
GstDriver::setEos (gint64 position)
{
  mEos = true;
  mLastValidPosition = position;
}

void
GstDriver::stop ()
{
  LOGV ("stop");
  gst_element_set_state (mPlaybin, GST_STATE_NULL);
  
  if (wait_for_set_state (500) != GST_STATE_CHANGE_SUCCESS) {
    LOGW ("TIMEOUT on stop request");
  }
  mState = GSTDRIVER_STATE_STOPPED;
}

void
GstDriver::pause ()
{
  LOGV ("pause");
  gst_element_set_state (mPlaybin, GST_STATE_PAUSED);
  mState = GSTDRIVER_STATE_PAUSED;
  mPausedByUser = TRUE;
  LOGV ("PAUSED THE PIPELINE - mPausedByUser = %d", mPausedByUser);
}


void
GstDriver::reset ()
{
  LOGV ("reset");
}

void
GstDriver::quit ()
{
  int state = -1;

  LOGV ("quit");

  if (mPlaybin) {
    GstBus *bus;
    bus = gst_pipeline_get_bus (GST_PIPELINE (mPlaybin));
    LOGV ("flush bus messages");
    if (bus != NULL) {
      gst_bus_set_flushing (bus, TRUE);
      gst_object_unref (bus);
    }
    LOGV ("free pipeline %s", gst_element_get_name (mPlaybin));
    state = gst_element_set_state (mPlaybin, GST_STATE_NULL);
    LOGV ("set pipeline state to NULL: %d (0:Failure, 1:Success, 2:Async, 3:NO_PREROLL)", state);
    gst_object_unref (mPlaybin);
    mPlaybin = NULL;
  }

  if (mMainLoop) {
    g_source_destroy (mBusWatch);
	g_source_unref(mBusWatch);
	mBusWatch = NULL;
    g_main_loop_quit (mMainLoop);
    g_thread_join (mMainThread);
	mMainThread = NULL;
    g_main_loop_unref (mMainLoop);
	mMainLoop = NULL;
    g_main_context_unref (mMainCtx);
	mMainCtx = NULL;
  }

  mState = GSTDRIVER_STATE_END;
}

void
GstDriver::getStreamsInfo ()
{
  LOGV ("getStreamsInfo");

  // Audio/Video
  if (!mHaveStreamInfo) {
    gint n_audio, n_video;

    g_object_get (G_OBJECT (mPlaybin), "n-audio", &n_audio, NULL);
    g_object_get (G_OBJECT (mPlaybin), "n-video", &n_video, NULL);

    if (n_audio > 0) {
      mHaveStreamAudio = TRUE;
      mNbAudioStream = n_audio;
    }

    if (n_video > 0) {
      mHaveStreamVideo = TRUE;
    }

    mHaveStreamInfo = TRUE;
  }
}

/*static*/ GstBusSyncReply
GstDriver::bus_message (GstBus * bus, GstMessage * msg, gpointer data)
{
  GstDriver *ed = (GstDriver *) data;
  GstPlayer *parent = (GstPlayer *) ed->mparent;

  UNUSED (bus);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS: 
  {
      LOGV ("bus receive message EOS");
      /* set state to paused (we want that "isPlaying" fct returns false after eos) */
      ed->mState = GSTDRIVER_STATE_COMPLETED;

	  gst_element_set_state(ed->mPlaybin, GST_STATE_PAUSED);
	  
      if (ed->mAudioOut != 0) {
         ed->mAudioOut->stop();
      }

      ed->setEos(ed->getDuration());
      LOGV("set position on eos %"GST_TIME_FORMAT,
          GST_TIME_ARGS(ed->getPosition()));

      if (parent)
        parent->sendEvent (MEDIA_PLAYBACK_COMPLETE);
	      

      break;
    }

    case GST_MESSAGE_ERROR:
    {
      GError *err;
      gchar *debug;
      gboolean informParent = true;

      gst_message_parse_error (msg, &err, &debug);
      LOGE ("bus receive message ERROR %d: %s from %s", err->code, err->message,
          debug);
      // Several audio streams can be detected so before return an error check if none audio can be rendered
      if (strstr (debug, "icbaudiodec") != NULL) {
        ed->mNbAudioStreamError++;
        if (ed->mHaveStreamInfo) {
          if (ed->mNbAudioStream >= ed->mNbAudioStreamError) {
            LOGW ("%d error(s) on Audio Side whereas %d audio stream(s)",
                ed->mNbAudioStreamError, ed->mNbAudioStream);
            informParent = false;
          }
        } else {
          LOGW ("%d warning(s) detected", ed->mNbAudioStreamError);
          informParent = false;
        }
      }
      g_error_free (err);
      g_free (debug);

      if (informParent) {
        if (parent) {
          parent->sendEvent (MEDIA_ERROR, 0);
        }
        ed->mState = GSTDRIVER_STATE_ERROR;
      }
      break;
    }

    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old_state, new_state, pending;

      gst_message_parse_state_changed (msg, &old_state, &new_state, &pending);
      	 
	 
      /* we only care about pipeline state change messages */
      if (GST_MESSAGE_SRC (msg) != GST_OBJECT_CAST (ed->mPlaybin))
        break;
      
      LOGV ("bus receive message STATE_CHANGED old %d new %d pending %d", old_state, new_state, pending);
      
      //
      if ((new_state == GST_STATE_PLAYING) && (old_state == GST_STATE_PAUSED))
        ed->getStreamsInfo ();

      LOGV ("bus receive message STATE_CHANGED");
      if ((new_state == GST_STATE_PAUSED) && (old_state == GST_STATE_READY)) {
        ed->getStreamsInfo ();

        // Check it's necessary to return an audio error or not
        if (ed->mNbAudioStreamError) {
          LOGW ("%d error(s) on Audio Side whereas %d audio stream(s)",
              ed->mNbAudioStreamError, ed->mNbAudioStream);
        }
        if (ed->mNbAudioStream != 0
            && (ed->mNbAudioStream <= ed->mNbAudioStreamError)) {
          // If there is at least on audio stream and each audio streams has generated
          // an error, then return error
          LOGE ("Audio Stream error : MEDIA_ERROR ; enter GSTDRIVER_STATE_ERROR state");
          if (parent) {
            parent->sendEvent (MEDIA_ERROR, 0);
          }
          ed->mState = GSTDRIVER_STATE_ERROR;
        }

        if ((ed->mPlaybackType == GSTDRIVER_PLAYBACK_TYPE_RTSP) && parent) {
          LOGV ("bus handler send event MEDIA_PREPARED");
		  ed->mState = GSTDRIVER_STATE_PREPARED;
          parent->sendEvent (MEDIA_PREPARED);
        }
        
        if (ed->mVideoBin) {
          int width = 0, height = 0;
          if (ed->mHaveStreamInfo && ed->mHaveStreamVideo) {
            if (GstPad * pad =
                gst_element_get_static_pad (ed->mVideoBin, "sink")) {
              gst_video_get_size (GST_PAD (pad), &width, &height);
              gst_object_unref (GST_OBJECT (pad));
            }
            if (parent) {
              LOGV ("bus handler send event MEDIA_SET_VIDEO_SIZE");
              parent->sendEvent (MEDIA_SET_VIDEO_SIZE, width, height);
            }
          }
        }
      }
    }
      break;

    case GST_MESSAGE_BUFFERING:
    {
      gint percent = 0;

      gst_message_parse_buffering (msg, &percent);
      LOGV ("buffering %d/100", percent);

      if (ed->mPlaybackType == GSTDRIVER_PLAYBACK_TYPE_HTTP) {
        if (percent == 100) {
          LOGV ("Buffering complete");
          if ((ed->mState == GSTDRIVER_STATE_INITIALIZED) && parent) {
            LOGV ("Sending MEDIA_PREPARED");
			ed->mState = GSTDRIVER_STATE_PREPARED;
            parent->sendEvent (MEDIA_PREPARED);
          } else if (ed->mPausedByUser == FALSE) {
            LOGV ("buffer level hit high watermark -> PLAYING");
            gst_element_set_state (ed->mPlaybin, GST_STATE_PLAYING);
          }
        } else {
          gst_element_set_state (ed->mPlaybin, GST_STATE_PAUSED);
        }
      }

      if (parent) {
        parent->sendEvent (MEDIA_BUFFERING_UPDATE, percent);
      }
      break;
    }
    case GST_MESSAGE_ELEMENT:
    {

      if (msg) {
        const GstStructure *gstruct = gst_message_get_structure (msg);

        if (gstruct) {
          const char *gname = gst_structure_get_name (gstruct);
          gint percent;
          if (!strcasecmp (gname, "progress")) {
            //element is Qtdemux with structure name= progress
            if (gst_structure_get_int (gstruct, "percent", &percent))
              LOGV ("QTDemux: progress %d %s", percent, " %");
          } else {
            LOGV ("%s: structure name=%s",
                GST_ELEMENT_NAME (GST_MESSAGE_SRC (msg)),
                gst_structure_get_name (gstruct));
          }

          if (gst_structure_has_name (gstruct, "missing-plugin")) {
            gchar *detail;

            detail = gst_missing_plugin_message_get_installer_detail (msg);
            LOGV ("a plugin is missing send error message %s", detail);
            g_free (detail);
            if (parent) {
              parent->sendEvent (MEDIA_ERROR, 0);
            }
            ed->mState = GSTDRIVER_STATE_ERROR;
          }
        }

      }
      break;
    }
    case GST_MESSAGE_QOS:
    {
      /* These variables are documented at:
       * http://cgit.freedesktop.org/gstreamer/gstreamer/tree/docs/design/part-qos.txt#n53*/
      gboolean live;
      GstClockTime running_time, stream_time, timestamp, duration;
      gint64 jitter;
      gdouble proportion;
      gint quality;
      GstFormat format;
      guint64 processed, dropped;

      /* The QoS message API is documented here:
       * http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer/html/gstreamer-GstMessage.html */
      gst_message_parse_qos (msg, &live, &running_time, &stream_time,
          &timestamp, &duration);
      gst_message_parse_qos_values (msg, &jitter, &proportion, &quality);
      gst_message_parse_qos_stats (msg, &format, &processed, &dropped);

      LOGV ("QOS: live %s, running time %" GST_TIME_FORMAT ", stream time %"
          GST_TIME_FORMAT "," " timestamp %" GST_TIME_FORMAT ", duration %"
          GST_TIME_FORMAT "\n" "QOS: jitter %" G_GINT64_FORMAT
          ", proportion %f, quality %d\n" "QOS: format %s, processed %"
          G_GUINT64_FORMAT ", dropped %" G_GUINT64_FORMAT "\n",
          live ? "true" : "false", GST_TIME_ARGS (running_time),
          GST_TIME_ARGS (stream_time), GST_TIME_ARGS (timestamp),
          GST_TIME_ARGS (duration), jitter, proportion, quality,
          format == GST_FORMAT_BUFFERS ? "frames" : "samples", processed,
          dropped);
      break;
    }

    case GST_MESSAGE_DURATION:
    {
      GstFormat format;
      gint64 duration;

      gst_message_parse_duration (msg, &format, &duration);
      LOGV ("get duration message format %s duration %lld",
          gst_format_get_name (format), duration);
      if (format == GST_FORMAT_TIME) {
        ed->mDuration = duration;
      }

      break;
    }

    case GST_MESSAGE_ASYNC_DONE:
    {
      LOGV ("receive message ASYNC DONE on bus");
      if ((ed->mState == GSTDRIVER_STATE_INITIALIZED)
          && (ed->mPlaybackType == GSTDRIVER_PLAYBACK_TYPE_LOCAL_FILE)
          && parent) {
        LOGV ("Sending MEDIA_PREPARED");
        ed->mState = GSTDRIVER_STATE_PREPARED;
        parent->sendEvent (MEDIA_PREPARED);
      }
      break;
    }

    default:
      //LOGV("get unhandled message on bus");
      LOGV ("get unhandled message on bus %s %s", GST_MESSAGE_SRC_NAME (msg),
          GST_MESSAGE_TYPE_NAME (msg));
      break;
  }

  return GST_BUS_PASS;
}

/*static*/ void
GstDriver::debug_log (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, GstDebugMessage * message, gpointer data)
{
  gint pid;
  GstClockTime elapsed;
  GstDriver *ed = (GstDriver *) data;

  UNUSED (file);
  UNUSED (object);

  if (level > gst_debug_category_get_threshold (category))
    return;

  pid = getpid ();

  elapsed = GST_CLOCK_DIFF (ed->mGst_info_start_time,
      gst_util_get_timestamp ());


  g_printerr ("%" GST_TIME_FORMAT " %5d %s %s %s:%d %s\r\n",
      GST_TIME_ARGS (elapsed),
      pid,
      gst_debug_level_get_name (level),
      gst_debug_category_get_name (category), function, line,
      gst_debug_message_get (message));
}


void
GstDriver::init_gstreamer ()
{
  // do the init of gstreamer there
  GError *err = NULL;
  char debug[PROPERTY_VALUE_MAX];
  char trace[PROPERTY_VALUE_MAX];

  property_get ("persist.gst.debug", debug, "0");
  LOGV ("persist.gst.debug property %s", debug);
  setenv ("GST_DEBUG", debug, 1);

  property_get ("persist.gst.trace", trace, "/dev/console");
  LOGV ("persist.gst.trace property %s", trace);
  LOGV ("route the trace to %s", trace);
  setenv ("GST_DEBUG_FILE", trace, 1);

  setenv ("GST_REGISTRY", "/data/data/gstreamer/registry.bin", 0);
  LOGV ("gstreamer init check");
  if (!gst_init_check (NULL, NULL, &err)) {
    LOGE ("Could not initialize GStreamer: %s\n",
        err ? err->message : "unknown error occurred");
    if (err) {
      g_error_free (err);
    }
  }
}

// The Client in the MetadataPlayerService calls this method on
// the native player to retrieve all or a subset of metadata.
//
// @param ids SortedList of metadata ID to be fetch. If empty, all
//            the known metadata should be returned.
// @param[inout] records Parcel where the player appends its metadata.
// @return OK if the call was successful.
status_t
GstDriver::getMetadata (const SortedVector < media::Metadata::Type > &ids,
    Parcel * records)
{
  using media::Metadata;

  //if (!mSetupDone || !mGstDriver) {
  //    return INVALID_OPERATION;
  //}

  if (ids.size () != 0) {
    LOGW ("Metadata filtering not implemented, ignoring.");
  }

  Metadata metadata (records);
  bool ok = true;

  // Right now, we only communicate info about the liveness of the
  // stream to enable/disable pause and seek in the UI.
  bool live = false;            //mPlayerDriver->isLiveStreaming();
  if (mPlaybin &&
      ((mState == GSTDRIVER_STATE_PREPARED) ||
          (mState == GSTDRIVER_STATE_STARTED) ||
          (mState == GSTDRIVER_STATE_PAUSED))) {
    gchar *url;
    g_object_get (GST_OBJECT_CAST (mPlaybin), "uri", &(url), NULL);

    if ((strncasecmp (url, "rtsp", 4) == 0) ||
        (strncasecmp (url, "http", 4) == 0)) {
      live = true;
    }
  }
//    ok = ok && metadata.appendBool(Metadata::kPauseAvailable, !live);
  ok = ok && metadata.appendBool (Metadata::kSeekBackwardAvailable, !live);
  ok = ok && metadata.appendBool (Metadata::kSeekForwardAvailable, !live);
  return ok ? OK : UNKNOWN_ERROR;
}
