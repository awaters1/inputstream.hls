/*
 * segment_storage.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <cstring>

#include "globals.h"
#include "segment_storage.h"

#define LOGTAG                  "[SegmentStorage] "

SegmentStorage::SegmentStorage() :
offset(0),
read_segment_data_index(0),
write_segment_data_index(0),
has_room(true),
segment_data(MAX_SEGMENTS) {
}

bool SegmentStorage::start_segment(hls::Segment segment) {
  SegmentData &current_segment_data = segment_data.at(write_segment_data_index);
  if (current_segment_data.can_overwrite == false) {
    return false;
  } else if (current_segment_data.segment.valid && write_segment_data_index == read_segment_data_index) {
    // We are overwriting an existing element so incremet read pointer
    read_segment_data_index = (read_segment_data_index + 1) % MAX_SEGMENTS;
  }
  offset += current_segment_data.contents.length();
  current_segment_data.segment = segment;
  current_segment_data.contents.clear();
  current_segment_data.can_overwrite = false;
  return true;
}

void SegmentStorage::write_segment(hls::Segment segment, std::string data) {
  if (segment_data.at(write_segment_data_index).segment == segment) {
    segment_data.at(write_segment_data_index).contents += data;
  }
}

void SegmentStorage::end_segment(hls::Segment segment) {
  if (segment_data.at(write_segment_data_index).segment == segment) {
    write_segment_data_index = (write_segment_data_index + 1) % MAX_SEGMENTS;
  }
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


hls::Segment SegmentStorage::read(uint64_t pos, size_t &size, uint8_t * const destination) {
  size_t destination_offset = 0;
  uint32_t current_read_segment_index = read_segment_data_index;
  uint64_t next_offset = offset;
  hls::Segment first_segment;
  size_t data_read = 0;
  size_t wanted_data = size;
  while(size > 0) {
    size_t relative_offset;
    if (pos >= next_offset) {
      relative_offset = pos - next_offset;
    } else {
      relative_offset = 0; // start at beginning of segment
    }
    SegmentData &current_segment = segment_data.at(current_read_segment_index);
    if (!current_segment.segment.valid) {
      break;
    }
    bool go_to_next_segment = false;
    if (relative_offset < current_segment.contents.length()) {
      if (!first_segment.valid) {
        first_segment = current_segment.segment;
      }
      size_t data_left_in_segment = current_segment.contents.length() - relative_offset;
      size_t data_to_read_from_segment;
      if (data_left_in_segment < size) {
        data_to_read_from_segment = data_left_in_segment;
        next_offset += current_segment.contents.length();
      } else {
        data_to_read_from_segment = size;
      }
      std::memcpy(destination + destination_offset,
          current_segment.contents.c_str() + relative_offset, data_to_read_from_segment);
      destination_offset += data_to_read_from_segment;
      size -= data_to_read_from_segment;
      data_read += data_to_read_from_segment;
    } else {
      // We read all of the data in this segment so it is safe to overwrite
      current_segment.can_overwrite = true;
      next_offset += current_segment.contents.length();
    }
    current_read_segment_index = (current_read_segment_index + 1)% MAX_SEGMENTS;
  }
  if (data_read < wanted_data) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Not enough data read", __FUNCTION__);
  }
  size = data_read;
  return first_segment;
}

bool SegmentStorage::has_segment(hls::Segment segment) {
  for(auto &data : segment_data) {
    if (data.segment == segment) {
      return true;
    }
  }
  return false;
}

void SegmentStorage::reset_segment(hls::Segment segment) {
  for(auto &data : segment_data) {
    if (data.segment == segment) {
      data.can_overwrite = false;
      break;
    }
  }
}

uint64_t SegmentStorage::get_segment_start_position(hls::Segment segment) {
  uint64_t start_position = offset;
  for(uint32_t start = read_segment_data_index; start < MAX_SEGMENTS + read_segment_data_index; ++start) {
    if (segment_data.at(start % MAX_SEGMENTS).segment == segment) {
      return start_position;
    }
    start_position += segment_data.at(start % MAX_SEGMENTS).contents.length();
  }
  return offset;
}
