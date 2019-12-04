#ifndef CHARSET_CONVERTER_H
#define CHARSET_CONVERTER_H

#include <string>
#include <unordered_map>
#include <map>
#include <utility>
#include <memory>

#include <boost/optional.hpp>

#include "cmap.h"
#include "common.h"
#include "object_storage.h"


class CharsetConverter
{
public:
    enum { SPACE_WIDTH_ARRAY_FRACTION = 2, SPACE_WIDTH_SCALAR_FRACTION = 5, NO_SPACE_WIDTH = 0, DEFAULT_SPACE_WIDTH = 500};
    CharsetConverter(const std::string &encoding, unsigned int space_width_arg);
    CharsetConverter(std::unordered_map<unsigned int, std::string> &&difference_map_arg, unsigned int space_width_arg);
    CharsetConverter(unsigned int space_width_arg = NO_SPACE_WIDTH) noexcept;
    CharsetConverter(const cmap_t *cmap_arg, unsigned int space_width_arg);
    text_chunk_t get_string(const std::string &s, matrix_t &Tm, const matrix_t &CTM,
                            double Tfs, double Tc, double Tw, double Th, double Tj, double &gtx) const;
    text_chunk_t get_strings_from_array(const std::string &array, matrix_t &Tm, const matrix_t &CTM,
                                        double Tfs, double Tc, double Tw, double Th, double &gtx) const;
    static std::unique_ptr<CharsetConverter> get_from_dictionary(const std::map<std::string,
                                                                 std::pair<std::string, pdf_object_t>> &dictionary,
                                                                 const ObjectStorage &storage,
                                                                 unsigned int space_width);
    static unsigned int get_space_width(const ObjectStorage &storage,
                                        const std::map<std::string, std::pair<std::string, pdf_object_t>> &font_dict);
    unsigned int get_space_width() const;
    void set_default_space_width();
private:
    enum PDFEncode_t {DEFAULT, MAC_EXPERT, MAC_ROMAN, WIN, OTHER, UTF8, IDENTITY, TO_UNICODE, DIFFERENCE_MAP};

    std::string custom_decode_symbol(const std::string &s, size_t &i) const;
    void adjust_coordinates(matrix_t &Tm, double Tfs, double Tc, double Tw, double Th, double Tj, size_t len, double &tx) const;
    void adjust_tx(double Tfs, double Tc, double Tw, double Th, double Tj, size_t len, double &tx) const;
    static std::unique_ptr<CharsetConverter> get_diff_map_converter(PDFEncode_t encoding,
                                                                    const std::string &array,
                                                                    const ObjectStorage &storage,
                                                                    unsigned int space_width);
    static boost::optional<std::string> get_symbol_string(const std::string &name);
private:
    const cmap_t *custom_encoding;
    const char *charset;
    PDFEncode_t PDFencode;
    const std::unordered_map<unsigned int, std::string> difference_map;
    unsigned int space_width;
    static const std::unordered_map<unsigned int, std::string> standard_encoding;
    static const std::unordered_map<unsigned int, std::string> mac_roman_encoding;
    static const std::unordered_map<unsigned int, std::string> mac_expert_encoding;
    static const std::unordered_map<unsigned int, std::string> win_ansi_encoding;
    static const std::unordered_map<std::string, const char*> encoding2charset;
    static const std::map<PDFEncode_t, const std::unordered_map<unsigned int, std::string>&> standard_encodings;
    static const std::unordered_map<std::string, std::string> symbol_table;
};

#endif //CHARSET_CONVERTER
