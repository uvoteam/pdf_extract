#ifndef FONT_H
#define FONT_H

#include <map>
#include <string>

#include "object_storage.h"
#include "common.h"

class Fonts
{
public:
    enum { NO_HEIGHT = 0, NO_DESCENT = 0, RISE_DEFAULT = 0 };
    Fonts(const ObjectStorage &storage, const dict_t &fonts_dict);
    const dict_t& get_current_font_dictionary() const;
    unsigned int get_height() const;
    void set_current_font(const std::string &font_arg);
    void set_rise(int rise_arg);
    int get_rise() const;
private:
    void insert_descent(const std::string &font_name, const dict_t &font_desc);
    void insert_height(const std::string &font_name, const dict_t &font_desc);
    void validate_current_font() const;
    std::string current_font;
    std::map<std::string, dict_t> dictionary_per_font;
    std::map<std::string, int> heights;
    std::map<std::string, int> descents;
    int rise;
};

#endif
