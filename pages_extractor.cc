#include <array>
#include <utility>
#include <unordered_set>
#include <vector>
#include <memory>
#include <stack>
#include <algorithm>
#include <iostream> //temp

#include <boost/optional.hpp>

#include <math.h>

#include "common.h"
#include "object_storage.h"
#include "charset_converter.h"
#include "cmap.h"
#include "pages_extractor.h"
#include "coordinates.h"

using namespace std;
using namespace boost;
namespace
{
    const double LINE_OVERLAP = 0.5;
    const double CHAR_MARGIN = 2.0;
    const double WORD_MARGIN = 0.1;
    
    bool is_hoverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return obj2.x0 <= obj1.x1 && obj1.x0 <= obj2.x1;
    }

    bool is_voverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return obj2.y0 <= obj1.y1 && obj1.y0 <= obj2.y1;
    }

    double hoverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return min(fabs(obj1.x0 - obj2.x1), fabs(obj1.x1 - obj2.x0));
    }

    double voverlap(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return min(fabs(obj1.y0 - obj2.y1), fabs(obj1.y1 - obj2.y0));
    }

    double hdistance(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return min(fabs(obj1.x0 - obj2.x1), fabs(obj1.x1 - obj2.x0));
    }

    double vdistance(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return min(fabs(obj1.y0 - obj2.y1), fabs(obj1.y1 - obj2.y0));
    }

    double height(const coordinates_t &obj)
    {
        return obj.y1 - obj.y0;
    }

    double width(const coordinates_t &obj)
    {
        return obj.x1 - obj.x0;
    }

    bool is_halign(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return is_voverlap(obj1, obj2) &&
               (min(height(obj1), height(obj2)) * LINE_OVERLAP < voverlap(obj1, obj2)) &&
               (hdistance(obj1, obj2) < max(width(obj1), width(obj2)) * CHAR_MARGIN);
    }

    bool is_valign(const coordinates_t &obj1, const coordinates_t &obj2)
    {
        return is_hoverlap(obj1, obj2) &&
               (min(width(obj1), width(obj2)) * LINE_OVERLAP < hoverlap(obj1, obj2)) &&
               (vdistance(obj1, obj2) < max(height(obj1), height(obj2)) * CHAR_MARGIN);
    }

    bool merge_lines(vector<text_line_t> &lines, size_t j, size_t i)
    {
        // if (is_valign(lines[j].coordinates, lines[i].coordinates) &&
        //     !is_halign(lines[j].coordinates, lines[i].coordinates))
        //     cout << lines[j] << endl;
        if (is_halign(lines[j].coordinates, lines[i].coordinates))
        {
            for (text_chunk_t &chunk : lines[i].chunks) lines[j].chunks.push_back(std::move(chunk));
            lines[j].coordinates.x0 = min(lines[j].coordinates.x0, lines[i].coordinates.x0);
            lines[j].coordinates.x1 = max(lines[j].coordinates.x1, lines[i].coordinates.x1);
            lines[j].coordinates.y0 = min(lines[j].coordinates.y0, lines[i].coordinates.y0);
            lines[j].coordinates.y1 = max(lines[j].coordinates.y1, lines[i].coordinates.y1);
            lines.erase(lines.begin() + i);
            return true;
        }
        return false;
    }

    void make_text_lines(vector<text_line_t> &chunks)
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

    string render_text(vector<text_line_t> &chunks)
    {
        for (const text_line_t &chunk : chunks)
            {
                cout << '(' << chunk.coordinates.x0 << "," << chunk.coordinates.y0 << ")("  << chunk.coordinates.x1 << "," << chunk.coordinates.y1 << ")" << chunk.chunks[0].text << endl;
            }
        return string();
        string result;
        make_text_lines(chunks);
        sort(chunks.begin(), chunks.end(),
             [](const text_line_t &a, const text_line_t &b) -> bool
             {
                 return a.coordinates.y0 > b.coordinates.y0;
             });
        for (text_line_t &line : chunks)
        {
            sort(line.chunks.begin(), line.chunks.end(),
                 [](const text_chunk_t &a, const text_chunk_t &b) -> bool
                 {
                     return a.coordinates.x0 < b.coordinates.x0;
                 });
            for (size_t i = 0; i < line.chunks.size(); ++i)
            {
                result += line.chunks[i].text;
                if ((i != line.chunks.size() - 1) &&
                    line.chunks[i].coordinates.x1 < line.chunks[i + 1].coordinates.x0 -
                    width(line.chunks[i + 1].coordinates) / utf8_length(line.chunks[i + 1].text) * WORD_MARGIN)
                {
                    result += ' ';
                }
            }
            result += '\n';
        }
        return result;
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

    dict_t get_dict_or_indirect_dict(const pair<string, pdf_object_t> &data, const ObjectStorage &storage)
    {
        switch (data.second)
        {
        case DICTIONARY:
            return get_dictionary_data(data.first, 0);
        case INDIRECT_OBJECT:
            return get_dictionary_data(get_indirect_object_data(data.first, storage, DICTIONARY).first, 0);
        default:
            throw pdf_error(FUNC_STRING + "wrong object type " + to_string(data.second));
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
                            get_media_box(data, boost::none),
                            get_rotate(data, 0));
}

void PagesExtractor::get_pages_resources_int(unordered_set<unsigned int> &checked_nodes,
                                             const dict_t &parent_dict,
                                             const dict_t &parent_font,
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
        if (checked_nodes.count(id) == 0)
        {
            checked_nodes.insert(id);
            const pair<string, pdf_object_t> page_dict = storage.get_object(id);
            if (page_dict.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
            const dict_t dict_data = get_dictionary_data(page_dict.first, 0);
            if (dict_data.at("/Type").first == "/Page")
            {
                pages.push_back(id);
                fonts.insert(make_pair(id, Fonts(storage, get_fonts(dict_data, parent_font))));
                media_boxes.insert(make_pair(id, get_media_box(dict_data, parent_media_box).value()));
                rotates.insert(make_pair(id, get_rotate(dict_data, parent_rotate)));
            }
            else
            {
                get_pages_resources_int(checked_nodes,
                                        dict_data,
                                        get_fonts(dict_data, parent_font),
                                        get_media_box(dict_data, parent_media_box),
                                        get_rotate(dict_data, parent_rotate));

            }
        }
    }
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
    const vector<pair<string, pdf_object_t>> array_data = get_array_data(array, 0);
    if (array_data.size() != RECTANGLE_ELEMENTS_NUM)
    {
        throw pdf_error(FUNC_STRING + "wrong size of array. Size:" + to_string(array_data.size()));
    }
    mediabox_t result;
    for (size_t i = 0; i < result.size(); ++i) result[i] = stod(array_data.at(i).first);
    return result;
}

optional<mediabox_t> PagesExtractor::get_media_box(const dict_t &dictionary,
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
        text += extract_text(page_content, page_id);
    }
    return text;
}

optional<unique_ptr<CharsetConverter>> PagesExtractor::get_font_from_encoding(const dict_t &font_dict) const
{
    auto it = font_dict.find("/Encoding");
    if (it == font_dict.end()) return boost::none;
    const pair<string, pdf_object_t> encoding = (it->second.second == INDIRECT_OBJECT)?
                                                get_indirect_object_data(it->second.first, storage) : it->second;
    switch (encoding.second)
    {
    case DICTIONARY:
        return CharsetConverter::get_from_dictionary(get_dictionary_data(encoding.first, 0), storage);
    case NAME_OBJECT:
        return unique_ptr<CharsetConverter>(new CharsetConverter(encoding.first));
    default:
        throw pdf_error(FUNC_STRING + "wrong /Encoding type: " + to_string(encoding.second) + " val=" + encoding.first);
    }
}

unique_ptr<CharsetConverter> PagesExtractor::get_font_encoding(const string &font, unsigned int page_id)
{
    const dict_t &font_dict = fonts.at(page_id).get_current_font_dictionary();
    optional<unique_ptr<CharsetConverter>> r = get_font_from_tounicode(font_dict);
    if (r) return std::move(*r);
    r = get_font_from_encoding(font_dict);
    if (r) return std::move(*r);
    return unique_ptr<CharsetConverter>(new CharsetConverter());
}

optional<unique_ptr<CharsetConverter>> PagesExtractor::get_font_from_tounicode(const dict_t &font_dict)
{
    auto it = font_dict.find("/ToUnicode");
    if (it == font_dict.end()) return boost::none;
    switch (it->second.second)
    {
    case INDIRECT_OBJECT:
    {
        const pair<unsigned int, unsigned int> cmap_id_gen = get_id_gen(it->second.first);
        if (cmap_storage.count(cmap_id_gen.first) == 0)
        {
            cmap_storage.insert(make_pair(cmap_id_gen.first, get_cmap(doc, storage, cmap_id_gen, decrypt_data)));
        }
        return unique_ptr<CharsetConverter>(new CharsetConverter(&cmap_storage[cmap_id_gen.first]));
    }
    case NAME_OBJECT:
        return boost::none;
    default:
        throw pdf_error(FUNC_STRING + "/ToUnicode wrong type: " + to_string(it->second.second) + " val:" + it->second.first);
    }
}

string PagesExtractor::extract_text(const string &page_content, unsigned int page_id)
{
    static const unordered_set<string> adjust_tokens = {"Tz", "TL", "T*", "Tc", "Tw", "Td", "TD", "Tm"};
    unique_ptr<CharsetConverter> encoding(new CharsetConverter());
    Coordinates coordinates(rotates.at(page_id), media_boxes.at(page_id));
    stack<pair<pdf_object_t, string>> st;
    bool in_text_block = false;
    vector<text_line_t> texts;
    for (size_t i = 0; i < page_content.length();)
    {
        i = skip_spaces(page_content, i, false);
        if (in_text_block && put2stack(st, page_content, i)) continue;
        size_t end = page_content.find_first_of(" \r\n\t/[(<", i + 1);
        if (end == string::npos) end = page_content.length();
        const string token = page_content.substr(i, end - i);
        i = end;
        if (token == "BT")
        {
            coordinates.set_default();
            in_text_block = true;
            continue;
        }
        else if (token == "ET")
        {
            in_text_block = false;
            continue;
        }
        else if (token == "cm") coordinates.set_CTM(st);
        else if (token == "q") coordinates.push_CTM();
        else if (token == "Q") coordinates.pop_CTM();

        if (!in_text_block)
        {
            st.push(make_pair(VALUE, token));
            continue;
        }
        //vertical fonts are not implemented
        if (token == "Tj" && !encoding->is_vertical())
        {
            texts.push_back(encoding->get_string(decode_string(pop(st).second), coordinates, 0, fonts.at(page_id)));
        }
        else if (adjust_tokens.count(token))
        {
            coordinates.set_coordinates(token, st);
        }
        else if (token == "'")
        {
            coordinates.set_coordinates(token, st);
            texts.push_back(encoding->get_string(decode_string(pop(st).second), coordinates, 0, fonts.at(page_id)));
        }
        else if (token == "Ts")
        {
            fonts.at(page_id).set_rise(strict_stol(pop(st).second));
        }
        else if (token == "\"")
        {
            const string str = pop(st).second;
            coordinates.set_coordinates(token, st);
            texts.push_back(encoding->get_string(str, coordinates, 0, fonts.at(page_id)));
        }
        //vertical fonts are not implemented
        else if (token == "TJ" && !encoding->is_vertical())
        {
            const vector<text_line_t> tj_texts = encoding->get_strings_from_array(pop(st).second,
                                                                                  coordinates,
                                                                                  fonts.at(page_id));
            texts.insert(texts.end(), tj_texts.begin(), tj_texts.end());
        }
        else if (token == "Tf")
        {
            coordinates.set_coordinates(token, st);
            const string font = pop(st).second;
            fonts.at(page_id).set_current_font(font);
            encoding = get_font_encoding(font, page_id);
        }
        else
        {
            st.push(make_pair(VALUE, token));
        }
    }
    return render_text(texts);
}
