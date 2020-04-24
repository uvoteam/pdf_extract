#include <utility>
#include <string>
#include <vector>
#include <array>

#include "fonts.h"
#include "object_storage.h"
#include "common.h"

using namespace std;

Fonts::Fonts(const ObjectStorage &storage, const dict_t &fonts_dict): rise(RISE_DEFAULT)
{
        for (const pair<string, pair<string, pdf_object_t>> &p : fonts_dict)
        {
            const dict_t font_dict = get_dict_or_indirect_dict(p.second, storage);
            Font_type_t type = insert_type(p.first, font_dict);
            if (type == TYPE_3) insert_matrix_type3(p.first, font_dict);
            dictionary_per_font.insert(make_pair(p.first, font_dict));
            auto it = font_dict.find("/FontDescriptor");
            const dict_t desc_dict = (it == font_dict.end())? dict_t() : get_dict_or_indirect_dict(it->second, storage);

            insert_width(storage, p.first, desc_dict);
            insert_height(p.first, desc_dict, storage);
            insert_descent(p.first, desc_dict);
            insert_ascent(p.first, desc_dict);
        }
}

double Fonts::get_width(unsigned int code) const
{
    auto it = widths.at(current_font).find(code);
    if (it == widths.at(current_font).end()) return default_width.at(current_font);
    return it->second * get_scales().first;
}

void Fonts::get_widths_from_w(const ObjectStorage &storage, const string &font_name)
{
    const dict_t &font = dictionary_per_font.at(font_name);
    default_width.insert(make_pair(font_name, stod(get_dict_val(font, "/DW", DW_DEFAULT))));
    auto it = font.find("/W");
    if (it == font.end()) return;
    const string array = (it->second.second == ARRAY)? it->second.first :
                                                       get_indirect_object_data(it->second.first, storage, ARRAY).first;
    vector<pair<string, pdf_object_t>> result = get_array_data(array, 0);
    for (pair<string, pdf_object_t> &p : result)
    {
        if (p.second == INDIRECT_OBJECT) p = get_indirect_object_data(p.first, storage);
    }

    for (size_t i = 0; i < result.size();)
    {
        switch (result.at(i + 1).second)
        {
        case VALUE:
        {
            unsigned int first_char = strict_stoul(result[i].first);
            unsigned int last_char = strict_stoul(result[i + 1].first);
            double width = stod(result.at(i + 2).first);
            for (unsigned int j = first_char; j <= last_char; ++j) widths.at(font_name).insert(make_pair(j, width));
            i += 3;
            break;
        }
        case ARRAY:
        {
            unsigned int start_char = strict_stoul(result[i].first);
            const vector<pair<string, pdf_object_t>> w_array = get_array_data(result[i + 1].first, 0);
            for (const pair<string, pdf_object_t> &p : w_array)
            {
                widths.at(font_name).insert(make_pair(start_char, stod(p.first)));
                ++start_char;
            }
            break;
        }
        default:
            throw pdf_error(FUNC_STRING + "wrong type for val " + result[i + 1].first +
                            " type=" + to_string(result[i + 1].second));
        }
    }
}

void Fonts::get_widths_from_widths(const ObjectStorage &storage,
                                   const string &font_name,
                                   const pair<string, pdf_object_t> &array_arg,
                                   const dict_t &font_desc)
{
    const dict_t &font = dictionary_per_font.at(font_name);
    unsigned int first_char = strict_stoul(get_dict_val(font, "/FirstChar", FIRST_CHAR_DEFAULT));
    default_width.insert(make_pair(font_name, stod(get_dict_val(font_desc, "/MissingWidth", MISSING_WIDTH_DEFAULT))));
    const string array = (array_arg.second == ARRAY)? array_arg.first :
                                                      get_indirect_object_data(array_arg.first, storage, ARRAY).first;
    const vector<pair<string, pdf_object_t>> result = get_array_data(array, 0);
    for (size_t i = 0; i < result.size(); ++i)
    {
        const pair<string, pdf_object_t> &p = result[i];
        const string val = (p.second == INDIRECT_OBJECT)? get_indirect_object_data(p.first, storage).first : p.first;
        widths.at(font_name).insert(make_pair(i + first_char, stod(val)));
    }
}

void Fonts::insert_width(const ObjectStorage &storage, const string &font_name, const dict_t &font_desc)
{
    widths.insert(make_pair(font_name, map<unsigned int, double>()));
    auto it = dictionary_per_font.at(font_name).find("/Widths");
    if (it != dictionary_per_font.at(font_name).end())
    {
        get_widths_from_widths(storage, font_name, it->second, font_desc);
        return;
    }
    get_widths_from_w(storage, font_name);
}

void Fonts::insert_matrix_type3(const string &font_name, const dict_t &font)
{
    const pair<string, pdf_object_t> p = font.at("/FontMatrix");
    if (p.second != ARRAY) throw pdf_error(FUNC_STRING + "/FontMatrix must be ARRAY. Type=" + to_string(p.second) +
                                           " value=" + p.first);
    vector<pair<string, pdf_object_t>> data = get_array_data(p.first, 0);
    if (data.size() != MATRIX_ELEMENTS) throw pdf_error(FUNC_STRING + "/FontMatrix must have " +
                                                        to_string(MATRIX_ELEMENTS) + " elements");
    array<double, MATRIX_ELEMENTS> matrix;
    for (size_t i = 0; i < MATRIX_ELEMENTS; ++i)
    {
        if (data[i].second != VALUE) throw pdf_error(FUNC_STRING + "/FontMatrix element must be VALUE.Type=" +
                                                     to_string(data[i].second) + " value=" + data[i].first);
        matrix[i] = stod(data[i].first);
    }
    font_matrix_type_3.insert(make_pair(font_name, std::move(matrix)));
}

Fonts::Font_type_t Fonts::insert_type(const string &font_name, const dict_t &font)
{
    const string type = font.at("/Subtype").first;
    if (type == "Type3")
    {
        types.insert(make_pair(font_name, TYPE_3));
        return TYPE_3;
    }
    types.insert(make_pair(font_name, OTHER));
    return OTHER;
}

void Fonts::set_rise(double rise_arg)
{
    rise = rise_arg;
}

double Fonts::get_rise() const
{
    return rise;
}

void Fonts::insert_height(const string &font_name, const dict_t &font_desc, const ObjectStorage &storage)
{
    auto it = font_desc.find("/FontBBox");
    if (it == font_desc.end())
    {
        heights.insert(make_pair(font_name, Fonts::NO_HEIGHT));
        return;
    }
    const string array_str = (it->second.second == ARRAY)? it->second.first :
                                                           get_indirect_object_data(it->second.first, storage, ARRAY).first;
    vector<pair<string, pdf_object_t>> array = get_array_data(array_str, 0);
    heights.insert(make_pair(font_name, stod(array.at(3).first) - stod(array.at(1).first)));
}

void Fonts::insert_descent(const string &font_name, const dict_t &font_desc)
{
    auto it = font_desc.find("/Descent");
    if (it == font_desc.end())
    {
        descents.insert(make_pair(font_name, Fonts::NO_DESCENT));
        return;
    }
    descents.insert(make_pair(font_name, stod(it->second.first)));
}

void Fonts::insert_ascent(const string &font_name, const dict_t &font_desc)
{
    auto it = font_desc.find("/Ascent");
    if (it == font_desc.end())
    {
        ascents.insert(make_pair(font_name, Fonts::NO_ASCENT));
        return;
    }
    ascents.insert(make_pair(font_name, stod(it->second.first)));
}

double Fonts::get_height() const
{
    validate_current_font();
    double height = heights.at(current_font);
    if (height == NO_HEIGHT) return get_ascent() - get_descent();
    return height * get_scales().second;
}

double Fonts::get_descent() const
{
    validate_current_font();
    return descents.at(current_font) * get_scales().second;
}

double Fonts::get_ascent() const
{
    validate_current_font();
    return ascents.at(current_font) * get_scales().second;
}

const dict_t& Fonts::get_current_font_dictionary() const
{
    validate_current_font();
    return dictionary_per_font.at(current_font);
}

void Fonts::set_current_font(const string &font)
{
    current_font = font;
}

void Fonts::validate_current_font() const
{
    if (current_font.empty()) throw pdf_error(FUNC_STRING + "current font is not set");
}

pair<double, double> Fonts::get_scales() const
{
    if (types.at(current_font) == OTHER) return make_pair(HSCALE_NO_TYPE_3, VSCALE_NO_TYPE_3);
    return apply_matrix_norm(font_matrix_type_3.at(current_font), make_pair(1, 1));
}

const double Fonts::VSCALE_NO_TYPE_3 = 0.001;
const double Fonts::HSCALE_NO_TYPE_3 = 0.001;
const double Fonts::NO_HEIGHT = 0;
const double Fonts::NO_DESCENT = 0;
const double Fonts::RISE_DEFAULT = 0;
const double Fonts::NO_ASCENT = 0;
const string Fonts::FIRST_CHAR_DEFAULT = "0";
const string Fonts::MISSING_WIDTH_DEFAULT = "0";
const string Fonts::DW_DEFAULT = "1000";
