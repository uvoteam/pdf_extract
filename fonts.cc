#include <utility>
#include <string>
#include <array>
#include <unordered_map>
#include <map>

#include "fonts.h"
#include "object_storage.h"
#include "common.h"

using namespace std;

namespace
{
    enum { DESCENDANT_ARRAY_NUM = 1 };
}

Fonts::Fonts(const ObjectStorage &storage, const dict_t &fonts_dict): rise(RISE_DEFAULT)
{
        for (const dict_t::value_type &p : fonts_dict)
        {
            dict_t font_dict = get_dict_or_indirect_dict(p.second, storage);
            Font_type_t type = insert_type(p.first, font_dict);
            if (type == TYPE_3) insert_matrix_type3(p.first, font_dict);
            insert_descendant(font_dict, storage);
            dictionary_per_font.insert(make_pair(p.first, font_dict));
            auto it = font_dict.find("/FontDescriptor");
            const dict_t desc_dict = (it == font_dict.end())? dict_t() : get_dict_or_indirect_dict(it->second, storage);

            it = font_dict.find("/BaseFont");
            string base_font;
            if (it != font_dict.end()) base_font = it->second.first;
            insert_width(storage, p.first, desc_dict, base_font);
            insert_height(p.first, desc_dict, storage, base_font);
            insert_descent(p.first, desc_dict, base_font);
            insert_ascent(p.first, desc_dict, base_font);
        }
}

void Fonts::insert_descendant(dict_t &font, const ObjectStorage &storage)
{
    const string type = font.at("/Subtype").first;
    if (type != "/Type0") return;
    const array_t array = get_array_or_indirect_array(font.at("/DescendantFonts"), storage);
    if (array.size() != DESCENDANT_ARRAY_NUM)
    {
        throw pdf_error(FUNC_STRING + "DescendantFonts array must have" +
                        to_string(DESCENDANT_ARRAY_NUM) + " element. Size=" + to_string(array.size()));
    }
    const dict_t descendant = get_dict_or_indirect_dict(array[0], storage);
    font.insert(descendant.begin(), descendant.end());
}

double Fonts::get_width(unsigned int code) const
{
    auto it = widths.at(current_font).find(code);
    if (it == widths.at(current_font).end()) return default_width.at(current_font) * get_scales().first;
    return it->second * get_scales().first;
}

double Fonts::get_width(const string &s) const
{
    double result = 0;
    for (char c : s) result += get_width(static_cast<unsigned char>(c));
    return result;
}

void Fonts::insert_widths_from_w(const ObjectStorage &storage, const string &font_name, const string &base_font)
{
    const dict_t &font = dictionary_per_font.at(font_name);
    default_width.insert(make_pair(font_name, stod(get_dict_val(font, "/DW", DW_DEFAULT))));
    auto it = font.find("/W");
    if (it == font.end())
    {
        if (standard_widths.count(base_font)) widths[font_name] = standard_widths.at(base_font);
        return;
    }
    array_t result = get_array_or_indirect_array(it->second, storage);
    for (array_t::value_type &p : result)
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
            const array_t w_array = get_array_data(result[i + 1].first, 0);
            for (const array_t::value_type &p : w_array)
            {
                widths.at(font_name).insert(make_pair(start_char, stod(p.first)));
                ++start_char;
            }
            i += 2;
            break;
        }
        default:
            throw pdf_error(FUNC_STRING + "wrong type for val " + result[i + 1].first +
                            " type=" + to_string(result[i + 1].second));
        }
    }
}

void Fonts::insert_widths_from_widths(const ObjectStorage &storage,
                                      const string &font_name,
                                      const dict_t &font_desc,
                                      const string &base_font)
{
    const dict_t &font = dictionary_per_font.at(font_name);
    unsigned int first_char = strict_stoul(get_dict_val(font, "/FirstChar", FIRST_CHAR_DEFAULT));
    default_width.insert(make_pair(font_name, stod(get_dict_val(font_desc, "/MissingWidth", MISSING_WIDTH_DEFAULT))));
    auto it = font.find("/Widths");
    if (it == font.end())
    {
        if (standard_widths.count(base_font)) widths[font_name] = standard_widths.at(base_font);
        return;
    }
    const array_t result = get_array_or_indirect_array(it->second, storage);
    for (unsigned int i = 0; i < result.size(); ++i)
    {
        const pair<string, pdf_object_t> &p = result[i];
        const string val = (p.second == INDIRECT_OBJECT)? get_indirect_object_data(p.first, storage).first : p.first;
        widths.at(font_name).insert(make_pair(i + first_char, stod(val)));
    }
}

void Fonts::insert_width(const ObjectStorage &storage,
                         const string &font_name,
                         const dict_t &font_desc,
                         const string &base_font)
{
    widths.insert(make_pair(font_name, map<unsigned int, double>()));
    const string type = dictionary_per_font.at(font_name).at("/Subtype").first;
    if (type == "/CIDFontType0" || type == "/CIDFontType2" || type == "/Type0")
    {
        insert_widths_from_w(storage, font_name, base_font);
        return;
    }
    insert_widths_from_widths(storage, font_name, font_desc, base_font);
}

void Fonts::insert_matrix_type3(const string &font_name, const dict_t &font)
{
    const pair<string, pdf_object_t> p = font.at("/FontMatrix");
    if (p.second != ARRAY) throw pdf_error(FUNC_STRING + "/FontMatrix must be ARRAY. Type=" + to_string(p.second) +
                                           " value=" + p.first);
    const array_t data = get_array_data(p.first, 0);
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

void Fonts::insert_height(const string &font_name,
                          const dict_t &font_desc,
                          const ObjectStorage &storage,
                          const string &base_font)
{
    auto it = font_desc.find("/FontBBox");
    if (it == font_desc.end())
    {
        auto it = std_metrics.find(base_font);
        if (it == std_metrics.end())
        {
            heights.insert(make_pair(font_name, Fonts::NO_HEIGHT));
            return;
        }
        heights.insert(make_pair(font_name, it->second.height));
        return;
    }
    const array_t array = get_array_or_indirect_array(it->second, storage);
    heights.insert(make_pair(font_name, stod(array.at(3).first) - stod(array.at(1).first)));
}

void Fonts::insert_descent(const string &font_name, const dict_t &font_desc, const string &base_font)
{
    auto it = font_desc.find("/Descent");
    if (it == font_desc.end())
    {
        auto it = std_metrics.find(base_font);
        if (it == std_metrics.end())
        {
            descents.insert(make_pair(font_name, Fonts::NO_DESCENT));
            return;
        }
        descents.insert(make_pair(font_name, it->second.descent));
        return;
    }
    descents.insert(make_pair(font_name, stod(it->second.first)));
}

void Fonts::insert_ascent(const string &font_name, const dict_t &font_desc, const string &base_font)
{
    auto it = font_desc.find("/Ascent");
    if (it == font_desc.end())
    {
        auto it = std_metrics.find(base_font);
        if (it == std_metrics.end())
        {
            ascents.insert(make_pair(font_name, Fonts::NO_ASCENT));
            return;
        }
        ascents.insert(make_pair(font_name, it->second.ascent));
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
const unordered_map<string, Fonts::font_metric_t> Fonts::std_metrics = {
    {"/Courier", font_metric_t(627, -194, 1052)},
    {"/Courier-Bold", font_metric_t(627, -194, 1060)},
    {"/Courier-BoldOblique", font_metric_t(627, -194, 1060)},
    {"/Courier-Oblique", font_metric_t(627, -194, 1052)},
    {"/Helvetica", font_metric_t(718, -207, 1156)},
    {"/Helvetica-Bold", font_metric_t(718, -207, 1190)},
    {"/Helvetica-BoldOblique", font_metric_t(718, -207, 1190)},
    {"/Helvetica-Oblique", font_metric_t(718, -207, 1156)},
    {"/Symbol", font_metric_t(NO_ASCENT, NO_DESCENT, 1303)},
    {"/Times-Bold", font_metric_t(683, -217, 1153)},
    {"/Times-BoldItalic", font_metric_t(683, -217, 1139)},
    {"/Times-Italic", font_metric_t(683, -217, 1100)},
    {"/Times-Roman", font_metric_t(683, -217, 1116)},
    {"/ZapfDingbats", font_metric_t(NO_ASCENT, NO_DESCENT, 963)}};

const unordered_map<string, map<unsigned int, double>> Fonts::standard_widths =
    #include "standard_widths.h"
    ;
