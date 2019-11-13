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
        if (!charset) return s;
        return to_utf<char>(s, charset);
    }
}

extern const string standard_font_encoding[256];
extern const string mac_expert_font_encoding[256];
extern const string mac_roman_font_encoding[256];
extern const string win_ansi_font_encoding[256];

const get_string_t standard_encoding{get_ascii, standard_font_encoding, nullptr};
const get_string_t identity_h_encoding{get_ascii, nullptr, nullptr};
const get_string_t identity_v_encoding{get_ascii, nullptr, nullptr};
const get_string_t mac_expert_encoding{get_ascii, mac_expert_font_encoding, nullptr};
const get_string_t mac_roman_encoding{get_ascii, mac_roman_font_encoding, nullptr};
const get_string_t win_ansi_encoding{get_ascii, win_ansi_font_encoding, nullptr};
const get_string_t unicns_ucs2_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unicns_ucs2_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t gbk_euc_h_encoding{get_utf, nullptr, "GBK"};
const get_string_t gbk_euc_v_encoding{get_utf, nullptr, "GBK"};
const get_string_t gb_h_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t gb_v_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t gb_euc_h_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gb_euc_v_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbpc_euc_h_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbpc_euc_v_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbt_h_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t gbt_v_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t gbt_euc_h_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbt_euc_v_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbtpc_euc_h_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbtpc_euc_v_encoding{get_utf, nullptr, "EUC-CN"};
const get_string_t gbkp_euc_h_encoding{get_utf, nullptr, "GBK"};
const get_string_t gbkp_euc_v_encoding{get_utf, nullptr, "GBK"};
const get_string_t gbk2k_h_encoding{get_utf, nullptr, "GB18030"};
const get_string_t gbk2k_v_encoding{get_utf, nullptr, "GB18030"};
const get_string_t unigb_ucs2_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unigb_ucs2_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unigb_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t unigb_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t unigb_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unigb_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unigb_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unigb_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t b5_h_encoding{get_utf, nullptr, "Big5"};
const get_string_t b5_v_encoding{get_utf, nullptr, "Big5"};
const get_string_t b5pc_h_encoding{get_utf, nullptr, "Big5"};
const get_string_t b5pc_v_encoding{get_utf, nullptr, "Big5"};
const get_string_t eten_b5_h_encoding{get_utf, nullptr, "Big5"};
const get_string_t eten_b5_v_encoding{get_utf, nullptr, "Big5"};
const get_string_t etenms_b5_h_encoding{get_utf, nullptr, "Big5"};
const get_string_t etenms_b5_v_encoding{get_utf, nullptr, "Big5"};
const get_string_t cns1_h_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t cns1_v_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t cns2_h_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t cns2_v_encoding{get_utf, nullptr, "ISO-2022-CN"};
const get_string_t cns_euc_h_encoding{get_utf, nullptr, "EUC-TW"};
const get_string_t cns_euc_v_encoding{get_utf, nullptr, "EUC-TW"};
const get_string_t unicns_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t unicns_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t unicns_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unicns_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unicns_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unicns_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t ethk_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t ethk_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkdla_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkdla_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkdlb_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkdlb_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkgccs_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkgccs_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkm314_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkm314_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkm471_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkm471_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkscs_b5_h_encoding{get_utf, nullptr, "Big-5"};
const get_string_t hkscs_b5_v_encoding{get_utf, nullptr, "Big-5"};
const get_string_t h_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t v_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t euc_h_encoding{get_utf, nullptr, "EUC-JP"};
const get_string_t euc_v_encoding{get_utf, nullptr, "EUC-JP"};
const get_string_t pv83_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t pv83_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t add_h_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t add_v_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t add_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t add_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t ext_h_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t ext_v_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t ext_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t ext_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t nwp_h_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t nwp_v_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t pv90_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t pv90_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t ms90_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t ms90_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t msp90_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t msp90_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t h78_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t v78_encoding{get_utf, nullptr, "ISO-2022-JP"};
const get_string_t rksj78_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t rksj78_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t ms78_rksj_h_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t ms78_rksj_v_encoding{get_utf, nullptr, "Shift-JIS"};
const get_string_t euc78_h_encoding{get_utf, nullptr, "EUC-JP"};
const get_string_t euc78_v_encoding{get_utf, nullptr, "EUC-JP"};
const get_string_t unijis_ucs2_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis_ucs2_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis_ucs2_hw_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis_ucs2_hw_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t unijis_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t unijis_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijis_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijis2004_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t unijis2004_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t unijis2004_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis2004_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unijis2004_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijis2004_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijisx0213_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijisx0213_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijisx02132004_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unijisx02132004_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t uniakr_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t uniakr_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t uniakr_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t uniakr_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t uniakr_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t uniakr_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t ksc_h_encoding{get_utf, nullptr, "ISO-2022-KR"};
const get_string_t ksc_v_encoding{get_utf, nullptr, "ISO-2022-KR"};
const get_string_t ksc_euc_h_encoding{get_utf, nullptr, "EUC-KR"};
const get_string_t ksc_euc_v_encoding{get_utf, nullptr, "EUC-KR"};
const get_string_t kscpc_euc_h_encoding{get_utf, nullptr, "EUC-KR"};
const get_string_t kscpc_euc_v_encoding{get_utf, nullptr, "EUC-KR"};
const get_string_t kscms_euc_h_encoding{get_utf, nullptr, "UHC"};
const get_string_t kscms_euc_v_encoding{get_utf, nullptr, "UHC"};
const get_string_t kscms_euc_hw_h_encoding{get_utf, nullptr, "UHC"};
const get_string_t kscms_euc_hw_v_encoding{get_utf, nullptr, "UHC"};
const get_string_t ksc_johab_h_encoding{get_utf, nullptr, "UHC"};
const get_string_t ksc_johab_v_encoding{get_utf, nullptr, "UHC"};
const get_string_t uniks_ucs2_h_encoding{get_utf, nullptr, "UTF16-be"};
const get_string_t uniks_ucs2_v_encoding{get_utf, nullptr, "UTF16-be"};
const get_string_t uniks_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t uniks_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t uniks_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t uniks_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t uniks_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t uniks_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t hojo_h_encoding{get_utf, nullptr, "ISO-2022-JP-1"};
const get_string_t hojo_v_encoding{get_utf, nullptr, "ISO-2022-JP-1"};
const get_string_t hojo_euc_h_encoding{get_utf, nullptr, "EUC-JP"};
const get_string_t hojo_euc_v_encoding{get_utf, nullptr, "EUC-JP"};
const get_string_t unihojo_ucs2_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unihojo_ucs2_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unihojo_utf8_h_encoding{get_utf, nullptr, nullptr};
const get_string_t unihojo_utf8_v_encoding{get_utf, nullptr, nullptr};
const get_string_t unihojo_utf16_h_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unihojo_utf16_v_encoding{get_utf, nullptr, "UTF-16be"};
const get_string_t unihojo_utf32_h_encoding{get_utf, nullptr, "UTF-32be"};
const get_string_t unihojo_utf32_v_encoding{get_utf, nullptr, "UTF-32be"};
