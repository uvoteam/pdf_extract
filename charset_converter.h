#ifndef CHARSET_CONVERTER_H
#define CHARSET_CONVERTER_H

#include <string>
#include <unordered_map>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include "cmap.h"
#include "object_storage.h"
#include "coordinates.h"
#include "fonts.h"

class CharsetConverter
{
public:
    CharsetConverter(const std::string &encoding);
    CharsetConverter(std::unordered_map<unsigned int, std::string> &&difference_map_arg);
    CharsetConverter() noexcept;
    CharsetConverter(const cmap_t *cmap_arg);
    text_chunk_t get_string(const std::string &s, Coordinates &coordinates, double Tj, const Fonts &fonts) const;
    std::vector<text_chunk_t> get_strings_from_array(const std::string &array,
                                                     Coordinates &coordinates,
                                                     const Fonts &fonts) const;
    static std::unique_ptr<CharsetConverter> get_from_dictionary(const dict_t &dictionary, const ObjectStorage &storage);
private:
    enum PDFEncode_t {DEFAULT, MAC_EXPERT, MAC_ROMAN, WIN, OTHER, UTF8, IDENTITY, TO_UNICODE, DIFFERENCE_MAP};

    std::string custom_decode_symbol(const std::string &s, size_t &i) const;
    static std::unique_ptr<CharsetConverter> get_diff_map_converter(PDFEncode_t encoding,
                                                                    const std::string &array,
                                                                    const ObjectStorage &storage);
    static boost::optional<std::string> get_symbol_string(const std::string &name);
private:
    const cmap_t *custom_encoding;
    const char *charset;
    PDFEncode_t PDFencode;
    const std::unordered_map<unsigned int, std::string> difference_map;
    static const std::unordered_map<unsigned int, std::string> standard_encoding;
    static const std::unordered_map<unsigned int, std::string> mac_roman_encoding;
    static const std::unordered_map<unsigned int, std::string> mac_expert_encoding;
    static const std::unordered_map<unsigned int, std::string> win_ansi_encoding;
    static const std::unordered_map<std::string, const char*> encoding2charset;
    static const std::map<PDFEncode_t, const std::unordered_map<unsigned int, std::string>&> standard_encodings;
    static const std::unordered_map<std::string, std::string> symbol_table;
};

#endif //CHARSET_CONVERTER
