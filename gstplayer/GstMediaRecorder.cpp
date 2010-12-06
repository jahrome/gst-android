/*
 * =====================================================================================
 *
 *	   Filename:  GstMediaRecorder.cpp
 *
 *	Description:  
 *
 *		Version:  1.0
 *		Created:  03/31/2009 01:40:04 PM
 *	   Revision:  none
 *	   Compiler:  gcc
 *
 *		 Author:  Benjamin Gaignard
 *		Company:  STEricsson
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
#define LOG_TAG "GstMediaRecorder"

#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <asnd_api.h>

#include "GstMediaRecorder.h"
#include <utils/Log.h>
#ifndef STECONF_ANDROID_VERSION_FROYO
#include <ui/CameraParameters.h>
#include <utils/Errors.h>
#include <media/mediarecorder.h>
#include <media/AudioSystem.h>
#include <ui/ISurface.h>
#include <ui/ICamera.h>
#include <ui/Camera.h>
#else 
#include <camera/CameraParameters.h>
#include <utils/Errors.h>
#include <media/AudioSystem.h>
#include <surfaceflinger/ISurface.h>
#include <camera/ICamera.h>
#include <camera/Camera.h>
#endif
#include <fcntl.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappbuffer.h>
#include <gsticbvideo.h>
#include <binder/MemoryBase.h>
#include <cutils/properties.h>

using namespace android;

GstMediaRecorder::GstMediaRecorder()
{
	LOGV("GstMediaRecorder constructor");
	mCamera = NULL;
	mSurface = NULL;
	mFlags = 0;
	
	mVideoBin = NULL;
	mAudioBin = NULL;
	mPipeline = NULL;
	
	mUse_video_src = FALSE;
	mUse_audio_src = FALSE;
	
	mVideoSrc = NULL;
	mAudioSrc = NULL;

	mOutFilePath = NULL;

	mMaxDuration = -1;
	mMaxFileSize = -1;
	mCurrentFileSize = 0;
	mTimer = NULL;

	//default init
	mFps = 15;
	mWidth = 176;
	mHeight = 144;
	mOutput_format = OUTPUT_FORMAT_DEFAULT;
	mVideo_encoder = VIDEO_ENCODER_DEFAULT;
	mAudio_encoder = AUDIO_ENCODER_DEFAULT;
	mAudio_source  = AUDIO_SOURCE_MIC;

	mAudioSampleRate = 48000;
	mAudioChannels = 2;
	mAudioBitrate = 192000;
	mVideoBitrate = 786432;
	mVTMode = 0;
	mIPeriod = 0;
	mIMBRefreshMode = 0;

	if (!g_thread_supported ()) {
		LOGV("GstMediaRecorder GLib thread init");
		g_thread_init (NULL);
	}

	// setup callback listener
	mCameraListener = new AndroidGstCameraListener(this);

	/* create and init the EOS mutex now */
	mEOSlock = g_mutex_new ();
	g_mutex_lock (mEOSlock);
	mIsEos = FALSE;

	if(snd_hwdep_open (&mHwdep_handle,"hw:0,0", O_RDWR) < 0){
		LOGE("Error %d opening hwdep device\n", errno);
	}
}

GstMediaRecorder::~GstMediaRecorder()
{
	LOGV("GstMediaRecorder destructor");

	if (mCamera != NULL) {
		mCamera->setListener(NULL);
		if ((mFlags & FLAGS_HOT_CAMERA) == 0) {
			LOGV("GstMediaRecorder camera was cold when we started, stopping preview");
			mCamera->stopPreview();
		}
		if (mFlags & FLAGS_SET_CAMERA) {
			LOGV("GstMediaRecorder unlocking camera to return to app");
			mCamera->unlock();
		} else {
			LOGV("GstMediaRecorder disconnect from camera");
			mCamera->disconnect();
		}
		mCamera.clear();
	}

	mFlags = 0;

	// don't send eos but release the pipeline
	release_pipeline();

	if(mOutFilePath) {
		g_free(mOutFilePath);
	}
	mCameraListener.clear();

	// free mutex
	g_mutex_free (mEOSlock);
	mEOSlock = NULL;
	
	// free timer
	g_timer_destroy(mTimer);
	mTimer = NULL;

	if(mHwdep_handle) {
		snd_hwdep_close(mHwdep_handle);
	}

}

status_t GstMediaRecorder::init()
{
	LOGV("GstMediaRecorder init");

	return OK;
}

status_t GstMediaRecorder::setAudioSource(audio_source as)
{
	//LOGV("GstMediaRecorder setAudioSource %s", (as==AUDIO_SOURCE_DEFAULT)?"AUDIO_SOURCE_DEFAULT":"AUDIO_SOURCE_MIC");
	mAudio_source = as;
	mUse_audio_src = TRUE;
	switch (as)
	{
	case AUDIO_SOURCE_DEFAULT:
		LOGV("GstMediaRecorder setAudioSource DEFAULT (MIC)");
		//the default value is equal to AUDIO_SOURCE_MIC	
		mAudio_source = AUDIO_SOURCE_MIC;
		break;
	case AUDIO_SOURCE_MIC:
		LOGV("GstMediaRecorder setAudioSource MIC");
		break;		
	case AUDIO_SOURCE_VOICE_UPLINK:
		LOGV("GstMediaRecorder setAudioSource VOICE_UPLINK");
		break;		
	case AUDIO_SOURCE_VOICE_DOWNLINK:
		LOGV("GstMediaRecorder setAudioSource VOICE_DOWNLINK");
		break;		
	case AUDIO_SOURCE_CAMCORDER:
		LOGV("GstMediaRecorder setAudioSource CAMCORDER");
		break;			
	case AUDIO_SOURCE_VOICE_RECOGNITION:
		LOGV("GstMediaRecorder setAudioSource VOICE_RECOGNITION");
		break;			
	case AUDIO_SOURCE_VOICE_CALL:
		LOGV("GstMediaRecorder setAudioSource VOICE_CALL");
		break;		
	default:
		break;
	}
	return OK;
}

status_t GstMediaRecorder::setVideoSource(video_source vs)
{
	LOGV("GstMediaRecorder setVideoSource %s", (vs==VIDEO_SOURCE_DEFAULT)?"VIDEO_SOURCE_DEFAULT":"VIDEO_SOURCE_CAMERA");
	switch (vs) 
	{
	case VIDEO_SOURCE_DEFAULT:
		//the default value is equal to VIDEO_SOURCE_CAMERA
		mUse_video_src = TRUE;
		break;
	case VIDEO_SOURCE_CAMERA:
		mUse_video_src = TRUE;
		break;
	default: 
		mUse_video_src = FALSE;
		break;
	}
	return OK;
}

status_t GstMediaRecorder::setOutputFormat(output_format of)
{
	LOGV("GstMediaRecorder setOutputFormat %d", of);
	mOutput_format = of;

	switch(of)
	{
	case OUTPUT_FORMAT_DEFAULT:
		LOGV("GstMediaRecorder setOutputFormat DEFAULT (3GPP)");
		mOutput_format = OUTPUT_FORMAT_THREE_GPP;
		break;
	case OUTPUT_FORMAT_THREE_GPP:
		LOGV("GstMediaRecorder setOutputFormat 3GPP");
		break;
	case OUTPUT_FORMAT_MPEG_4:
		LOGV("GstMediaRecorder setOutputFormat MPEG4");
		break;
	case OUTPUT_FORMAT_RAW_AMR:
		LOGV("GstMediaRecorder setOutputFormat RAW AMR (AMR NB)");
		break;
	case OUTPUT_FORMAT_LIST_END:
		break;
	case OUTPUT_FORMAT_AMR_WB: 
		LOGV(" AMR WB");
		break;
	case OUTPUT_FORMAT_AAC_ADIF: 
		LOGV(" AAC ADIF");
		break;
	case OUTPUT_FORMAT_AAC_ADTS: 
		LOGV(" AAC ADTS");
		break;
	}
	return OK;
}

status_t GstMediaRecorder::setAudioEncoder(audio_encoder ae)
{
	//LOGV("GstMediaRecorder setAudioEncoder %s", (ae==AUDIO_ENCODER_DEFAULT)?"AUDIO_ENCODER_DEFAULT":"AUDIO_ENCODER_AMR_NB");
	mAudio_encoder = ae;	
	switch(mAudio_encoder)
	{
	case AUDIO_ENCODER_DEFAULT:
	case AUDIO_ENCODER_AMR_NB:
		LOGV("GstMediaRecorder setAudioEncoder AMR NB");
		mAudio_encoder = AUDIO_ENCODER_AMR_NB;	
		break;
	case AUDIO_ENCODER_AMR_WB:
		LOGV("GstMediaRecorder setAudioEncoder AMR WB");
		break;
	case AUDIO_ENCODER_AAC:
		LOGV("GstMediaRecorder setAudioEncoder AAC");
		break;
	case AUDIO_ENCODER_AAC_PLUS:
		LOGV("GstMediaRecorder setAudioEncoder AAC PLUS");
		break;
	case AUDIO_ENCODER_EAAC_PLUS:
		LOGV("GstMediaRecorder setAudioEncoder EAAC PLUS");
		break;
	default: 
		LOGV("GstMediaRecorder setAudioEncoder AMR NB");
		mAudio_encoder = AUDIO_ENCODER_AMR_NB;	
		break;
	}
	return OK;
}

status_t GstMediaRecorder::setVideoEncoder(video_encoder ve)
{
	LOGV("GstMediaRecorder setVideoEncoder %d", ve);
	mVideo_encoder = ve;
	switch(mVideo_encoder)
	{
		case VIDEO_ENCODER_DEFAULT:
			LOGV("GstMediaRecorder setVideoEncoder DEFAULT (MPEG4)");
			mVideo_encoder = VIDEO_ENCODER_MPEG_4_SP;
			break;
		case VIDEO_ENCODER_H263:
			LOGV("GstMediaRecorder setVideoEncoder H263");
			break;
		case VIDEO_ENCODER_H264:
			LOGV("GstMediaRecorder setVideoEncoder H264");
			break;
		case VIDEO_ENCODER_MPEG_4_SP:
			LOGV("GstMediaRecorder setVideoEncoder MPEG4");
			break;
	}
	return OK;
}

status_t GstMediaRecorder::setVideoSize(int width, int height)
{
	LOGV("GstMediaRecorder setVideoSize width=%d height=%d", width, height);
	mWidth = width;
	mHeight = height;
	return OK;
}

status_t GstMediaRecorder::setVideoFrameRate(int frames_per_second)
{
	LOGV("GstMediaRecorder setVideoFrameRate %d fps", frames_per_second);
	mFps = frames_per_second;
	return OK;
}

status_t GstMediaRecorder::setCamera(const sp<ICamera>& camera)
{
	LOGV("GstMediaRecorder setCamera");
	
	mFlags &= ~ FLAGS_SET_CAMERA | FLAGS_HOT_CAMERA;
	if (camera == NULL) {
		LOGV("camera is NULL");
		return OK;
	}

	// Connect our client to the camera remote
	mCamera = Camera::create(camera);
	if (mCamera == NULL) {
		LOGV("Unable to connect to camera");
		return OK;
	}

	LOGV("Connected to camera");
	mFlags |= FLAGS_SET_CAMERA;
	if (mCamera->previewEnabled()) {
		mFlags |= FLAGS_HOT_CAMERA;
		LOGV("camera is hot");
	}
	mUse_video_src = TRUE;
	return OK;
}

status_t GstMediaRecorder::setPreviewSurface(const sp<ISurface>& surface)
{
	LOGV("GstMediaRecorder setPreviewSurface");
	mSurface = surface;
	return OK;
}

status_t GstMediaRecorder::setOutputFile(const char *path)
{
	LOGV("GstMediaRecorder setOutputFile %s", path);
	mOutFilePath = g_strdup_printf("file://%s", path);
		mOutFilePath_fd = -1;
	return OK;
}
status_t GstMediaRecorder::setOutputFile(int fd, int64_t offset, int64_t length)
{
	LOGV("GstMediaRecorder setOutputFile for fd : fd=%d offset=%lld length=%lld", fd, offset, length);
	GST_UNUSED(offset);
	GST_UNUSED(length);
	mOutFilePath = g_strdup_printf("fd://%d",fd);
		mOutFilePath_fd = fd;
	return OK;
}

status_t GstMediaRecorder::setParameters(const String8& params)
{
	LOGV("GstMediaRecorder setParameters");

	if(strstr(params, "max-duration") != NULL) {
		sscanf(params,"max-duration=%lld", &mMaxDuration);
	}
	if(strstr(params, "max-filesize") != NULL) {
		sscanf(params,"max-filesize=%lld", &mMaxFileSize);
	}
	if(strstr(params, "audio-param-sampling-rate") != NULL) {
		sscanf(params,"audio-param-sampling-rate=%lld", &mAudioSampleRate);
			if ( (mAudioSampleRate < 8000) || (mAudioSampleRate > 48000) )
			mAudioSampleRate = 48000;

	}
	if(strstr(params, "audio-param-number-of-channels") != NULL) {
		sscanf(params,"audio-param-number-of-channels=%lld", &mAudioChannels);
		if ( (mAudioChannels < 0) || (mAudioChannels > 2) )
			mAudioChannels = 2;
	}
	if(strstr(params, "audio-param-encoding-bitrate") != NULL) {
		sscanf(params,"audio-param-encoding-bitrate=%lld", &mAudioBitrate);
		if ( (mAudioBitrate < 0) || (mAudioBitrate > 192000) )
			mAudioBitrate = 128000;
	}
	if(strstr(params, "video-param-encoding-bitrate") != NULL) {
		sscanf(params,"video-param-encoding-bitrate=%lld", &mVideoBitrate);
		if ( (mVideoBitrate < 0) || (mVideoBitrate > 786432) )
			mVideoBitrate = 360000;
	} 
	if(strstr(params, "vt-mode") != NULL) {
		sscanf(params,"vt-mode=%d", &mVTMode);
	}
	if(strstr(params, "i-mb-refresh") != NULL) {
		sscanf(params,"i-mb-refresh=%d", &mIMBRefreshMode);
	}
	if(strstr(params, "i-period") != NULL) {
		sscanf(params,"i-period=%d", &mIPeriod);
	}
	if(strstr(params, "video-bitrate") != NULL) {
		sscanf(params,"video-bitrate=%lld", &mVideoBitrate);
	}
	//if (mCamera != NULL) {
		//send the parameters to the camera to set specific effect or others parameters
	//	mCamera->setParameters(params);
	//}
	LOGV("GstMediaRecorder  max duration %lld max file size %lld", mMaxDuration, mMaxFileSize);
	return OK;
}
status_t GstMediaRecorder::setListener(const sp<IMediaPlayerClient>& listener)
{
	LOGV("GstMediaRecorder setListener");
	mListener = listener;
	return OK;
}

status_t GstMediaRecorder::prepare()
{
	LOGV("GstMediaRecorder prepare");

	 // create a camera if the app didn't supply one
	if ((mCamera == 0) && (mUse_video_src == TRUE)) {
		mCamera = Camera::connect();
	}

	if (mCamera != NULL && mSurface != NULL) {
		LOGV("GstMediaRecorder set preview display surface");
		mCamera->setPreviewDisplay(mSurface);
	}
	
	if (mCamera != NULL) {
		LOGV("GstMediaRecorder set camera parameters width=%d height=%d fps=%d", mWidth, mHeight, mFps);
		String8 s = mCamera->getParameters();
		CameraParameters p(s);
		p.setPreviewSize(mWidth, mHeight);

		if (mCamera->previewEnabled()) {
			s = p.flatten();
			mCamera->setParameters(s);
			mFlags |= FLAGS_HOT_CAMERA;
			LOGV("GstMediaRecorder preview camera already enabled");
		}else {
			p.setPreviewFrameRate(mFps);
			s = p.flatten();
			mCamera->setParameters(s);
			mCamera->startPreview();
			mFlags &= ~FLAGS_HOT_CAMERA;
		}
	}

	return build_record_graph();
}

typedef struct {
	sp<IMemory>	frame;
	sp<Camera>	camera;
} record_callback_cookie; 

static void video_frame_release(GstICBVideoBuffer* buffer)
{
	//LOGE("GstMediaRecorder video frame release");

	record_callback_cookie* cookie = (record_callback_cookie*)(buffer->ctx);

	cookie->camera->releaseRecordingFrame(cookie->frame);

	cookie->frame.clear();

	g_free(cookie);
}

/*static*/ void GstMediaRecorder::record_callback(const sp<IMemory>& frame, void *cookie)
{
	ssize_t offset = 0;
	size_t size = 0;
	video_frame_t video_frame = VIDEO_FRAME_INIT;
	GstBuffer* buffer;
	GstClockTime duration;

	//LOGE("GstMediaRecorder 	record callback");
	record_callback_cookie * lcookie = g_new0 (record_callback_cookie, 1);
	sp<IMemoryHeap> heap = frame->getMemory(&offset, &size);
	
	GstMediaRecorder* mediarecorder = (GstMediaRecorder*) cookie;  
	if(mediarecorder->mVideoSrc == NULL) {
		LOGV("GstMediaRecorder record_callback the videosrc don't exist");
		mediarecorder->mCamera->stopRecording();
		return ;
	}

	video_frame.pmem_fd = heap->getHeapID();
	video_frame.pmem_offset = offset;
	video_frame.pmem_size = size;

	lcookie->frame = frame;
	lcookie->camera = mediarecorder->mCamera;

	buffer = gst_icbvideo_buffer_new(&video_frame, (GstMiniObjectFinalizeFunction) video_frame_release, 
			lcookie,
			GST_ELEMENT(mediarecorder->mVideoSrc));
	
	GST_BUFFER_SIZE(buffer) = size; //needed to build correct timestamp in basesrc

	duration = gst_util_uint64_scale_int (GST_SECOND, 1, mediarecorder->mFps);
	GST_BUFFER_DURATION(buffer) = duration; //needed to build correct duration in basesrc

	gst_app_src_push_buffer(GST_APP_SRC(mediarecorder->mVideoSrc), buffer);
}

GstStateChangeReturn GstMediaRecorder::wait_for_set_state(int timeout_msec)
{
	GstMessage *msg;
	GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

	/* Wait for state change */
	msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS(mPipeline),
			timeout_msec * GST_MSECOND, /* in nanosec */
			(GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE));

	if (msg) {
		if ((GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ASYNC_DONE))
			ret = GST_STATE_CHANGE_SUCCESS;

		gst_message_unref(msg);
	}

	return ret;
}

status_t GstMediaRecorder::start()
{
	GstStateChangeReturn ret;
	LOGV("GstMediaRecorder start recording");

	if(mPipeline == NULL) {
		LOGV("GstMediaRecorder start pipeline not created");
		return OK;
	}

	ret = gst_element_set_state (mPipeline, GST_STATE_PLAYING);
	
	// set the audio source device, open micro
	const sp<IAudioFlinger>& audioFlinger = AudioSystem::get_audio_flinger();
	if (audioFlinger != 0) {
		LOGV("GstMediaRecorder start recording: unmute the microphone");	
		audioFlinger->setMicMute(FALSE);
	}

	if (mCamera != NULL) {
		mCamera->setListener(mCameraListener);
		mCamera->startRecording();
	}

	if( ret == GST_STATE_CHANGE_ASYNC) {
		ret = wait_for_set_state(2000); // wait 2 second for state change
	}

	if(ret != GST_STATE_CHANGE_SUCCESS) {
		goto bail;
	}
	
	LOGV("GstMediaRecorder pipeline is in playing state");
	return OK;

bail:

	LOGV("GstMediaRecorder start failed");

	if (mCamera != NULL) {
		mCamera->stopRecording();
	}

	release_pipeline();

	return OK; // return OK to avoid execption in java
}

status_t GstMediaRecorder::stop()
{
	LOGV("GstMediaRecorder stop recording");

	if(mPipeline == NULL) {
		LOGV("GstMediaRecorder stop pipeline not created");
		return OK;
	}

	if (mCamera != NULL) {	
		mCamera->stopRecording();
		mCamera->setListener(NULL);
	}
	
	/* Send audio & video Eos */
	sendEos();
	
	if (mIsEos)	 
      g_mutex_lock (mEOSlock);
	
	// EOS has been receive now release the pipeline 
	return release_pipeline();

}

status_t GstMediaRecorder::release_pipeline()
{
	if(mPipeline == NULL) {
		return OK;
	}

	LOGV("GstMediaRecorder change pipeline state to NULL");
	gst_element_set_state (mPipeline, GST_STATE_NULL);
	gst_element_get_state (mPipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	LOGV("GstMediaRecorder unref pipeline");
	gst_object_unref(mPipeline);
	mPipeline = NULL;
	mVideoBin = NULL;
	mAudioBin = NULL;
	mVideoSrc = NULL;

	if (mOutFilePath_fd > -1) {
		::close(mOutFilePath_fd);
		mOutFilePath_fd = -1;
	}


	LOGV("GstMediaRecorder stop exit");

	return OK;
}

status_t GstMediaRecorder::close()
{
	LOGV("GstMediaRecorder close");
	
	return OK;
}

status_t GstMediaRecorder::reset()
{
	LOGV("GstMediaRecorder reset");
	release_pipeline();

	return OK;
}

status_t GstMediaRecorder::getMaxAmplitude(int *max)
{
	int ioParam;

	LOGV("GstMediaRecorder getMaxAmplitude");

	ioParam = 5; // device C0
	if(snd_hwdep_ioctl(mHwdep_handle,ASND_HWDEP_IOCTL_GET_MAX_AMP, (void*)&ioParam)<0) {
		LOGE("error : get max amplitude returned %d\n", errno);
		*max = 0;
		return OK;
	}
	*max = ioParam;
		
	return OK;
}

// create a video bin appsrc->icbvideoenc->capsfilter
GstElement* GstMediaRecorder::create_video_bin()
{
	GstElement *vbin;
	GstElement *video_src;
	GstElement *video_encoder, *video_format_filter;
	GstElement *video_queue;
	GstPad *pad;

	video_queue = NULL;

	if(mUse_video_src == FALSE) {
		// nothing the create in this case
		return NULL;
	}

	LOGV("GstMediaRecorder create_video_bin");

	LOGV("GstMediaRecorder create video appsrc");
	video_src = gst_element_factory_make("appsrc", "videosrc");
	if(!video_src) {
		LOGV("GstMediaRecorder can't create video src");
		return NULL;
	}

	g_object_set(G_OBJECT(video_src),"is-live", TRUE, NULL); // it is a pseudo live source
	g_object_set(G_OBJECT(video_src),"max-bytes", (guint64)mWidth*mHeight*3, NULL); // max byte limit equal to 2 frames
	g_object_set(G_OBJECT(video_src),"format", 2, NULL); // byte format 
	g_object_set(G_OBJECT(video_src),"block", true, NULL); // Block push-buffer when max-bytes are queued

	g_object_set(G_OBJECT(video_src) ,"caps", 
				gst_caps_new_simple ("video/x-raw-yuv", 
								"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('N','V','1','2'),
								"width", G_TYPE_INT, mWidth,
								"height", G_TYPE_INT, mHeight,	 
								"framerate", GST_TYPE_FRACTION, mFps, 1,
								 NULL),
				NULL);


	video_encoder = gst_element_factory_make("icbvideoenc",NULL);
		
	if(!video_encoder) {
			LOGE("GstMediaRecorder can't create video encoder");
			goto remove_video_src;
	}
	
	video_format_filter = gst_element_factory_make("capsfilter",NULL);
	
	if(!video_format_filter) {
		LOGE("GstMediaRecorder can't create video format filter");
		goto remove_video_encoder;
	}
	
	switch(mVideo_encoder)
	{
		case VIDEO_ENCODER_DEFAULT:
		case VIDEO_ENCODER_MPEG_4_SP:
			LOGV("GstMediaRecorder set video caps: video/mpeg, width=%d, height=%d, framerate=%d/1", mWidth, mHeight, mFps);
			g_object_set(G_OBJECT(video_format_filter) , "caps",
									gst_caps_new_simple ("video/mpeg", 
														 "width", G_TYPE_INT, mWidth,
														 "height", G_TYPE_INT, mHeight,	 
//VT														 "framerate", GST_TYPE_FRACTION, mFps, 1,
														 "mpegversion", G_TYPE_INT, 4,
														 NULL),		
									NULL);
			break;
		case VIDEO_ENCODER_H264:
			LOGV("GstMediaRecorder can't encode in h264");
			goto remove_video_format_filter;
		case VIDEO_ENCODER_H263:
		default:
			LOGV("GstMediaRecorder set video caps: video/x-h263, width=%d, height=%d, framerate=%d/1", mWidth, mHeight, mFps);
			g_object_set(	G_OBJECT(video_format_filter) , "caps", 
							gst_caps_new_simple (	"video/x-h263", 
													"width", G_TYPE_INT, mWidth,
													"height", G_TYPE_INT, mHeight,	 
//VT												"framerate", GST_TYPE_FRACTION, mFps, 1,
													NULL ), 
							NULL );
			break;
	}


	/* VT support */
	{
		GValue framerate = { 0 };
	int framerate_num = mFps;
	int framerate_denom = 1;
	int bitrate = mVideoBitrate;
	int i_period = mIPeriod;
	int i_mb_refresh = mIMBRefreshMode;
	int vt_mode = mVTMode;

	if (vt_mode) {
		g_object_set(G_OBJECT(video_encoder), "vt-mode", vt_mode, NULL);
	}
	if (bitrate) {
		g_object_set(G_OBJECT(video_encoder), "bitrate", bitrate, NULL);
	}
	if (i_period) {
		/* in seconds, convert to nb of frames */
		i_period = i_period*framerate_num/framerate_denom;
		g_object_set(G_OBJECT(video_encoder), "i-period", i_period, NULL);
	}
	if (i_mb_refresh) {
		g_object_set(G_OBJECT(video_encoder), "i-mb-refresh", i_mb_refresh, NULL);
	}

		/* ! take care of framerate because of fraction type, 
		 use g_object_set_property with a gvalue instead g_object_set */
		g_value_init (&framerate, GST_TYPE_FRACTION);
		gst_value_set_fraction (&framerate, framerate_num, framerate_denom);
		g_object_set_property(G_OBJECT(video_encoder), "framerate", &framerate);
		g_value_unset(&framerate);
	}

	video_queue = gst_element_factory_make("queue", NULL);
	g_object_set(G_OBJECT(video_queue), "max-size-time", 2000000000, NULL);

		
	LOGV("GstMediaRecorder create vbin");
	vbin = gst_bin_new("vbin");
	if(!vbin) {
		LOGE("GstMediaRecorder can't create vbin");
		goto remove_video_format_filter;
	}

	gst_bin_add_many (GST_BIN_CAST(vbin), video_src, video_encoder, video_format_filter, video_queue, NULL);

	LOGV("GstMediaRecorder link video_src->->queue->video_encoder->video_format_filter->queue");
	if(!gst_element_link_many(video_src, video_encoder, video_format_filter, video_queue, NULL)) {
		LOGE("GstMediaRecorder can't link elements");
		goto remove_vbin;
	}

	LOGV("GstMediaRecorder create src ghost pad in vbin");
	pad = gst_element_get_static_pad (video_queue, "src");
	gst_element_add_pad (vbin, gst_ghost_pad_new ("src", pad));
	gst_object_unref (pad);

	mVideoSrc = video_src;

	return vbin;

remove_vbin:
	gst_object_unref(vbin);
remove_video_format_filter:
	gst_object_unref(video_format_filter);
	gst_object_unref(video_queue);
remove_video_encoder:
	gst_object_unref(video_encoder);
remove_video_src:
	gst_object_unref(video_src);
	return NULL;
}

// create a audio bin icbaudiosrc->icbaudioenc->capsfilter
GstElement*	GstMediaRecorder::create_audio_bin()
{
	GstElement *abin;
	GstElement *audio_src, *audio_enc, *audio_format_filter;
	GstElement *audio_queue;
	GstPad *pad;
	gint recordsrc;

	if(mUse_audio_src == FALSE) {
		return NULL;
	}
	LOGV("GstMediaRecorder create_audio_bin");

	LOGV("GstMediaRecorder create audio src");
	audio_src = gst_element_factory_make("icbaudiosrc","icbaudiosrc0"); // do not change the element name
	
	if(!audio_src) {
		LOGE("GstMediaRecorder can't create audio source");
		return NULL;
	}

	// set the audio source device
	LOGV("GstMediaRecorder set device to audio src");
	g_object_set(G_OBJECT(audio_src), "device", "C0", NULL); 

	// set the record source
	LOGV("GstMediaRecorder set record src");
	recordsrc = mAudio_source;
	if (recordsrc > 0 ) recordsrc--;
	g_object_set(G_OBJECT(audio_src), "recordsrc", recordsrc, NULL); 
	
	LOGV("GstMediaRecorder create audio encoder");
	audio_enc = gst_element_factory_make("icbaudioenc", "icbaudioenc0"); // do not change the element name

	if(!audio_enc) {
		LOGE("GstMediaRecorder can't create audio encoder");
		goto remove_audio_src;
	}

//	g_object_set(G_OBJECT(audio_enc),"bitrate", mAudioBitrate, NULL);
//	g_object_set(G_OBJECT(audio_enc),"channel", mAudioChannels, NULL);	
//	g_object_set(G_OBJECT(audio_enc),"freq", mAudioSampleRate, NULL);

	// configure audio encoder
	LOGV("GstMediaRecorder set properties to audio encoder");
	switch(mAudio_encoder)
	{
	case AUDIO_ENCODER_AMR_WB:
		// configure audio encoder for AMR-WB 
		LOGV("GstMediaRecorder set properties to audio encoder for AMR_WB");
		if((mOutput_format == OUTPUT_FORMAT_RAW_AMR) && (mUse_video_src == FALSE)) {
			// in AMR RAW format we will not have muxer after audio encoder so use the amr storage format
			g_object_set(G_OBJECT(audio_enc), "stream-type", 2, "format", 2, "bitrate", (gint64)(23850), "freq", (gint)16000, "channel", 1, (gchar*)NULL);
		}else {
			g_object_set(G_OBJECT(audio_enc), "stream-type", 2, "format", 3, "bitrate", (gint64)(23850), "freq", (gint)16000, "channel", 1, (gchar*)NULL);  
		}
		audio_format_filter = gst_element_factory_make("capsfilter",NULL);
		g_object_set(G_OBJECT(audio_format_filter) , "caps", 
				gst_caps_new_simple ("audio/AMR-WB", 
								"rate", G_TYPE_INT, 16000,
								"channels", G_TYPE_INT, 1,	 
								 NULL),
								 NULL);
		break;
	case AUDIO_ENCODER_AAC:
	case AUDIO_ENCODER_AAC_PLUS:
	case AUDIO_ENCODER_EAAC_PLUS:
		// configure audio encoder for AAC 
		LOGV("GstMediaRecorder set properties to audio encoder for AAC");
		g_object_set(G_OBJECT(audio_enc), "stream-type", 3, "format", 1, "bitrate", (gint64)(16000), "freq", (gint)32000, "channel", 2, (gchar*)NULL);  		
		audio_format_filter = gst_element_factory_make("capsfilter",NULL);
		g_object_set(G_OBJECT(audio_format_filter) , "caps", 
					gst_caps_new_simple ("audio/mpeg", 
								"mpegversion", G_TYPE_INT, 4,
								"rate", G_TYPE_INT, 32000,
								"channels", G_TYPE_INT, 2,	 
								 NULL),
								 NULL);	
		break;
	case AUDIO_ENCODER_DEFAULT:
	case AUDIO_ENCODER_AMR_NB:
	default:
		// configure audio encoder for AMR-NB 
		LOGV("GstMediaRecorder set properties to audio encoder for AMR_NB");
		if((mOutput_format == OUTPUT_FORMAT_RAW_AMR) && (mUse_video_src == FALSE)) {
			// in AMR RAW format we will not have muxer after audio encoder so use the amr storage format
			g_object_set(G_OBJECT(audio_enc), "stream-type", 1, "format", 2, "bitrate", (gint64)(12200), "freq", (gint)8000, "channel", 1, (gchar*)NULL);  
		}else {
			g_object_set(G_OBJECT(audio_enc), "stream-type", 1, "format", 3, "bitrate", (gint64)(12200), "freq", (gint)8000, "channel", 1, (gchar*)NULL);  
		}
		audio_format_filter = gst_element_factory_make("capsfilter",NULL);
		g_object_set(G_OBJECT(audio_format_filter) , "caps", 
					gst_caps_new_simple ("audio/AMR", 
								"rate", G_TYPE_INT, 8000,
								"channels", G_TYPE_INT, 1,	 
								 NULL),
								 NULL);			
		break;
	}

	audio_queue =  gst_element_factory_make("queue", "audio_queue");
	g_object_set(G_OBJECT(audio_queue), "max-size-time", 2000000000, NULL);

	LOGV("GstMediaRecorder create audio bin");
	abin = gst_bin_new("abin");

	if(!abin) {
		LOGE("GstMediaRecorder can't create abin");
		goto remove_audio_enc;
	}

	LOGV("GstMediaRecorder add element to audio bin");
	gst_bin_add_many (GST_BIN_CAST(abin), audio_src, audio_enc, audio_format_filter, audio_queue, NULL);

	LOGV("GstMediaRecorder link audio_src->audio_enc");
	if(!gst_element_link_many(audio_src, audio_enc, audio_format_filter, audio_queue, NULL)) {
		LOGE("GstMediaRecorder can't link audio_src->audio_enc");
		goto remove_abin;
	}	

	LOGV("GstMediaRecorder create src ghost pad in abin");
	pad = gst_element_get_static_pad (audio_queue, "src");
	gst_element_add_pad (abin, gst_ghost_pad_new ("src", pad));
	gst_object_unref (pad);

	mAudioSrc = audio_src;
	return abin;

remove_abin:
	gst_object_unref(abin);
remove_audio_enc:
	gst_object_unref(audio_format_filter);
	gst_object_unref(audio_queue);
	gst_object_unref(audio_enc);
remove_audio_src:
	gst_object_unref(audio_src);
	return NULL;
}

/*static*/ GstBusSyncReply GstMediaRecorder::bus_message(GstBus *bus, GstMessage * msg, gpointer data)
{
	GstMediaRecorder *mediarecorder = (GstMediaRecorder*)data;
	if(bus)	{
		// do nothing except remove compilation warning
	}


	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_EOS: {
				LOGV("GstMediaRecorder bus receive message EOS");
				/* unlock mutex  */
				g_mutex_unlock (mediarecorder->mEOSlock);
				break;
		}
		case GST_MESSAGE_ERROR: {
				GError* err;
				gchar* debug;

				gst_message_parse_error(msg, &err, &debug);
				LOGE("GstMediaRecorder bus receive message ERROR %d: %s from %s", err->code, err->message, debug);
				
				if (mediarecorder->mListener != NULL) {
					mediarecorder->mListener->notify(MEDIA_RECORDER_EVENT_ERROR, MEDIA_RECORDER_ERROR_UNKNOWN,err->code);
				}
				g_error_free(err);
				g_free(debug);
				break;
		}
		default:
			// do nothing
			break;
	}

	return GST_BUS_PASS;
}

void GstMediaRecorder::sendEos()
{ 
      if(!mIsEos) {
		LOGV("GstMediaRecorder : forcing EOS");

  		// only sen EOS once
		mIsEos = TRUE;
		
		/* stop audio recording */
		if (mAudioSrc != NULL) {			
			/* send EOS */
			g_object_set(G_OBJECT(mAudioSrc), "eos", TRUE, NULL);
	   
			/* reset mAudioSrc (will avoid to send another eos upon stop request */
			mAudioSrc = NULL;
		}
		        
		/* stop video recording */
		if (mVideoSrc != NULL) {
			/* send EOS */
			gst_app_src_end_of_stream(GST_APP_SRC(mVideoSrc));
	    
			/* reset mVideoSrc (will avoid to send another eos upon stop request */
			mVideoSrc = NULL;
		}		        
	}    
}



/*static*/ void	GstMediaRecorder::handoff(GstElement* object, GstBuffer* buffer, gpointer user_data)
{
	GstMediaRecorder *mediarecorder = (GstMediaRecorder*)user_data;
	gulong microsecond;
	int sizeMargin=0;
	mediarecorder->mCurrentFileSize += GST_BUFFER_SIZE(buffer);
	
	if(mediarecorder->mTimer == NULL) {
		mediarecorder->mTimer = g_timer_new();
	}

	//LOGE("GstMediaRecorder handoff current file size %lld duration %lld", mediarecorder->mCurrentFileSize, (gint64)g_timer_elapsed(mediarecorder->mTimer, &microsecond)*1000);

	if((mediarecorder->mMaxDuration != -1) && (mediarecorder->mMaxDuration <= (gint64)(g_timer_elapsed(mediarecorder->mTimer, &microsecond)*1000) )) {
		LOGV("GstMediaRecorder reached recording time limit");
		if(mediarecorder->mListener != NULL) {
			mediarecorder->mListener->notify(MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_MAX_DURATION_REACHED, 0);
		}
		/* Send audio & video Eos */
		mediarecorder->sendEos();

		g_object_set(object, "signal-handoffs", FALSE, NULL);
		return;
	}

	/* consider a margin before stopping (because we will still get data to flush the pipeline */
	if (mediarecorder->mAudioSrc != NULL)
   	      sizeMargin+=3000; /* 3kB for Audio recording */

	if (mediarecorder->mVideoSrc != NULL)
   	      sizeMargin+=50000; /* 50kB for video recording */

	if((mediarecorder->mMaxFileSize != -1) && (mediarecorder->mMaxFileSize <= mediarecorder->mCurrentFileSize + sizeMargin)) {
		LOGV("GstMediaRecorder reached recording size limit");
		if(mediarecorder->mListener != NULL) {
			mediarecorder->mListener->notify(MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED, 0);
		}
		/* Send audio & video Eos */
		mediarecorder->sendEos();

		g_object_set(object, "signal-handoffs", FALSE, NULL);
		return;
	}
}

/*static*/ void GstMediaRecorder::debug_log (GstDebugCategory * category, GstDebugLevel level,
							const gchar * file, const gchar * function, gint line,
							GObject * object, GstDebugMessage * message, gpointer data)
{
	gint pid;
	GstClockTime elapsed;
	GstMediaRecorder* mediarecorder = (GstMediaRecorder*)data;

	GST_UNUSED(file);
	GST_UNUSED(object);

	if (level > gst_debug_category_get_threshold (category))
		return;

	pid = getpid ();

	elapsed = GST_CLOCK_DIFF (mediarecorder->mGst_info_start_time,
	  gst_util_get_timestamp ());


	g_printerr ("%" GST_TIME_FORMAT " %5d %s %s %s:%d %s\r\n",
		GST_TIME_ARGS (elapsed),
		pid,
		gst_debug_level_get_name (level),
		gst_debug_category_get_name (category), function, line,
		gst_debug_message_get (message));
}



status_t GstMediaRecorder::build_record_graph ()
{
	GstElement *muxer, *identity, *sink;
	GstBus *bus;
	GError *err = NULL;
	int argc=3;
	char **argv;
	char str0[] =  "";
	//char str1[] =  "";
	char str2[] =  "";
	char trace[PROPERTY_VALUE_MAX];
		  
	argv = (char **)malloc(sizeof(char *) * argc);
	argv[0] = (char *) malloc( sizeof(char) * (strlen(str0) + 1));
	argv[2] = (char *) malloc( sizeof(char) * (strlen(str2) + 1));
	strcpy( argv[0], str0);
	strcpy( argv[2], str2);

	char value[PROPERTY_VALUE_MAX];
	property_get("persist.gst.debug", value, "0");
	LOGV("persist.gst.debug property %s", value);
	argv[1] = (char *) malloc( sizeof(char) * (strlen(value) + 1));
	strcpy( argv[1], value);  

	property_get("persist.gst.trace", trace, "/dev/console");
	LOGV("persist.gst.trace property %s", trace);
	LOGV("route the trace to %s", trace);
	int fd_trace = open(trace, O_RDWR);
	if(fd_trace != 1) {
		dup2(fd_trace, 0);
		dup2(fd_trace, 1);
		dup2(fd_trace, 2);
		::close(fd_trace);
	}

	mGst_info_start_time = gst_util_get_timestamp ();
	gst_debug_remove_log_function(debug_log);
	gst_debug_add_log_function(debug_log, this);
	gst_debug_remove_log_function(gst_debug_log_default);

	LOGV("GstMediaRecorder gstreamer init check");
	// init gstreamer 	
	if(!gst_init_check (&argc, &argv, &err)) {
		LOGE ("GstMediaRecorder Could not initialize GStreamer: %s\n", err ? err->message : "unknown error occurred");
		if (err) {
			g_error_free (err);
		}
	}

	LOGV("GstMediaRecorder create pipeline");
	mPipeline = gst_pipeline_new (NULL);
	if(!mPipeline) {
		LOGE("GstMediaRecorder can't create pipeline");
		goto bail;
	}

	// verbose info (as gst-launch -v)
	// Activate the trace with the command: "setprop persist.gst.verbose 1"
	property_get("persist.gst.verbose", value, "0");
	LOGV("persist.gst.verbose property = %s", value);
	if (value[0] == '1') {
		LOGV("Activate deep_notify");
		g_signal_connect (mPipeline, "deep_notify",
				G_CALLBACK (gst_object_default_deep_notify), NULL);
	}

	LOGV("GstMediaRecorder register bus callback");	
	bus = gst_pipeline_get_bus(GST_PIPELINE (mPipeline));
	gst_bus_set_sync_handler (bus, bus_message, this);
	gst_object_unref (bus);

	if((mOutput_format == OUTPUT_FORMAT_RAW_AMR) && (mUse_video_src == FALSE) ) {
		// in RAW AMR format don't use any muxer
		LOGV("GstMediaRecorder use identity as muxer in RAW_AMR format");
		muxer = gst_element_factory_make("identity", NULL);
	} 
	else {
		LOGV("GstMediaRecorder use gppmux");
		muxer = gst_element_factory_make("gppmux", NULL);
	}

	if(!muxer) {
		LOGE("GstMediaRecorder can't create muxer");
		goto bail1;
	}

	gst_bin_add (GST_BIN_CAST(mPipeline), muxer);

	LOGV("GstMediaRecorder create sink from uri %s", mOutFilePath);
	sink = gst_element_make_from_uri(GST_URI_SINK, mOutFilePath, NULL);
	if(!sink) {
		LOGE("GstMediaRecorder can't create sink %s", mOutFilePath);
		goto bail1;
	}

	g_object_set(G_OBJECT(sink), "async", FALSE, NULL);

	gst_bin_add (GST_BIN_CAST(mPipeline), sink);
	
	LOGV("GstMediaRecorder create identity");
	identity = gst_element_factory_make("identity", NULL);
	if(!identity) {
		LOGE("GstMediaRecorder can't identity element");
		goto bail1;
	}
	gst_bin_add (GST_BIN_CAST(mPipeline), identity);
	
	mCurrentFileSize = 0;
	g_signal_connect (identity, "handoff", G_CALLBACK (handoff), this);
	g_object_set(G_OBJECT(identity), "signal-handoffs", TRUE, NULL);
	
	mVideoBin = create_video_bin();
	if(mVideoBin) {
		gst_bin_add (GST_BIN_CAST(mPipeline),mVideoBin); 
		LOGV("GstMediaRecorder link vbin to muxer");
		if(!gst_element_link(mVideoBin, muxer)) {
			LOGE("GstMediaRecorder can't link vbin to muxer");
		}
	}

	mAudioBin = create_audio_bin();
	if(mAudioBin) {
		gst_bin_add (GST_BIN_CAST(mPipeline),mAudioBin);
		LOGV("GstMediaRecorder link abin to muxer");
		if(!gst_element_link(mAudioBin, muxer)) {
			LOGE("GstMediaRecorder can't link abin to muxer");
		}
	}

	if(!mAudioBin && !mVideoBin)
	{
		LOGE("GstMediaRecorder both audiobin and videobin are NULL !!!!!");
		goto bail1;
	}
	LOGV("GstMediaRecorder link muxer->identity->sink");
	if(!gst_element_link_many(muxer, identity, sink, NULL)) {
		LOGE("GstMediaRecorder can't link muxer->identity->sink");
	}


	gst_element_set_state (mPipeline, GST_STATE_READY);
	gst_element_get_state (mPipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	return OK;

bail1:
	LOGV("GstMediaRecorder change pipeline state to NULL");
	gst_element_set_state (mPipeline, GST_STATE_NULL);
	gst_element_get_state (mPipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	LOGV("GstMediaRecorder  unref pipeline");
	gst_object_unref(mPipeline);
bail:
	mPipeline = NULL;
	mVideoBin = NULL;
	mAudioBin = NULL;
	mVideoSrc = NULL;
	return UNKNOWN_ERROR;
}

void GstMediaRecorder::postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr)
{
	ssize_t offset = 0;
	size_t size = 0;
	video_frame_t video_frame = VIDEO_FRAME_INIT;
	GstBuffer* buffer;
	GstClockTime duration;
	GST_UNUSED(timestamp);
	GST_UNUSED(msgType);

	//LOGV("postDataTimestamp");
	record_callback_cookie * lcookie = g_new0 (record_callback_cookie, 1);
	sp<IMemoryHeap> heap = dataPtr->getMemory(&offset, &size);
	
	if(mVideoSrc == NULL) {
		LOGE(" record_callback the videosrc don't exist");
		mCamera->stopRecording();
		return ;
	}

	video_frame.pmem_fd = heap->getHeapID();
	video_frame.pmem_offset = offset;
	video_frame.pmem_size = size;

	lcookie->frame = dataPtr;
	lcookie->camera = mCamera;

	buffer = gst_icbvideo_buffer_new(&video_frame, (GstMiniObjectFinalizeFunction) video_frame_release, lcookie
									,GST_ELEMENT(mVideoSrc) );
	GST_BUFFER_SIZE(buffer) = size; //needed to build correct timestamp in basesrc
	GST_BUFFER_TIMESTAMP(buffer) = timestamp;

	/* Last frame of video not encoded */
	duration = gst_util_uint64_scale_int (GST_SECOND, 1, mFps );
	GST_BUFFER_DURATION(buffer) = duration; //needed to build correct duration in basesrc

	gst_app_src_push_buffer(GST_APP_SRC(mVideoSrc), buffer);
}

// camera callback interface
void AndroidGstCameraListener::postData(int32_t msgType, const sp<IMemory>& dataPtr)
{
	GST_UNUSED(msgType); GST_UNUSED(dataPtr); 
}

void AndroidGstCameraListener::postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr)
{	
	//LOGI("AndroidGstCameraListener::postDataTimestamp %lld ns", timestamp);
	if ((mRecorder != NULL) && (msgType == CAMERA_MSG_VIDEO_FRAME)) {
		mRecorder->postDataTimestamp(timestamp, msgType, dataPtr);
	}
}

