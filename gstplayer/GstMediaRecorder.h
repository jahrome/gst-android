/*
 * =====================================================================================
 *
 *       Filename:  GstMediaRecorder.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/31/2009 01:36:56 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Benjamin Gaignard
 *        Company:  STEricsson
 *
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

#ifndef GSTMEDIARECODER_H
#define GSTMEDIARECODER_H

#include <alsa/asoundlib.h>

#include <utils/Log.h>
#ifndef STECONF_ANDROID_VERSION_FROYO
#include <ui/CameraParameters.h>
#include <utils/Errors.h>
#include <media/mediarecorder.h>
#include <ui/ISurface.h>
#include <ui/ICamera.h>
#include <ui/Camera.h>
#else
#include <camera/CameraParameters.h>
#include <utils/Errors.h>
#include <surfaceflinger/ISurface.h>
#include <camera/ICamera.h>
#include <camera/Camera.h>
#include <gst/gst.h>
#include <media/MediaRecorderBase.h>
#include <utils/String8.h>
#endif
#include <gst/gst.h>

#define GST_UNUSED(x) (void)x;

namespace android
{

  class AndroidGstCameraListener;

#ifndef STECONF_ANDROID_VERSION_FROYO
  class GstMediaRecorder
#else
  class GstMediaRecorder:public MediaRecorderBase
#endif
  {
  public:
    GstMediaRecorder ();
    ~GstMediaRecorder ();

    status_t init ();
    status_t setAudioSource (audio_source as);
    status_t setVideoSource (video_source vs);
    status_t setOutputFormat (output_format of);
    status_t setAudioEncoder (audio_encoder ae);
    status_t setVideoEncoder (video_encoder ve);
    status_t setVideoSize (int width, int height);
    status_t setVideoFrameRate (int frames_per_second);
    status_t setCamera (const sp < ICamera > &camera);
    status_t setPreviewSurface (const sp < ISurface > &surface);
    status_t setOutputFile (const char *path);
    status_t setOutputFile (int fd, int64_t offset, int64_t length);
    status_t setParameters (const String8 & params);
    status_t setListener (const sp < IMediaPlayerClient > &listener);
    status_t prepare ();
    status_t start ();
    status_t stop ();
    status_t close ();
    status_t reset ();
    status_t getMaxAmplitude (int *max);

    void postDataTimestamp (nsecs_t timestamp, int32_t msgType,
        const sp < IMemory > &dataPtr);

  private:
    enum AndroidCameraFlags
    {
      FLAGS_SET_CAMERA = 1L << 0,
      FLAGS_HOT_CAMERA = 1L << 1,
      FLAGS_HOT_MICRO = 1L << 2,
    };

      sp < IMediaPlayerClient > mListener;
      sp < Camera > mCamera;
      sp < ISurface > mSurface;
    int mFlags;
    gchar *mOutFilePath;
    int mWidth;
    int mHeight;
    int mFps;
    int mOutput_format;
    int mVideo_encoder;
    int mAudio_encoder;
    int mAudio_source;
    int mUse_video_src;         // set to TRUE when the appli has set a video src
    int mUse_audio_src;         // set to TRUE when the appli has set a audio src
    int mOutFilePath_fd;

    int mVTMode;
    int mIPeriod;
    int mIMBRefreshMode;
    gboolean mIsEos;

    static GstBusSyncReply bus_message (GstBus * bus, GstMessage * msg,
        gpointer data);
    static void record_callback (const sp < IMemory > &frame, void *cookie);
    static void handoff (GstElement * object, GstBuffer * arg0,
        gpointer user_data);
    status_t build_record_graph ();
    GstStateChangeReturn wait_for_set_state (int timeout_msec);

    void sendEos ();
    status_t release_pipeline ();

    GstElement *create_video_bin ();
    GstElement *create_audio_bin ();
    GstElement *mVideoBin, *mAudioBin;
    GstElement *mPipeline;
    GstElement *mVideoSrc;
    GstElement *mAudioSrc;
    GMutex *mEOSlock;

    gint64 mMaxDuration;
    GTimer *mTimer;
    gint64 mMaxFileSize;
    gint64 mCurrentFileSize;
    gint64 mAudioSampleRate;
    gint64 mAudioChannels;
    gint64 mAudioBitrate;
    gint64 mVideoBitrate;

    GstClockTime mGst_info_start_time;
    static void debug_log (GstDebugCategory * category, GstDebugLevel level,
        const gchar * file, const gchar * function, gint line,
        GObject * object, GstDebugMessage * message, gpointer data);

    snd_hwdep_t *mHwdep_handle;

    // callback interface
      sp < AndroidGstCameraListener > mCameraListener;
  };

  class AndroidGstCameraListener:public CameraListener
  {
  public:
    AndroidGstCameraListener (GstMediaRecorder * recorder)
    {
      mRecorder = recorder;
    }
    virtual void notify (int32_t msgType, int32_t ext1, int32_t ext2)
    {
      GST_UNUSED (msgType);
      GST_UNUSED (ext1);
      GST_UNUSED (ext2);
    }
    virtual void postData (int32_t msgType, const sp < IMemory > &dataPtr);
    virtual void postDataTimestamp (nsecs_t timestamp, int32_t msgType,
        const sp < IMemory > &dataPtr);
    void release ()
    {
      mRecorder = NULL;
    }
  private:
    AndroidGstCameraListener ();
    GstMediaRecorder *mRecorder;
  };

};                              // namespace android
#endif //GSTMEDIARECODER_H
