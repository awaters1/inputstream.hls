/*
 * kodi_hls.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "hls/HLS.h"
#include "kodi_hls.h"
#include "globals.h"

bool download_playlist(const char *url, hls::Playlist &playlist) {
  // open the file
  void* file = xbmc->CURLCreate(url);
  if (!file)
    return false;
  playlist.set_url(url);
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable", "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip");
  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE);

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE];
  bool read;
  while ((read = xbmc->ReadFileString(file, buf, CHUNKSIZE)) > 0 && read) {
    // Take \n off of buffer
    buf[strlen(buf) - 1] = '\0';
    bool ret = playlist.write_data(buf);
    xbmc->Log(ADDON::LOG_DEBUG, "Read Line %s", buf);
    if (!ret) {
      break;
    }
  }

  //download_speed_ = xbmc->GetFileDownloadSpeed(file);

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished", url);

  return !read;
}

void KodiMasterPlaylist::select_media_playlist() {
  // Just selects the first one for now
  for(std::vector<hls::MediaPlaylist>::iterator it = media_playlist.begin(); it != media_playlist.end(); ++it) {
      download_playlist(it->get_url().c_str(), *it);
      break;
  }
}

bool KodiMasterPlaylist::open(const char *url) {
  download_playlist(url, *this);
}
