
#pragma once

#include <string>
#include <vector>

#include "gtest/gtest_prod.h"

namespace hls
{
  class MediaPlaylist {
    public:
      uint32_t bandwidth;
      std::string program_id;
      std::string url;
    };

  class MasterPlaylist {
  public:
    std::string get_base_url() { return base_url; };
    std::vector<MediaPlaylist*> get_streams() { return streams; };
    MasterPlaylist();
    ~MasterPlaylist();
  protected:
    bool write_data(std::string line);
    void set_url(std::string url);
  private:
    FRIEND_TEST(HlsTest, GetAttributeValue);
    std::string get_attribute_value(std::string line, std::string attribute_name);
    uint32_t get_number_attribute_value(std::string line, std::string attribute_name);

    bool is_m3u8;
    bool in_stream;
    std::string url;
    std::string base_url;
    std::vector<MediaPlaylist*> streams;
  };

  class FileMasterPlaylist : public MasterPlaylist {
  public:
    bool open(const char *file_path);
  };
}
