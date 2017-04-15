#pragma once
/*
 *      Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "downloader.h"

static const int BANDWIDTH_BINS = 5;
static const double COEFFICIENTS[] = {
    0.5,
    0.3,
    0.1,
    0.05,
    0.05
};

class KodiDownloader : public Downloader {
public:
  KodiDownloader(double bandwidth);
  void download(std::string location, uint32_t byte_offset, uint32_t byte_length, std::function<bool(std::string)> func);
  std::string download(std::string location);
  // Bytes per second
  double get_current_bandwidth();
  double get_average_bandwidth();
private:
  double bandwidth_measurements[BANDWIDTH_BINS];
  uint32_t current_measurement_index;
  uint32_t number_of_measurements;
};
