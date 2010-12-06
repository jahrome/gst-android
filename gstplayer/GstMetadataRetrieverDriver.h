/*
 * =====================================================================================
 * 
 *       Filename:  GstMetadataRetrieverDriver.h
 *       Copyright ST-Ericsson 2009
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

#ifndef  GST_METADATARETRIEVER_DRIVER_INC
#define  GST_METADATARETRIEVER_DRIVER_INC

#include <gst/gst.h>
#include <utils/List.h>
#include <utils/Log.h>
#include <media/mediametadataretriever.h>

namespace android {


class GstMetadataRetrieverDriver
{
public:
	GstMetadataRetrieverDriver();
	~GstMetadataRetrieverDriver();

    void setup(int mode);
    void setDataSource(const char* url);
	void setFdDataSource(int fd, gint64 offset, gint64 length);
    void prepareSync();
    void seekSync(gint64 p);
    void quit();
    gint64 getPosition();
    gint64 getDuration();
    int	 getStatus();
	void getVideoSize(int* width, int* height);
    void endOfData();
	gchar* getMetadata(gchar* tag);
	void getCaptureFrame(guint8 **data);
	void getAlbumArt(guint8 **data, guint64 *size);
	void getFrameRate(int* framerate);

private:
	GstElement* mPipeline;
	GstElement* mAppsrc;
	gchar*		mUri;

	static GstBusSyncReply bus_message(GstBus *bus, GstMessage * msg, gpointer data);
	
	GstTagList *	mTag_list;

	void			parseMetadataInfo();

	guint64			mFdSrcOffset_min;
	guint64			mFdSrcOffset_max;
	guint64			mFdSrcOffset_current;
	gint			mFd;
	

	static gboolean have_video_caps (GstElement * uridecodebin, GstCaps * caps);
	static gboolean are_audio_caps (GstElement * uridecodebin, GstCaps * caps);
	static gboolean are_video_caps (GstElement * uridecodebin, GstCaps * caps);

	static gboolean autoplug_continue (GstElement* object, GstPad* pad, GstCaps* caps, GstMetadataRetrieverDriver* ed);

	static void		source_changed_cb (GObject *obj, GParamSpec *pspec, GstMetadataRetrieverDriver* ed);
	static void		need_data (GstElement * object, guint size, GstMetadataRetrieverDriver* ed);
	static gboolean	seek_data (GstElement * object, guint64 offset, GstMetadataRetrieverDriver* ed);

	int				mState;

	gboolean		mHaveStreamVideo;

	GstBuffer		*mAlbumArt;

	void			init_gstreamer();
	GstClockTime    mGst_info_start_time;
	static void		debug_log (GstDebugCategory * category, GstDebugLevel level,
							const gchar * file, const gchar * function, gint line,
							GObject * object, GstDebugMessage * message, gpointer data);

	int					mMode;
};

}; // namespace android

#endif   /* ----- #ifndef GST_METADATARETRIEVER_DRIVER_INC  ----- */

