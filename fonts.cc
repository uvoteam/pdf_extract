#include <utility>
#include <string>
#include <vector>

#include "fonts.h"
#include "object_storage.h"
#include "common.h"

using namespace std;

Fonts::Fonts(const ObjectStorage &storage, const dict_t &fonts_dict)
{
        for (const pair<string, pair<string, pdf_object_t>> &p : fonts_dict)
        {
            const dict_t font_dict = get_dictionary_data(get_indirect_object_data(p.second.first,
                                                                                  storage,
                                                                                  DICTIONARY).first, 0);
            dictionary_per_font.insert(make_pair(p.first, font_dict));
            const dict_t font_desc_dict = get_dictionary_data(get_indirect_object_data(font_dict.at("/FontDescriptor").first,
                                                                                       storage,
                                                                                       DICTIONARY).first, 0);
            auto it = font_desc_dict.find("/FontBBox");
            if (it == font_desc_dict.end())
            {
                heights.insert(make_pair(p.first, Fonts::NO_HEIGHT));
                continue;
            }
            vector<pair<string, pdf_object_t>> array = get_array_data(it->second.first, 0);
            heights.insert(make_pair(p.first, strict_stol(array.at(3).first) - strict_stol(array.at(1).first)));
        }
}

unsigned int Fonts::get_height() const
{
    validate_current_font();
    return heights.at(current_font);
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
