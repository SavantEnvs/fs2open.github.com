// Minimal engine environment for the parselo/encoding harnesses.
#include "globalincs/pstypes.h"
#include "globalincs/systemvars.h"
#include "globalincs/memory/memory.h"
#include "cfile/cfile.h"
#include "cmdline/cmdline.h"
#include "localization/localize.h"
#include "localization/fhash.h"
#include "mission/missionparse.h"
#include "mod_table/mod_table.h"
#include "osapi/dialogs.h"
#include "parse/sexp.h"
#include "graphics/2d.h"
#include "math/vecmat.h"

#include <cstdarg>
#include <cstdio>

bool running_unittests = true;
int Fred_running = 0;
int Cmdline_noparseerrors = 0;
int Game_mode = 0;
int Is_standalone = 0;
int Lcl_pl = 0;
int Num_unknown_loadout_classes = 0;
bool Unicode_text_mode = false;
sexp_variable Sexp_variables[MAX_SEXP_VARIABLES];
alignas(16) unsigned char Ship_info[sizeof(SCP_vector<int>)];
alignas(16) unsigned char Weapon_info[sizeof(SCP_vector<int>)];

namespace memory {
const quiet_alloc_t quiet_alloc;
void out_of_memory() { throw std::bad_alloc(); }
}

namespace os { namespace dialogs {
static std::string fmt(const char* format, va_list ap) {
	char buf[512];
	vsnprintf(buf, sizeof(buf), format, ap);
	return buf;
}
void Error(const char*, int, const char* format, ...) {
	va_list ap; va_start(ap, format);
	std::string s = fmt(format, ap);
	va_end(ap);
	throw ErrorException(s);
}
void Warning(const char*, int, const char* format, ...) {
	va_list ap; va_start(ap, format);
	std::string s = fmt(format, ap);
	va_end(ap);
	throw WarningException(s);
}
void ReleaseWarning(const char*, int, const char* format, ...) {
	va_list ap; va_start(ap, format);
	std::string s = fmt(format, ap);
	va_end(ap);
	throw WarningException(s);
}
}}

int fhash_active() { return 0; }
void fhash_add_str(const char*, int) {}
void lcl_ext_localize(const char* in, char* out, size_t max_len, int* id) {
	if (id) *id = -2;
	if (max_len == 0) return;
	strncpy(out, in, max_len - 1);
	out[max_len - 1] = 0;
}
void lcl_ext_localize(const SCP_string& in, SCP_string& out, int* id) {
	if (id) *id = -2;
	out = in;
}
int ship_info_lookup(const char*) { return -1; }
int ship_name_lookup(const char*, int) { return -1; }
int weapon_info_lookup(const char*) { return -1; }
int fireball_info_lookup(const char*) { return -1; }
int get_index_sexp_variable_name(const char*) { return -1; }
int get_index_sexp_variable_name(SCP_string&) { return -1; }
void gr_get_string_size(int* w, int* h, const char* text, float, size_t len) {
	size_t n = len == std::string::npos ? strlen(text) : len;
	if (w) *w = (int)(n * 8);
	if (h) *h = 12;
}
void vm_fix_matrix(matrix*) {}

CFILE* _cfopen(const char*, int, const char*, const char*, int, bool, uint32_t) { return nullptr; }
int cfclose(CFILE*) { return 0; }
int cfilelength(CFILE*) { return 0; }
int cfread(void*, int, int, CFILE*) { return 0; }
int cfseek(CFILE*, int, int) { return -1; }
int cf_get_file_list(SCP_vector<SCP_string>&, int, const char*, int, SCP_vector<file_list_info>*, uint32_t) { return 0; }
