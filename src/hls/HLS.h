
#pragma once

#include <string>
#include <vector>

#include "gtest/gtest_prod.h"

namespace hls
{
  class Resource {
  public:
    std::string get_url() { return url; };
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
    std::string description;
    uint32_t media_sequence;
    std::string aes_uri;
    std::string aes_iv;
    bool encrypted;
    uint32_t byte_length;
    uint32_t byte_offset;
  };

  class MediaPlaylist : public Playlist {
    friend class MasterPlaylist;
  public:
    MediaPlaylist();
    ~MediaPlaylist();
    uint32_t bandwidth;
    std::string program_id;
    bool encrypted;
    std::string aes_uri;
    std::string aes_iv;
    bool live;
    void add_segment(Segment segment) { segments.push_back(segment); };
    std::vector<Segment> get_segments() { return segments; };
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
    std::vector<MediaPlaylist> get_media_playlist() { return media_playlist; };
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
