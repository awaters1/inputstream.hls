/*
 * download_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <string>

class Downloader {
public:
  std::string download(std::string location) {
    return download(location, 0, 0);
  };
  virtual std::string download(std::string location, uint32_t byte_offset, uint32_t byte_length) = 0;
  virtual double get_average_bandwidth() = 0;
  virtual double get_current_bandwidth() = 0;
};
