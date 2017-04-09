/*
 * segment_storage.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <cstring>

#include "segment_storage.h"

SegmentStorage::SegmentStorage() :
offset(0),
read_segment_data_index(0),
write_segment_data_index(0),
has_room(true),
segment_data(MAX_SEGMENTS) {
}

bool SegmentStorage::start_segment(hls::Segment segment) {
  if (!has_room) {
    return false;
  }
  segment_data.at(write_segment_data_index).segment = segment;
  segment_data.at(write_segment_data_index).contents.clear();
  return true;
}

void SegmentStorage::write_segment(std::string data) {
  segment_data.at(write_segment_data_index).contents += data;
}

void SegmentStorage::end_segment() {
  write_segment_data_index = (write_segment_data_index + 1) % MAX_SEGMENTS;
  has_room = write_segment_data_index != read_segment_data_index;
}

size_t SegmentStorage::get_size() {
  size_t size(0);
  for(auto &s : segment_data) {
    size += s.contents.length();
  }
  return size;
}

bool SegmentStorage::has_data(uint64_t pos, size_t size) {
  return pos >= offset && ((pos - offset) + size) <= (get_size());
}

void SegmentStorage::read(uint64_t pos, size_t size, uint8_t * const destination) {
  size_t destination_offset = 0;
  while(size > 0) {
    size_t relative_offset;
    if (pos >= offset) {
      relative_offset = pos - offset;
    } else {
      relative_offset = 0; // start at beginning of segment
    }
    SegmentData &current_segment = segment_data.at(read_segment_data_index);
    if (!current_segment.segment.valid) {
      break;
    }
    bool go_to_next_segment = false;
    if (relative_offset < current_segment.contents.length()) {
      size_t data_left_in_segment = current_segment.contents.length() - relative_offset;
      size_t data_to_read_from_segment;
      if (data_left_in_segment < size) {
        data_to_read_from_segment = data_left_in_segment;
      } else {
        data_to_read_from_segment = size;
      }
      std::memcpy(destination + destination_offset,
          current_segment.contents.c_str() + relative_offset, data_to_read_from_segment);
      destination_offset += data_to_read_from_segment;
      size -= data_to_read_from_segment;
      go_to_next_segment = relative_offset + data_to_read_from_segment == current_segment.contents.length();
    } else {
       go_to_next_segment = true;
    }
    if (go_to_next_segment) {
      read_segment_data_index = (read_segment_data_index + 1) % MAX_SEGMENTS;
      offset += current_segment.contents.length();
      has_room = true;
    }
  }
}
