#include <string>
#include <utility>

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
    double get_width_identity(const string &s, const Fonts &fonts)
    {
        double result = 0;
        for (size_t i = 0; i < s.length(); i += 2) result += fonts.get_width(string2num(s.substr(i, 2)));
        return result;
    }
}

CharsetConverter::CharsetConverter() noexcept : encode(), charset(nullptr)
{
}

CharsetConverter::CharsetConverter(const std::string &encoding)
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
    else if (encoding == "/Identity-H" || encoding == "/Identity-V")
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

pair<string, double> CharsetConverter::get_string(const string &s, const Fonts &fonts) const
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
