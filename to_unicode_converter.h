#ifndef TO_UNICODE_CONVERTER
#define TO_UNICODE_CONVERTER

#include <string>
#include <utility>

#include "fonts.h"
#include "cmap.h"

class ToUnicodeConverter
{
public:
    explicit ToUnicodeConverter(cmap_t &&custom_encoding_arg);
    ToUnicodeConverter() noexcept;
    bool is_empty() const;
    bool is_vertical() const;
    std::pair<std::string, float> custom_decode_symbol(const std::string &s, size_t &i, const Fonts &fonts) const;
private:
    const cmap_t custom_encoding;
    bool empty;
};

#endif //TOUNICODE_CONVERTER
