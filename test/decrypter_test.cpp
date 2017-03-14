/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include <fstream>
#include "gtest/gtest.h"

#include "../src/hls/decrypter.h"

namespace hls {

std::string load_file_contents(const char* file_name) {
  std::ifstream file(file_name);
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  return ostrm.str();
}

TEST(DecrypterTest, Decrypt) {
  std::string aes_key = "3uGvlV84qanaLAtEEPNMBw==";
  std::string aes_iv = "0x9f11a1b6a9fe0d800f5c9688370e694d";
  std::string encrypted_data = load_file_contents("hls/encrypted_segment.ts");
  std::string gold_decrypted_data = load_file_contents("hls/decrpyted_segment.ts");

  std::string decrypted_data = decrypt(aes_key, aes_iv, encrypted_data);
  EXPECT_EQ(decrypted_data, gold_decrypted_data);
}

}
