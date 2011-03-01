/*
 * =====================================================================================
 * 
 *       Filename:  GstPlayer.h
 * Copyright ST-Ericsson 2009
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

#ifndef  GSTPLAYER_INC
#define  GSTPLAYER_INC

#include <utils/Errors.h>
#include <media/MediaPlayerInterface.h>
#include "GstDriver.h"

namespace android {
class GstPlayer : public MediaPlayerInterface
{
public:
                        GstPlayer();
    virtual             ~GstPlayer();

    virtual status_t    initCheck();
    virtual status_t    setSigBusHandlerStructTLSKey(pthread_key_t key);
    virtual status_t    setDataSource(const char *url);
    virtual status_t    setDataSource(int fd, int64_t offset, int64_t length);
    virtual status_t    setVideoSurface(const sp<ISurface>& surface);
    virtual status_t    prepare();
    virtual status_t    prepareAsync();
    virtual status_t    start();
    virtual status_t    stop();
    virtual status_t    pause();
    virtual bool        isPlaying();
    virtual status_t    seekTo(int msec);
    virtual status_t    getCurrentPosition(int *msec);
    virtual status_t    getDuration(int *msec);
    virtual status_t    reset();
    virtual status_t    setLooping(int loop);
    virtual player_type playerType() { return GST_PLAYER; }

#ifdef ANDROID_1_6
	virtual status_t    getVideoWidth(int *w);
    virtual status_t    getVideoHeight(int *h);
	virtual status_t    setSigBusHandlerStructTLSKey(pthread_key_t key);
#endif	
	
	//new on eclair
	// Invoke a generic method on the player by using opaque parcels
    // for the request and reply.
    //
    // @param request Parcel that is positioned at the start of the
    //                data sent by the java layer.
    // @param[out] reply Parcel to hold the reply data. Cannot be null.
    // @return OK if the call was successful.
	virtual status_t    invoke(const Parcel& request, Parcel *reply);
	// The Client in the MetadataPlayerService calls this method on
    // the native player to retrieve all or a subset of metadata.
    //
    // @param ids SortedList of metadata ID to be fetch. If empty, all
    //            the known metadata should be returned.
    // @param[inout] records Parcel where the player appends its metadata.
    // @return OK if the call was successful.
    virtual status_t    getMetadata(const SortedVector<media::Metadata::Type>& ids,
							        Parcel *records);


    // make available to GstDriver
    void        sendEvent(int msg, int ext1=0, int ext2=0) { MediaPlayerBase::sendEvent(msg, ext1, ext2); }
	virtual status_t setDataSource(const char *url, const KeyedVector<String8, String8> *headers);
private:
    //static void         do_nothing(status_t s, void *cookie, bool cancelled) { if(s) { /* warning removal*/ } if(cookie) { /* warning removal*/ }  if(cancelled) { /* warning removal*/ }  }
	void				checkSetup();

	GstDriver*			mGstDriver;
	bool				mSetupDone;
};

}; // namespace android

#endif   /* ----- #ifndef GSTPLAYER_INC  ----- */

