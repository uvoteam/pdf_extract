#ifndef FONT_H
#define FONT_H

#include <map>
#include <string>

#include "object_storage.h"
#include "common.h"

class Fonts
{
public:
    enum { NO_HEIGHT = 0 };
    Fonts(const ObjectStorage &storage, const dict_t &fonts_dict);
    const dict_t& get_current_font_dictionary() const;
    unsigned int get_height() const;
    void set_current_font(const std::string &font_arg);
private:
    void validate_current_font() const;
    std::string current_font;
    std::map<std::string, dict_t> dictionary_per_font;
    std::map<std::string, unsigned int> heights;
};

#endif
