
#pragma once

#include <string>
#include <vector>

#include "gtest/gtest_prod.h"

namespace hls
{
  class Playlist {
  public:
    std::string get_url() { return url; };
    std::string get_base_url() { return base_url; };
  protected:
    virtual bool write_data(std::string line) = 0;
    void set_url(std::string url);
    std::string url;
    std::string base_url;
  };

  class MediaPlaylist : public Playlist {
    friend class MasterPlaylist;
  public:
    uint32_t bandwidth;
    std::string program_id;
  protected:
    bool write_data(std::string line);
  };

  class MasterPlaylist : public Playlist {
  public:
    std::vector<MediaPlaylist*> get_streams() { return streams; };
    MasterPlaylist();
    ~MasterPlaylist();
  protected:
    bool write_data(std::string line);
  private:
    FRIEND_TEST(HlsTest, GetAttributeValue);
    std::string get_attribute_value(std::string line, std::string attribute_name);
    uint32_t get_number_attribute_value(std::string line, std::string attribute_name);

    bool is_m3u8;
    bool in_stream;

    std::vector<MediaPlaylist*> streams;
  };

  class FileMasterPlaylist : public MasterPlaylist {
  public:
    bool open(const char *file_path);
  };
}
