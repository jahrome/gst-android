/*
 * =====================================================================================
 *
 *       Filename:  GsticbAndroidVideoSink.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  02/16/2009 10:58:34 AM
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

#define LOG_TAG "GstIcbAndroidVideoSink"

/* Uncomment the line below to see LOGV traces in release mode. 
 * See cutils/log.h for more information */
/*#define LOG_NDEBUG 0 */

/***** Android includes *****/ 
#include <binder/MemoryHeapBase.h>    /* DONT_MAP_LOCALLY */
#include <utils/Log.h>

/***** Gstreamer includes *****/
#include <gst/video/video.h>
#include <gst/base/gstbasesink.h>

/***** Icebird includes *****/
#include <gsticbvideo.h>
#include "GsticbAndroidVideoSink.h"

using namespace android;

/* Debugging category */
GST_DEBUG_CATEGORY_STATIC (gst_debug_gsticbandroidvideosink);
#define GST_CAT_DEFAULT gst_debug_gsticbandroidvideosink

/* ElementFactory information */
static const GstElementDetails gst_icbandroidvideosink_details =
GST_ELEMENT_DETAILS ((gchar*)"Icebird android video sink",
    (gchar*)"Sink/Video",
    (gchar*)"Icebird android video sink",
    (gchar*)"Benjamin Gaignard <Benjamin.Gaignard@stericsson.com>");

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, " 
                     "format = (fourcc) NV12," 
                     "width = (int) [16, 400],"  
                     "height = (int) [16, 352]," /* to allow CIFp 288x352 */
                     "framerate = (fraction) [0, 10000]"));

enum
{
  PROP_0,
  PROP_SURFACE,
  PROP_WIDTH,
  PROP_HEIGHT
};


static GstVideoSinkClass *parent_class = NULL;

static void 
gst_icbandroidvideosink_buffers_add(GstIcbAndroidVideoSink *sink, GstBuffer *newBuf)
{
  LOGV("sink->mGstBuffers[%d]=%p", sink->mGstBufferIndex, sink->mGstBuffers[sink->mGstBufferIndex]);
  /* check if there is an empty buffer in the array ? */
  if (sink->mGstBuffers[sink->mGstBufferIndex]){	
    /* unref the old buffer */
    gst_buffer_unref(sink->mGstBuffers[sink->mGstBufferIndex]);
  }

  /* save & ref the new buffer */
  sink->mGstBuffers[sink->mGstBufferIndex] = newBuf;
  gst_buffer_ref(sink->mGstBuffers[sink->mGstBufferIndex]);

  /* go to the next array item (cyclic array) */
  sink->mGstBufferIndex ++;
  if (sink->mGstBufferIndex == sink->mGstBuffersCount) {
    sink->mGstBufferIndex = 0;
  }
}

static void 
gst_icbandroidvideosink_buffers_clean(GstIcbAndroidVideoSink *sink)
{
  for (int i=0; i < sink->mGstBuffersCount; i++) {
    if (sink->mGstBuffers[i]) {
      gst_buffer_unref(sink->mGstBuffers[i]);
      sink->mGstBuffers[i] = NULL;
    }
  }
}


static gboolean
gst_icbandroidvideosink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
   return TRUE;
}

static GstFlowReturn
gst_icbandroidvideosink_show_frame (GstBaseSink * bsink, GstBuffer * inbuf)
{
  GstIcbAndroidVideoSink *sink = GST_ICB_ANDROID_VIDEO_SINK (bsink);

  if (sink->mSurface == NULL) {
    LOGD("mSurface not yet initialized");	
    return GST_FLOW_OK;
  }

  /* Initialization */
  if (!sink->mInitialized) {
    LOGV("android video sink initialization");
    int frameSize;

    /* test width and height */
    GstCaps * caps = GST_BUFFER_CAPS(inbuf);
    GstStructure *structure = NULL;

    structure = gst_caps_get_structure (caps, 0);

    if (!gst_structure_get_int (structure, "width", &sink->mFrameWidth) || 
        !gst_structure_get_int (structure, "height", &sink->mFrameHeight)) {
      LOGE("Can't get width and height");  
      sink->mFrameWidth = 240;
      sink->mFrameHeight = 160;
    }

    LOGV("Icebird android video sink width %d height %d", 
        sink->mFrameWidth, sink->mFrameHeight);

    if (GST_IS_ICBVIDEO_BUFFER(inbuf)) {
      /***** Hardware *****/
      LOGV("Hardware video sink (pmem and copybit)");

      video_frame_t *frame = &(GST_ICBVIDEO_BUFFER(inbuf)->frame);
      LOGV("Video frame pmem_fd %d, pmem_size %lu", 
          frame->pmem_fd, frame->pmem_size);

      /* Compute the "real" video size according to the woi */
      if (frame->flags & VIDEO_FRAME_WOI) {
        sink->mFrameWidth = frame->woi.dimensions.width;
        sink->mFrameHeight = frame->woi.dimensions.height;
      } else {
        sink->mFrameWidth = frame->desc.dimensions.width;
        sink->mFrameHeight = frame->desc.dimensions.height;
      }
      
      sink->mFrameHeap = new MemoryHeapBase(frame->pmem_fd, 
          frame->pmem_size, MemoryHeapBase::DONT_MAP_LOCALLY);

      if (sink->mFrameHeap->heapID() < 0) {
        LOGE("Error creating pmem heap");
        return GST_FLOW_OK;
      }

      LOGV("Create pmem heap");
      sink->mFrameHeap->setDevice("/dev/pmem");
      sink->mFrameHeapPmem = new MemoryHeapPmem(sink->mFrameHeap, 0);
      sink->mFrameHeapPmem->slap();
      sink->mFrameHeap.clear();

      LOGV("registerBuffers");
	  ISurface::BufferHeap buffers(sink->mFrameWidth, sink->mFrameHeight,
          ALIGN_FRAME_WIDTH(frame->desc.dimensions.width), 
          ALIGN_FRAME_HEIGHT(frame->desc.dimensions.height),
                                   PIXEL_FORMAT_YCbCr_420_SP,
          0, 
								   0,
          sink->mFrameHeapPmem);
	  sink->mSurface->registerBuffers(buffers);

    } 
    else
    {
      /***** Software *****/
      LOGV("Software video sink (memcpy)");

      /* FIXME check if color format is RGB565! */
      frameSize = sink->mFrameWidth * sink->mFrameHeight * 2; /* w*h*rgb565 size */
      /* create frame buffer heap and register with surfaceflinger */
      sink->mFrameHeap = new MemoryHeapBase(frameSize * sink->kBufferCount);
      if (sink->mFrameHeap->heapID() < 0) {
        LOGE("Error creating frame buffer heap");
        return GST_FLOW_OK;
      }
      
      ISurface::BufferHeap buffers(sink->mFrameWidth, sink->mFrameHeight,
				   sink->mFrameWidth, sink->mFrameHeight,
                                   PIXEL_FORMAT_RGB_565,
                                   0, 
				   0,
				   sink->mFrameHeap);
      sink->mSurface->registerBuffers(buffers);

      /* create frame buffers */
      for (int i = 0; i < sink->kBufferCount; i++) {
        sink->mFrameBuffers[i] = i * frameSize;
      }
      
      sink->mFrameBufferIndex = 0;
    }

    sink->mInitialized = TRUE;
  }

  /* Frame sink */
  if (GST_IS_ICBVIDEO_BUFFER(inbuf)) {
    /***** Hardware *****/
    LOGV("Hardware video sink (pmem and copybit)");

    video_frame_t *frame = &(GST_ICBVIDEO_BUFFER(inbuf)->frame);

    /* Insert this video buffer in the buffers array and 
    * "ref" it to postpone its recycle to give more time
    * for the UI to use the video buffer */
    gst_icbandroidvideosink_buffers_add(sink, inbuf);

    LOGV("post buffer: pmem_offset=%lu, cts=%d", frame->pmem_offset, frame->cts);
    sink->mSurface->postBuffer(frame->pmem_offset);
  } else {
    /***** Software *****/
    LOGV("Software video sink (memcpy)");

    memcpy(static_cast<char*>(sink->mFrameHeap->base()) + 
        sink->mFrameBuffers[sink->mFrameBufferIndex], GST_BUFFER_DATA(inbuf), 
        GST_BUFFER_SIZE(inbuf));

    LOGV("post buffer");
    sink->mSurface->postBuffer(sink->mFrameBuffers[sink->mFrameBufferIndex]);

    /* Prepare next buffer */
    sink->mFrameBufferIndex++;
    if (sink->mFrameBufferIndex == sink->kBufferCount) {
      sink->mFrameBufferIndex = 0;
    }
  }

  return GST_FLOW_OK;
}



/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

static void
gst_icbandroidvideosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIcbAndroidVideoSink *sink = GST_ICB_ANDROID_VIDEO_SINK (object);

  GST_OBJECT_LOCK (sink);

  switch (prop_id) {
    case PROP_SURFACE: {
      LOGV("Icebird Android video sink: set surface from void* to sp<ISurface>");
      ISurface * tmp_ptr = static_cast<ISurface*>(g_value_get_pointer (value));
      sink->mSurface = tmp_ptr;
      break;
    }
  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (sink);
}

static void
gst_icbandroidvideosink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstIcbAndroidVideoSink *sink = GST_ICB_ANDROID_VIDEO_SINK (object);

  GST_OBJECT_LOCK (sink);

  switch (prop_id) {
    case PROP_WIDTH:
      g_value_set_int (value, sink->mFrameWidth);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, sink->mFrameHeight);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (sink);
}

/* Finalize is called only once, dispose can be called multiple times.
 * We use mutexes and don't reset stuff to NULL here so let's register
 * as a finalize. */
static void
gst_icbandroidvideosink_finalize (GObject * object)
{
  GstIcbAndroidVideoSink *sink = GST_ICB_ANDROID_VIDEO_SINK (object);
 
  sink->mInitialized = FALSE;

  /* clean buffers list */
  gst_icbandroidvideosink_buffers_clean(sink);

  if(sink->mSurface != NULL ) {
    if (sink->mSurface.get())  {
      LOGV("unregisterBuffers");
      sink->mSurface->unregisterBuffers();
      sink->mSurface.clear();
    }

    /* free frame buffers */
    LOGV("free frame buffers");
    for (int i = 0; i < sink->kBufferCount; i++) 
      sink->mFrameBuffers[i] = 0;

    /* free heaps */
    LOGV("free mFrameHeap");
    sink->mFrameHeap.clear();
    LOGV("free mFrameHeapPmem");
    sink->mFrameHeapPmem.clear();

  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_icbandroidvideosink_sink_event (GstPad * pad, GstEvent * event)
{
  GstIcbAndroidVideoSink *sink = (GstIcbAndroidVideoSink *) (GST_OBJECT_PARENT (pad));
  GstBaseSinkClass *bclass;
  GstBaseSink *bsink;
  GstPadEventFunction event_base_sink;
  gboolean ret = TRUE;

  bsink = GST_BASE_SINK (gst_pad_get_parent (pad));
  bclass = GST_BASE_SINK_GET_CLASS (bsink);

  event_base_sink =  GST_PAD_EVENTFUNC (bsink->sinkpad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      /* flush pending ref-ed buffers */
      gst_icbandroidvideosink_buffers_clean(sink);
      break;

    default:
      break;
  }

  /* call base sink (we just add some 
     specific actions and wanted 
     to keep base sink way...) */
  sink->bsink_event(pad, event);

  gst_object_unref (bsink);

  return ret;
}


static void
gst_icbandroidvideosink_init (GstIcbAndroidVideoSink * sink)
{
  GstBaseSinkClass *bclass;
  GstBaseSink *bsink;

  bsink = GST_BASE_SINK (sink);
  bclass = GST_BASE_SINK_GET_CLASS (bsink);

  /* override max-lateness to 100ms instead of default 
     one (20ms) to limit frame drop on high-load system */
  gst_base_sink_set_max_lateness (bsink, 100 * GST_MSECOND);

  sink->mInitialized = FALSE;
  sink->mSurface = NULL;

  /* basesink event callback surcharge
    to intercept FLUSH event in order
    to flush pending ref-ed buffers when
    seeking */
  sink->bsink_event =  GST_PAD_EVENTFUNC (bsink->sinkpad);
  gst_pad_set_event_function (bsink->sinkpad,
      GST_DEBUG_FUNCPTR (gst_icbandroidvideosink_sink_event));

  /* initialize buffers array */
  sink->mGstBufferIndex = 0;
  for (int i = 0; i < sink->mGstBuffersCount; i++) 
      sink->mGstBuffers[i] = 0;
}

static void
gst_icbandroidvideosink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_set_details (element_class, &gst_icbandroidvideosink_details);
}

static void
gst_icbandroidvideosink_class_init (GstIcbAndroidVideoSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;

  parent_class = (GstVideoSinkClass *) g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_icbandroidvideosink_set_property;
  gobject_class->get_property = gst_icbandroidvideosink_get_property;
  gobject_class->finalize = gst_icbandroidvideosink_finalize;

  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_icbandroidvideosink_setcaps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_icbandroidvideosink_show_frame);
  gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_icbandroidvideosink_show_frame);


  /* install properties */
  g_object_class_install_property (gobject_class, PROP_SURFACE,
      g_param_spec_pointer ("surface", "Surface",
          "The target surface for video", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "Width", "video width",
          0x0000, 0xFFFF, 240, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "Height", "video height",
          0x0000, 0xFFFF, 160, G_PARAM_READABLE));

}

GType
gst_icbandroidvideosink_get_type (void)
{
  static GType icbandroidvideosink_type = 0;

  if (!icbandroidvideosink_type) {
    static const GTypeInfo icbandroidvideosink_info = {
      sizeof (GstIcbAndroidVideoSinkClass),
      gst_icbandroidvideosink_base_init,
      NULL,
      (GClassInitFunc) gst_icbandroidvideosink_class_init,
      NULL,
      NULL,
      sizeof (GstIcbAndroidVideoSink),
      0,
      (GInstanceInitFunc) gst_icbandroidvideosink_init,
      NULL
    };

    icbandroidvideosink_type =
        g_type_register_static (GST_TYPE_VIDEO_SINK, "icbandroidvideosink",
        &icbandroidvideosink_info, (GTypeFlags)0);
    GST_DEBUG_CATEGORY_INIT (gst_debug_gsticbandroidvideosink, 
        "icbandroidvideosink", 0, "Icebird android video sink");
  }

  return icbandroidvideosink_type;
}


gboolean
gst_icbandroidvideosink_plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "icbandroidvideosink", GST_RANK_PRIMARY,
      GST_TYPE_ICB_ANDROID_VIDEO_SINK))
    return FALSE;

  return TRUE;
}
