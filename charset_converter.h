#ifndef FONT_ENCODINGS_H
#define FONT_ENCODINGS_H

#include <string>
#include <unordered_map>

struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

class CharsetConverter
{
public:
    enum PDFEncode_t {DEFAULT, MAC_EXPERT, MAC_ROMAN, WIN, OTHER, UTF8, IDENTITY, CUSTOM};
public:
    CharsetConverter(const std::string &encoding);
    CharsetConverter(PDFEncode_t PDFencode_arg);
    std::string get_string(const std::string &s) const;
private:
    //not const because of move constructor usage
    std::unordered_map<unsigned int, std::string> custom_encoding;
    const char *charset;
    PDFEncode_t PDFencode;
    static const std::unordered_map<unsigned int, std::string> standard_encoding;
    static const std::unordered_map<unsigned int, std::string> mac_roman_encoding;
    static const std::unordered_map<unsigned int, std::string> mac_expert_encoding;
    static const std::unordered_map<unsigned int, std::string> win_ansi_encoding;
    static const std::unordered_map<std::string, const char*> encoding2charset;
    static const std::unordered_map<PDFEncode_t,
                                    const std::unordered_map<unsigned int, std::string>&,
                                    EnumClassHash> standard_encodings;
};

#endif //FONT_ENCODINGS_H
