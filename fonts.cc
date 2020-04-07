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
            const dict_t font_dict = get_dictionary_data(get_indirect_object_data(p.second.first,
                                                                                  storage,
                                                                                  DICTIONARY).first, 0);
            Font_type_t type = insert_type(p.first, font_dict);
            if (type == TYPE_3) insert_matrix_type3(p.first, font_dict);
            dictionary_per_font.insert(make_pair(p.first, font_dict));
            const dict_t font_desc_dict = get_dictionary_data(get_indirect_object_data(font_dict.at("/FontDescriptor").first,
                                                                                       storage,
                                                                                       DICTIONARY).first, 0);
            insert_height(p.first, font_desc_dict);
            insert_descent(p.first, font_desc_dict);
        }
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

void Fonts::set_rise(int rise_arg)
{
    rise = rise_arg;
}

int Fonts::get_rise() const
{
    return rise;
}

void Fonts::insert_height(const string &font_name, const dict_t &font_desc)
{
    auto it = font_desc.find("/FontBBox");
    if (it == font_desc.end())
    {
        heights.insert(make_pair(font_name, Fonts::NO_HEIGHT));
        return;
    }
    vector<pair<string, pdf_object_t>> array = get_array_data(it->second.first, 0);
    heights.insert(make_pair(font_name, strict_stol(array.at(3).first) - strict_stol(array.at(1).first)));
}

void Fonts::insert_descent(const string &font_name, const dict_t &font_desc)
{
    auto it = font_desc.find("/Descent");
    if (it == font_desc.end())
    {
        descents.insert(make_pair(font_name, Fonts::NO_DESCENT));
        return;
    }
    descents.insert(make_pair(font_name, strict_stol(it->second.first)));
}

void Fonts::insert_ascent(const string &font_name, const dict_t &font_desc)
{
    auto it = font_desc.find("/Ascent");
    if (it == font_desc.end())
    {
        ascents.insert(make_pair(font_name, Fonts::NO_ASCENT));
        return;
    }
    ascents.insert(make_pair(font_name, strict_stol(it->second.first)));
}

double Fonts::get_height() const
{
    validate_current_font();
    int height = heights.at(current_font);
    if (height == 0) height = ascents.at(current_font) - descents.at(current_font);
    return height * get_vscale();
}

double Fonts::get_descent() const
{
    validate_current_font();
    return descents.at(current_font) * get_vscale();
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

double Fonts::get_vscale() const
{
    if (types.at(current_font) == OTHER) return VSCALE_NO_TYPE_3;
    return apply_matrix_norm(font_matrix_type_3.at(current_font), make_pair(1, 1)).second;
}

const double Fonts::VSCALE_NO_TYPE_3 = 0.001;
