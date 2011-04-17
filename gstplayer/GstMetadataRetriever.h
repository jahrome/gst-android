/*
 * =====================================================================================
 *
 *       Filename:  GstMetadataRetriever.h
 *  Copyright  ST-Ericsson 2009
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

#ifndef GST_METADATARETRIEVER_H
#define GST_METADATARETRIEVER_H


#include <utils/Errors.h>
#include <media/MediaMetadataRetrieverInterface.h>
#include <media/mediametadataretriever.h>
#include "GstMetadataRetrieverDriver.h"

namespace android
{

  class GstMetadataRetriever:public MediaMetadataRetrieverInterface
  {
  public:
    GstMetadataRetriever ();
    virtual ~ GstMetadataRetriever ();

    virtual status_t setDataSource (const char *url);
    virtual status_t setDataSource (int fd, int64_t offset, int64_t length);
    virtual status_t setMode (int mode);
    virtual status_t getMode (int *mode) const;
    virtual VideoFrame *captureFrame ();
    virtual MediaAlbumArt *extractAlbumArt ();
    virtual const char *extractMetadata (int keyCode);

  private:
    static void do_nothing (status_t s, void *cookie, bool cancelled)
    {
      if (s) {                  /* warning removal */
      }
      if (cookie)
      {                         /* warning removal */
      }
      if (cancelled)
      {                         /* warning removal */
      }
    }

    GstMetadataRetrieverDriver *mGstDriver;
    int mMode;
    int mLocked;

  };

};                              // namespace android

#endif // GST_METADATARETRIEVER_H
