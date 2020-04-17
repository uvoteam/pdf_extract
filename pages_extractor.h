#ifndef PAGES_EXTRACTOR_H
#define PAGES_EXTRACTOR_H

#include <array>
#include <string>
#include <utility>
#include <map>
#include <memory>
#include <unordered_set>

#include <boost/optional.hpp>

#include "common.h"
#include "object_storage.h"
#include "cmap.h"
#include "fonts.h"

enum {RECTANGLE_ELEMENTS_NUM = 4};
using mediabox_t = std::array<double, RECTANGLE_ELEMENTS_NUM>;

class CharsetConverter;
class PagesExtractor
{
public:
    PagesExtractor(unsigned int catalog_pages_id,
                   const ObjectStorage &storage_arg,
                   const dict_t &decrypt_data_arg,
                   const std::string &doc_arg);
    std::string get_text();
private:
    boost::optional<mediabox_t>get_media_box(const dict_t &dictionary,
                                             const boost::optional<mediabox_t> &parent_media_box) const;
    mediabox_t parse_rectangle(const std::pair<std::string, pdf_object_t> &rectangle) const;
    std::string extract_text(const std::string &page_content, unsigned int page_id);
    void get_pages_resources_int(std::unordered_set<unsigned int> &checked_nodes,
                                 const dict_t &parent_dict,
                                 const dict_t &parent_fonts,
                                 const boost::optional<mediabox_t> &parent_media_box,
                                 unsigned int parent_rotate);
    dict_t get_fonts(const dict_t &dictionary, const dict_t &parent_fonts) const;
    std::unique_ptr<CharsetConverter> get_font_encoding(const std::string &font, unsigned int page_id);
    boost::optional<std::unique_ptr<CharsetConverter>> get_font_from_encoding(const dict_t &font_dict) const;
    boost::optional<std::unique_ptr<CharsetConverter>> get_font_from_tounicode(const dict_t &font_dict);
private:
    const std::string &doc;
    const ObjectStorage &storage;
    const dict_t &decrypt_data;
    std::map<unsigned int, Fonts> fonts;
    std::vector<unsigned int> pages;
    std::map<unsigned int, mediabox_t> media_boxes;
    std::map<unsigned int, unsigned int> rotates;
    std::map<unsigned int, cmap_t> cmap_storage;
};

#endif //PAGES_EXTRACTOR_H
