#include <string>

#include <boost/locale/encoding.hpp>

#include "font_encodings.h"

using namespace std;
using namespace boost::locale::conv;


namespace
{
    string get_ascii(const string &s, const string *encoding_table, const char *charset)
    {
        if (!encoding_table) return s;
        string result;
        result.reserve(s.length());
        for (char c : s) result.append(encoding_table[static_cast<unsigned char>(c)]);

        return result;
    }

    string get_utf(const string &s, const string *encoding_table, const char *charset)
    {
        return to_utf<char>(s, charset);
    }
}

extern const string standard_font_encoding[256];
extern const string mac_expert_font_encoding[256];
extern const string mac_roman_font_encoding[256];
extern const string win_ansi_font_encoding[256];

const get_char_t standard_encoding{get_ascii, standard_font_encoding, nullptr};
const get_char_t identity_h_encoding{get_ascii, nullptr, nullptr};
const get_char_t identity_v_encoding{get_ascii, nullptr, nullptr};
const get_char_t mac_expert_encoding{get_ascii, mac_expert_font_encoding, nullptr};
const get_char_t mac_roman_encoding{get_ascii, mac_roman_font_encoding, nullptr};
const get_char_t win_ansi_encoding{get_ascii, win_ansi_font_encoding, nullptr};
const get_char_t unicns_ucs2_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_char_t unicns_ucs2_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_char_t gbk_euc_h_encoding{get_utf, nullptr, "GBK"};
const get_char_t gbk_euc_v_encoding{get_utf, nullptr, "GBK"};
const get_char_t gb_h_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_char_t gb_v_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_char_t gb_euc_h_encoding{get_utf, nullptr, "EUC-CN"};
const get_char_t gb_euc_v_encoding{get_utf, nullptr, "EUC-CN"};
