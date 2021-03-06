#ifndef DIFF_CONVERTER
#define DIFF_CONVERTER

#include <unordered_map>
#include <string>
#include <utility>

#include <boost/optional.hpp>

#include "fonts.h"
#include "object_storage.h"
#include "common.h"

class DiffConverter
{
public:
    DiffConverter() noexcept;
    explicit DiffConverter(std::unordered_map<unsigned int, std::string> &&difference_map_arg);
    boost::optional<std::string> get_char(char c) const;
    std::pair<std::string, float> get_string(const std::string &s, const Fonts &fonts) const;
    bool is_empty() const;
    static DiffConverter get_converter(const dict_t &dictionary,
                                       const std::pair<std::string, pdf_object_t> &differences,
                                       const ObjectStorage &storage);
private:
    const std::unordered_map<unsigned int, std::string> difference_map;
    bool empty;
    static const std::unordered_map<std::string, std::string> symbol_table;
};

#endif //DIFF_CONVERTER
