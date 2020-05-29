#ifndef CONVERTER_DATA_H
#define CONVERTER_DATA_H

#include <string>
#include <unordered_map>
#include <map>

enum PDFEncode_t {DEFAULT, MAC_EXPERT, MAC_ROMAN, WIN, IDENTITY, OTHER, UTF8, NONE};
extern const std::unordered_map<unsigned int, std::string> standard_encoding;
extern const std::unordered_map<unsigned int, std::string> mac_roman_encoding;
extern const std::unordered_map<unsigned int, std::string> mac_expert_encoding;
extern const std::unordered_map<unsigned int, std::string> win_ansi_encoding;
extern const std::unordered_map<std::string, const char*> encoding2charset;
extern const std::map<PDFEncode_t, const std::unordered_map<unsigned int, std::string>&> standard_encodings;


#endif //CONVERTER_DATA_H
