#ifndef PAGES_EXTRACTOR_H
#define PAGES_EXTRACTOR_H

#include <string>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <boost/optional.hpp>

#include "common.h"
#include "object_storage.h"
#include "fonts.h"
#include "coordinates.h"
#include "diff_converter.h"
#include "to_unicode_converter.h"
#include "converter_engine.h"

enum {RECTANGLE_ELEMENTS_NUM = 4};
using mediabox_t = std::array<float, RECTANGLE_ELEMENTS_NUM>;

class PagesExtractor
{
public:
    PagesExtractor(unsigned int catalog_pages_id,
                   const ObjectStorage &storage_arg,
                   const dict_t &decrypt_data_arg,
                   const std::string &doc_arg);
    std::string get_text();
    struct extract_argument_t
    {
        std::vector<std::vector<text_chunk_t>> &result;
        ConverterEngine *encoding;
        std::vector<std::pair<pdf_object_t, std::string>> &st;
        Coordinates &coordinates;
        const std::string &resource_id;
        bool &in;
        const std::string &content;
   };
public:
    void do_Do(extract_argument_t &arg, size_t &i);
    void do_Tj(extract_argument_t &arg, size_t &i);
    void do_quote(extract_argument_t &arg, size_t &i);
    void do_double_quote(extract_argument_t &arg, size_t &i);
    void do_Ts(extract_argument_t &arg, size_t &i);
    void do_BT(extract_argument_t &arg, size_t &i);
    void do_ET(extract_argument_t &arg, size_t &i);
    void do_Q(extract_argument_t &arg, size_t &i);
    void do_T_star(extract_argument_t &arg, size_t &i);
    void do_TD(extract_argument_t &arg, size_t &i);
    void do_TJ(extract_argument_t &arg, size_t &i);
    void do_TL(extract_argument_t &arg, size_t &i);
    void do_Tc(extract_argument_t &arg, size_t &i);
    void do_Td(extract_argument_t &arg, size_t &i);
    void do_Tf(extract_argument_t &arg, size_t &i);
    void do_Tm(extract_argument_t &arg, size_t &i);
    void do_Tw(extract_argument_t &arg, size_t &i);
    void do_Tz(extract_argument_t &arg, size_t &i);
    void do_cm(extract_argument_t &arg, size_t &i);
    void do_q(extract_argument_t &arg, size_t &i);
    void do_BI(extract_argument_t &arg, size_t &i);
private:
    std::vector<std::pair<unsigned int, unsigned int>> get_id_gen_ap_n(const dict_t &page_dict, unsigned int page_id);
    std::string get_stream_contents(unsigned int page_id, const std::vector<std::pair<unsigned int, unsigned int>> &ids_gen, std::unordered_set<unsigned int> &visited_ids);
    std::string get_stream_contents_no_exception(unsigned int page_id, const std::vector<std::pair<unsigned int, unsigned int>> &ids_gen, std::unordered_set<unsigned int> &visited_ids);
    DiffConverter get_diff_converter(const boost::optional<std::pair<std::string, pdf_object_t>> &encoding) const;
    ToUnicodeConverter get_to_unicode_converter(const dict_t &font_dict);
    boost::optional<mediabox_t> get_box(const dict_t &dictionary,
                                        const boost::optional<mediabox_t> &parent_media_box) const;
    mediabox_t parse_rectangle(const std::pair<std::string, pdf_object_t> &rectangle) const;
    std::vector<std::vector<text_chunk_t>> extract_text(const std::string &page_content,
                                                        const std::string &resource_id,
                                                        const boost::optional<matrix_t> CTM);
    void get_pages_resources_int(std::unordered_set<unsigned int> &checked_nodes,
                                 const dict_t &parent_dict,
                                 const Fonts &parent_fonts,
                                 const boost::optional<mediabox_t> &parent_media_box,
                                 unsigned int parent_rotate);
    Fonts get_fonts(const dict_t &dictionary, const Fonts &parent_fonts) const;
    ConverterEngine* get_font_encoding(const std::string &font, const std::string &resource_id);
    boost::optional<std::pair<std::string, pdf_object_t>> get_encoding(const dict_t &font_dict) const;
    bool get_XObject_data(const std::string &page_id, const std::string &XObject_name, const std::string &resource_name);
private:
    const std::string &doc;
    const ObjectStorage &storage;
    const dict_t &decrypt_data;
    std::unordered_map<std::string, Fonts> fonts;
    std::vector<unsigned int> pages;
    std::unordered_map<std::string, dict_t> dicts;
    std::unordered_map<std::string, mediabox_t> media_boxes;
    std::unordered_map<std::string, unsigned int> rotates;
    std::unordered_map<std::string, std::unordered_map<std::string, ConverterEngine>> converter_engine_cache;
    std::unordered_map<std::string, std::string> XObject_streams;
    std::unordered_map<std::string, matrix_t> XObject_matrices;
    std::unordered_map<unsigned int, cmap_t> cmap_cache;
    std::unordered_map<std::string, dict_t> XObjects_cache;
};

#endif //PAGES_EXTRACTOR_H
