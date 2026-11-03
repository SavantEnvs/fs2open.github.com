// FS1/FS2 table-file scrambling: detect, decode arbitrary data, and re-encode.
#include "globalincs/pstypes.h"
#include "parse/encrypt.h"

#include <cstdint>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	encrypt_init();
	if (size < 5)
		return -1;
	const uint8_t sel = data[0];
	const char* in = reinterpret_cast<const char*>(data + 1);
	const int n = static_cast<int>(size - 1);

	if (sel & 1) {
		// decode: the FS1 7-bit format unpacks 7 bytes into 8 chars, so size the output for that
		std::vector<char> src(in, in + n);
		const int dst_cap = (n / 7 + 2) * 8;
		std::vector<char> dst(dst_cap);
		int out_len = 0;
		(void)is_encrypted(src.data());
		(void)encrypt_type(src.data());
		unencrypt(src.data(), n, dst.data(), &out_len);
		if (out_len < 0 || out_len > dst_cap)
			return -1;
	} else {
		// encode then decode
		std::vector<char> text(in, in + n);
		// the 7-bit packer shifts signed chars; the format is for ASCII text
		if (!((sel >> 1) & 1) || ((sel >> 2) & 1) == 0) {
			for (auto& c : text)
				c &= 0x7f;
		}
		std::vector<char> scr(static_cast<size_t>(n) * 2 + 64);
		int scr_len = 0;
		encrypt(text.data(), n, scr.data(), &scr_len, (sel >> 1) & 1, ((sel >> 2) & 1) != 0);
		if (scr_len < 4 || scr_len > static_cast<int>(scr.size()))
			return -1;
		std::vector<char> back((scr_len / 7 + 2) * 8);
		int back_len = 0;
		unencrypt(scr.data(), scr_len, back.data(), &back_len);
	}
	return 0;
}
