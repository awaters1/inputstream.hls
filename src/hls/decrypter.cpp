/*
 * decrypter.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <string>

#include "../helpers.h"
#include "decrypter.h"

std::string decrypt(std::string b64_aes_key, std::string iv_str, std::string encrypted_data_str) {
  uint8_t *aes_key = new uint8_t[16];
  uint32_t aes_key_len = 16;
  b64_decode(b64_aes_key.c_str(), b64_aes_key.length(), aes_key, aes_key_len);

  const uint8_t* iv = reinterpret_cast<const uint8_t*>(iv_str.c_str());
  const uint8_t* encrypted_data = reinterpret_cast<const uint8_t*>(encrypted_data_str.c_str());

  AP4_UI08 *output = new AP4_UI08[encrypted_data_str.length()];

  delete aes_key;
}

void decrypt(const AP4_UI08 *aes_key, const AP4_UI08 *aes_iv, const AP4_UI08 *encrypted_data, AP4_Size encrypted_data_size, AP4_UI08 *output) {

  AP4_BlockCipher* cbc_d_block_cipher;
  AP4_DefaultBlockCipherFactory::Instance.CreateCipher(
      AP4_BlockCipher::AES_128,
      AP4_BlockCipher::DECRYPT,
      AP4_BlockCipher::CBC,
      NULL,
      aes_key,
      16,
      cbc_d_block_cipher);

  cbc_d_block_cipher->Process(encrypted_data, encrypted_data_size, output, aes_iv);
}
