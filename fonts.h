#ifndef FONT_H
#define FONT_H

#include <map>
#include <string>
#include <array>

#include "object_storage.h"
#include "common.h"

class Fonts
{
public:
    Fonts(const ObjectStorage &storage, const dict_t &fonts_dict);
    const dict_t& get_current_font_dictionary() const;
    double get_height() const;
    void set_current_font(const std::string &font_arg);
    void set_rise(int rise_arg);
    int get_rise() const;
    double get_descent() const;
    double get_vscale() const;
private:
    enum { NO_HEIGHT = 0, NO_DESCENT = 0, RISE_DEFAULT = 0, NO_ASCENT };
    enum Font_type_t { TYPE_3, OTHER };
    Font_type_t insert_type(const std::string &font_name, const dict_t &font_desc);
    void insert_descent(const std::string &font_name, const dict_t &font_desc);
    void insert_ascent(const std::string &font_name, const dict_t &font_desc);
    void insert_height(const std::string &font_name, const dict_t &font_desc);
    void validate_current_font() const;
    void insert_matrix_type3(const std::string &font_name, const dict_t &font_desc);
    std::string current_font;
    std::map<std::string, dict_t> dictionary_per_font;
    std::map<std::string, int> heights;
    std::map<std::string, int> descents;
    std::map<std::string, int> ascents;
    std::map<std::string, Font_type_t> types;
    std::map<std::string, std::array<double, MATRIX_ELEMENTS>> font_matrix_type_3;
    int rise;
    static const double VSCALE_NO_TYPE_3;
};

#endif
