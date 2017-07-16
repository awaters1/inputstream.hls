/*
 * download_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <sstream>
#include <iostream>
#include <inttypes.h>

#include "../globals.h"
#include "kodi_downloader.h"

KodiDownloader::KodiDownloader(double bandwidth) :
  number_of_measurements(1),
  current_measurement_index(1) {
  for(uint32_t i = 0; i < BANDWIDTH_BINS; ++i) {
    bandwidth_measurements[i] = 0;
  }
  bandwidth_measurements[0] = bandwidth;
}

double KodiDownloader::get_current_bandwidth() {
  return bandwidth_measurements[current_measurement_index];
}

double KodiDownloader::get_average_bandwidth() {
  double sum = 0;
  for(int i = current_measurement_index; i < current_measurement_index + BANDWIDTH_BINS; ++i) {
    sum += bandwidth_measurements[i % BANDWIDTH_BINS];
  }
  if (number_of_measurements > 0) {
    return sum / (double) number_of_measurements;
  }
  return 0;
}

void KodiDownloader::download(std::string url, uint32_t byte_offset, uint32_t byte_length, std::function<bool(std::string)> func) {
  // open the file
  void* file = xbmc->CURLCreate(url.c_str());
  if (!file)
    func("");
  if (byte_length) {
      char rangebuf[128];
      sprintf(rangebuf, "bytes=%" PRIu64 "-%" PRIu64, uint64_t(byte_offset),
          uint64_t(byte_offset) + uint64_t(byte_length));
      xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Range", rangebuf);
  }

  // xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE);
  xbmc->CURLOpen(file, 0);

  // read the file
  char *buf = (char*)malloc(4*1024);
  size_t nbRead, nbReadOverall = 0;
  std::string ret;
  while ((nbRead = xbmc->ReadFile(file, buf, 4 * 1024)) > 0 && ~nbRead) {
    nbReadOverall+= nbRead;
    bool successfull = func(std::string(buf, nbRead));
    if (!successfull) {
      xbmc->Log(ADDON::LOG_DEBUG, "Download cancelled");
      break;
    }
  }
  free(buf);

  if (!nbReadOverall)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Download %s doesn't provide any data: invalid, nbRead: %d", url.c_str(), nbRead);
    func("");
  }

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished", url.c_str());
}

std::string KodiDownloader::download(std::string url) {
  // open the file
  void* file = xbmc->CURLCreate(url.c_str());
  if (!file)
    return "";
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable" , "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Connection", "keep-alive");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip, deflate");

  xbmc->CURLOpen(file, XFILE::READ_CHUNKED | XFILE::READ_NO_CACHE);

  // read the file
  char *buf = (char*)malloc(16*1024);
  size_t nbRead, nbReadOverall = 0;
  std::string ret;
  while ((nbRead = xbmc->ReadFile(file, buf, 16 * 1024)) > 0 && ~nbRead) {
    nbReadOverall+= nbRead;
    ret += std::string(buf, nbRead);
  }
  free(buf);

  if (!nbReadOverall)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Download %s doesn't provide any data: invalid", url.c_str());
    return "";
  }
  xbmc->CloseFile(file);

  return ret;
}

void KodiDownloader::record_bandwidth(double bandwidth_kbps) {
  if (current_measurement_index >= BANDWIDTH_BINS) {
    current_measurement_index = 0;
  }
  bandwidth_measurements[current_measurement_index++] = bandwidth_kbps * 1024;
  if (number_of_measurements < BANDWIDTH_BINS) {
    ++number_of_measurements;
  }
}
