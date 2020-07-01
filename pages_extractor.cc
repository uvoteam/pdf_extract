#include <utility>
#include <unordered_set>
#include <vector>
#include <stack>
#include <algorithm>
#include <iostream> //temp

#include <boost/optional.hpp>

#include <math.h>

#include "common.h"
#include "object_storage.h"
#include "charset_converter.h"
#include "diff_converter.h"
#include "to_unicode_converter.h"
#include "cmap.h"
#include "pages_extractor.h"
#include "coordinates.h"

#include "converter_engine.h"

using namespace std;
using namespace boost;

#define DO_BT(COORDINATES, IN_TEXT_BLOCK) {                   \
        COORDINATES.set_default();              \
        IN_TEXT_BLOCK = true;                   \
        continue;                               \
    }

#define DO_ET(IN_TEXT_BLOCK) {\
        IN_TEXT_BLOCK = false;\
        continue;\
}

namespace
{
    struct dist_t
    {
        dist_t(unsigned char c_arg,
               float d_arg,
               size_t obj1_arg,
               size_t obj2_arg) noexcept : d(d_arg), obj1(obj1_arg), obj2(obj2_arg), c(c_arg)
        {
        }
        float d;
        size_t obj1;
        size_t obj2;
        unsigned char c;
    };

    enum { MATRIX_ELEMENTS_NUM = 6, PDF_STRINGS_NUM = 300 /*for optimization*/ };
    constexpr float LINE_OVERLAP = 0.5;
    constexpr float CHAR_MARGIN = 2.0;
    constexpr float WORD_MARGIN = 0.1;
    constexpr float LINE_MARGIN = 0.5;
    constexpr float BOXES_FLOW = 0.5;

    bool operator<(const dist_t &obj1, const dist_t &obj2)
    {
        if (obj1.c != obj2.c) return obj1.c < obj2.c;
        return obj1.d < obj2.d;
    }

    bool is_between(const vector<text_chunk_t> &groups, size_t obj1, size_t obj2)
    {
        float x0 = min(groups[obj1].coordinates.x0, groups[obj2].coordinates.x0);
        float y0 = min(groups[obj1].coordinates.y0, groups[obj2].coordinates.y0);
        float x1 = max(groups[obj1].coordinates.x1, groups[obj2].coordinates.x1);
        float y1 = max(groups[obj1].coordinates.y1, groups[obj2].coordinates.y1);
        return find_if(groups.begin(), groups.end(), [x0, y0, x1, y1, obj1, obj2, &groups](const text_chunk_t &obj)
                       {
                           const coordinates_t &coord = obj.coordinates;
                           if (coord.x0 >= x0 && coord.y0 >= y0 && coord.x1 <= x1 && coord.y1 <= y1 &&
                               !obj.is_empty &&
                               !(obj == groups[obj1]) && !(obj == groups[obj2])) return true;
                           return false;
                       }) != groups.end();
    }

    size_t create_group(vector<text_chunk_t> &groups, size_t obj1, size_t obj2)
    {
        float pos1 = (1 - BOXES_FLOW) * (groups[obj1].coordinates.x0) -
                     (1 + BOXES_FLOW) * (groups[obj1].coordinates.y0 + groups[obj1].coordinates.y1);
        float pos2 = (1 - BOXES_FLOW) * (groups[obj2].coordinates.x0) -
                     (1 + BOXES_FLOW) * (groups[obj2].coordinates.y0 + groups[obj2].coordinates.y1);
        size_t o1 = (pos1 <= pos2)? obj1 : obj2;
        size_t o2 = (pos1 <= pos2)? obj2 : obj1;

        for (const text_t &text : groups[o2].texts)
        {
            if (text.coordinates.x0 < groups[o1].coordinates.x0) groups[o1].coordinates.x0 = text.coordinates.x0;
            if (text.coordinates.x1 > groups[o1].coordinates.x1) groups[o1].coordinates.x1 = text.coordinates.x1;
            if (text.coordinates.y0 < groups[o1].coordinates.y0) groups[o1].coordinates.y0 = text.coordinates.y0;
            if (text.coordinates.y1 > groups[o1].coordinates.y1) groups[o1].coordinates.y1 = text.coordinates.y1;
        }
        //optimization
        groups[o1].texts.insert(groups[o1].texts.end(),
                                std::make_move_iterator(groups[o2].texts.begin()),
                                std::make_move_iterator(groups[o2].texts.end()));
        groups[o2].is_empty = true;
        return o1;
    }

    string get_resource_name(const string &page, const string &object)
    {
        return "/" + page + "/" + object;
    }

    matrix_t init_CTM(unsigned int rotate, const mediabox_t &media_box)
    {
        if (rotate == 90) return matrix_t{0, -1, 1, 0, -media_box.at(1), media_box.at(2)};
        if (rotate == 180) return matrix_t{-1, 0, 0, -1, media_box.at(2), media_box.at(3)};
        if (rotate == 270) return matrix_t{0, 1, -1, 0, media_box.at(3), -media_box.at(0)};
        return matrix_t{1, 0, 0, 1, -media_box.at(0), -media_box.at(1)};
    }

    bool is_voverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return obj2.y0 <= obj1.y1 && obj1.y0 <= obj2.y1;
    }

    bool is_hoverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return obj2.x0 <= obj1.x1 && obj1.x0 <= obj2.x1;
    }

    float voverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return is_voverlap(obj1, obj2)? min(fabs(obj1.y0 - obj2.y1), fabs(obj1.y1 - obj2.y0)) : 0;
    }

    float hdistance(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return is_hoverlap(obj1, obj2)? 0 : min(fabs(obj1.x0 - obj2.x1), fabs(obj1.x1 - obj2.x0));
    }

    float height(const coordinates_t &obj)
    {
        return obj.y1 - obj.y0;
    }

    float width(const text_chunk_t &obj)
    {
        return (obj.coordinates.x1 - obj.coordinates.x0) / obj.string_len;
    }

    float width(const text_t &obj)
    {
        return (obj.coordinates.x1 - obj.coordinates.x0) / utf8_length(obj.text);
    }

    float width(const coordinates_t &obj)
    {
        return obj.x1 - obj.x0;
    }

    bool is_halign(const text_chunk_t &obj1, const text_chunk_t &obj2)
    {
        return is_voverlap(obj1.coordinates, obj2.coordinates) &&
               (min(height(obj1.coordinates), height(obj2.coordinates)) * LINE_OVERLAP <
                voverlap(obj1.coordinates, obj2.coordinates)) &&
               (hdistance(obj1.coordinates, obj2.coordinates) < max(width(obj1), width(obj2)) * CHAR_MARGIN);
    }

    text_chunk_t merge_lines(vector<text_chunk_t> &&lines)
    {
        lines.erase(remove_if(lines.begin(),
                              lines.end(),
                              [](const text_chunk_t& line) {
                                  return width(line.coordinates) <= 0 || height(line.coordinates) <= 0;}),
                    lines.end());
        if (lines.empty()) return text_chunk_t();
        sort(lines.begin(), lines.end(),
             [](const text_chunk_t &a, const text_chunk_t &b) -> bool
             {
                 if (a.coordinates.y1 != b.coordinates.y1) return a.coordinates.y1 > b.coordinates.y1;
                 return a.coordinates.x0 < b.coordinates.x0;
             });
        text_chunk_t result(lines[0].texts[0].text + '\n', std::move(lines[0].coordinates));
        for (size_t i = 1; i < lines.size(); ++i)
        {
            result.texts[0].text += lines[i].texts[0].text + '\n';
            if (lines[i].coordinates.x0 < result.coordinates.x0) result.coordinates.x0 = lines[i].coordinates.x0;
            if (lines[i].coordinates.x1 > result.coordinates.x1) result.coordinates.x1 = lines[i].coordinates.x1;
            if (lines[i].coordinates.y0 < result.coordinates.y0) result.coordinates.y0 = lines[i].coordinates.y0;
            if (lines[i].coordinates.y1 > result.coordinates.y1) result.coordinates.y1 = lines[i].coordinates.y1;
            result.string_len += lines[i].string_len;
        }
        result.texts[0].coordinates = result.coordinates;

        return result;
    }

    void add2line(text_chunk_t &line, const text_chunk_t &obj)
    {
        line.string_len += obj.string_len;
        for (const text_t &text : obj.texts) line.texts.push_back(std::move(text));
        if (obj.coordinates.x0 < line.coordinates.x0) line.coordinates.x0 = obj.coordinates.x0;
        if (obj.coordinates.x1 > line.coordinates.x1) line.coordinates.x1 = obj.coordinates.x1;
        if (obj.coordinates.y0 < line.coordinates.y0) line.coordinates.y0 = obj.coordinates.y0;
        if (obj.coordinates.y1 > line.coordinates.y1) line.coordinates.y1 = obj.coordinates.y1;
    }

    vector<text_chunk_t> traverse_symbols(const vector<text_chunk_t> &chunks)
    {
        if (chunks.empty()) vector<text_chunk_t>();
        vector<text_chunk_t> result;
        text_chunk_t line;
        const text_chunk_t *obj0 = nullptr;
        for (const text_chunk_t &obj1 : chunks)
        {
            if (obj0)
            {
                bool is_cmp = is_halign(*obj0, obj1);
                if (is_cmp && !line.is_empty)
                {
                    add2line(line, obj1);
                }
                else if (!line.is_empty)
                {
                    result.push_back(line);
                    line.is_empty = true;
                }
                else if (is_cmp)
                {
                    line = *obj0;
                    line.is_empty = false;
                    add2line(line, obj1);
                }
                else
                {
                    result.push_back(*obj0);
                }
            }
            obj0 = &obj1;
        }
        if (line.is_empty && obj0) result.push_back(*obj0);
        if (!line.is_empty) result.push_back(line);
        return result;
    }

    bool is_neighbour_lines(const text_chunk_t &obj1, const text_chunk_t &obj2)
    {
        if (obj1.is_empty || obj2.is_empty) return false;
        float height1 = height(obj1.coordinates), height2 = height(obj2.coordinates);
        float d = LINE_MARGIN * max(height1, height2);
        if (fabs(height1 - height2) < d &&
            obj2.coordinates.x1 > obj1.coordinates.x0 && obj2.coordinates.x0 < obj1.coordinates.x1 &&
            obj2.coordinates.y0 < obj1.coordinates.y1 + d && obj2.coordinates.y1 > obj1.coordinates.y0 - d &&
            (fabs(obj1.coordinates.x0 - obj2.coordinates.x0) < d ||
             fabs(obj1.coordinates.x1 - obj2.coordinates.x1) < d))
        {
            return true;
        }
        return false;
    }

    vector<text_chunk_t> get_neighbour_lines(vector<text_chunk_t> &&lines, text_chunk_t&& line_arg)
    {
        vector<text_chunk_t> result;
        result.push_back(std::move(line_arg));
        for (size_t i = 0; i < result.size(); ++i)
        {
            for (text_chunk_t &line : lines)
            {
                if (is_neighbour_lines(line, result[i])) result.push_back(std::move(line));
            }
        }
        return result;
    }

    vector<text_chunk_t> make_text_boxes(vector<text_chunk_t> &&lines)
    {
        vector<text_chunk_t> text_boxes;
        auto not_empty = [](const text_chunk_t &line) { return !line.is_empty; };
        for (auto it = find_if(lines.begin(), lines.end(), not_empty);
             it != lines.end();
             it = find_if(it, lines.end(), not_empty))
        {
            text_chunk_t line = merge_lines(get_neighbour_lines(std::move(lines), *std::make_move_iterator(it)));
            if (line.is_empty) continue;
            text_boxes.push_back(std::move(line));
        }
        return text_boxes;
    }

    vector<text_chunk_t> make_text_lines(vector<text_chunk_t> &chunks)
    {
        //clear empty strings
        chunks.erase(remove_if(chunks.begin(),
                               chunks.end(),
                               [](const text_chunk_t& chunk) {
                                   return chunk.string_len == 0;
                               }),
                     chunks.end());

        vector<text_chunk_t> result = traverse_symbols(chunks);
        for (text_chunk_t &line : result)
        {
            if (line.texts.empty()) continue;
            vector<text_t> whole_line{text_t(line.coordinates)};
            sort(line.texts.begin(), line.texts.end(),
                 [](const text_t &a, const text_t &b) -> bool
                 {
                     return a.coordinates.x0 < b.coordinates.x0;
                 });
            for (size_t i = 0; i < line.texts.size(); ++i)
            {
                whole_line[0].text += line.texts[i].text;
                if ((i != line.texts.size() - 1) &&
                    line.texts[i].coordinates.x1 < line.texts[i + 1].coordinates.x0 -
                    width(line.texts[i + 1]) * WORD_MARGIN)
                {
                    whole_line[0].text += ' ';
                }
            }
            line.texts = std::move(whole_line);
        }
        return result;
    }

    float get_dist(const text_chunk_t &obj1, const text_chunk_t &obj2)
    {
        float x0 = min(obj1.coordinates.x0, obj2.coordinates.x0);
        float y0 = min(obj1.coordinates.y0, obj2.coordinates.y0);
        float x1 = max(obj1.coordinates.x1, obj2.coordinates.x1);
        float y1 = max(obj1.coordinates.y1, obj2.coordinates.y1);
        return (x1 - x0) * (y1 - y0) -
               width(obj1.coordinates) * height(obj1.coordinates) - width(obj2.coordinates) * height(obj2.coordinates);
    }

    text_chunk_t make_plane(vector<text_chunk_t> &&boxes)
    {
        if (boxes.empty()) return text_chunk_t();
        vector<dist_t> dists;
        dists.reserve(boxes.size() * (boxes.size() - 1));
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            for (size_t j = i + 1; j < boxes.size(); ++j) dists.emplace_back(0, get_dist(boxes[i], boxes[j]), i, j);
        }

        while (!dists.empty())
        {
            auto it = min_element(dists.begin(), dists.end());
            if (it->c == 0 && is_between(boxes, it->obj1, it->obj2))
            {
                it->c = 1;
                continue;
            }
            const dist_t dist = *it;
            dists.erase(remove_if(dists.begin(), dists.end(), [&dist] (const dist_t &o) {
                        if (o.obj1 == dist.obj1 || o.obj1 == dist.obj2 ||
                            o.obj2 == dist.obj1 || o.obj2 == dist.obj2) return true;
                        return false;
                        }), dists.end());
            size_t group = create_group(boxes, dist.obj1, dist.obj2);
            for (size_t i = 0; i < boxes.size(); ++i)
            {
                if (i == group || boxes[i].is_empty) continue;
                dists.emplace_back(0, get_dist(boxes[group], boxes[i]), group, i);
            }
        }

        for (text_chunk_t &group : boxes)
        {
            if (!group.is_empty) return std::move(group);
        }
        throw pdf_error(FUNC_STRING + "all objects are moved");
    }

    string make_string(const text_chunk_t &group)
    {
        if (group.is_empty) return string();
        string result;
        for (const text_t &box : group.texts) result += box.text;
        return result;
    }

    string render_text(vector<text_chunk_t> &chunks)
    {
/*        for (const text_chunk_t &chunk : lines) cout << '(' << chunk.coordinates.x0 << ',' << chunk.coordinates.y0 << ")(" << chunk.coordinates.x1 << ',' << chunk.coordinates.y1 << ')' << chunk.texts[0].text << endl;*/
        return make_string(make_plane(make_text_boxes(make_text_lines(chunks))));
    }

    string output_content(unordered_set<unsigned int> &visited_contents,
                          const string &buffer,
                          const ObjectStorage &storage,
                          const pair<unsigned int, unsigned int> &id_gen,
                          const dict_t &decrypt_data)
    {
        const pair<string, pdf_object_t> content_pair = storage.get_object(id_gen.first);
        if (content_pair.second == ARRAY)
        {
            vector<pair<unsigned int, unsigned int>> contents = get_set(content_pair.first);
            string result;
            for (const pair<unsigned int, unsigned int> &p : contents)
            {
                //avoid infinite recursion
                if (visited_contents.count(p.first)) continue;
                visited_contents.insert(p.first);
                result += output_content(visited_contents, buffer, storage, p, decrypt_data);
            }
            return result;
        }
        return get_stream(buffer, id_gen, storage, decrypt_data);
    }

    vector<pair<unsigned int, unsigned int>> get_contents_id_gen(const pair<string, pdf_object_t> &page_pair)
    {
        if (page_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
        const dict_t data = get_dictionary_data(page_pair.first, 0);
        auto it = data.find("/Contents");
        // "/Contents" key can be absent for Page. In this case Page is empty
        if (it == data.end()) return vector<pair<unsigned int, unsigned int>>();
        vector<pair<unsigned int, unsigned int>> contents_id_gen;
        const string &contents_data = it->second.first;
        switch (it->second.second)
        {
        case ARRAY:
            contents_id_gen = get_set(contents_data);
            return contents_id_gen;
        case INDIRECT_OBJECT:
            contents_id_gen.push_back(get_id_gen(contents_data));
            return contents_id_gen;
        default:
            throw pdf_error(FUNC_STRING + "/Contents type must be ARRAY or INDIRECT_OBJECT");
            break;
        }
    }

    bool put2stack(stack<pair<pdf_object_t, string>> &st, const string &buffer, size_t &i)
    {
        switch (buffer[i])
        {
        case '(':
            st.push(make_pair(STRING, get_string(buffer, i)));
            return true;
        case '<':
            buffer.at(i + 1) == '<'? st.push(make_pair(DICTIONARY, get_dictionary(buffer, i))) :
                                     st.push(make_pair(STRING, get_string(buffer, i)));
            return true;
        case '[':
            st.push(make_pair(ARRAY, get_array(buffer, i)));
            return true;
        default:
            return false;
        }
    }

    unsigned int get_rotate(const dict_t &dictionary, unsigned int parent_rotate)
    {
        auto it = dictionary.find("/Rotate");
        if (it != dictionary.end())
        {
            unsigned int v = strict_stoul(it->second.first);
            if (v % 90 != 0) throw pdf_error(FUNC_STRING + "/Rotate must be multiple of 90.Val:" + to_string(v));
            return v;
        }
        return parent_rotate;
    }

    CharsetConverter get_charset_converter(const optional<pair<string, pdf_object_t>> &encoding)
    {
        if (!encoding) return CharsetConverter(string());
        if (encoding->second == NAME_OBJECT) return CharsetConverter(encoding->first);
        const dict_t dictionary = get_dictionary_data(encoding->first, 0);
        auto it = dictionary.find("/Differences");
        if (it != dictionary.end()) return CharsetConverter();
        it = dictionary.find("/BaseEncoding");
        return (it == dictionary.end())? CharsetConverter(string()) : CharsetConverter(it->second.first);
    }

    bool is_skip_unused(const string &content, size_t &i, const string &token)
    {
        if (token == "BI")
        {
            while (true)
            {
                i = content.find("EI", i);
                if (i == string::npos)
                {
                    i = content.length();
                    return true;
                }
                i += sizeof("EI") - 1;
                if (i == content.length() || is_blank(content[i])) return true;
            }
        }
        else
        {
            return false;
        }
    }

    string get_token(const string &page_content, size_t &i)
    {
        size_t end = page_content.find_first_of(" \r\n\t/[(<", i + 1);
        if (end == string::npos) end = page_content.length();
        string result = page_content.substr(i, end - i);
        i = end;
        return result;
    }
}

PagesExtractor::PagesExtractor(unsigned int catalog_pages_id,
                               const ObjectStorage &storage_arg,
                               const dict_t &decrypt_data_arg,
                               const string &doc_arg) :
                               doc(doc_arg), storage(storage_arg), decrypt_data(decrypt_data_arg)
{
    const pair<string, pdf_object_t> catalog_pair = storage.get_object(catalog_pages_id);
    if (catalog_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "catalog must be DICTIONARY");
    const dict_t data = get_dictionary_data(catalog_pair.first, 0);
    auto it = data.find("/Type");
    if (it == data.end() || it->second.first != "/Pages")
    {
        throw pdf_error("In root catalog type must be '/Type /Pages'");
    }
    unordered_set<unsigned int> checked_nodes;
    get_pages_resources_int(checked_nodes,
                            data,
                            get_fonts(data, Fonts(storage, dict_t())),
                            get_box(data, boost::none),
                            get_rotate(data, 0));
}

void PagesExtractor::get_pages_resources_int(unordered_set<unsigned int> &checked_nodes,
                                             const dict_t &parent_dict,
                                             const Fonts &parent_fonts,
                                             const optional<mediabox_t> &parent_media_box,
                                             unsigned int parent_rotate)
{
    auto it = parent_dict.find("/Type");
    if (it == parent_dict.end() || it->second.first != "/Pages") return;
    pair<string, pdf_object_t> kids = parent_dict.at("/Kids");
    if (kids.second != ARRAY) throw pdf_error(FUNC_STRING + "/Kids is not array");

    for (const pair<unsigned int, unsigned int> &page : get_set(kids.first))
    {
        unsigned int id = page.first;
        //avoid infinite recursion for 'bad' pdf
        if (checked_nodes.count(id)) continue;
        checked_nodes.insert(id);
        const pair<string, pdf_object_t> page_dict = storage.get_object(id);
        if (page_dict.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
        const dict_t dict_data = get_dictionary_data(page_dict.first, 0);
        if (dict_data.at("/Type").first == "/Page")
        {
            pages.push_back(id);
            const string id_str = to_string(id);
            const Fonts page_fonts = get_fonts(dict_data, parent_fonts);
            fonts.insert(make_pair(id_str, page_fonts));
            media_boxes.insert(make_pair(id_str, get_box(dict_data, parent_media_box).value()));
            rotates.insert(make_pair(id_str, get_rotate(dict_data, parent_rotate)));
            converter_engine_cache.insert(make_pair(id_str, map<string, ConverterEngine>()));;
            //avoiding infinite recursion
            unordered_set<unsigned int> visited_XObjects;
            get_XObjects_data(id_str, dict_data, page_fonts, visited_XObjects);
        }
        else
        {
            get_pages_resources_int(checked_nodes,
                                    dict_data,
                                    get_fonts(dict_data, parent_fonts),
                                    get_box(dict_data, parent_media_box),
                                    get_rotate(dict_data, parent_rotate));

        }
    }
}

void PagesExtractor::get_XObject_data(const string &parent_id,
                                      const dict_t::value_type &XObject,
                                      const Fonts &parent_fonts,
                                      unordered_set<unsigned int> &visited_XObjects)
{
    if (XObject.second.second == INDIRECT_OBJECT)
    {
        unsigned int id = get_id_gen(XObject.second.first).first;
        if (visited_XObjects.count(id)) return;
        visited_XObjects.insert(id);
    }
    const dict_t dict = get_dict_or_indirect_dict(XObject.second, storage);
    const pair<string, pdf_object_t> p = dict.at("/Subtype");
    if (p.first != "/Form") return;
    auto it = dict.find("/BBox");
    if (it == dict.end()) return;
    const string resource_name = get_resource_name(parent_id, XObject.first);
    const Fonts page_fonts = get_fonts(dict, parent_fonts);
    fonts.insert(make_pair(resource_name, page_fonts));
    converter_engine_cache.insert(make_pair(resource_name, map<string, ConverterEngine>()));
    XObject_streams.insert(make_pair(resource_name, get_stream(doc,
                                                               get_id_gen(XObject.second.first),
                                                               storage,
                                                               decrypt_data)));
    it = dict.find("Matrix");
    if (it == dict.end())
    {
        XObject_matrices.insert(make_pair(resource_name, IDENTITY_MATRIX));
    }
    else
    {
        const array_t numbers = get_array_data(it->second.first, 0);
        if (numbers.size() != MATRIX_ELEMENTS_NUM) throw pdf_error(FUNC_STRING + "matrix must have " +
                                                                   to_string(MATRIX_ELEMENTS_NUM) +
                                                                   "elements. Data = " + it->second.first);
        XObject_matrices.insert(make_pair(resource_name, matrix_t{stof(numbers[0].first), stof(numbers[1].first),
                                                                  stof(numbers[2].first), stof(numbers[3].first),
                                                                  stof(numbers[4].first), stof(numbers[5].first)}));
    }
    get_XObjects_data(resource_name, dict, page_fonts, visited_XObjects);
}

void PagesExtractor::get_XObjects_data(const string &parent_id,
                                       const dict_t &page,
                                       const Fonts &parent_fonts,
                                       unordered_set<unsigned int> &visited_XObjects)
{
    auto it = page.find("/Resources");
    if (it == page.end()) return;
    const dict_t resources = get_dict_or_indirect_dict(it->second, storage);
    it = resources.find("/XObject");
    if (it == resources.end()) return;
    const dict_t XObjects = get_dict_or_indirect_dict(it->second, storage);
    for (const dict_t::value_type &p : XObjects) get_XObject_data(parent_id, p, parent_fonts, visited_XObjects);
}

Fonts PagesExtractor::get_fonts(const dict_t &dictionary, const Fonts &parent_fonts) const
{
    auto it = dictionary.find("/Resources");
    if (it == dictionary.end()) return parent_fonts;
    const dict_t resources = get_dict_or_indirect_dict(it->second, storage);
    it = resources.find("/Font");
    if (it == resources.end()) return Fonts(storage, dict_t());
    return Fonts(storage, get_dict_or_indirect_dict(it->second, storage));
}

mediabox_t PagesExtractor::parse_rectangle(const pair<string, pdf_object_t> &rectangle) const
{
    if (rectangle.second != ARRAY && rectangle.second != INDIRECT_OBJECT)
    {
        throw pdf_error(FUNC_STRING + "wrong type=" + to_string(rectangle.second) + " val:" + rectangle.first);
    }
    const string array = (rectangle.second == INDIRECT_OBJECT)? storage.get_object(get_id_gen(rectangle.first).first).first :
                                                                rectangle.first;
    const array_t array_data = get_array_data(array, 0);
    if (array_data.size() != RECTANGLE_ELEMENTS_NUM)
    {
        throw pdf_error(FUNC_STRING + "wrong size of array. Size:" + to_string(array_data.size()));
    }
    mediabox_t result;
    for (size_t i = 0; i < result.size(); ++i) result[i] = stof(array_data.at(i).first);
    return result;
}

optional<mediabox_t> PagesExtractor::get_box(const dict_t &dictionary,
                                             const optional<mediabox_t> &parent_media_box) const
{
    auto it = dictionary.find("/MediaBox");
    if (it != dictionary.end()) return parse_rectangle(it->second);
    return parent_media_box;
}

string PagesExtractor::get_text()
{
    string text;
    for (unsigned int page_id : pages)
    {
        vector<pair<unsigned int, unsigned int>> contents_id_gen = get_contents_id_gen(storage.get_object(page_id));
        string page_content;
        unordered_set<unsigned int> visited_contents;
        for (const pair<unsigned int, unsigned int> &id_gen : contents_id_gen)
        {
            page_content += output_content(visited_contents, doc, storage, id_gen, decrypt_data);
        }
        for (vector<text_chunk_t> &r : extract_text(page_content, to_string(page_id), boost::none)) text += render_text(r);
    }
    return text;
}

optional<pair<string, pdf_object_t>> PagesExtractor::get_encoding(const dict_t &font_dict) const
{
    auto it = font_dict.find("/Encoding");
    if (it == font_dict.end()) return boost::none;
    const pair<string, pdf_object_t> encoding = (it->second.second == INDIRECT_OBJECT)?
                                                get_indirect_object_data(it->second.first, storage) : it->second;
    if (encoding.second != DICTIONARY && encoding.second != NAME_OBJECT)
    {
        throw pdf_error(FUNC_STRING + "wrong /Encoding type: " + to_string(encoding.second) + " val=" + encoding.first);
    }
    return encoding;
}

DiffConverter PagesExtractor::get_diff_converter(const optional<pair<string, pdf_object_t>> &encoding) const
{
    if (!encoding || encoding->second == NAME_OBJECT) return DiffConverter();
    const dict_t dictionary = get_dictionary_data(encoding->first, 0);
    auto it2 = dictionary.find("/Differences");
    if (it2 == dictionary.end()) return DiffConverter();
    return DiffConverter::get_converter(dictionary, it2->second, storage);
}

ToUnicodeConverter PagesExtractor::get_to_unicode_converter(const dict_t &font_dict)
{
    auto it = font_dict.find("/ToUnicode");
    if (it == font_dict.end()) return ToUnicodeConverter();
    switch (it->second.second)
    {
    case INDIRECT_OBJECT:
        return ToUnicodeConverter(get_cmap(doc, storage, get_id_gen(it->second.first), decrypt_data));
    case NAME_OBJECT:
        return ToUnicodeConverter();
    default:
        throw pdf_error(FUNC_STRING + "/ToUnicode wrong type: " + to_string(it->second.second) + " val:" + it->second.first);
    }
}

ConverterEngine* PagesExtractor::get_font_encoding(const string &font, const string &resource_id)
{
    auto it = converter_engine_cache.at(resource_id).find(font);
    if (it != converter_engine_cache[resource_id].end()) return &it->second;
    const dict_t &font_dict = fonts.at(resource_id).get_current_font_dictionary();
    optional<pair<string, pdf_object_t>> encoding = get_encoding(font_dict);
    converter_engine_cache[resource_id].insert(make_pair(font, ConverterEngine(get_charset_converter(encoding),
                                                                               get_diff_converter(encoding),
                                                                               get_to_unicode_converter(font_dict))));
    return &converter_engine_cache[resource_id][font];
}

ConverterEngine* PagesExtractor::do_tf(Coordinates &coordinates,
                                       stack<pair<pdf_object_t, string>> &st,
                                       const string &resource_id,
                                       const string &token)
{
    coordinates.set_coordinates(token, st);
    const string font = pop(st).second;
    fonts.at(resource_id).set_current_font(font);
    return get_font_encoding(font, resource_id);
}

void PagesExtractor::do_tj(vector<text_chunk_t> &result,
                           const ConverterEngine *encoding,
                           stack<pair<pdf_object_t, string>> &st,
                           Coordinates &coordinates,
                           const string &resource_id) const
{
    result.push_back(encoding->get_string(decode_string(pop(st).second), coordinates, 0, fonts.at(resource_id)));
}

void PagesExtractor::do_TJ(vector<text_chunk_t> &result,
                           const ConverterEngine *encoding,
                           stack<pair<pdf_object_t, string>> &st,
                           Coordinates &coordinates,
                           const string &resource_id) const
{
    vector<text_chunk_t> tj_texts = encoding->get_strings_from_array(pop(st).second, coordinates, fonts.at(resource_id));
    result.insert(result.end(), std::make_move_iterator(tj_texts.begin()), std::make_move_iterator(tj_texts.end()));
}

void PagesExtractor::do_do(vector<vector<text_chunk_t>> &result,
                           const string &XObject,
                           const string &resource_id,
                           const matrix_t &parent_ctm)
{
     const string resource_name = get_resource_name(resource_id, XObject);
     auto it = XObject_streams.find(resource_name);
     if (it != XObject_streams.end())
     {
         const matrix_t ctm = XObject_matrices.at(resource_name) * parent_ctm;
         for (vector<text_chunk_t> &r : extract_text(it->second, resource_name, ctm)) result.push_back(std::move(r));
     }
}

void PagesExtractor::do_quote(vector<text_chunk_t> &result,
                              Coordinates &coordinates,
                              const ConverterEngine *encoding,
                              stack<pair<pdf_object_t, string>> &st,
                              const string &resource_id,
                              const string &token) const
{
    coordinates.set_coordinates(token, st);
    result.push_back(encoding->get_string(decode_string(pop(st).second), coordinates, 0, fonts.at(resource_id)));
}

void PagesExtractor::do_double_quote(vector<text_chunk_t> &result,
                                     Coordinates &coordinates,
                                     const ConverterEngine *encoding,
                                     stack<pair<pdf_object_t, string>> &st,
                                     const string &resource_id,
                                     const string &token) const
{
    const string str = pop(st).second;
    coordinates.set_coordinates(token, st);
    result.push_back(encoding->get_string(str, coordinates, 0, fonts.at(resource_id)));
}

void PagesExtractor::do_ts(const string &resource_id, float rise)
{
    fonts.at(resource_id).set_rise(rise);
}

vector<vector<text_chunk_t>> PagesExtractor::extract_text(const string &page_content,
                                                          const string &resource_id,
                                                          const optional<matrix_t> CTM)
{
    static const unordered_set<string> adjust_tokens = {"Tz", "TL", "T*", "Tc", "Tw", "Td", "TD", "Tm"};
    static const unordered_set<string> ctm_tokens = {"cm", "q", "Q"};
    ConverterEngine *encoding = nullptr;
    Coordinates coordinates(CTM? *CTM : init_CTM(rotates.at(resource_id), media_boxes.at(resource_id)));
    stack<pair<pdf_object_t, string>> st;
    bool in_text_block = false;
    vector<vector<text_chunk_t>> result(1);
    result[0].reserve(PDF_STRINGS_NUM);
    for (size_t i = 0; i != string::npos && i < page_content.length(); i = skip_comments(page_content, i, false))
    {
        if (in_text_block && put2stack(st, page_content, i)) continue;
        const string token = get_token(page_content, i);
        if (is_skip_unused(page_content, i, token)) continue;
        if (token == "BT") DO_BT(coordinates, in_text_block)
        else if (token == "ET") DO_ET(in_text_block)
        else if (ctm_tokens.count(token)) coordinates.ctm_work(token, st);
        else if (token == "Do") do_do(result, pop(st).second, resource_id, coordinates.get_CTM());
        else if (token == "Tf") encoding = do_tf(coordinates, st, resource_id, token);

        if (!in_text_block)
        {
            st.push(make_pair(VALUE, token));
            continue;
        }
        //vertical fonts are not implemented
        if (token == "Tj" && encoding && !encoding->is_vertical()) do_tj(result[0], encoding, st, coordinates, resource_id);
        else if (adjust_tokens.count(token)) coordinates.set_coordinates(token, st);
        else if (token == "'" && encoding) do_quote(result[0], coordinates, encoding, st, resource_id, token);
        else if (token == "Ts") do_ts(resource_id, stof(pop(st).second));
        else if (token == "\"" && encoding) do_double_quote(result[0], coordinates, encoding, st, resource_id, token);
        //vertical fonts are not implemented
        else if (token == "TJ" &&
                 encoding &&
                 !encoding->is_vertical()) do_TJ(result[0], encoding, st, coordinates, resource_id);
        else st.push(make_pair(VALUE, token));
    }

    return result;
}
