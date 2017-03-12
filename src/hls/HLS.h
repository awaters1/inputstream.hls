
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
  protected:
    void set_url(std::string url);
    std::string url;
    std::string base_url;
  };

  class Playlist : public Resource {
  public:
    Playlist() : is_m3u8(false) {};
  protected:
    virtual bool write_data(std::string line) = 0;
    bool is_m3u8;
  };

  class Segment : public Resource {
    friend class MediaPlaylist;
  public:
    float duration;
    std::string description;
    uint32_t media_sequence;
  };

  class MediaPlaylist : public Playlist {
    friend class MasterPlaylist;
  public:
    MediaPlaylist();
    ~MediaPlaylist();
    uint32_t bandwidth;
    std::string program_id;
    std::vector<Segment*> get_segments() { return segments; };
  protected:
    bool write_data(std::string line);
  private:
    FRIEND_TEST(HlsTest, GetAttributes);
    std::vector<std::string> get_attributes(std::string line);

    bool in_segment;
    float segment_target_duration;
    uint32_t starting_media_sequence;
    uint32_t current_media_sequence;
    std::vector<Segment*> segments;
  };

  class MasterPlaylist : public Playlist {
  public:
    std::vector<MediaPlaylist*> get_media_playlist() { return media_playlist; };
    MasterPlaylist();
    ~MasterPlaylist();
  protected:
    bool write_data(std::string line);
  private:
    FRIEND_TEST(HlsTest, GetAttributeValue);
    std::string get_attribute_value(std::string line, std::string attribute_name);
    uint32_t get_number_attribute_value(std::string line, std::string attribute_name);

    bool in_stream;

    std::vector<MediaPlaylist*> media_playlist;
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
