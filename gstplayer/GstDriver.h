/*
 * =====================================================================================
 * 
 *       Filename:  GstDriver.h
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

#ifndef  GSTDRIVER_INC
#define  GSTDRIVER_INC

#include <media/MediaPlayerInterface.h>
#include <media/AudioSystem.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <utils/List.h>
#include <utils/Log.h>
#include <surfaceflinger/ISurface.h>

// pmem interprocess shared memory support
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/MemoryHeapPmem.h>

#define GST_UNUSED(x) (void)x;

namespace android
{


  class GstDriver
  {
  public:
    GstDriver (MediaPlayerInterface * parent);
    ~GstDriver ();

    void setup ();
    void setDataSource (const char *url);
    void setFdDataSource (int fd, gint64 offset, gint64 length);
    void setVideoSurface (const sp < ISurface > &surface);
    bool setAudioSink (sp < MediaPlayerInterface::AudioSink > audiosink);
    void prepareAsync ();
    void prepareSync ();
    void start ();
    void stop ();
    void pause ();
    void seek (gint64 p);
    void reset ();
    void quit ();
    gint64 getPosition ();
    gint64 getDuration ();
    void setEos(gint64 position);
    int getStatus ();
    void getVideoSize (int *width, int *height);
    void setVolume (float left, float right);
    void setAudioStreamType (int streamType);
    void endOfData ();

    // The Client in the MetadataPlayerService calls this method on
    // the native player to retrieve all or a subset of metadata.
    //
    // @param ids SortedList of metadata ID to be fetch. If empty, all
    //            the known metadata should be returned.
    // @param[inout] records Parcel where the player appends its metadata.
    // @return OK if the call was successful.
    status_t getMetadata (const SortedVector < media::Metadata::Type > &ids,
        Parcel * records);

    //State described on the Developper reference
    enum GstDriverState
    {
      GSTDRIVER_STATE_IDLE = 10,        //GST_STATE_NULL 
      GSTDRIVER_STATE_INITIALIZED,
      GSTDRIVER_STATE_PREPARED, //GST_STATE_READY
      GSTDRIVER_STATE_STARTED,  //GST_STATE_PLAYING
      GSTDRIVER_STATE_PAUSED,   //GST_STATE_PAUSED               
      GSTDRIVER_STATE_STOPPED,  //GST_STATE_PAUSED
      GSTDRIVER_STATE_COMPLETED,        //GST_STATE_PAUSED
      GSTDRIVER_STATE_ERROR,
      GSTDRIVER_STATE_END,
    };

  private:
      MediaPlayerInterface * mparent;
    GstElement *mVideoBin;
    GstElement *mAudioBin;
    // internal audio sink
      sp < MediaPlayerInterface::AudioSink > mAudioOut;
    GstElement *mPlaybin;
    GstElement *mAppsrc;

    static GstBusSyncReply bus_message (GstBus * bus, GstMessage * msg,
        gpointer data);

    guint64 mFdSrcOffset_min;
    guint64 mFdSrcOffset_max;
    guint64 mFdSrcOffset_current;
    gint mFd;
    gint64 mLastValidPosition;

    static void source_changed_cb (GObject * obj, GParamSpec * pspec,
        GstDriver * ed);
    static void need_data (GstAppSrc * src, guint length, gpointer user_data);
    static gboolean seek_data (GstAppSrc * src, guint64 offset,
        gpointer user_data);

    int mState;
    gboolean mEos;

    gboolean mLoop;

    gboolean mHaveStreamInfo;
    gboolean mHaveStreamAudio;
    gboolean mHaveStreamVideo;

    // AUDIO path management
    int mAudioStreamType;
    uint32_t mAudioFlingerGstId;
      sp < IAudioFlinger > mAudioFlinger;
    float mAudioLeftVolume;
    float mAudioRightVolume;

    guint mNbAudioStream;
    guint mNbAudioStreamError;

    void getStreamsInfo ();

    void init_gstreamer ();
    GstClockTime mGst_info_start_time;
    GstStateChangeReturn wait_for_set_state (int timeout_msec);
    static void debug_log (GstDebugCategory * category, GstDebugLevel level,
        const gchar * file, const gchar * function, gint line,
        GObject * object, GstDebugMessage * message, gpointer data);

    gboolean mPausedByUser;     /* false if paused by buffering logic. user pausing takes precedent */
    
	gint64	 mDuration;

	enum GstDriverPlaybackType 
	{
		GSTDRIVER_PLAYBACK_TYPE_UNKNOWN,
		GSTDRIVER_PLAYBACK_TYPE_LOCAL_FILE,
		GSTDRIVER_PLAYBACK_TYPE_RTSP,
		GSTDRIVER_PLAYBACK_TYPE_HTTP
	};

	int		mPlaybackType;

  GMainContext*   mMainCtx;
  GMainLoop*      mMainLoop;
  static gpointer do_loop (GstDriver *ed);
  GThread*        mMainThread;
  GSource*        mBusWatch;
  };

};                              // namespace android

#endif /* ----- #ifndef GSTDRIVER_INC  ----- */
