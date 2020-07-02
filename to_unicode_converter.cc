#include <string>
#include <utility>

#include <boost/optional.hpp>
#include <boost/locale/encoding.hpp>

#include "to_unicode_converter.h"
#include "cmap.h"
#include "fonts.h"
#include "common.h"

using namespace std;
using namespace boost::locale::conv;


ToUnicodeConverter::ToUnicodeConverter(cmap_t &custom_encoding_arg) :
                                      custom_encoding(custom_encoding_arg),
                                      empty(false)
{
}

ToUnicodeConverter::ToUnicodeConverter() noexcept : custom_encoding(boost::none), empty(true)
{
}

bool ToUnicodeConverter::is_empty() const
{
    return empty;
}

bool ToUnicodeConverter::is_vertical() const
{
    if (!empty && custom_encoding->is_vertical) return true;
    return false;
}

pair<string, float> ToUnicodeConverter::custom_decode_symbol(const string &s, size_t &i, const Fonts &fonts) const
{
    for (unsigned char n : custom_encoding->sizes)
    {
        size_t left = s.length() - i;
        if (left < n) break;
        const string symbol = s.substr(i, n);
        auto it = custom_encoding->utf_map.find(symbol);
        if (it == custom_encoding->utf_map.end()) continue;
        if (it->second.first == cmap_t::NOT_CONVERTED)
        {
            it->second.second = to_utf<char>(it->second.second, "UTF-16be");
            it->second.first = cmap_t::CONVERTED;
        }
        i += n;
        return make_pair(it->second.second, fonts.get_width(string2num(symbol)));
    }
    return make_pair(string(), 0);
}
