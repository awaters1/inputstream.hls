/*
 * ring_buffer.cpp
 *
 *  Created on: Apr 6, 2017
 *      Author: watersan
 */

#include <cstring>
#include <algorithm>

#include "ring_buffer.h"

RingBuffer::RingBuffer(size_t max_size) :
  max_size(max_size),
  start(0),
  end(0),
  offset(0) {
  contents.reserve(max_size);
}

size_t RingBuffer::get_size() {
  if (offset > 0) {
    return max_size;
  } else {
    return end - start;
  }
}

size_t RingBuffer::put(const std::string &data) {
  size_t new_length = get_size() + data.length();
  size_t bytes_displaced = 0;

  if (new_length >= max_size) {
    size_t overflow = (new_length - max_size);
    if (overflow > 0) {
      start += overflow;
      offset += overflow;
      bytes_displaced = overflow;
    }
  }
  size_t data_start = end;
  end += data.length();
  size_t data_end = end;

  // Normalize
  start = (start % max_size);
  end = (end % max_size);

  // Copy data
  if (data_end < max_size) {
    // No wrap around
    std::copy(data.begin(), data.end(), contents.begin() + data_start);
  } else {
    // Wrap around
    size_t first_part_size = max_size - data_start;
    size_t second_part_size = data.length() - first_part_size;
    std::copy(data.begin(), data.begin() + first_part_size, contents.begin() + data_start);
    std::copy(data.begin() + first_part_size, data.end(), contents.begin());
  }

  return bytes_displaced;
}

size_t RingBuffer::read(size_t pos, size_t size, uint8_t * const destination) {
  if (pos < offset) {
    if (offset - pos > size) {
      return 0;
    }
    size = size - (offset - pos);
    pos = offset;
  }
  if (size > max_size) {
    size = max_size;
  }
  size_t data_start = start + (pos - offset);
  size_t data_end = data_start + size;

  data_start = data_start % max_size;
  data_end = data_end % max_size;
  // Copy data
  if (data_start < data_end) {
    // No wrap around
    std::memcpy(destination, contents.c_str() + data_start, size);
  } else {
    // Wrap around
    size_t first_part_size = max_size - data_start;
    size_t second_part_size = size - first_part_size;
    memcpy(destination, contents.c_str() + data_start, first_part_size);
    memcpy(destination + first_part_size, contents.c_str(), second_part_size);
  }
  return size;
}

bool RingBuffer::has_data(size_t pos, size_t size) {
  return pos >= offset && (pos + size) <= (offset + get_size());
}

size_t RingBuffer::get_data_end_pos() {
  return offset + get_size();
}

