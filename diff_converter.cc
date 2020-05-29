#include <unordered_map>
#include <string>
#include <utility>
#include <algorithm>

#include <boost/optional.hpp>

#include "diff_converter.h"
#include "common.h"
#include "object_storage.h"
#include "fonts.h"
#include "converter_data.h"

using namespace std;
namespace
{
    PDFEncode_t get_encoding(const string &encoding)
    {
        if (encoding == "/StandardEncoding") return DEFAULT;
        if (encoding == "/MacRomanEncoding") return MAC_ROMAN;
        if (encoding == "/MacExpertEncoding") return MAC_EXPERT;
        if (encoding == "/WinAnsiEncoding") return WIN;
        throw pdf_error(FUNC_STRING + "wrong /BaseEncoding value:" + encoding);
    }
}

DiffConverter::DiffConverter() noexcept : empty(true)
{
}

DiffConverter::DiffConverter(unordered_map<unsigned int, string> &&difference_map_arg):
                             difference_map(std::move(difference_map_arg)),
                             empty(false)
{
}

DiffConverter DiffConverter::get_converter(const dict_t &dictionary, const string &array, const ObjectStorage &storage)
{
    auto it = dictionary.find("/BaseEncoding");
    PDFEncode_t encoding = (it == dictionary.end())? DEFAULT : get_encoding(it->second.first);
    unordered_map<unsigned int, string> code2symbol = standard_encodings.at(encoding);
    const array_t array_data = get_array_data(array, 0);

    auto start_it = find_if(array_data.begin(),
                            array_data.end(),
                            [](const pair<string, pdf_object_t> &p) { return (p.second == VALUE)? true : false;});
    if (start_it == array_data.end()) return DiffConverter(std::move(code2symbol));
    unsigned int code = strict_stoul(start_it->first);

    for (auto it = start_it; it != array_data.end(); ++it)
    {
        const pair<string, pdf_object_t> symbol = (it->second == INDIRECT_OBJECT)?
                                                  get_indirect_object_data(it->first, storage) : *it;
        switch (symbol.second)
        {
        case VALUE:
            code = strict_stoul(symbol.first);
            break;
        case NAME_OBJECT:
        {
            auto it = symbol_table.find(symbol.first);
            if (it != symbol_table.end()) code2symbol[code] = it->second;
            ++code;
            break;
        }
        default:
            throw pdf_error(FUNC_STRING + "wrong symbol type=" + to_string(symbol.second) + " val=" + symbol.first);
        }

    }
    return DiffConverter(std::move(code2symbol));
}

pair<string, double> DiffConverter::get_string(const string &s, const Fonts &fonts) const
{
        string str;
        str.reserve(s.length());
        double width = 0;
        for (char c : s)
        {
            auto it = difference_map.find(static_cast<unsigned char>(c));
            if (it != difference_map.end() && !it->second.empty())
            {
                str.append(it->second);
                width += fonts.get_width(static_cast<unsigned char>(c));
            }
        }
        return make_pair(std::move(str), width);
}

boost::optional<string> DiffConverter::get_char(char c) const
{
    auto it = difference_map.find(static_cast<unsigned char>(c));
    if (it != difference_map.end()) return it->second;
    return boost::none;
}

bool DiffConverter::is_empty() const
{
    return empty;
}

const std::unordered_map<string, string> DiffConverter::symbol_table = {
    #include "symbol_table.h"
};
