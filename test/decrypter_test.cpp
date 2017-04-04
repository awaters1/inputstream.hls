/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include <iostream>
#include "gtest/gtest.h"
#include "helpers.h"

#include "../src/hls/decrypter.h"

namespace hls {

TEST(DecrypterTest, Decrypt) {
  std::string aes_key = "3uGvlV84qanaLAtEEPNMBw==";
  std::string aes_iv = "0x9f11a1b6a9fe0d800f5c9688370e694d";
  std::string encrypted_data = load_file_contents("test/hls/encrypted_segment.ts");
  std::string gold_decrypted_data = load_file_contents("test/hls/decrypted_segment.ts");

  std::string decrypted_data = decrypt(aes_key, aes_iv, encrypted_data);
  decrypted_data = decrypted_data.substr(0, gold_decrypted_data.length());
  EXPECT_EQ(gold_decrypted_data.length(), decrypted_data.length());
  EXPECT_TRUE(decrypted_data == gold_decrypted_data);
}

TEST(DecrypterTest, Decrypt2) {
  std::string aes_key = load_file_contents("test/encrypted/aes_key");
  std::string aes_iv = load_file_contents("test/encrypted/aes_iv");
  std::string encrypted_data = load_file_contents("test/encrypted/D00000002.ts");
  std::string gold_decrypted_data = load_file_contents("test/encrypted/D00000002-decrypted.ts");

  std::string decrypted_data = decrypt(aes_key, aes_iv, encrypted_data);
  decrypted_data = decrypted_data.substr(0, gold_decrypted_data.length());
  EXPECT_EQ(gold_decrypted_data.length(), decrypted_data.length());
  EXPECT_TRUE(decrypted_data == gold_decrypted_data);
}

TEST(DecrypterTest, DecryptPartial) {
  std::string aes_key = load_file_contents("test/encrypted/aes_key");
  std::string aes_iv = load_file_contents("test/encrypted/aes_iv");
  std::string encrypted_data = load_file_contents("test/encrypted/D00000002.ts");
  std::string gold_decrypted_data = load_file_contents("test/encrypted/D00000002-decrypted.ts");

  std::string decrypted_data = decrypt(aes_key, aes_iv, encrypted_data.substr(0, 128));
  EXPECT_TRUE(decrypted_data == gold_decrypted_data.substr(0, 128));

  aes_iv = encrypted_data.substr(112, 128);
  decrypted_data = decrypt(aes_key, aes_iv, encrypted_data.substr(128, 256));
  EXPECT_TRUE(decrypted_data == gold_decrypted_data.substr(128, 256));
}

}
