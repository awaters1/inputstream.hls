
#pragma once


namespace hls
{
  class MasterPlaylist {
  protected:
    bool write_data(std::string line);
  private:
    bool is_m3u8;
  };


  class FileMasterPlaylist : MasterPlaylist {
  public:
    bool open(const char *file_path);
  };
}
