#include <string>

#include "font_encodings.h"

using namespace std;

namespace
{
    const char* get_ascii(const string &s, size_t &i, const char **font_encoding)
    {
        size_t j = i;
        ++i;
        return font_encoding[static_cast<unsigned char>(s[j])];
    }

    const char* get_utf16(const string &s, size_t &i, const char **font_encoding)
    {
        size_t j = i;
        i += 2;
        return font_encoding[static_cast<unsigned char>(s[j]) << 8 | static_cast<unsigned char>(s.at(j + 1))];
    }
}

extern const char *standard_font_encoding[256];
extern const char *identity_v_font_encoding[256];
extern const char *identity_h_font_encoding[256];
extern const char *mac_expert_font_encoding[256];
extern const char *mac_roman_font_encoding[256];
extern const char *win_ansi_font_encoding[256];
extern const char *unicns_ucs2_h_font_encoding[65536];
extern const char *unicns_ucs2_v_font_encoding[65536];

const get_char_t standard_encoding{get_ascii, standard_font_encoding};
const get_char_t identity_h_encoding{get_ascii, identity_h_font_encoding};
const get_char_t identity_v_encoding{get_ascii, identity_v_font_encoding};
const get_char_t mac_expert_encoding{get_ascii, mac_expert_font_encoding};
const get_char_t mac_roman_encoding{get_ascii, mac_roman_font_encoding};
const get_char_t win_ansi_encoding{get_ascii, win_ansi_font_encoding};
const get_char_t unicns_ucs2_h_encoding{get_utf16, unicns_ucs2_h_font_encoding};
const get_char_t unicns_ucs2_v_encoding{get_utf16, unicns_ucs2_v_font_encoding};
