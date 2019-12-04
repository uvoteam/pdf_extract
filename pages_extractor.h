#ifndef PAGES_EXTRACTOR_H
#define PAGES_EXTRACTOR_H

#include <array>
#include <string>
#include <utility>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include "common.h"
#include "object_storage.h"
#include "cmap.h"
#include "charset_converter.h"

class PagesExtractor
{
public:
    PagesExtractor(unsigned int catalog_pages_id,
                   const ObjectStorage &storage_arg,
                   const std::map<std::string, std::pair<std::string, pdf_object_t>> &decrypt_data_arg,
                   const std::string &doc_arg);
    std::string get_text();
private:
    enum {RECTANGLE_ELEMENTS_NUM = 4};
    using cropbox_t = std::array<double, RECTANGLE_ELEMENTS_NUM>;
    matrix_t init_CTM(unsigned int page_id) const;
    std::string extract_text(const std::string &page_content, unsigned int page_id);
    cropbox_t parse_rectangle(const std::pair<std::string, pdf_object_t> &rectangle) const;
    void get_pages_resources_int(const std::map<std::string, std::pair<std::string, pdf_object_t>> &parent_dict,
                                 const boost::optional<std::map<std::string, std::pair<std::string, pdf_object_t>>> &parent_fonts,
                                 const boost::optional<cropbox_t> &parent_crop_box,
                                 const boost::optional<unsigned int> &parent_rotate);
    boost::optional<std::map<std::string, std::pair<std::string, pdf_object_t>>> get_fonts(const std::map<std::string,
                                                                          std::pair<std::string, pdf_object_t>> &dictionary,
                                                                          const boost::optional<std::map<std::string,
                                                                          std::pair<std::string,
                                                                          pdf_object_t>>> &parent_fonts) const;
    boost::optional<cropbox_t> get_crop_box(const std::map<std::string, std::pair<std::string, pdf_object_t>> &dictionary,
                           const boost::optional<cropbox_t> &parent_crop_box) const;
    std::unique_ptr<CharsetConverter> get_font_encoding(const std::string &font, unsigned int page_id);
    boost::optional<std::unique_ptr<CharsetConverter>> get_font_from_encoding(std::map<std::string, std::pair<std::string,
                                                                              pdf_object_t>> &font_dict,
                                                                              unsigned int width) const;
    boost::optional<std::unique_ptr<CharsetConverter>> get_font_from_tounicode(const std::map<std::string,
                                                                               std::pair<std::string,
                                                                               pdf_object_t>> &font_dict,
                                                                               unsigned int width);
private:
    const std::string &doc;
    const ObjectStorage &storage;
    const std::map<std::string, std::pair<std::string, pdf_object_t>> &decrypt_data;
    std::vector<unsigned int> pages;
    std::map<unsigned int, std::map<std::string, std::pair<std::string, pdf_object_t>>> fonts;
    std::map<unsigned int, cropbox_t> crop_boxes;
    std::map<unsigned int, unsigned int> rotates;
    std::map<unsigned int, cmap_t> cmap_storage;
    std::map<std::string, unsigned int> width_storage;
};

#endif //PAGES_EXTRACTOR_H
