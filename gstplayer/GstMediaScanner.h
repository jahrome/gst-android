/*
 * =====================================================================================
 *
 *       Filename:  GstMediaScanner.cpp
 *  Copyright: ST-Ericsson 2009
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

#ifndef GSTMEDIASCANNER_H
#define GSTMEDIASCANNER_H

#include <media/mediascanner.h>

namespace android
{

  struct GstMediaScanner:public MediaScanner
  {
    GstMediaScanner ();
    virtual ~ GstMediaScanner ();

    virtual status_t processFile (const char *path, const char *mimeType,
        MediaScannerClient & client);

    virtual char *extractAlbumArt (int fd);

  };

  struct GstMediaScannerClient:public MediaScannerClient
  {
    GstMediaScannerClient ()
    {                           // call MediaScannerClient::MediaScannerClient
      LOGV ("GstMediaScannerClient construtor");
    }

     ~GstMediaScannerClient ()
    {                           // call MediaScanner::~MediaScanner
      LOGV ("GstMediaScannerClient destructor");
    }
    // non-virtual functions
    void setLocale (const char *locale);
    void beginFile ();
    bool addStringTag (const char *name, const char *value);
    void endFile ();
    void convertValues (uint32_t encoding);

    // pure virtual functions
    bool scanFile (const char *path, long long lastModified,
        long long fileSize);
    bool handleStringTag (const char *name, const char *value);
    bool setMimeType (const char *mimeType);
    bool addNoMediaFolder (const char *path);
  };

}                               // namespace android


#endif // GSTMEDIASCANNER_H
