
#pragma once

#include <string>
#include <vector>

#include "gtest/gtest_prod.h"

namespace hls
{
  class MasterStream {
    public:
      uint32_t bandwidth;
      std::string program_id;
    };

  class MasterPlaylist {
  protected:
    bool write_data(std::string line);
  private:
    FRIEND_TEST(HlsTest, GetAttributeValue);
    std::string get_attribute_value(std::string line, std::string attribute_name);
    uint32_t get_number_attribute_value(std::string line, std::string attribute_name);

    bool is_m3u8;
    bool in_stream;
    std::vector<MasterStream> streams;
  };




  class FileMasterPlaylist : MasterPlaylist {
  public:
    bool open(const char *file_path);
  };
}
