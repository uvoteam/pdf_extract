#include <array>
#include <utility>
#include <unordered_set>
#include <vector>
#include <stack>
#include <algorithm>
#include <iostream> //temp
#include <set>

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
#include "plane.h"
#include "converter_engine.h"

using namespace std;
using namespace boost;

#define DO_BT() {\
        coordinates.set_default();              \
        in_text_block = true;                   \
        continue;                               \
    }

#define DO_ET() {\
        in_text_block = false;\
        continue;\
}

#define DO_DO() {\
        const string XObject = pop(st).second;\
        const string resource_name = get_resource_name(resource_id, XObject);\
        auto it = XObject_streams.find(resource_name);\
        if (it != XObject_streams.end())\
        {\
            const matrix_t ctm = XObject_matrices.at(resource_name) * coordinates.get_CTM();\
            for (vector<text_chunk_t> &r : extract_text(it->second, resource_name, ctm)) result.push_back(std::move(r));\
        }\
}

#define DO_TF() {\
        coordinates.set_coordinates(token, st);\
        const string font = pop(st).second;\
        fonts.at(resource_id).set_current_font(font);\
        encoding = get_font_encoding(font, resource_id);\
        }

#define DO_Tj() {\
        result[0].push_back(encoding->get_string(decode_string(pop(st).second), coordinates, 0, fonts.at(resource_id)));\
    }

#define DO_QUOTE() {\
        coordinates.set_coordinates(token, st);\
        result[0].push_back(encoding->get_string(decode_string(pop(st).second), coordinates, 0, fonts.at(resource_id)));\
    }

#define DO_TS() {\
        fonts.at(resource_id).set_rise(stof(pop(st).second));\
    }

#define DO_DOUBLE_QUOTE() {\
        const string str = pop(st).second;\
        coordinates.set_coordinates(token, st);\
        result[0].push_back(encoding->get_string(str, coordinates, 0, fonts.at(resource_id)));\
}

#define DO_TJ() {\
        const vector<text_chunk_t> tj_texts = encoding->get_strings_from_array(pop(st).second,\
                                                                               coordinates,\
                                                                               fonts.at(resource_id));\
        result[0].insert(result[0].end(), tj_texts.begin(), tj_texts.end()); \
}




namespace
{
    struct dist_t
    {
        dist_t(unsigned char c_arg,
               float d_arg,
               const text_chunk_t &obj1_arg,
               const text_chunk_t &obj2_arg) noexcept : c(c_arg), d(d_arg), obj1(obj1_arg), obj2(obj2_arg)
        {
        }
        bool operator<(const dist_t &arg) const
        {
            if (c != arg.c) return c < arg.c;
            return d < arg.d;
        }
        dist_t& operator=(dist_t &&arg) = default;
        dist_t& operator=(const dist_t &arg) = default;
        dist_t(dist_t &&arg) = default;
        dist_t(const dist_t &arg) = default;
        unsigned char c;
        float d;
        text_chunk_t obj1;
        text_chunk_t obj2;
    };

    enum { MATRIX_ELEMENTS_NUM = 6, PDF_STRINGS_NUM = 300 /*for optimization*/ };
    const float LINE_OVERLAP = 0.5;
    const float CHAR_MARGIN = 2.0;
    const float WORD_MARGIN = 0.1;
    const float LINE_MARGIN = 0.5;
    const float BOXES_FLOW = 0.5;

    dist_t pop(set<dist_t> &dists)
    {
        if (dists.empty()) throw pdf_error(FUNC_STRING + "dists is empty");
        dist_t dist = std::move(*dists.begin());
        dists.erase(dists.begin());
        return dist;
    }

    text_chunk_t create_group(text_chunk_t &&obj1, text_chunk_t &&obj2)
    {
        float pos1 = (1 - BOXES_FLOW) * (obj1.coordinates.x0()) -
                     (1 + BOXES_FLOW) * (obj1.coordinates.y0() + obj1.coordinates.y1());
        float pos2 = (1 - BOXES_FLOW) * (obj2.coordinates.x0()) -
                     (1 + BOXES_FLOW) * (obj2.coordinates.y0() + obj2.coordinates.y1());
        text_chunk_t &&o1 = (pos1 <= pos2)? std::move(obj1) : std::move(obj2);
        text_chunk_t &&o2 = (pos1 <= pos2)? std::move(obj2) : std::move(obj1);

        text_chunk_t result = std::move(o1);
        for (const text_t &text : o2.texts)
        {
            result.coordinates.set_x0(min(result.coordinates.x0(), text.coordinates.x0()));
            result.coordinates.set_x1(max(result.coordinates.x1(), text.coordinates.x1()));
            result.coordinates.set_y0(min(result.coordinates.y0(), text.coordinates.y0()));
            result.coordinates.set_y1(max(result.coordinates.y1(), text.coordinates.y1()));
        }
        //optimization
        result.texts.insert(result.texts.end(),
                            std::make_move_iterator(o2.texts.begin()),
                            std::make_move_iterator(o2.texts.end()));
        result.is_group = true;
        return result;
    }

    string get_resource_name(const string &page, const string &object)
    {
        return "/" + page + "/" + object;
    }

    matrix_t init_CTM(unsigned int rotate, const mediabox_t &media_box)
    {
        if (rotate == 90) return matrix_t{{0, -1, 0},
                                         {1, 0, 0},
                                         {-media_box.at(1), media_box.at(2), 1}};
        if (rotate == 180) return matrix_t{{-1, 0, 0},
                                           {0, -1, 0},
                                           {media_box.at(2), media_box.at(3), 1}};
        if (rotate == 270) return matrix_t{{0, 1, 0},
                                           {-1, 0, 0},
                                           {media_box.at(3), -media_box.at(0), 1}};
        return matrix_t{{1, 0, 0},
                        {0, 1, 0},
                        {-media_box.at(0), -media_box.at(1), 0}};
    }

    bool is_voverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return obj2.y0() <= obj1.y1() && obj1.y0() <= obj2.y1();
    }

    bool is_hoverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return obj2.x0() <= obj1.x1() && obj1.x0() <= obj2.x1();
    }

    float hoverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return min(fabs(obj1.x0() - obj2.x1()), fabs(obj1.x1() - obj2.x0()));
    }

    float voverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return is_voverlap(obj1, obj2)? min(fabs(obj1.y0() - obj2.y1()), fabs(obj1.y1() - obj2.y0())) : 0;
    }

    float hdistance(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return is_hoverlap(obj1, obj2)? 0 : min(fabs(obj1.x0() - obj2.x1()), fabs(obj1.x1() - obj2.x0()));
    }

    float height(const coordinates_t &obj)
    {
        return obj.y1() - obj.y0();
    }

    float width(const text_chunk_t &obj)
    {
        return (obj.coordinates.x1() - obj.coordinates.x0()) / obj.string_len;
    }

    float width(const text_t &obj)
    {
        return (obj.coordinates.x1() - obj.coordinates.x0()) / utf8_length(obj.text);
    }

    float width(const coordinates_t &obj)
    {
        return obj.x1() - obj.x0();
    }

    bool is_halign(const text_chunk_t &obj1, const text_chunk_t &obj2)
    {
        return is_voverlap(obj1.coordinates, obj2.coordinates) &&
               (min(height(obj1.coordinates), height(obj2.coordinates)) * LINE_OVERLAP <
                voverlap(obj1.coordinates, obj2.coordinates)) &&
               (hdistance(obj1.coordinates, obj2.coordinates) < max(width(obj1), width(obj2)) * CHAR_MARGIN);
    }

    bool is_neighbour(const text_t &obj1, const text_t &obj2)
    {
        float height1 = height(obj1.coordinates), height2 = height(obj2.coordinates);
        float d = LINE_MARGIN * height1;
        if (fabs(height1 - height2) < d &&
            obj2.coordinates.x1() > obj1.coordinates.x0() && obj2.coordinates.x0() < obj1.coordinates.x1() &&
            obj2.coordinates.y0() < obj1.coordinates.y1() + d && obj2.coordinates.y1() > obj1.coordinates.y0() - d &&
            (fabs(obj1.coordinates.x0() - obj2.coordinates.x0()) < d ||
             fabs(obj1.coordinates.x1() - obj2.coordinates.x1()) < d))
           {
               return true;
           }
        return false;
    }

    bool merge_lines(vector<text_chunk_t> &lines, size_t j, size_t i)
    {
        bool merge = false;
        for (size_t jj = 0; jj < lines[j].texts.size() && !merge; ++jj)
        {
            for (size_t ii = 0; ii < lines[i].texts.size(); ++ii)
            {
                if (is_neighbour(lines[j].texts[jj], lines[i].texts[ii]))
                {
                    merge = true;
                    break;
                }
            }
        }
        if (!merge) return false;
        lines[j].string_len += lines[i].string_len;
        lines[j].texts.insert(lines[j].texts.end(),
                              std::make_move_iterator(lines[i].texts.begin()),
                              std::make_move_iterator(lines[i].texts.end()));

        lines[j].coordinates.set_x0(min(lines[j].coordinates.x0(), lines[i].coordinates.x0()));
        lines[j].coordinates.set_x1(max(lines[j].coordinates.x1(), lines[i].coordinates.x1()));
        lines[j].coordinates.set_y0(min(lines[j].coordinates.y0(), lines[i].coordinates.y0()));
        lines[j].coordinates.set_y1(max(lines[j].coordinates.y1(), lines[i].coordinates.y1()));
        lines.erase(lines.begin() + i);
        return true;
    }

    void add2line(text_chunk_t &line, const text_chunk_t &obj)
    {
        line.string_len += obj.string_len;
        for (const text_t &text : obj.texts) line.texts.push_back(std::move(text));
        line.coordinates.set_x0(min(line.coordinates.x0(), obj.coordinates.x0()));
        line.coordinates.set_x1(max(line.coordinates.x1(), obj.coordinates.x1()));
        line.coordinates.set_y0(min(line.coordinates.y0(), obj.coordinates.y0()));
        line.coordinates.set_y1(max(line.coordinates.y1(), obj.coordinates.y1()));
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
                if (is_cmp && line.is_group)
                {
                    add2line(line, obj1);
                }
                else if (line.is_group)
                {
                    result.push_back(line);
                    line.is_group = false;
                }
                else if (is_cmp)
                {
                    line = *obj0;
                    line.is_group = true;
                    add2line(line, obj1);
                }
                else
                {
                    result.push_back(*obj0);
                }
            }
            obj0 = &obj1;
        }
        if (!line.is_group && obj0) result.push_back(*obj0);
        if (line.is_group) result.push_back(line);
        return result;
    }

    void traverse_lines(vector<text_chunk_t> &chunks)
    {
    NEXT:
        for (size_t j = 0; j < chunks.size(); ++j)
        {
            for (size_t i = 0; i < chunks.size(); ++i)
            {
                if (j == i) continue;
                if (merge_lines(chunks, i, j)) goto NEXT;
            }
        }
    }

    void make_text_boxes(vector<text_chunk_t> &chunks)
    {
        traverse_lines(chunks);
        chunks.erase(remove_if(chunks.begin(),
                               chunks.end(),
                               [](const text_chunk_t& chunk) {
                                   return width(chunk.coordinates) <= 0 || height(chunk.coordinates) <= 0;}),
                     chunks.end());
        for (text_chunk_t &box : chunks)
        {
            if (box.texts.empty()) continue;
            vector<text_t> whole_box{text_t(box.coordinates)};
            sort(box.texts.begin(), box.texts.end(),
                 [](const text_t &a, const text_t &b) -> bool
                 {
                     if (a.coordinates.y1() != b.coordinates.y1()) return a.coordinates.y1() > b.coordinates.y1();
                     return a.coordinates.x0() < b.coordinates.x0();
                 });
            for (size_t i = 0; i < box.texts.size(); ++i)
            {
                whole_box[0].text += box.texts[i].text + '\n';
            }
            box.texts = std::move(whole_box);
        }
    }

    vector<text_chunk_t> make_text_lines(vector<text_chunk_t> &chunks)
    {
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
                     return a.coordinates.x0() < b.coordinates.x0();
                 });
            for (size_t i = 0; i < line.texts.size(); ++i)
            {
                whole_line[0].text += line.texts[i].text;
                if ((i != line.texts.size() - 1) &&
                    line.texts[i].coordinates.x1() < line.texts[i + 1].coordinates.x0() -
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
        float x0 = min(obj1.coordinates.x0(), obj2.coordinates.x0());
        float y0 = min(obj1.coordinates.y0(), obj2.coordinates.y0());
        float x1 = max(obj1.coordinates.x1(), obj2.coordinates.x1());
        float y1 = max(obj1.coordinates.y1(), obj2.coordinates.y1());
        return (x1 - x0) * (y1 - y0) -
               width(obj1.coordinates) * height(obj1.coordinates) - width(obj2.coordinates) * height(obj2.coordinates);
    }

    Plane make_plane(const vector<text_chunk_t> &chunks)
    {
        set<dist_t> dists;
        Plane plane(chunks);

        for (size_t i = 0; i < chunks.size(); ++i)
        {
            for (size_t j = i + 1; j < chunks.size(); ++j) dists.insert(dist_t(0,
                                                                               get_dist(chunks[i], chunks[j]),
                                                                               chunks[i],
                                                                               chunks[j]));
        }

        while (!dists.empty())
        {
            dist_t dist = pop(dists);
            if (dist.c == 0 && plane.is_any(dist.obj1, dist.obj2))
            {
                dists.insert(dist_t(1, dist.d, dist.obj1, dist.obj2));
                continue;
            }
            plane.remove(dist.obj1);
            plane.remove(dist.obj2);
            for (auto it = dists.begin(); it != dists.end();)
            {
                if (it->obj1 == dist.obj1 || it->obj1 == dist.obj2 || it->obj2 == dist.obj1 || it->obj2 == dist.obj2)
                {
                    it = dists.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            text_chunk_t group = create_group(std::move(dist.obj1), std::move(dist.obj2));
            for (const text_chunk_t &obj : plane.get_objects()) dists.insert(dist_t(0, get_dist(group, obj), group, obj));
            plane.add(group);
        }
        return plane;
    }

    string make_string(const Plane &plane)
    {
        string result;
        for (const text_chunk_t &group : plane.get_objects()) //must be 1
        {
            for (const text_t &box : group.texts) result += box.text;
        }
        return result;
    }

    string render_text(vector<text_chunk_t> &chunks)
    {
        vector<text_chunk_t> lines = make_text_lines(chunks);
/*        for (const text_chunk_t &chunk : lines) cout << '(' << chunk.coordinates.x0() << ',' << chunk.coordinates.y0() << ")(" << chunk.coordinates.x1() << ',' << chunk.coordinates.y1() << ')' << chunk.texts[0].text << endl;*/
        make_text_boxes(lines);
        return make_string(make_plane(lines));
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
                            get_fonts(data, dict_t()),
                            get_box(data, boost::none),
                            get_rotate(data, 0));
}

void PagesExtractor::get_pages_resources_int(unordered_set<unsigned int> &checked_nodes,
                                             const dict_t &parent_dict,
                                             const dict_t &parent_fonts,
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
            const dict_t fonts_dict = get_fonts(dict_data, parent_fonts);
            fonts.insert(make_pair(id_str, Fonts(storage, fonts_dict)));
            media_boxes.insert(make_pair(id_str, get_box(dict_data, parent_media_box).value()));
            rotates.insert(make_pair(id_str, get_rotate(dict_data, parent_rotate)));
            converter_engine_cache.insert(make_pair(id_str, map<string, ConverterEngine>()));;
            //avoiding infinite recursion
            unordered_set<unsigned int> visited_XObjects;
            get_XObjects_data(id_str, dict_data, fonts_dict, visited_XObjects);
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
                                      const dict_t &parent_fonts,
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
    const dict_t font = get_fonts(dict, parent_fonts);
    fonts.insert(make_pair(resource_name, Fonts(storage, font)));
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
        XObject_matrices.insert(make_pair(resource_name, matrix_t{{stof(numbers[0].first), stof(numbers[1].first), 0},
                                                                  {stof(numbers[2].first), stof(numbers[3].first), 0},
                                                                  {stof(numbers[4].first), stof(numbers[5].first), 0}}));
    }
    get_XObjects_data(resource_name, dict, font, visited_XObjects);
}

void PagesExtractor::get_XObjects_data(const string &parent_id,
                                       const dict_t &page,
                                       const dict_t &parent_fonts,
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

dict_t PagesExtractor::get_fonts(const dict_t &dictionary, const dict_t &parent_fonts) const
{
    auto it = dictionary.find("/Resources");
    if (it == dictionary.end()) return parent_fonts;
    const dict_t resources = get_dict_or_indirect_dict(it->second, storage);
    it = resources.find("/Font");
    if (it == resources.end()) return dict_t();
    return get_dict_or_indirect_dict(it->second, storage);
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
        for (vector<text_chunk_t> &r : extract_text(page_content, to_string(page_id), boost::none))
        {
            text += render_text(r);
        }
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
    vector<vector<text_chunk_t>> result;
    result.push_back(vector<text_chunk_t>());
    result[0].reserve(PDF_STRINGS_NUM);
    for (size_t i = 0; i < page_content.length();)
    {
        i = skip_comments(page_content, i, false);
        if (in_text_block && put2stack(st, page_content, i)) continue;
        const string token = get_token(page_content, i);
        if (is_skip_unused(page_content, i, token)) continue;
        if (token == "BT") DO_BT()
        else if (token == "ET") DO_ET()
        else if (ctm_tokens.count(token)) coordinates.ctm_work(token, st);
        else if (token == "Do") DO_DO()
        else if (token == "Tf") DO_TF()

        if (!in_text_block)
        {
            st.push(make_pair(VALUE, token));
            continue;
        }
        //vertical fonts are not implemented
        if (token == "Tj" && encoding && !encoding->is_vertical()) DO_Tj()
        else if (adjust_tokens.count(token)) coordinates.set_coordinates(token, st);
        else if (token == "'" && encoding) DO_QUOTE()
        else if (token == "Ts") DO_TS()
        else if (token == "\"" && encoding) DO_DOUBLE_QUOTE()
        //vertical fonts are not implemented
        else if (token == "TJ" && encoding && !encoding->is_vertical()) DO_TJ()
        else st.push(make_pair(VALUE, token));
    }

    return result;
}
