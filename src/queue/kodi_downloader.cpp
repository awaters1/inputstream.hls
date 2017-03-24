/*
 * download_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <sstream>

#include "kodi_downloader.h"

std::string KodiDownloader::download(std::string url, uint32_t byte_offset, uint32_t byte_length) {
  // open the file
  void* file = xbmc->CURLCreate(url.c_str());
  if (!file)
    return false;
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable" , "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Connection", "keep-alive");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip, deflate");
  if (byte_length) {
      char rangebuf[128];
      sprintf(rangebuf, "bytes=%" PRIu64 "-%" PRIu64, uint64_t(byte_offset),
          uint64_t(byte_offset) + uint64_t(byte_length));
      xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Range", rangebuf);
  }

  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE | XFILE::READ_AUDIO_VIDEO);

  // read the file
  char *buf = (char*)malloc(1024*1024);
  size_t nbRead, nbReadOverall = 0;
  std::string ret;
  while ((nbRead = xbmc->ReadFile(file, buf, 1024 * 1024)) > 0 && ~nbRead) {
    nbReadOverall+= nbRead;
    ret += std::string(buf, nbRead);
  }
  free(buf);

  if (!nbReadOverall)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Download %s doesn't provide any data: invalid", url.c_str());
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
  download_speed *= 8;

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished, average download speed: %0.4lf", url.c_str(), download_speed);

  return ret;
}
