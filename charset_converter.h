#ifndef CHARSET_CONVERTER_H
#define CHARSET_CONVERTER_H

#include <string>
#include <utility>

#include <boost/optional.hpp>

#include "converter_data.h"
#include "fonts.h"

class CharsetConverter
{
public:
    CharsetConverter() noexcept;
    explicit CharsetConverter(const std::string &encoding);
    std::pair<std::string, float> get_string(const std::string &s, const Fonts &fonts) const;
    boost::optional<std::string> get_char(char c) const;
    bool is_empty() const;
    bool is_vertical() const;
private:
    const std::string encoding;
    PDFEncode_t encode;
    const char* charset;
};

#endif //CHARSET_CONVERTER_H
