// Known-answer probe over the real engine text/parse/crypto code, built with the project's normal
// flags. mayhem/test.sh runs it and compares the printed values with golden constants.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "globalincs/pstypes.h"
#include "def_files/def_files.h"
#include "parse/parselo.h"
#include "parse/md5_hash.h"
#include "parse/encrypt.h"
#include "utils/base64.h"
#include "utils/encoding.h"
#include "utils/unicode.h"
#include "osapi/dialogs.h"

extern bool Unicode_text_mode;

static void hex(const char* key, const std::string& s)
{
	printf("%s", key);
	for (unsigned char c : s)
		printf("%02x", c);
	printf("\n");
}

static void probe_parse()
{
	static const char table[] =
		"$Name: Test Ship ; trailing comment\n"
		"$Count: 42\n"
		"$Ratio: 3.5\n"
		"$Pos: 1.0, -2.5, 3.25\n"
		"+List: (1 2 3 5 8)\n"
		"/* block\n   comment */ $After: 7\n"
		"$Quoted: \"hello world\"\n"
		"#End\n";
	std::vector<char> raw(sizeof(table) * 4), proc(sizeof(table) * 4);
	default_file f;
	f.path_type = "data/tables";
	f.filename = "kat.tbl";
	f.data = table;
	f.size = sizeof(table) - 1;
	try {
		read_file_text_from_default(f, proc.data(), raw.data());
		reset_parse(proc.data());

		char name[64];
		int count = 0, after = 0;
		float ratio = 0;
		vec3d pos;
		SCP_vector<int> list;
		SCP_string quoted;

		required_string("$Name:");
		stuff_string(name, F_NAME, sizeof(name));
		printf("parse.name=%s\n", name);
		required_string("$Count:");
		stuff_int(&count);
		printf("parse.count=%d\n", count);
		required_string("$Ratio:");
		stuff_float(&ratio);
		printf("parse.ratio=%.2f\n", ratio);
		required_string("$Pos:");
		stuff_vec3d(&pos);
		printf("parse.pos=%.2f,%.2f,%.2f\n", pos.xyz.x, pos.xyz.y, pos.xyz.z);
		if (optional_string("+List:")) {
			stuff_int_list(list);
		}
		int sum = 0;
		for (int v : list)
			sum += v;
		printf("parse.list=%zu,%d\n", list.size(), sum);
		required_string("$After:");
		stuff_int(&after);
		printf("parse.after=%d\n", after);
		required_string("$Quoted:");
		stuff_string(quoted, F_NAME);
		printf("parse.quoted=%s\n", quoted.c_str());
		printf("parse.end=%d\n", check_for_string("#End") ? 1 : 0);
		stop_parse();
	} catch (const std::exception& e) {
		printf("parse.exception=%s\n", e.what());
	}
}

static void probe_text()
{
	// "abc" is the RFC 1321 test vector
	printf("md5.abc=%s\n", md5_hash("abc", 3).c_str());
	printf("md5.empty=%s\n", md5_hash("", 0).c_str());
	// RFC 4648 test vectors
	const unsigned char man[] = {'M', 'a', 'n'};
	printf("b64.enc=%s\n", base64_encode(man, 3).c_str());
	printf("b64.dec=%s\n", base64_decode("Zm9vYmFy").c_str());
	// FNV-1a 32 of "a" is 0xe40c292c
	printf("fnv1a.a=%08x\n", (unsigned)hash_fnv1a("a", 1));

	// Latin-1 "ss" ligature maps to ASCII "ss"
	char out[16] = {0};
	maybe_convert_foreign_characters("\xdf", out, true);
	printf("foreign.ss=%s\n", out);

	Unicode_text_mode = true;
	SCP_string conv;
	bool ok = unicode::convert_encoding(conv, "caf\xe9", unicode::Encoding::Encoding_iso8859_1,
	                                    unicode::Encoding::Encoding_utf8);
	printf("conv.l1_to_u8=%d,", ok ? 1 : 0);
	hex("", conv);
	const char u8[] = "h\xc3\xa9llo";
	printf("cp.count=%zu\n", unicode::num_codepoints(u8, u8 + strlen(u8)));
	printf("ascii.only=%d,%d\n", unicode::string_is_ascii_only("plain", 5) ? 1 : 0,
	       unicode::string_is_ascii_only(u8, strlen(u8)) ? 1 : 0);

	printf("enc.utf16le=%d\n", (int)util::guess_encoding(SCP_string("\xff\xfeh\0i\0", 6), true));
	printf("enc.utf8bom=%d,%d\n", (int)util::guess_encoding(SCP_string("\xef\xbb\xbfhi", 5), true),
	       util::has_bom(SCP_string("\xef\xbb\xbfhi", 5)) ? 1 : 0);

	char s1[] = "  padded text \t ";
	drop_leading_white_space(s1);
	drop_trailing_white_space(s1);
	printf("ws.trim=[%s]\n", s1);
	char s2[] = "aa\"\"bb\"\"";
	consolidate_double_characters(s2, '"');
	printf("dbl.quote=%s\n", s2);
	char s3[] = "keep#drop";
	end_string_at_first_hash_symbol(s3, false);
	printf("hash.cut=%s\n", s3);
}

static void probe_crypt()
{
	encrypt_init();
	for (int mode = 0; mode < 3; ++mode) {
		char text[] = "$Name: Rapier\n+Cost: 1200 ; sneaky comment\n";
		const int len = (int)strlen(text);
		std::vector<char> scr(len * 2 + 64);
		int slen = 0;
		// mode 0: FS2 style, 1: FS1 7-bit, 2: FS1 8-bit
		encrypt(text, len, scr.data(), &slen, mode == 2, mode == 0);
		const bool enc = is_encrypted(scr.data()) != 0;
		std::string type = encrypt_type(scr.data());
		std::vector<char> back(slen + 64, 0);
		int blen = 0;
		unencrypt(scr.data(), slen, back.data(), &blen);
		std::string flat(back.data(), blen);
		for (auto& c : flat)
			if (c == '\n')
				c = '|';
		printf("crypt.mode%d=%d,%s,%d,%s\n", mode, enc ? 1 : 0, type.c_str(), blen, flat.c_str());
	}
}

int main()
{
	probe_parse();
	probe_text();
	probe_crypt();
	return 0;
}
