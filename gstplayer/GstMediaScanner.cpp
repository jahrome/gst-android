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

#ifndef GSTMEDIASCANNER
#define GSTMEDIASCANNER

#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "GstMediaScanner"

#include <utils/Log.h>
#include <media/mediascanner.h>
#include <media/mediametadataretriever.h>
#include <private/media/VideoFrame.h>
#include <utils/StringArray.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <binder/IMemory.h>
#ifdef STECONF_ANDROID_VERSION_FROYO
#include "GstMediaScanner.h"
#endif

#define UNUSED(x) (void)x

// flags used for native encoding detection
enum
{
  kEncodingNone = 0,
  kEncodingShiftJIS = (1 << 0),
  kEncodingGBK = (1 << 1),
  kEncodingBig5 = (1 << 2),
  kEncodingEUCKR = (1 << 3),
  kEncodingAll =
      (kEncodingShiftJIS | kEncodingGBK | kEncodingBig5 | kEncodingEUCKR),
};



namespace android
{
#ifndef STECONF_ANDROID_VERSION_FROYO
  MediaScanner::MediaScanner ()
  :mLocale (NULL)
  {
  }

  MediaScanner::~MediaScanner ()
  {
    if (mLocale)
      free (mLocale);
  }
#else
  GstMediaScanner::GstMediaScanner ()
  {
  }

  GstMediaScanner::~GstMediaScanner ()
  {
  }
#endif

#ifndef STECONF_ANDROID_VERSION_FROYO
  status_t MediaScanner::processFile (const char *path, const char *mimeType,
      MediaScannerClient & client)
#else
  status_t GstMediaScanner::processFile (const char *path, const char *mimeType,
      MediaScannerClient & client)
#endif
  {
    const char *extension = strrchr (path, '.');

    UNUSED (mimeType);

    LOGV ("MediaScanner processFile %s", path);

    //first check if extension is supported 
    if (!extension) {
      return -1;
    }

    if (
        //audio file
        !strcasecmp (extension, ".mp3") ||
        !strcasecmp (extension, ".ogg") ||
        !strcasecmp (extension, ".oga") ||
        !strcasecmp (extension, ".mid") ||
        !strcasecmp (extension, ".midi") ||
        !strcasecmp (extension, ".smf") ||
        !strcasecmp (extension, ".xmf") ||
        !strcasecmp (extension, ".rtttl") ||
        !strcasecmp (extension, ".imy") ||
        !strcasecmp (extension, ".rtx") ||
        !strcasecmp (extension, ".ota") ||
        !strcasecmp (extension, ".wma") ||
        !strcasecmp (extension, ".m4a") ||
        !strcasecmp (extension, ".amr") ||
        !strcasecmp (extension, ".awb") ||
        !strcasecmp (extension, ".wav") || !strcasecmp (extension, ".aac") ||
        //video file
        !strcasecmp (extension, ".mp4") ||
        !strcasecmp (extension, ".m4v") ||
        !strcasecmp (extension, ".3gp") ||
        !strcasecmp (extension, ".3gpp") ||
        !strcasecmp (extension, ".3gp2") ||
        !strcasecmp (extension, ".3gpp2") ||
        !strcasecmp (extension, ".3g2") ||
        !strcasecmp (extension, ".avi") ||
        !strcasecmp (extension, ".mov") ||
        !strcasecmp (extension, ".wmv") ||
        !strcasecmp (extension, ".asf") || !strcasecmp (extension, ".divx")
        ) {
      // extension of multimedia file supported
    } else {
      LOGV ("MediaScanner processFile extension %s not supported", extension);
      return -1;
    }

    sp < MediaMetadataRetriever > retriever = new MediaMetadataRetriever ();
    retriever->setMode (METADATA_MODE_METADATA_RETRIEVAL_ONLY);
    status_t status = retriever->setDataSource (path);

    if (status != NO_ERROR) {
      LOGE ("MediaScanner setDataSource failed (%d)", status);
      retriever->disconnect ();
      return status;
    }
    // init client
#ifndef STECONF_ANDROID_VERSION_FROYO
    client.setLocale (mLocale);
#else
    client.setLocale (locale ());
#endif
    client.beginFile ();

    const char *value;

    // extract metadata from the file
    value = retriever->extractMetadata (METADATA_KEY_IS_DRM_CRIPPLED);
    if (value && strcmp (value, "true") == 0) {
      // we don't support WMDRM currently
      // setting this invalid mimetype will make the java side ignore this file
      client.setMimeType ("audio/x-wma-drm");
    }
    value = retriever->extractMetadata (METADATA_KEY_CODEC);
    if (value && strcmp (value, "Windows Media Audio 10 Professional") == 0) {
      // we don't support WM 10 Professional currently
      // setting this invalid mimetype will make the java side ignore this file
      client.setMimeType ("audio/x-wma-10-professional");
    }

    value = retriever->extractMetadata (METADATA_KEY_ALBUM);
    if (value)
      client.addStringTag ("album", value);

    // Look for "author" tag first, if it is not found, try "artist" tag
    value = retriever->extractMetadata (METADATA_KEY_AUTHOR);
    if (!value) {
      value = retriever->extractMetadata (METADATA_KEY_ARTIST);
    }
    if (value)
      client.addStringTag ("artist", value);
    value = retriever->extractMetadata (METADATA_KEY_COMPOSER);
    if (value)
      client.addStringTag ("composer", value);
    value = retriever->extractMetadata (METADATA_KEY_GENRE);
    if (value)
      client.addStringTag ("genre", value);
    value = retriever->extractMetadata (METADATA_KEY_TITLE);
    if (value)
      client.addStringTag ("title", value);
    value = retriever->extractMetadata (METADATA_KEY_YEAR);
    if (value)
      client.addStringTag ("year", value);
    value = retriever->extractMetadata (METADATA_KEY_CD_TRACK_NUMBER);
    if (value)
      client.addStringTag ("tracknumber", value);

    retriever->disconnect ();
    // send info to java layer
    client.endFile ();
    return status;
  }
#ifndef STECONF_ANDROID_VERSION_FROYO
  status_t MediaScanner::processDirectory (const char *path,
      const char *extensions, MediaScannerClient & client,
      ExceptionCheck exceptionCheck, void *exceptionEnv)
  {
    LOGV ("MediaScanner processDirectory %s", path);
    // do not process directories with .nomedia file
    char *nomedia = new char[strlen (path) + strlen ("/.nomedia") + 2];
    strcpy (nomedia, path);
    strcat (nomedia, "/.nomedia");

    if (access (nomedia, F_OK) == 0) {
      LOGV ("MediaScanner %s found don't process this directory", nomedia);
      delete nomedia;
      return OK;
    }
    delete nomedia;

    struct dirent *entry;
    DIR *dir = opendir (path);
    if (!dir) {
      LOGV ("MediaScanner can't open directory %s", path);
      return -1;
    }
    // now check all entries in this directory
    while ((entry = readdir (dir))) {
      const char *name = entry->d_name;
      LOGV ("MediaScanner entry name %s/%s", path, name);
      if (name == NULL) {
        continue;
      }
      // ignore "." and ".."
      if (name[0] == '.') {
        continue;
      }

      int type = entry->d_type;
      if (type == DT_UNKNOWN) {
        // If the type is unknown, stat() the file instead.
        // This is sometimes necessary when accessing NFS mounted filesystems, but
        // could be needed in other cases well.
        struct stat statbuf;
        if (stat (path, &statbuf) == 0) {
          if (S_ISREG (statbuf.st_mode)) {
            type = DT_REG;
          } else if (S_ISDIR (statbuf.st_mode)) {
            type = DT_DIR;
          }
        } else {
          LOGD ("stat() failed for %s: %s", path, strerror (errno));
        }
      }

      if (type == DT_REG || type == DT_DIR) {
        bool isDirectory = (type == DT_DIR);

        char *nextPath = new char[strlen (path) + strlen (name) + 2];
        strcpy (nextPath, path);
        strcat (nextPath, "/");
        strcat (nextPath, name);

        if (isDirectory) {
          int err =
              processDirectory (nextPath, extensions, client, exceptionCheck,
              exceptionEnv);
          if (err) {
            LOGV ("Error processing '%s' - skipping\n", path);
            continue;
          }
        } else {
          struct stat statbuf;
          stat (nextPath, &statbuf);
          if (statbuf.st_size > 0) {
            client.scanFile (nextPath, statbuf.st_mtime, statbuf.st_size);
          }
        }
        delete nextPath;
      }

    }

    closedir (dir);
    return OK;
  }

  void MediaScanner::setLocale (const char *locale)
  {
    //LOGE("MediaScanner set locale %s", locale);
    if (mLocale) {
      free (mLocale);
      mLocale = NULL;
    }
    if (locale) {
      mLocale = strdup (locale);
    }
  }
#endif

// extracts album art as a block of data
// output: is a jpeg + 4 bytes of header to give jpeg size
#ifndef STECONF_ANDROID_VERSION_FROYO
  char *MediaScanner::extractAlbumArt (int fd)
#else
  char *GstMediaScanner::extractAlbumArt (int fd)
#endif
  {
    LOGV ("MediaScanner extractAlbumArt %d", fd);
    struct stat statbuf;
    char *data = NULL;

    sp < MediaMetadataRetriever > retriever = new MediaMetadataRetriever ();
    retriever->setMode (METADATA_MODE_METADATA_RETRIEVAL_ONLY);
    // make stat to get fd size
    fstat (fd, &statbuf);
    retriever->setDataSource (fd, 0, statbuf.st_size);

    sp < IMemory > albumArt = retriever->extractAlbumArt ();

    if (albumArt != NULL) {
      MediaAlbumArt *albumArtCopy =
          static_cast < MediaAlbumArt * >(albumArt->pointer ());

      data = (char *) malloc (albumArtCopy->mSize + 4);
      if (data) {
        long *len = (long *) data;
        *len = albumArtCopy->mSize;
        memcpy (data + 4, (char *) albumArtCopy + sizeof (MediaAlbumArt), *len);
      }
    }
    retriever->disconnect ();
    return data;
  }

#ifndef STECONF_ANDROID_VERSION_FROYO
  void MediaScanner::uninitializeForThread ()
  {
  }

  status_t MediaScanner::doProcessDirectory (char *path, int pathRemaining,
      const char *extensions, MediaScannerClient & client,
      ExceptionCheck exceptionCheck, void *exceptionEnv)
  {
    //LOGE("MediaScanner doProcessDirectory %s", path);
    // empty function: need to keep compatibility with mediascanner interface
    UNUSED (path);
    UNUSED (pathRemaining);
    UNUSED (extensions);
    UNUSED (client);
    UNUSED (exceptionCheck);
    UNUSED (exceptionEnv);

    return OK;
  }

  void MediaScanner::initializeForThread ()
  {
  }

  MediaScannerClient::MediaScannerClient ()
:    mNames (NULL), mValues (NULL), mLocaleEncoding (kEncodingNone) {
    LOGV ("MediaScannerClient construtor");
  }

  MediaScannerClient::~MediaScannerClient () {
    LOGV ("MediaScannerClient destructor");
    if (mNames != NULL)
      delete mNames;

    if (mValues != NULL)
      delete mValues;
  }

  void MediaScannerClient::setLocale (const char *locale)
  {
    LOGV ("MediaScannerClient set locale %s", locale);
  }

  void MediaScannerClient::beginFile ()
  {
    LOGV ("MediaScannerClient beginFile");
    mNames = new StringArray;
    mValues = new StringArray;
  }

  bool MediaScannerClient::addStringTag (const char *name, const char *value)
  {
    LOGV ("MediaScannerClient addStringTag %s : %s", name, value);
    mNames->push_back (name);
    mValues->push_back (value);
    return true;
  }

  void MediaScannerClient::endFile ()
  {
    LOGV ("MediaScannerClient endFile");
    // finally, push all name/value pairs to the client

    if (mNames) {
      for (int i = 0; i < mNames->size (); i++) {
        if (!handleStringTag (mNames->getEntry (i), mValues->getEntry (i))) {
          break;
        }
      }
    }

    if (mNames != NULL)
      delete mNames;

    if (mValues != NULL)
      delete mValues;

    mNames = NULL;
    mValues = NULL;
  }

  void MediaScannerClient::convertValues (uint32_t encoding)
  {
    LOGV ("MediaScannerClient convertValues %d", encoding);
  }
#else
  void GstMediaScannerClient::setLocale (const char *locale)
  {
    LOGV ("GstMediaScannerClient set locale %s", locale);
    MediaScannerClient::setLocale (locale);
  }

  void GstMediaScannerClient::beginFile ()
  {
    LOGV ("GstMediaScannerClient beginFile");
    MediaScannerClient::beginFile ();
  }

  bool GstMediaScannerClient::addStringTag (const char *name, const char *value)
  {
    LOGV ("GstMediaScannerClient addStringTag %s : %s", name, value);
    return MediaScannerClient::addStringTag (name, value);
  }

  void GstMediaScannerClient::endFile ()
  {
    LOGV ("GstMediaScannerClient endFile");
    MediaScannerClient::endFile ();
  }

  void GstMediaScannerClient::convertValues (uint32_t encoding)
  {
    LOGV ("GstMediaScannerClient convertValues %d", encoding);
    MediaScannerClient::convertValues (encoding);
  }

/*
 * place holder for functions
 */
  bool GstMediaScannerClient::scanFile (const char *path,
      long long lastModified, long long fileSize)
  {
    LOGV ("GstMediaScannerClient scanFile");
    return false;
  }

  bool GstMediaScannerClient::handleStringTag (const char *name,
      const char *value)
  {
    LOGV ("GstMediaScannerClient handleStringTag");
    return false;
  }

  bool GstMediaScannerClient::setMimeType (const char *mimeType)
  {
    LOGV ("GstMediaScannerClient setMimeType");
    return false;
  }

  bool GstMediaScannerClient::addNoMediaFolder (const char *path)
  {
    LOGV ("GstMediaScannerClient addNoMediaFolder");
    return false;
  }
#endif


};                              //namespace android


#endif // GSTMEDIASCANNER
