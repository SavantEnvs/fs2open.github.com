// libFuzzer harness: fs2open table-file text parsing (parse/parselo.cpp).
// Bytes are installed as an in-memory "default file" (the same path the engine uses for embedded
// tables) and then walked with the stuff_*/required_string family that every .tbl/.tbm/.fs2 reader uses.
#include <cstdint>
#include <cstring>
#include <vector>

#include "globalincs/pstypes.h"
#include "def_files/def_files.h"
#include "parse/parselo.h"
#include "osapi/dialogs.h"

static void walk(const uint8_t* ops, size_t nops)
{
	for (size_t i = 0; i < nops; ++i) {
		char buf[256];
		SCP_string s;
		int iv;
		float fv;
		vec3d v3;
		switch (ops[i] % 14) {
		case 0:  stuff_int(&iv); break;
		case 1:  stuff_float(&fv); break;
		case 2:  stuff_string(buf, F_NAME, sizeof(buf)); break;
		case 3:  stuff_string(s, F_RAW); break;
		case 4:  stuff_string_line(s); break;
		case 5:  stuff_vec3d(&v3); break;
		case 6:  { SCP_vector<int> l; stuff_int_list(l); } break;
		case 7:  { SCP_vector<SCP_string> l; stuff_string_list(l); } break;
		case 8:  optional_string("$Name:"); break;
		case 9:  skip_token(); break;
		case 10: skip_to_string("#End"); break;
		case 11: stuff_string_white(s); break;
		case 12: { SCP_vector<float> l; stuff_float_list(l); } break;
		case 13: advance_to_eoln(nullptr); break;
		}
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2)
		return -1;
	size_t nops = data[0] % 32 + 1;
	const uint8_t* ops = data + 1;
	if (nops > size - 1)
		nops = size - 1;
	const uint8_t* text = data + 1;
	size_t tlen = size - 1;

	std::vector<char> copy(text, text + tlen);
	default_file f;
	f.path_type = "data/tables";
	f.filename = "fuzz.tbl";
	f.data = copy.data();
	f.size = copy.size();

	try {
		// Own buffers: the legacy Latin-1 conversion can expand text beyond the
		// engine's file-sized default buffer (known upstream limitation).
		std::vector<char> raw(tlen * 4 + 16), proc(tlen * 4 + 16);
		read_file_text_from_default(f, proc.data(), raw.data());
		reset_parse(proc.data());
		walk(ops, nops);
	} catch (const parse::ParseException&) {
		stop_parse();
		return -1;
	} catch (const os::dialogs::DialogException&) {
		stop_parse();
		return -1;
	}
	stop_parse();
	return 0;
}
