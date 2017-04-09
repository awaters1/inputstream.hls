/*
 * ring_buffer.h
 *
 *  Created on: Apr 6, 2017
 *      Author: watersan
 */

#pragma once

#include <memory>

class RingBuffer {
public:
  RingBuffer(size_t max_size);
  // @return the number of bytes displaced
  size_t put(const std::string &data);
  // @return the number of bytes read
  size_t read(size_t pos, size_t size, uint8_t * const destination);
  bool has_data(size_t pos, size_t size);
  size_t get_size();
  size_t get_data_end_pos();
private:
  // offset from the true beginning of the data
  size_t offset;
  size_t max_size;
  size_t start;
  size_t end;
  std::string contents;
};
