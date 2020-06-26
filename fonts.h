#ifndef FONT_H
#define FONT_H

#include <map>
#include <string>
#include <array>
#include <utility>
#include <unordered_map>

#include "object_storage.h"
#include "common.h"

class Fonts
{
public:
    Fonts(const ObjectStorage &storage, const dict_t &fonts_dict);
    const dict_t& get_current_font_dictionary() const;
    float get_height() const;
    void set_current_font(const std::string &font_arg);
    void set_rise(float rise_arg);
    float get_rise() const;
    float get_descent() const;
    float get_ascent() const;
    std::pair<float, float> get_scales() const;
    float get_width(unsigned int code) const;
    float get_width(const std::string &s) const;
private:
    enum Font_type_t { TYPE_3, OTHER };
    Font_type_t insert_type(const std::string &font_name, const dict_t &font_desc);
    void insert_descendant(dict_t &font, const ObjectStorage &storage);
    void insert_descent(const std::string &font_name,
                        const dict_t &font_desc,
                        const dict_t &font,
                        const std::string &base_font,
                        Font_type_t type,
                        const ObjectStorage &storage);
    void insert_ascent(const std::string &font_name,
                       const dict_t &font_desc,
                       const dict_t &font,
                       const std::string &base_font,
                       Font_type_t type,
                       const ObjectStorage &storage);
    void insert_height(const std::string &font_name,
                       const dict_t &font_desc,
                       const ObjectStorage &storage,
                       const std::string &base_font);
    void validate_current_font() const;
    void insert_matrix_type3(const std::string &font_name, const dict_t &font_desc);
    void insert_width(const ObjectStorage &storage,
                      const std::string &font_name,
                      const dict_t &font_desc,
                      const std::string &base_font);
    void insert_widths_from_widths(const ObjectStorage &storage,
                                   const std::string &font_name,
                                   const dict_t &font_desc,
                                   const std::string &base_font);
    void insert_widths_from_w(const ObjectStorage &storage, const std::string &font_name, const std::string &base_font);
    std::string current_font;
    std::map<std::string, dict_t> dictionary_per_font;
    std::map<std::string, float> heights;
    std::map<std::string, float> descents;
    std::map<std::string, float> ascents;
    std::map<std::string, Font_type_t> types;
    std::map<std::string, std::map<unsigned int, float>> widths;
    std::map<std::string, float> default_width;
    std::map<std::string, matrix_t> font_matrix_type_3;
    float rise;

    struct font_metric_t
    {
        font_metric_t(float ascent_arg, float descent_arg, float height_arg) noexcept :
                      ascent(ascent_arg),
                      descent(descent_arg),
                      height(height_arg)
        {
        }
        float ascent;
        float descent;
        float height;
    };

    static const float VSCALE_NO_TYPE_3;
    static const float HSCALE_NO_TYPE_3;
    static const unsigned int FIRST_CHAR_DEFAULT;
    static const float MISSING_WIDTH_DEFAULT;
    static const float DW_DEFAULT;
    static const float NO_HEIGHT;
    static const float NO_DESCENT;
    static const float RISE_DEFAULT;
    static const float NO_ASCENT;
    static const std::unordered_map<std::string, font_metric_t> std_metrics;
    static const std::unordered_map<std::string, std::map<unsigned int, float>> standard_widths;
};

#endif
