/*
 * =====================================================================================
 *
 *       Filename:  GstPlayer.cpp
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
#define LOG_TAG "GstPlayer"

#include "GstPlayer.h"
#include <utils/Log.h>

#define UNUSED(x) (void)x

namespace android
{

  GstPlayer::GstPlayer ()
  {
    LOGV ("GstPlayer constructor");
    mGstDriver = new GstDriver ((MediaPlayerInterface *) this);
    mSetupDone = false;
    checkSetup ();
    //LOGV("GstPlayer constructor exit");
  }

  void GstPlayer::checkSetup ()
  {
    if (mSetupDone) {
      return;
    }
    mSetupDone = true;
    LOGV ("GstPlayer send GstDriver Setup");
    mGstDriver->setup ();
  }
  status_t GstPlayer::initCheck ()
  {
    //LOGV("GstPlayer initCheck");
    return OK;
  }

  GstPlayer::~GstPlayer () {
    LOGV ("GstPlayer destructor");
    mGstDriver->quit ();
    if (mGstDriver) {
      delete mGstDriver;
    }
    mGstDriver = NULL;
  }

  status_t GstPlayer::setSigBusHandlerStructTLSKey (pthread_key_t key)
  {
    //LOGV("GstPlayer setSigBusHandlerStructTLSKey");
    UNUSED (key);
    return OK;
  }

  status_t GstPlayer::setDataSource (const char *url,
      const KeyedVector < String8, String8 > *headers)
  {
    LOGI ("setDataSource('%s')", url);
    return setDataSource (url);
  }

  status_t GstPlayer::setDataSource (const char *url)
  {
    LOGI ("GstPlayer setDataSource(%s)", url);
    checkSetup ();
    mGstDriver->setDataSource (url);
    mGstDriver->setAudioSink (mAudioSink);
    return OK;
  }

  status_t GstPlayer::setDataSource (int fd, int64_t offset, int64_t length)
  {

    LOGI ("GstPlayer setDataSource(%d, %lld, %lld)", fd, offset, length);
    checkSetup ();

    mGstDriver->setFdDataSource (fd, offset, length);
    mGstDriver->setAudioSink (mAudioSink);
    return OK;
  }

  status_t GstPlayer::setVideoSurface (const sp < ISurface > &surface)
  {
    LOGV ("GstPlayer setVideoSurface(%p)", surface.get ());
    checkSetup ();
    mGstDriver->setVideoSurface (surface);
    return OK;
  }

  status_t GstPlayer::prepare ()
  {
    // prepare
    LOGV ("GstPlayer prepare");
    checkSetup ();
    mGstDriver->prepareSync ();
    {
      int width;
      int height;
      mGstDriver->getVideoSize (&width, &height);
      sendEvent (MEDIA_SET_VIDEO_SIZE, width, height);
    }
    return OK;
  }

  status_t GstPlayer::prepareAsync ()
  {
    LOGV ("GstPlayer prepareAsync");
    checkSetup ();
    // No need to run a sequence of commands.
    // The only command needed to run is PLAYER_PREPARE.
    mGstDriver->prepareAsync ();
    return OK;
  }

  status_t GstPlayer::start ()
  {
    LOGV ("GstPlayer start");
    mGstDriver->start ();
    return OK;
  }

  status_t GstPlayer::stop ()
  {
    LOGV ("GstPlayer stop");
    mGstDriver->stop ();
    return OK;
  }

  status_t GstPlayer::pause ()
  {
    LOGV ("GstPlayer pause");
    mGstDriver->pause ();
    return OK;
  }

  bool GstPlayer::isPlaying ()
  {
    int status = mGstDriver->getStatus ();
    LOGI ("GstPlayer %s playing",
        (status == GstDriver::GSTDRIVER_STATE_STARTED) ? "is" : "isn't");
    return (status == GstDriver::GSTDRIVER_STATE_STARTED);
  }

  status_t GstPlayer::getCurrentPosition (int *msec)
  {
    LOGV ("GstPlayer getCurrentPosition");
    *msec = mGstDriver->getPosition ();
    return OK;
  }

  status_t GstPlayer::getDuration (int *msec)
  {
    LOGV ("GstPlayer getDuration");
    *msec = mGstDriver->getDuration ();
    return OK;
  }

  status_t GstPlayer::seekTo (int msec)
  {
    int tmpduration;
    int seek_position = msec;
    LOGV ("GstPlayer seekTo(%d)", msec);
    getDuration (&tmpduration);
    if (msec >= tmpduration)
      seek_position = tmpduration - 1000;
    mGstDriver->seek (seek_position);
    return OK;
  }

  status_t GstPlayer::reset ()
  {
    LOGV ("GstPlayer reset");
    mGstDriver->quit ();
    delete mGstDriver;

    mGstDriver = new GstDriver ((MediaPlayerInterface *) this);
    mSetupDone = false;

    return NO_ERROR;
  }

  status_t GstPlayer::setLooping (int loop)
  {
    LOGV ("GstPlayer setLooping(%d)", loop);
    return OK;
  }

//eclair
  status_t GstPlayer::invoke (const Parcel & request, Parcel * reply)
  {
    GST_UNUSED (request)
        GST_UNUSED (reply)
        return INVALID_OPERATION;
  }

// The Client in the MetadataPlayerService calls this method on
// the native player to retrieve all or a subset of metadata.
//
// @param ids SortedList of metadata ID to be fetch. If empty, all
//            the known metadata should be returned.
// @param[inout] records Parcel where the player appends its metadata.
// @return OK if the call was successful.
  status_t GstPlayer::getMetadata (const SortedVector < media::Metadata::Type >
      &ids, Parcel * records)
  {
    if (!mSetupDone || !mGstDriver) {
      return INVALID_OPERATION;
    }
    return mGstDriver->getMetadata (ids, records);
  }

};                              // namespace android
