#ifndef CONVERTER_ENGINE_H
#define CONVERTER_ENGINE_H

#include <string>

#include "charset_converter.h"
#include "diff_converter.h"
#include "to_unicode_converter.h"
#include "coordinates.h"
#include "fonts.h"

class ConverterEngine
{
public:
    ConverterEngine(CharsetConverter &&charset_converter_arg,
                    DiffConverter &&diff_converter_arg,
                    ToUnicodeConverter &&to_unicode_converter_arg);
    ConverterEngine() = default;
    bool is_vertical() const;
    text_chunk_t get_string(const std::string &s, Coordinates &coordinates, double Tj, const Fonts &fonts) const;
    std::vector<text_chunk_t> get_strings_from_array(const std::string &array,
                                                     Coordinates &coordinates,
                                                     const Fonts &fonts) const;

private:
    const CharsetConverter charset_converter;
    const DiffConverter diff_converter;
    const ToUnicodeConverter to_unicode_converter;
};

#endif //CONVERTER_ENGINE_H
