/*
 * =====================================================================================
 *
 *       Filename:  GsticbAndroidVideoSink.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  02/16/2009 10:58:12 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
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

#ifndef __GST_ICBANDROIDVIDEOSINK_H__
#define __GST_ICBANDROIDVIDEOSINK_H__

/***** Gstreamer includes *****/
#include <gst/video/gstvideosink.h>

/***** Android includes *****/
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/MemoryHeapPmem.h>
#ifndef STECONF_ANDROID_VERSION_FROYO
#include <ui/ISurface.h>
#else
#include <surfaceflinger/ISurface.h>
#endif


using namespace android;

G_BEGIN_DECLS
#define GST_TYPE_ICB_ANDROID_VIDEO_SINK \
  (gst_icbandroidvideosink_get_type())
#define GST_ICB_ANDROID_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ICB_ANDROID_VIDEO_SINK, GstIcbAndroidVideoSink))
#define GST_ICB_ANDROID_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ICB_ANDROID_VIDEO_SINK, GstIcbAndroidVideoSinkClass))
#define GST_IS_ICB_ANDROID_VIDEO_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ICB_ANDROID_VIDEO_SINK))
#define GST_IS_ICB_ANDROID_VIDEO_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ICB_ANDROID_VIDEO_SINK))
typedef struct _GstIcbAndroidVideoSink GstIcbAndroidVideoSink;
typedef struct _GstIcbAndroidVideoSinkClass GstIcbAndroidVideoSinkClass;

struct _GstIcbAndroidVideoSink
{
  /* Our element stuff */
  GstVideoSink videosink;

    sp < ISurface > mSurface;

    sp < MemoryHeapBase > mFrameHeap;
    sp < MemoryHeapPmem > mFrameHeapPmem;

  /* Frame buffer support */
  static const int kBufferCount = 2;
  size_t mFrameBuffers[kBufferCount];
  int mFrameBufferIndex;

  gboolean mInitialized;

  int mFrameWidth;
  int mFrameHeight;

  // GstBuffer used to avoid buffer release while used by the UI
  int mGstBufferIndex;
  static const int mGstBuffersCount = 3;
  GstBuffer *mGstBuffers[mGstBuffersCount];
  GstPadEventFunction bsink_event;
};

struct _GstIcbAndroidVideoSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_icbandroidvideosink_get_type (void);

gboolean gst_icbandroidvideosink_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /*__GST_ICBANDROIDVIDEOSINK_H__*/
