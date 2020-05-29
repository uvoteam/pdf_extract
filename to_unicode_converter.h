#ifndef TO_UNICODE_CONVERTER
#define TO_UNICODE_CONVERTER

#include <string>
#include <utility>

#include "fonts.h"
#include "cmap.h"

class ToUnicodeConverter
{
public:
    explicit ToUnicodeConverter(const cmap_t *custom_encoding_arg) noexcept;
    ToUnicodeConverter() noexcept;
    bool is_empty() const;
    bool is_vertical() const;
    std::pair<std::string, double> custom_decode_symbol(const std::string &s, size_t &i, const Fonts &fonts) const;
private:
    const cmap_t *custom_encoding;
};

#endif //TOUNICODE_CONVERTER
