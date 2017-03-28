#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

std::string load_file_contents(const char* file_name) {
  std::ifstream file(file_name);
  if (!file.is_open()) {
    std::cerr << "Unable to open " << file_name << std::endl;
  }
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  return ostrm.str();
}
