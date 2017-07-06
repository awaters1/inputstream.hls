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

#include <functional>
#include <string>

class Downloader {
public:
  virtual std::string download(std::string location) = 0;
  virtual void download(std::string location, uint32_t byte_offset, uint32_t byte_length,
      std::function<bool(std::string)> func) {
    func(download(location));
  }
  virtual double get_average_bandwidth() = 0;
  virtual double get_current_bandwidth() = 0;
  virtual void record_bandwidth(double bandwidth) = 0;
};
