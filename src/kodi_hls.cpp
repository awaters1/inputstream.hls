/*
 * kodi_hls.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "globals.h"

#include "kodi_hls.h"

bool download_playlist_impl(const char *url, hls::Playlist &playlist) {
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
    if (!ret) {
      break;
    }
  }

  xbmc->CloseFile(file);
  return !read;
}

hls::MediaPlaylist KodiSession::download_playlist(std::string url) {
  hls::MediaPlaylist playlist;
  download_playlist_impl(url.c_str(), playlist);
  return playlist;
}

std::string KodiSession::download_aes_key(std::string aes_uri) {
  // open the file
  void* file = xbmc->CURLCreate(aes_uri.c_str());
  if (!file)
    return "";
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable", "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip");
  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE);

  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE];
  size_t read;
  std::string aes_key;
  while ((read = xbmc->ReadFile(file, buf, CHUNKSIZE)) > 0 && read) {
    aes_key = std::string(buf, read);
    break;
  }
  xbmc->CloseFile(file);

  return aes_key;
}

bool KodiSession::download_segment(hls::ActiveSegment *active_segment) {
  // open the file
  std::string url = active_segment->get_url();
  void* file = xbmc->CURLCreate(url.c_str());
  if (!file)
    return false;
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable" , "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Connection", "keep-alive");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip, deflate");
  if (active_segment->get_byte_length()) {
      char rangebuf[128];
      sprintf(rangebuf, "bytes=%" PRIu64 "-%" PRIu64, uint64_t(active_segment->get_byte_offset()),
          uint64_t(active_segment->get_byte_offset()) + uint64_t(active_segment->get_byte_length()));
      xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Range", rangebuf);
  }

  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE | XFILE::READ_AUDIO_VIDEO);

  // read the file
  char *buf = (char*)malloc(1024*1024);
  size_t nbRead, nbReadOverall = 0;
  while ((nbRead = xbmc->ReadFile(file, buf, 1024 * 1024)) > 0 && ~nbRead && active_segment->write_data(buf, nbRead)) nbReadOverall+= nbRead;
  free(buf);

  if (!nbReadOverall)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Download %s doesn't provide any data: invalid", url);
    return false;
  }

  double current_download_speed_ = xbmc->GetFileDownloadSpeed(file);
  //Calculate the new downloadspeed to 1MB
  static const size_t ref_packet = 1024 * 1024;
  if (nbReadOverall >= ref_packet)
    download_speed = current_download_speed_;
  else {
    double ratio = (double)nbReadOverall / ref_packet;
    download_speed = (download_speed * (1.0 - ratio)) + current_download_speed_*ratio;
  }

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished, average download speed: %0.4lf", url.c_str(), download_speed);

  return nbRead == 0;
}

void KodiMasterPlaylist::select_media_playlist() {
  if (media_playlist.empty()) {
    // Assume the URL we were given was a media playlist and not a master playlist
    hls::MediaPlaylist playlist;
    download_playlist_impl(get_url().c_str(), playlist);
    media_playlist.push_back(playlist);
  }
  // Just selects the first one for now
  for(std::vector<hls::MediaPlaylist>::iterator it = media_playlist.begin(); it != media_playlist.end(); ++it) {
      download_playlist_impl(it->get_url().c_str(), *it);
      break;
  }
}

bool KodiMasterPlaylist::open(const char *url) {
  download_playlist_impl(url, *this);
}
