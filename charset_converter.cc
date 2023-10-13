#include <string>
#include <utility>
#include <unordered_set>

#include <boost/locale/encoding.hpp>
#include <boost/optional.hpp>

#include "charset_converter.h"
#include "converter_data.h"
#include "fonts.h"
#include "common.h"

using namespace std;
using namespace boost::locale::conv;


namespace
{
    float get_width_identity(const string &s, const Fonts &fonts)
    {
        float result = 0;
        for (size_t i = 0; i < s.length(); i += 2) result += fonts.get_width(string2num(s.substr(i, 2)));
        return result;
    }
}

CharsetConverter::CharsetConverter() noexcept : encode(), charset(nullptr)
{
}

CharsetConverter::CharsetConverter(const std::string &encoding_arg) : encoding(encoding_arg)
{
    if (encoding.empty())
    {
        encode = DEFAULT;
        charset = nullptr;
    }
    else if (encoding == "/WinAnsiEncoding")
    {
        encode = WIN;
        charset = nullptr;
    }
    else if (encoding == "/MacRomanEncoding")
    {
        encode = MAC_ROMAN;
        charset = nullptr;
    }
    else if (encoding == "/MacExpertEncoding")
    {
        encode = MAC_EXPERT;
        charset = nullptr;
    }
    else if (encoding == "/Identity-H" || encoding == "/Identity-V" || !encoding2charset.count(encoding))
    {
        encode = IDENTITY;
        charset = nullptr;
    }
    else
    {
        charset = encoding2charset.at(encoding);
        encode = charset? OTHER : UTF8;
    }
}

bool CharsetConverter::is_vertical() const
{
    static const unordered_set<string> vertical_fonts{"/Identity-V", "/UniCNS-UCS2-V", "/GBK-EUC_V", "/GBpc-EUC-V",
                                                      "/GBT-V", "/GBT-EUC-V", "/GBTpc-EUC-V", "/GBKp-EUC-V", "/GBK2K-V",
                                                      "/UniGB-UCS2-V", "/UniGB-UTF8-V", "/UniGB-UTF16-V", "/UniGB-UTF32-V",
                                                      "/B5-V", "/B5pc-V", "/ETen-B5-V", "/ETenms-B5-V", "/CNS1-V",
                                                      "/CNS2-V", "/CNS-EUC-V", "/UniCNS-UTF8-V", "/UniCNS-UTF16-V",
                                                      "/UniCNS-UTF32-V", "/ETHK-B5-V", "/HKdla-B5-V", "/HKdlb-B5-V",
                                                      "/HKgccs-B5-V", "/HKm314-B5-V", "/HKm471-B5-V",
                                                      "/HKscs-B5-V", "/V", "/RKSJ-V", "/EUC-V", "/83pv-RKSJ-V", "/Add-V",
                                                      "/Add-RKSJ-V", "/Ext-V", "/Ext-RKSJ-V", "/NWP-V",
                                                      "/90pv-RKSJ-V", "/90ms-RKSJ-V", "/90msp-RKSJ-V",
                                                      "/78-V", "/78-RKSJ-V", "/78ms-RKSJ-V", "/78-EUC-V", "/UniJIS-UCS2-V",
                                                      "/UniJIS-UCS2-HW-V", "/UniJIS-UTF8-V", "/UniJIS-UTF16-V",
                                                      "/UniJIS-UTF32-V", "/UniJIS2004-UTF8-V",
                                                      "/UniJIS2004-UTF16-V", "/UniJIS2004-UTF32-V",
                                                      "/UniJISX0213-UTF32-V", "/UniJISX02132004-UTF32-V",
                                                      "/UniAKR-UTF8-V", "/UniAKR-UTF16-V", "/UniAKR-UTF32-V",
                                                      "/KSC-V", "/KSC-EUC-V",
                                                      "/KSCpv-EUC-V", "/KSCms-EUC-V", "/KSCms-EUC-HW-V",
                                                      "/KSC-Johab-V", "/UniKS-UCS2-V",
                                                      "/UniKS-UTF8-V", "/UniKS-UTF16-V",
                                                      "/UniKS-UTF32-V", "/Hojo-V", "/Hojo-EUC-V",
                                                      "/UniHojo-UCS2-V", "/UniHojo-UTF8-V", "/UniHojo-UTF16-V",
                                                      "/UniHojo-UTF32-V"};

    if (vertical_fonts.count(encoding)) return true;
    return false;
}

pair<string, float> CharsetConverter::get_string(const string &s, const Fonts &fonts) const
{
    switch (encode)
    {
    case UTF8:
        return make_pair(s, fonts.get_width(s));
    case IDENTITY:
        return make_pair(to_utf<char>(s, "UTF-16be"), get_width_identity(s, fonts));
    case DEFAULT:
    case MAC_EXPERT:
    case MAC_ROMAN:
    case WIN:
    {
        const unordered_map<unsigned int, string> &standard_encoding = standard_encodings.at(encode);
        string str;
        str.reserve(s.length());
        for (char c : s)
        {
            auto it = standard_encoding.find(static_cast<unsigned char>(c));
            if (it != standard_encoding.end()) str.append(it->second);
        }
        return make_pair(std::move(str), fonts.get_width(s));
    }
    case OTHER:
        return make_pair(to_utf<char>(s, charset), fonts.get_width(s));
    default:
        throw pdf_error(FUNC_STRING + "wrong encode value: " + to_string(encode));
    }
}

boost::optional<string> CharsetConverter::get_char(char c) const
{
    PDFEncode_t enc = (encode == MAC_EXPERT || encode == MAC_ROMAN || encode == WIN)? encode : DEFAULT;
    const unordered_map<unsigned int, string> &standard_encoding = standard_encodings.at(enc);
    auto it = standard_encoding.find(static_cast<unsigned char>(c));
    if (it != standard_encoding.end()) return it->second;
    return boost::none;
}

bool CharsetConverter::is_empty() const
{
    return encode == NONE;
}
