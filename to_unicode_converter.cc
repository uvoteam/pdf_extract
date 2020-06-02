#include <string>
#include <utility>

#include "to_unicode_converter.h"
#include "cmap.h"
#include "fonts.h"
#include "common.h"

using namespace std;

ToUnicodeConverter::ToUnicodeConverter(cmap_t &&custom_encoding_arg) :
                                      custom_encoding(std::move(custom_encoding_arg)),
                                      empty(false)
{
}

ToUnicodeConverter::ToUnicodeConverter() noexcept : empty(true)
{
}

bool ToUnicodeConverter::is_empty() const
{
    return empty;
}

bool ToUnicodeConverter::is_vertical() const
{
    if (!empty && custom_encoding.is_vertical) return true;
    return false;
}

pair<string, double> ToUnicodeConverter::custom_decode_symbol(const string &s, size_t &i, const Fonts &fonts) const
{
    for (unsigned char n : custom_encoding.sizes)
    {
        size_t left = s.length() - i;
        if (left < n) break;
        const string symbol = s.substr(i, n);
        auto it = custom_encoding.utf8_map.find(symbol);
        if (it == custom_encoding.utf8_map.end()) continue;
        i += n;
        return make_pair(it->second, fonts.get_width(string2num(symbol)));
    }
    return make_pair(string(), 0);
}
