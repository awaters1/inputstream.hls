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
  double download_speed;
};
