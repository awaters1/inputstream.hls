/*
 * download_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <functional>
#include <string>

class Downloader {
public:
  virtual std::string download(std::string location) = 0;
  virtual void download(std::string location, uint32_t byte_offset, uint32_t byte_length,
      std::function<void(std::string)> func) {
    func(download(location));
  }
  virtual double get_average_bandwidth() = 0;
  virtual double get_current_bandwidth() = 0;
};
