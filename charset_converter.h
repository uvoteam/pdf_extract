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
private:
    const char* charset;
    PDFEncode_t encode;
};

#endif //CHARSET_CONVERTER_H
