/*
 * decrypter.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <string>
#include <memory>

#include "../helpers.h"
#include "decrypter.h"

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

  delete cbc_d_block_cipher;
}

bool convert_hex_to_bytes(std::string hex, AP4_UI08 *iv, uint32_t iv_length) {
  uint32_t iv_counter = 0;
  for (uint32_t i = 0; i < hex.length(); i += 2) {
    std::string byteString = hex.substr(i, 2);
    AP4_UI08 byte = (char) strtol(byteString.c_str(), NULL, 16);
    iv[iv_counter++] = byte;
    if (iv_counter >= iv_length) {
      return true;
    }
  }
}


std::string decrypt(std::string b64_aes_key, std::string iv_str, std::string encrypted_data_str) {
  uint32_t aes_key_len = 16;
  std::unique_ptr<uint8_t> aes_key = std::unique_ptr<uint8_t>(new uint8_t[aes_key_len]);
  if (b64_aes_key.length() == 16) {
    memcpy(aes_key.get(), b64_aes_key.c_str(), aes_key_len);
  } else {
    b64_decode(b64_aes_key.c_str(), b64_aes_key.length(), aes_key.get(), aes_key_len);
  }
  if (iv_str.find("0x") == 0) {
    iv_str = iv_str.substr(2);
  }
  uint32_t iv_len = 16;
  std::unique_ptr<uint8_t> iv = std::unique_ptr<uint8_t>(new uint8_t[iv_len]);
  if (iv_str.length() == 32) {
    convert_hex_to_bytes(iv_str, iv.get(), 16);
  } else {
    memcpy(iv.get(), iv_str.c_str(), iv_len);
  }


  const uint8_t* encrypted_data = reinterpret_cast<const uint8_t*>(encrypted_data_str.c_str());

  std::unique_ptr<AP4_UI08> output = std::unique_ptr<AP4_UI08>(new AP4_UI08[encrypted_data_str.length()]);

  decrypt(aes_key.get(), iv.get(), encrypted_data, encrypted_data_str.length(), output.get());

  return std::string(reinterpret_cast<char*>(output.get()), encrypted_data_str.length());
}
