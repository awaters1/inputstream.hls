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

#include <string>
#include <vector>
#include <mutex>
#include <list>

#include "gtest/gtest_prod.h"

namespace hls
{
  class Resource {
  public:
    std::string get_url() const { return url; };
    std::string get_base_url() { return base_url; };

    void set_url(std::string url);
  protected:
    std::string url;
    std::string base_url;
  };

  class Playlist : public Resource {
  public:
    Playlist() : Resource(), is_m3u8(false) {};

    virtual bool write_data(std::string line) = 0;
  protected:
    FRIEND_TEST(HlsTest, GetAttributeValue);
    std::string get_attribute_value(std::string line, std::string attribute_name);
    uint32_t get_number_attribute_value(std::string line, std::string attribute_name);
    FRIEND_TEST(HlsTest, GetAttributes);
    std::vector<std::string> get_attributes(std::string line);
    FRIEND_TEST(HlsTest, GetAttributeValueString);
    std::string get_string_attribute_value(std::string line, std::string attribute_name);

    bool is_m3u8;
  };

  class Segment : public Resource {
    friend class MediaPlaylist;
  public:
    Segment();
    float duration;
    double time_in_playlist; // when the segment is in the playlist
    std::string description;
    uint32_t media_sequence;
    std::string aes_uri;
    std::string aes_iv;
    bool encrypted;
    bool valid;
    bool discontinuity;
    uint32_t byte_length;
    uint32_t byte_offset;
    bool operator==(Segment segment) const {
    	return get_url() == segment.get_url() &&
    			byte_length == segment.byte_length &&
				byte_offset == segment.byte_offset &&
				media_sequence == segment.media_sequence;
    }
  };

  class MediaPlaylist : public Playlist {
    friend class MasterPlaylist;
  public:
    MediaPlaylist();
    ~MediaPlaylist();
    bool operator!=(const MediaPlaylist& rhs){
      return this->get_url() != rhs.get_url();
    }
    uint32_t bandwidth;
    std::string program_id;
    bool encrypted;
    std::string aes_uri;
    std::string aes_iv;
    bool live;
    bool discontinuity;
    float get_segment_target_duration() { return segment_target_duration; };
    bool load_contents(std::string playlist_contents);
    bool valid;
    std::vector<Segment>& get_segments() { return segments; };
    void set_segments(std::list<Segment> other) {
       segments.clear();
       segments.insert(segments.end(), other.begin(), other.end());
    };
  protected:
    bool write_data(std::string line);
  private:
    bool in_segment;
    float segment_target_duration;
    uint32_t starting_media_sequence;
    uint32_t current_media_sequence;
    std::vector<Segment> segments;
  };

  class MasterPlaylist : public Playlist {
  public:
    MediaPlaylist& get_media_playlist(size_t index) { return media_playlist.at(index); };
    std::vector<MediaPlaylist>& get_media_playlists() { return media_playlist; };
    MasterPlaylist();
    ~MasterPlaylist();

    bool write_data(std::string line);
  protected:
    std::vector<MediaPlaylist> media_playlist;
  private:
    bool in_stream;
  };

  class FileMasterPlaylist : public MasterPlaylist {
  public:
    bool open(const char *file_path);
  };

  class FileMediaPlaylist : public MediaPlaylist {
  public:
    bool open(const char *file_path);
  };
}
