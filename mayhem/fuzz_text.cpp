// Text and encoding utilities: encoding guessing, UTF-8/Latin-1 conversion,
// parselo string helpers, MD5 and base64.
#include "globalincs/pstypes.h"
#include "parse/parselo.h"
#include "parse/md5_hash.h"
#include "utils/base64.h"
#include "utils/encoding.h"
#include "utils/string_utils.h"
#include "utils/unicode.h"
#include "osapi/dialogs.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern bool Unicode_text_mode;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2)
		return -1;
	const uint8_t sel = data[0];
	Unicode_text_mode = (sel & 0x80) != 0;
	const char* p = reinterpret_cast<const char*>(data + 1);
	const size_t n = size - 1;
	SCP_string str(p, n);
	// exact-size NUL-terminated heap copy so ASan sees overreads
	std::vector<char> cstr(str.c_str(), str.c_str() + n + 1);

	try {
		switch (sel % 12) {
		case 0:
			(void)util::guess_encoding(str, (sel & 0x40) != 0);
			(void)util::has_bom(str);
			break;
		case 1:
			(void)util::guessLatin1Encoding(p, n);
			break;
		case 2: {
			SCP_string out;
			using unicode::Encoding;
			Encoding e1 = static_cast<Encoding>((sel >> 4) % 3);
			Encoding e2 = static_cast<Encoding>((sel >> 2) % 3);
			unicode::convert_encoding(out, cstr.data(), e1, e2);
			break;
		}
		case 3:
			(void)unicode::string_is_ascii_only(p, n);
			(void)unicode::num_codepoints(p, p + n);
			break;
		case 4: {
			std::vector<char> out(n * 4 + 8);
			maybe_convert_foreign_characters(cstr.data(), out.data(), true);
			break;
		}
		case 5: {
			SCP_string s = str;
			unicode::Encoding e = (sel & 0x40) ? unicode::Encoding::Encoding_utf8 : unicode::Encoding::Encoding_iso8859_1;
			SCP_string out;
			unicode::convert_encoding(out, cstr.data(), e);
			break;
		}
		case 6:
			end_string_at_first_hash_symbol(cstr.data(), (sel & 0x40) != 0);
			break;
		case 7:
			consolidate_double_characters(cstr.data(), static_cast<char>(sel >> 1));
			drop_leading_white_space(cstr.data());
			drop_trailing_white_space(cstr.data());
			break;
		case 8: {
			SCP_string s = str;
			consolidate_double_characters(s, '"');
			drop_leading_white_space(s);
			drop_trailing_white_space(s);
			end_string_at_first_hash_symbol(s, true);
			break;
		}
		case 9: {
			SCP_string h = md5_hash(p, n);
			(void)h;
			break;
		}
		case 10: {
			std::string enc = base64_encode(data + 1, static_cast<unsigned int>(n));
			std::string dec = base64_decode(enc);
			(void)dec;
			(void)base64_decode(std::string(p, n));
			break;
		}
		case 11: {
			auto parts = util::split_string(std::string(p, n), static_cast<char>(sel >> 1));
			(void)util::isStringOneOf(str, parts);
			break;
		}
		}
	} catch (const os::dialogs::DialogException&) {
		return -1;
	} catch (const parse::ParseException&) {
		return -1;
	}
	return 0;
}
