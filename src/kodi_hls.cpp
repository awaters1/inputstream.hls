/*
 * kodi_hls.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>

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
    if (buf[strlen(buf) - 1] == '\n') {
      buf[strlen(buf) - 1] = '\0';
    }
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

void KodiMasterPlaylist::select_media_playlist() {
  if (media_playlist.empty()) {
    // Assume the URL we were given was a media playlist and not a master playlist
    hls::MediaPlaylist playlist;
    download_playlist_impl(get_url().c_str(), playlist);
    media_playlist.push_back(playlist);
  }
  for(std::vector<hls::MediaPlaylist>::iterator it = media_playlist.begin(); it != media_playlist.end(); ++it) {
    std::cout << "Media Playlist: bandwidth: " << it->bandwidth << " url: " << it->get_url() << "\n";
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
