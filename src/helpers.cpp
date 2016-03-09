/*
*      Copyright (C) 2016-2016 peak3d
*      http://www.peak3d.de
*
*  This Program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This Program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  <http://www.gnu.org/licenses/>.
*
*/

#include "helpers.h"
#include <cstring>
#include "oscompat.h"
#include <stdlib.h>

#ifndef BYTE
typedef unsigned char BYTE;
#endif

static const BYTE from_base64[] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 62, 255, 63,
52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 0, 255, 255, 255,
255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 63,
255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255 };


bool b64_decode(const char *in, unsigned int in_len, uint8_t *out, unsigned int &out_len)
{
	// Make sure string length is a multiple of 4
	char *in_copy(0);
	if (in_len > 3 && strnicmp(in + (in_len - 3), "%3D", 3) == 0)
	{
		in_copy = (char *)malloc(in_len + 1);
		strcpy(in_copy, in);
		in = in_copy;
		if (in_len > 6 && strnicmp(in + (in_len - 6), "%3D", 3) == 0)
		{
			strcpy(in_copy + (in_len - 6), "==");
			in_len -= 4;
		}
		else {
			strcpy(in_copy + (in_len - 3), "=");
			in_len -= 2;
		}
	}

	if (in_len & 3)
	{
		free(in_copy);
		return false;
	}

	unsigned int new_out_len = in_len / 4 * 3;
	if (in[in_len - 1] == '=') --new_out_len;
	if (in[in_len - 2] == '=') --new_out_len;
	if (new_out_len > out_len)
	{
		free(in_copy);
		return false;
	}
	out_len = new_out_len;

	for (size_t i = 0; i < in_len; i += 4)
	{
		// Get values for each group of four base 64 characters
		BYTE b4[4];
		b4[0] = (in[i + 0] <= 'z') ? from_base64[in[i + 0]] : 0xff;
		b4[1] = (in[i + 1] <= 'z') ? from_base64[in[i + 1]] : 0xff;
		b4[2] = (in[i + 2] <= 'z') ? from_base64[in[i + 2]] : 0xff;
		b4[3] = (in[i + 3] <= 'z') ? from_base64[in[i + 3]] : 0xff;

		// Transform into a group of three bytes
		BYTE b3[3];
		b3[0] = ((b4[0] & 0x3f) << 2) + ((b4[1] & 0x30) >> 4);
		b3[1] = ((b4[1] & 0x0f) << 4) + ((b4[2] & 0x3c) >> 2);
		b3[2] = ((b4[2] & 0x03) << 6) + ((b4[3] & 0x3f) >> 0);

		// Add the byte to the return value if it isn't part of an '=' character (indicated by 0xff)
		if (b4[1] != 0xff) *out++ = b3[0];
		if (b4[2] != 0xff) *out++ = b3[1];
		if (b4[3] != 0xff) *out++ = b3[2];
	}
	free(in_copy);
	return true;
}

static const char *to_base64 =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ\
abcdefghijklmnopqrstuvwxyz\
0123456789+/";

std::string b64_encode(unsigned char const* in, unsigned int in_len, bool urlEncode)
{
	std::string ret;
	int i(3);
	unsigned char c_3[3];
	unsigned char c_4[4];

	while (in_len) {
		i = in_len > 2 ? 3 : in_len;
		in_len -= i;
		c_3[0] = *(in++);
		c_3[1] = i > 1 ? *(in++) : 0;
		c_3[2] = i > 2 ? *(in++) : 0;

		c_4[0] = (c_3[0] & 0xfc) >> 2;
		c_4[1] = ((c_3[0] & 0x03) << 4) + ((c_3[1] & 0xf0) >> 4);
		c_4[2] = ((c_3[1] & 0x0f) << 2) + ((c_3[2] & 0xc0) >> 6);
		c_4[3] = c_3[2] & 0x3f;

		for (int j = 0; (j < i + 1); ++j)
		{
			if (urlEncode && to_base64[c_4[j]] == '+')
				ret += "%2B";
			else if (urlEncode && to_base64[c_4[j]] == '/')
				ret += "%2F";
			else
				ret += to_base64[c_4[j]];
		}
	}
	while ((i++ < 3))
		ret += urlEncode ? "%3D" : "=";
	return ret;
}
