/*
 * =====================================================================================
 *
 *       Filename:  GstMetadataRetriever.cpp
 * Copyright ST-Ericsson 2009
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

#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "GstMetadataRetriever"

#include "GstMetadataRetriever.h"
#include <utils/Log.h>

namespace android {

static GStaticMutex GstMetadataRetriever_mutex = G_STATIC_MUTEX_INIT;

GstMetadataRetriever::GstMetadataRetriever()
{
	mMode = METADATA_MODE_METADATA_RETRIEVAL_ONLY | METADATA_MODE_FRAME_CAPTURE_ONLY;
	mLocked = 0 ;

	LOGV("GstMetadataRetriever constructor");
	//g_static_mutex_lock (&GstMetadataRetriever_mutex);
	mGstDriver = new GstMetadataRetrieverDriver();
	LOGV("GstMetadataRetriever constructor exit");
}

GstMetadataRetriever::~GstMetadataRetriever()
{
	LOGV("GstMetadataRetriever destructor");
    mGstDriver->quit(); 
	if(mGstDriver) {
		delete mGstDriver;
	}
	mGstDriver = NULL;
	if (mLocked) {
		LOGV("GstMetadataRetriever destructor deactivate video protection");
		g_static_mutex_unlock(&GstMetadataRetriever_mutex);
	}
}
	

status_t GstMetadataRetriever::setDataSource(const char *url)
{
	status_t ret = OK;
	int status = 0;

	LOGV("GstMetadataRetriever setDataSource %s", url);

	mGstDriver->setDataSource(url);	
	mGstDriver->setup(mMode);
	mGstDriver->prepareSync();

	status  = mGstDriver->getStatus();

	LOGV("GstMetadataRetriever setDataSource %s", (status == GST_STATE_PAUSED)? "OK": "not correct state");
	if(status != GST_STATE_PAUSED)
		ret = UNKNOWN_ERROR;

	return ret;
}

status_t GstMetadataRetriever::setDataSource(int fd, int64_t offset, int64_t length)
{
	status_t ret = OK;
	int status = 0;

	LOGV("GstMetadataRetriever setDataSource fd=%d offset=%lld lenght=%lld", fd, offset, length);
	mGstDriver->setFdDataSource(fd, offset, length);	
	mGstDriver->setup(mMode);
    mGstDriver->prepareSync();
	
	status = mGstDriver->getStatus();
	LOGV("GstMetadataRetriever setDataSource %s", (status == GST_STATE_PAUSED)? "OK": "not correct state");
	if(status != GST_STATE_PAUSED)
		return UNKNOWN_ERROR;

    return ret;
}


status_t GstMetadataRetriever::setMode(int mode)
{
	LOGV("GstMetadataRetriever setMode mode=%d", mode);
	if (mode < METADATA_MODE_NOOP ||
        mode > METADATA_MODE_FRAME_CAPTURE_AND_METADATA_RETRIEVAL) 
	{
	   LOGE("set to invalid mode (%d)", mode);
       return BAD_VALUE;
	}
	
	if (mode & METADATA_MODE_FRAME_CAPTURE_ONLY) {
		if (!mLocked) {
			LOGV("GstMetadataRetriever setMode activate video protection");
			g_static_mutex_lock (&GstMetadataRetriever_mutex);
			mLocked = 1 ;
		} else {
			LOGV("GstMetadataRetriever::setMode video protection already activated");
		}
	} else { /* ! mode & METADATA_MODE_FRAME_CAPTURE_ONLY */
		if (mLocked) {
			LOGV("GstMetadataRetriever::setMode deactivate video protection");
			g_static_mutex_unlock (&GstMetadataRetriever_mutex);
			mLocked = 0 ;
		} else {
			LOGV("GstMetadataRetriever::setMode video protection already deactivated");
		}
	}
	mMode = mode;
	return OK;
}

status_t GstMetadataRetriever::getMode(int* mode) const
{
	*mode = mMode;
	LOGV("GstMetadataRetriever getMode mode%d", *mode);
	return OK;
}

VideoFrame* GstMetadataRetriever::captureFrame()
{
	int width, height, size;
	VideoFrame *vFrame = NULL;
	gint64 duration;

	LOGV("GstMetadataRetriever captureFrame");
	
	if (!mLocked) {
		LOGE("GstMetadataRetriever captureFrame video protection not activated => ERROR");
		return (NULL);
	}
	mGstDriver->getVideoSize(&width, &height);

	LOGV("GstMetadataRetriever captureFrame get video size %d x %d", width, height);
	// compute data size
	// FIXME: Check the Framebuffer color depth (if != RGB565)
	size = width * height * 2; // RGB565

	duration = mGstDriver->getDuration();
	if(duration) {
		mGstDriver->seekSync(duration/20);
	}

	if(size > 0) {
		vFrame = new VideoFrame();

		vFrame->mWidth = width;
    	vFrame->mHeight = height;
    	vFrame->mDisplayWidth  = width;
    	vFrame->mDisplayHeight = height;
    	vFrame->mSize = size;
		vFrame->mData = 0;

		mGstDriver->getCaptureFrame(&(vFrame->mData));

		if(vFrame->mData == 0) {
			LOGV("GstMetadataRetriever cant' allocate memory for video frame");
			delete vFrame;
			vFrame = NULL;
		}
	}

	return vFrame;
}

MediaAlbumArt* GstMetadataRetriever::extractAlbumArt()
{
	LOGV("GstMetadataRetriever extractAlbumArt");
	guint8* data = NULL;
	guint64 size = 0;

	mGstDriver->getAlbumArt(&data, &size);

	if(data && size) {
		MediaAlbumArt* albumArt = new MediaAlbumArt();
		albumArt->mSize = size;
        albumArt->mData = new uint8_t[size];
		memcpy(albumArt->mData, data, size);
		return albumArt; // must free by caller
	}

	return NULL;
}

const char* GstMetadataRetriever::extractMetadata(int keyCode)
{
	char * tag;
	char * ret;
	int msec;
	char* duration;

	LOGV("GstMetadataRetriever keyCode=%d", keyCode);

	switch (keyCode)
	{
		case METADATA_KEY_CD_TRACK_NUMBER:
			tag = strdup(GST_TAG_TRACK_NUMBER);
			break;
		case METADATA_KEY_ALBUM:   
			tag = strdup(GST_TAG_ALBUM);
			break;		
		case METADATA_KEY_AUTHOR:
		case METADATA_KEY_ARTIST:
			tag = strdup(GST_TAG_ARTIST);
			break;
		case METADATA_KEY_COMPOSER:
			tag = strdup(GST_TAG_COMPOSER);
			break;
		case METADATA_KEY_YEAR:
		case METADATA_KEY_DATE:
			tag = strdup(GST_TAG_DATE);
			break;
		case METADATA_KEY_GENRE:
			tag = strdup(GST_TAG_GENRE);
			break;
		case METADATA_KEY_TITLE:
			tag = strdup(GST_TAG_TITLE);
			break;
		case METADATA_KEY_DURATION:
		// Use Gst GetDuration instead of Tag one.
			msec = mGstDriver->getDuration();	
			duration = (char *)malloc(sizeof(char *)*55); 
			sprintf(duration,"%d",msec);
			return duration;
			//	tag = strdup(GST_TAG_DURATION);
			break;
		case METADATA_KEY_NUM_TRACKS:
			tag = strdup(GST_TAG_TRACK_COUNT);
			break;
		case METADATA_KEY_COMMENT:
			tag = strdup(GST_TAG_COMMENT);
			break;
		case METADATA_KEY_COPYRIGHT:
			tag = strdup(GST_TAG_COPYRIGHT);
			break;
		case METADATA_KEY_CODEC:
			tag = strdup(GST_TAG_CODEC);
			break;
		case METADATA_KEY_BIT_RATE:
			tag = strdup(GST_TAG_BITRATE);
			break;
		case METADATA_KEY_VIDEO_HEIGHT: 
			{
				int width, height;
				char *res;
				mGstDriver->getVideoSize(&width, &height);
				res = (char *)malloc(sizeof(char)*55); 
				sprintf(res,"%d",height);
				return res;
			}
		case METADATA_KEY_VIDEO_WIDTH:
			{
				int width, height;
				char *res;
				mGstDriver->getVideoSize(&width, &height);
				res = (char *)malloc(sizeof(char)*55); 
				sprintf(res,"%d",width);
				return res;
			}
		case METADATA_KEY_VIDEO_FORMAT:
			tag = strdup(GST_TAG_VIDEO_CODEC);
			break;

		case METADATA_KEY_FRAME_RATE:
			{
				int framerate;
				char *res;
				mGstDriver->getFrameRate(&framerate);
				res = (char *)malloc(sizeof(char)*55); 
				sprintf(res,"%d",framerate);
				return res;
			}
#ifdef STECONF_ANDROID_VERSION_FROYO
		case METADATA_KEY_WRITER:
			tag = strdup(GST_TAG_COMPOSER);
			break;
		case METADATA_KEY_MIMETYPE:
			tag = strdup(GST_TAG_CODEC);
			break;
		case METADATA_KEY_DISC_NUMBER:
			tag = strdup(GST_TAG_ALBUM_VOLUME_NUMBER);
			break;
		case METADATA_KEY_ALBUMARTIST:
			tag = strdup(GST_TAG_ALBUM_ARTIST);
			break;			
#endif
		case METADATA_KEY_IS_DRM_CRIPPLED:
		case METADATA_KEY_RATING:
		default:
			LOGV("unsupported metadata keycode %d", keyCode);
			return NULL;
	}

	LOGV("GstMetadataRetriever send request for |%s| ", tag);
	
	
	ret = mGstDriver->getMetadata(tag);
	
	LOGV("GstMetadataRetriever tag %s metadata %s", tag, ret);
	g_free(tag);

	return ret;
}

}; // namespace android
