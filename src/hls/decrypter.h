/*
 * decrypter.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "Ap4Protection.h"


std::string decrypt(std::string b64_aes_key, std::string iv_str, std::string encrypted_data_str);
