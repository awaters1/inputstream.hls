/*
 * file_downloader.h
 *
 *  Created on: Mar 22, 2017
 *      Author: anthony
 */

#pragma once

#include "downloader.h"

static const int BANDWIDTH_BINS = 5;
static const double COEFFICIENTS[] = {
    0.5,
    0.3,
    0.1,
    0.5,
    0.5
};

class KodiDownloader : public Downloader {
public:
  KodiDownloader();
  std::string download(std::string location, uint32_t byte_offset, uint32_t byte_length);
  // Bytes per second
  double get_current_bandwidth();
  double get_average_bandwidth();
private:
  double bandwidth_measurements[BANDWIDTH_BINS];
  uint32_t current_measurement_index;
  uint32_t number_of_measurements;
};
