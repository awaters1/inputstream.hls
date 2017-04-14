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
