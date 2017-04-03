/*
 * download_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <sstream>
#include <iostream>
#include <inttypes.h>

#include "../globals.h"
#include "kodi_downloader.h"

KodiDownloader::KodiDownloader() :
  number_of_measurements(0),
  current_measurement_index(0) {
  for(uint32_t i = 0; i < BANDWIDTH_BINS; ++i) {
    bandwidth_measurements[i] = 0;
  }
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
  return sum;
}

std::string KodiDownloader::download(std::string url, uint32_t byte_offset, uint32_t byte_length) {
  // open the file
  void* file = xbmc->CURLCreate(url.c_str());
  if (!file)
    return "";
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "seekable" , "0");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Connection", "keep-alive");
  xbmc->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "acceptencoding", "gzip, deflate");
  if (byte_length) {
      char rangebuf[128];
      sprintf(rangebuf, "bytes=%" PRIu64 "-%" PRIu64, uint64_t(byte_offset),
          uint64_t(byte_offset) + uint64_t(byte_length));
      xbmc->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "Range", rangebuf);
  }

  xbmc->CURLOpen(file, XFILE::READ_CHUNKED);

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
    return "";
  }

  // Convert to bits/second
  double current_download_speed_ = xbmc->GetFileDownloadSpeed(file) * 8;
  //Calculate the new downloadspeed to 1MB
  static const size_t ref_packet = 1024 * 1024;
  double bandwidth_measurement;
  if (nbReadOverall >= ref_packet) {
    bandwidth_measurement = current_download_speed_;
  } else {
    double ratio = (double)nbReadOverall / ref_packet;
    bandwidth_measurement = (get_current_bandwidth() * (1.0 - ratio)) + current_download_speed_*ratio;
  }
  if (current_measurement_index >= BANDWIDTH_BINS) {
    current_measurement_index = 0;
  }
  bandwidth_measurements[current_measurement_index++] = bandwidth_measurement;
  if (number_of_measurements < BANDWIDTH_BINS) {
    ++number_of_measurements;
  }

  xbmc->CloseFile(file);

  xbmc->Log(ADDON::LOG_DEBUG, "Download %s finished, download speed: %0.4lf, average: %0.4lf",
      url.c_str(), get_current_bandwidth(), get_average_bandwidth());

  return ret;
}


