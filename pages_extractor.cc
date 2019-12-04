#include <array>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>
#include <memory>
#include <stack>

#include <boost/optional.hpp>

#include "common.h"
#include "object_storage.h"
#include "charset_converter.h"
#include "cmap.h"
#include "pages_extractor.h"

using namespace std;
using namespace boost;

namespace
{
    enum
    {
        TH_DEFAULT = 100,
        TC_DEFAULT = 0,
        TW_DEFAULT = 0,
        TFS_DEFAULT = 1,
        TL_DEFAULT = 0
    };
    
    string output_content(const string &buffer,
                          const ObjectStorage &storage,
                          const pair<unsigned int, unsigned int> &id_gen,
                          const map<string, pair<string, pdf_object_t>> &decrypt_data)
    {
        const pair<string, pdf_object_t> content_pair = storage.get_object(id_gen.first);
        if (content_pair.second == ARRAY)
        {
            //TODO check infinite recursion
            vector<pair<unsigned int, unsigned int>> contents = get_set(content_pair.first);
            string result;
            for (const pair<unsigned int, unsigned int> &p : contents)
            {
                result += output_content(buffer, storage, p, decrypt_data);
            }
            return result;
        }
        return get_stream(buffer, id_gen, storage, decrypt_data);
    }

    vector<pair<unsigned int, unsigned int>> get_contents_id_gen(const pair<string, pdf_object_t> &page_pair)
    {
        if (page_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
        const map<string, pair<string, pdf_object_t>> data = get_dictionary_data(page_pair.first, 0);
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

    map<string, pair<string, pdf_object_t>> get_dict_or_indirect_dict(const pair<string, pdf_object_t> &data,
                                                                      const ObjectStorage &storage)
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

    pair<pdf_object_t, string> pop(stack<pair<pdf_object_t, string>> &st)
    {
        if (st.empty()) throw pdf_error(FUNC_STRING + "stack is empty");
        pair<pdf_object_t, string> result = st.top();
        st.pop();
        return result;
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

    matrix_t get_matrix(stack<pair<pdf_object_t, string>> &st)
    {
        double e = stod(pop(st).second);
        double f = stod(pop(st).second);
        double d = stod(pop(st).second);
        double c = stod(pop(st).second);
        double b = stod(pop(st).second);
        double a = stod(pop(st).second);
        return matrix_t{{a, b, 0},
                        {c, d, 0},
                        {e, f, 1}};
    }

    unsigned int get_rotate(const map<string, pair<string, pdf_object_t>> &dictionary, unsigned int parent_rotate)
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
                               const map<string, pair<string, pdf_object_t>> &decrypt_data_arg,
                               const string &doc_arg) :
                               doc(doc_arg), storage(storage_arg), decrypt_data(decrypt_data_arg)
{
    const pair<string, pdf_object_t> catalog_pair = storage.get_object(catalog_pages_id);
    if (catalog_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "catalog must be DICTIONARY");
    const map<string, pair<string, pdf_object_t>> data = get_dictionary_data(catalog_pair.first, 0);
    auto it = data.find("/Type");
    if (it == data.end() || it->second.first != "/Pages")
    {
        throw pdf_error("In root catalog type must be '/Type /Pages'");
    }
    get_pages_resources_int(data, map<string, pair<string, pdf_object_t>>(), boost::none, 0);
}

matrix_t PagesExtractor::init_CTM(unsigned int page_id) const
{
    cropbox_t cropbox = crop_boxes.at(page_id);
    unsigned int rotate = rotates.at(page_id);
    if (rotate == 90) return matrix_t{{0, -1, 0},
                                      {1, 0, 0},
                                      {-cropbox.at(1), cropbox.at(2), 1}};
    if (rotate == 180) return matrix_t{{-1, 0, 0},
                                       {0, -1, 0},
                                       {cropbox.at(2), cropbox.at(3), 1}}; 
    if (rotate == 270) return matrix_t{{0, 1, 0},
                                       {-1, 0, 0},
                                       {cropbox.at(3), -cropbox.at(0), 1}};
    return matrix_t{{1, 0, 0},
                    {0, 1, 0},
                    {-cropbox.at(0), -cropbox.at(1), 0}};
}

PagesExtractor::cropbox_t PagesExtractor::parse_rectangle(const pair<string, pdf_object_t> &rectangle) const
{
    if (rectangle.second != ARRAY)
    {
        throw pdf_error(FUNC_STRING + "wrong type=" + to_string(rectangle.second) + " val:" + rectangle.first);
    }
    const vector<pair<string, pdf_object_t>> array_data = get_array_data(rectangle.first, 0);
    if (array_data.size() != RECTANGLE_ELEMENTS_NUM)
    {
        throw pdf_error(FUNC_STRING + "wrong size of array. Size:" + to_string(array_data.size()));
    }
    cropbox_t result;
    for (size_t i = 0; i < result.size(); ++i) result[i] = stod(array_data.at(i).first);
    return result;
}

void PagesExtractor::get_pages_resources_int(const map<string, pair<string, pdf_object_t>> &parent_dict,
                                             const map<string, pair<string, pdf_object_t>> &parent_font,
                                             const optional<cropbox_t> &parent_crop_box,
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
        if (find(pages.begin(), pages.end(), id) == pages.end())
        {
            const pair<string, pdf_object_t> page_dict = storage.get_object(id);
            if (page_dict.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
            pages.push_back(id);
            const map<string, pair<string, pdf_object_t>> dict_data = get_dictionary_data(page_dict.first, 0);
            if (dict_data.at("/Type").first == "/Page")
            {
                const map<string, pair<string, pdf_object_t>> font = get_fonts(dict_data, parent_font);
                const optional<cropbox_t> crop_box = get_crop_box(dict_data, parent_crop_box);
                unsigned int rotate = get_rotate(dict_data, parent_rotate);
                fonts.insert(make_pair(id, font));
                crop_boxes.insert(make_pair(id, crop_box.value()));
                rotates.insert(make_pair(id, rotate));
                get_pages_resources_int(dict_data, font, crop_box, rotate);
            }
            get_pages_resources_int(dict_data, parent_font, parent_crop_box, parent_rotate);
        }
    }
}

map<string, pair<string, pdf_object_t>> PagesExtractor::get_fonts(const map<string,
                                                                            pair<string, pdf_object_t>> &dictionary,
                                                                            const map<string,
                                                                            pair<string, pdf_object_t>>
                                                                            &parent_fonts) const
{
    auto it = dictionary.find("/Resources");
    if (it == dictionary.end()) return parent_fonts;
    const map<string, pair<string, pdf_object_t>> resources = get_dict_or_indirect_dict(it->second, storage);
    it = resources.find("/Font");
    if (it == resources.end()) return map<string, pair<string, pdf_object_t>>();
    return get_dict_or_indirect_dict(it->second, storage);
}

optional<PagesExtractor::cropbox_t> PagesExtractor::get_crop_box(const map<string, pair<string, pdf_object_t>> &dictionary,
                                                                 const optional<cropbox_t> &parent_crop_box) const
{
    auto it = dictionary.find("/CropBox");
    if (it != dictionary.end()) return parse_rectangle(it->second);
    it = dictionary.find("/MediaBox");
    if (it != dictionary.end()) return parse_rectangle(it->second);
    return parent_crop_box;
}

string PagesExtractor::get_text()
{
    string text;
    for (unsigned int page_id : pages)
    {
        vector<pair<unsigned int, unsigned int>> contents_id_gen = get_contents_id_gen(storage.get_object(page_id));
        string page_content;
        for (const pair<unsigned int, unsigned int> &id_gen : contents_id_gen)
        {
            page_content += output_content(doc, storage, id_gen, decrypt_data);
        }
        text += extract_text(page_content, page_id);
    }
    return text;
}

optional<unique_ptr<CharsetConverter>> PagesExtractor::get_font_from_encoding(map<string, pair<string,
                                                                              pdf_object_t>> &font_dict,
                                                                              unsigned int width) const
{
    auto it = font_dict.find("/Encoding");
    if (it == font_dict.end()) return boost::none;
    const pair<string, pdf_object_t> encoding = (it->second.second == INDIRECT_OBJECT)?
                                                get_indirect_object_data(it->second.first, storage) : it->second;
    switch (encoding.second)
    {
    case DICTIONARY:
        return CharsetConverter::get_from_dictionary(get_dictionary_data(encoding.first, 0), storage, width);
    case NAME_OBJECT:
        return unique_ptr<CharsetConverter>(new CharsetConverter(encoding.first, width));
    default:
        throw pdf_error(FUNC_STRING + "wrong /Encoding type: " + to_string(encoding.second) + " val=" + encoding.first);
    }
}

unique_ptr<CharsetConverter> PagesExtractor::get_font_encoding(const string &font, unsigned int page_id)
{
    const map<string, pair<string, pdf_object_t>> &page_fonts = fonts.at(page_id);
    auto it = page_fonts.find(font);
    if (it == page_fonts.end()) return unique_ptr<CharsetConverter>(new CharsetConverter());
    map<string, pair<string, pdf_object_t>> font_dict = get_dict_or_indirect_dict(it->second, storage);
    auto it2 = width_storage.find(font);
    unsigned int width;
    if (it2 == width_storage.end())
    {
        width = CharsetConverter::get_space_width(storage, font_dict);
        width_storage.insert(make_pair(font, width));
    }
    else
    {
        width = it2->second;
    }
    optional<unique_ptr<CharsetConverter>> r = get_font_from_tounicode(font_dict, width);
    if (r) return std::move(*r);
    r = get_font_from_encoding(font_dict, width);
    if (r) return std::move(*r);
    return unique_ptr<CharsetConverter>(new CharsetConverter(width));
}

optional<unique_ptr<CharsetConverter>> PagesExtractor::get_font_from_tounicode(const map<string, pair<string,
                                                                               pdf_object_t>> &font_dict,
                                                                               unsigned int width)
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
        return unique_ptr<CharsetConverter>(new CharsetConverter(&cmap_storage[cmap_id_gen.first], width));
    }
    case NAME_OBJECT:
        return boost::none;
    default:
        throw pdf_error(FUNC_STRING + "/ToUnicode wrong type: " + to_string(it->second.second) + " val:" + it->second.first);
    }
}
#include <iostream> //temp
string PagesExtractor::extract_text(const string &page_content, unsigned int page_id)
{
    unique_ptr<CharsetConverter> encoding(new CharsetConverter());
    matrix_t CTM = init_CTM(page_id);
    matrix_t Tm = matrix_t{{1, 0, 0},
                           {0, 1, 0},
                           {0, 0, 1}};
    stack<pair<pdf_object_t, string>> st;
    bool in_text_block = false;
    double Tfs = TFS_DEFAULT;
    double Th = TH_DEFAULT;
    double Tc = TC_DEFAULT;
    double Tw = TW_DEFAULT;
    double TL = TL_DEFAULT;
    vector<text_chunk_t> texts;
    double gtx = 0;
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
            matrix_t Tm = matrix_t{{1, 0, 0},
                                   {0, 1, 0},
                                   {0, 0, 1}};
            Th = TH_DEFAULT;
            Tc = TC_DEFAULT;
            Tw = TW_DEFAULT;
            Tfs = TFS_DEFAULT;
            TL = TL_DEFAULT;
            encoding->set_default_space_width();
            in_text_block = true;
            continue;
        }
        if (token == "ET")
        {
            in_text_block = false;
            continue;
        }
        if (token == "cm") CTM = multiply_matrixes(get_matrix(st), CTM);
        if (!in_text_block) continue;
        if (token == "Tj")
        {
            texts.push_back(encoding->get_string(decode_string(pop(st).second), Tm, CTM, Tfs, Tc, Tw, Th, 0, gtx));
        }
        else if (token == "Tz")
        {
            Th = stod(pop(st).second);
        }
        else if (token == "'")
        {
            Tm = multiply_matrixes(matrix_t{{1, 0, 0},
                                            {0, 1, 0},
                                            {0, -TL, 1}}, Tm);
            texts.push_back(encoding->get_string(decode_string(pop(st).second), Tm, CTM, Tfs, Tc, Tw, Th, 0, gtx));
        }
        else if (token == "\"")
        {
            const string str = pop(st).second;
            Tc = stod(pop(st).second);
            Tw = stod(pop(st).second);
            Tm = multiply_matrixes(matrix_t{{1, 0, 0},
                                            {0, 1, 0},
                                            {0, -TL, 1}}, Tm);
            texts.push_back(encoding->get_string(str, Tm, CTM, Tfs, Tc, Tw, Th, 0, gtx));
        }
        else if (token == "TL")
        {
            TL = stod(pop(st).second);
        }
        else if (token == "TJ")
        {
            texts.push_back(encoding->get_strings_from_array(pop(st).second, CTM, Tm, Tfs, Tc, Tw, Th, gtx));
        }
        else if (token == "T*")
        {
            Tm = multiply_matrixes(matrix_t{{1, 0, 0},
                                            {0, 1, 0},
                                            {0, -TL, 1}}, Tm);
        }
        else if (token == "Tc")
        {
            Tc = stod(pop(st).second);
        }
        else if (token == "Tw")
        {
            Tw = stod(pop(st).second);
        }
        else if (token == "Td")
        {
            double ty = stod(pop(st).second);
            double tx = stod(pop(st).second);
            Tm = multiply_matrixes(matrix_t{{1, 0, 0},
                                            {0, 1, 0},
                                            {tx, ty, 1}}, Tm);
        }
        else if (token == "TD")
        {
            TL = -stod(pop(st).second);
            double tx = stod(pop(st).second);
            Tm = multiply_matrixes(matrix_t{{1, 0, 0},
                                            {0, 1, 0},
                                            {tx, -TL, 1}}, Tm);
        }
        else if (token == "Tm")
        {
            double f = stod(pop(st).second);
            double e = stod(pop(st).second);
            double d = stod(pop(st).second);
            double c = stod(pop(st).second);
            double b = stod(pop(st).second);
            double a = stod(pop(st).second);
            Tm = matrix_t{{a, b, 0},
                          {c, d, 0},
                          {e, f, 1}};
        }
        else if (token == "Tf")
        {
            Tfs = stod(pop(st).second);
            encoding = get_font_encoding(pop(st).second, page_id);
        }
        else
        {
            st.push(make_pair(VALUE, token));
        }
    }
    for (const text_chunk_t &chunk : texts) cout << '(' << chunk.x << ',' << chunk.y << ')' << chunk.text << endl;
    return string();
}
