#include <string>

#include <boost/locale/encoding.hpp>

#include "font_encodings.h"
#include "common.h"

using namespace std;
using namespace boost::locale::conv;


namespace
{
    enum encoding_t { UTF16, GBK};
    string get_ascii(const string &s, size_t &i, const string *font_encoding)
    {
        return font_encoding? font_encoding[static_cast<unsigned char>(s[i++])] : string(1, s[i++]);
    }

    template <encoding_t T> string get_2bytes(const string &s, size_t &i, const string *font_encoding)
    {
        size_t j = i;
        i += 2;
        switch (T)
        {
        case UTF16:
            return to_utf<char>(s.substr(j, 2), "UTF-16be");
        case GBK:
            return to_utf<char>(s.substr(j, 2), "GBK");
        default:
            throw pdf_error(FUNC_STRING + "unknown type: " + to_string(T));
        }
    }
}

extern const string standard_font_encoding[256];
extern const string mac_expert_font_encoding[256];
extern const string mac_roman_font_encoding[256];
extern const string win_ansi_font_encoding[256];

const get_char_t standard_encoding{get_ascii, standard_font_encoding};
const get_char_t identity_h_encoding{get_ascii, nullptr};
const get_char_t identity_v_encoding{get_ascii, nullptr};
const get_char_t mac_expert_encoding{get_ascii, mac_expert_font_encoding};
const get_char_t mac_roman_encoding{get_ascii, mac_roman_font_encoding};
const get_char_t win_ansi_encoding{get_ascii, win_ansi_font_encoding};
const get_char_t unicns_ucs2_h_encoding{get_2bytes<UTF16>, nullptr};
const get_char_t unicns_ucs2_v_encoding{get_2bytes<UTF16>, nullptr};
const get_char_t gbk_euc_h_encoding{get_2bytes<GBK>, nullptr};
const get_char_t gbk_euc_v_encoding{get_2bytes<GBK>, nullptr};
