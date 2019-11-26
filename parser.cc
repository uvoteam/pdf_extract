#include <string>
#include <cstring>
#include <map>
#include <utility>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <stack>

//for test
#include <fstream>
#include <iostream>


#include "common.h"
#include "charset_converter.h"
#include "object_storage.h"
#include "cmap.h"

using namespace std;
using pages_id_resources_t = vector<pair<unsigned int, map<string, pair<string, pdf_object_t>>>>;


enum {CROSS_REFERENCE_LINE_SIZE = 20,
      BYTE_OFFSET_LEN = 10, /* length for byte offset in cross reference record */
      GENERATION_NUMBER_LEN = 5 /* length for generation number */
};

bool is_prefix(const char *str, const char *pre);
uint64_t get_uint64(const string &src);
array<uint64_t, 3> get_cross_reference_entry(const string &stream, size_t &offset, const array<unsigned int, 3> &w);
pair<pdf_object_t, string> pop(vector<pair<pdf_object_t, string>> &st);
void get_offsets_internal_new(const string &stream,
                              const map<string, pair<string, pdf_object_t>> dictionary_data,
                              vector<size_t> &result);
size_t get_cross_ref_offset(const string &buffer);
pair<string, pair<string, pdf_object_t>> get_id(const string &buffer, size_t start, size_t end);
void append_object(const string &buf, size_t offset, vector<size_t> &objects);
char get_object_status(const string &buffer, size_t offset);
unique_ptr<CharsetConverter> get_font_encoding(const string &doc,
                                               const string &font,
                                               const map<string, pair<string, pdf_object_t>> &fonts,
                                               const ObjectStorage &storage,
                                               const map<string, pair<string, pdf_object_t>> &decrypt_data,
                                               map<unsigned int, cmap_t> &cmap_storage,
                                               map<string, unsigned int> &width_storage);
size_t get_xref_number(const string &buffer, size_t &offset);
vector<pair<size_t, size_t>> get_trailer_offsets(const string &buffer, size_t cross_ref_offset);
vector<pair<size_t, size_t>> get_trailer_offsets_old(const string &buffer, size_t cross_ref_offset);
vector<pair<size_t, size_t>> get_trailer_offsets_new(const string &buffer, size_t cross_ref_offset);
void get_object_offsets(const string &buffer, size_t offset, vector<size_t> &result);
void get_object_offsets_new(const string &buffer, size_t offset, vector<size_t> &result);
void get_object_offsets_old(const string &buffer, size_t offset, vector<size_t> &result);
void validate_offsets(const string &buffer, const vector<size_t> &offsets);
map<string, pair<string, pdf_object_t>> get_dict_or_indirect_dict(const pair<string, pdf_object_t> &data,
                                                                  const ObjectStorage &storage);
vector<size_t> get_all_object_offsets(const string &buffer,
                                      size_t cross_ref_offset,
                                      const vector<pair<size_t, size_t>> &trailer_offsets);
map<string, pair<string, pdf_object_t>> get_fonts(const map<string, pair<string, pdf_object_t>> &dictionary,
                                                  const ObjectStorage &storage,
                                                  const map<string, pair<string, pdf_object_t>> &parent_fonts);
map<size_t, size_t> get_id2offsets(const string &buffer,
                                   size_t cross_ref_offset,
                                   const vector<pair<size_t, size_t>> &trailer_offsets);
unsigned int get_cross_ref_entries(const map<string, pair<string, pdf_object_t>> dictionary_data,
                                   const array<unsigned int, 3> &w, size_t length);
array<unsigned int, 3> get_w(const map<string, pair<string, pdf_object_t>> &dictionary_data);
string get_text(const string &buffer,
                size_t cross_ref_offset,
                const ObjectStorage &storage,
                map<string, pair<string, pdf_object_t>> &encrypt_data);
pages_id_resources_t get_pages_id_fonts(unsigned int catalog_pages_id, const ObjectStorage &storage);
void get_pages_id_fonts_int(const map<string, pair<string, pdf_object_t>> &parent_dict,
                            const map<string, pair<string, pdf_object_t>> &parent_fonts,
                            const ObjectStorage &storage,
                            pages_id_resources_t &result);
vector<pair<unsigned int, unsigned int>> get_contents_id_gen(const pair<string, pdf_object_t> &page_pair);
string output_content(const string &buffer,
                      const ObjectStorage &storage,
                      const pair<unsigned int, unsigned int> &id_gen,
                      const map<string, pair<string, pdf_object_t>> &encrypt_data);
map<string, pair<string, pdf_object_t>> get_encrypt_data(const string &buffer,
                                                         size_t start,
                                                         size_t end,
                                                         const map<size_t, size_t> &id2offsets);
pair<string, pdf_object_t> get_object(const string &buffer, size_t id, const map<size_t, size_t> &id2offsets);
string extract_text(const string &doc,
                    const string &page,
                    const map<string, pair<string, pdf_object_t>> &fonts,
                    const ObjectStorage &storage,
                    const map<string, pair<string, pdf_object_t>> &decrypt_data,
                    map<unsigned int, cmap_t> &cmap_storage,
                    map<string, unsigned int> &width_storage);

bool is_prefix(const char *str, const char *pre)
{
    return strncmp(str, pre, strlen(pre)) == 0;
}

size_t get_cross_ref_offset(const string &buffer)
{
    size_t offset_start = buffer.rfind("startxref");
    if (offset_start == string::npos) throw pdf_error(FUNC_STRING + "can`t find startxref");
    offset_start += LEN("startxref");
    offset_start = skip_comments(buffer, offset_start);
    size_t offset_end = buffer.find_first_not_of("0123456789", offset_start);
    if (offset_end == string::npos) throw pdf_error(FUNC_STRING + "can`t find end of trailer offset number");
    size_t r = strict_stoul(buffer.substr(offset_start, offset_end - offset_start));
    if (r >= buffer.size())
    {
        throw pdf_error(FUNC_STRING + to_string(r) + " is larger than buffer size " + to_string(buffer.size()));
    }

    return r;
}

map<string, pair<string, pdf_object_t>> get_dict_or_indirect_dict(const pair<string, pdf_object_t> &data,
                                                                  const ObjectStorage &storage)
{
    switch (data.second)
    {
    case DICTIONARY:
        return get_dictionary_data(data.first, 0);
    case INDIRECT_OBJECT:
        return get_dictionary_data(get_indirect_dictionary(data.first, storage), 0);
    default:
        throw pdf_error(FUNC_STRING + "wrong object type " + to_string(data.second));
    }
}

void append_object(const string &buf, size_t offset, vector<size_t> &objects)
{
    if (offset + BYTE_OFFSET_LEN >= buf.length()) throw pdf_error(FUNC_STRING + "object info record is too small");
    if (buf[offset + BYTE_OFFSET_LEN] != ' ') throw pdf_error(FUNC_STRING + "no space for object info");
    objects.push_back(strict_stoul(buf.substr(offset, BYTE_OFFSET_LEN)));
}

char get_object_status(const string &buffer, size_t offset)
{
    size_t start_offset = offset + BYTE_OFFSET_LEN + GENERATION_NUMBER_LEN + 1;
    if (start_offset + 2 >= buffer.length()) throw pdf_error(FUNC_STRING + "object info record is too small");
    if (buffer[start_offset] != ' ') throw pdf_error(FUNC_STRING + "no space for object info record");
    if (strchr("\r\n ", buffer[start_offset + 2]) == NULL)
    {
        throw pdf_error(FUNC_STRING + "no newline for object info record");
    }
    char ret = buffer[start_offset + 1];
    if (ret != 'n' && ret != 'f') throw pdf_error(FUNC_STRING + "info object record status entry must be 'n' or 'f'");

    return ret;
}

size_t get_xref_number(const string &buffer, size_t &offset)
{
    offset = efind_first(buffer, "\r\t\n ", offset);
    offset = skip_spaces(buffer, offset);
    size_t end_offset = efind_first(buffer, "\r\t\n ", offset);
    size_t result = strict_stoul(buffer.substr(offset, end_offset - offset));
    offset = skip_spaces(buffer, end_offset);

    return result;
}

vector<pair<size_t, size_t>> get_trailer_offsets(const string &buffer, size_t cross_ref_offset)
{
    if (is_prefix(buffer.data() + cross_ref_offset, "xref")) return get_trailer_offsets_old(buffer, cross_ref_offset);
    return get_trailer_offsets_new(buffer, cross_ref_offset);
}

vector<pair<size_t, size_t>> get_trailer_offsets_old(const string &buffer, size_t cross_ref_offset)
{
    vector<pair<size_t, size_t>> trailer_offsets;
    while (true)
    {
        size_t end_offset;
        if ((end_offset = buffer.find("\r\nstartxref\r\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\nstartxref\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\rstartxref\r", cross_ref_offset)) == string::npos)
        {
            throw pdf_error(FUNC_STRING + "Can`t find startxref in pos: " + to_string(cross_ref_offset));
        }
        trailer_offsets.push_back(make_pair(cross_ref_offset, end_offset));
        size_t trailer_offset = efind(buffer, "trailer", cross_ref_offset);
        trailer_offset += LEN("trailer");
        map<string, pair<string, pdf_object_t>> data = get_dictionary_data(buffer, trailer_offset);
        auto it = data.find("/Prev");
        if (it == data.end()) break;
        if (it->second.second != VALUE) throw pdf_error(FUNC_STRING + "/Prev value is not PDF VALUE type");
        cross_ref_offset = strict_stoul(it->second.first);
    }

    return trailer_offsets;
}

vector<pair<size_t, size_t>> get_trailer_offsets_new(const string &buffer, size_t cross_ref_offset)
{
    vector<pair<size_t, size_t>> trailer_offsets;
    while (true)
    {
        size_t end_offset;
        if ((end_offset = buffer.find("\r\nstartxref\r\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\nstartxref\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\rstartxref\r", cross_ref_offset)) == string::npos)
        {
            throw pdf_error(FUNC_STRING + "Can`t find startxref in pos: " + to_string(cross_ref_offset));
        }
        trailer_offsets.push_back(make_pair(cross_ref_offset, end_offset));
        size_t dict_offset = efind(buffer, "<<", cross_ref_offset);
        map<string, pair<string, pdf_object_t>> data = get_dictionary_data(buffer, dict_offset);
        auto it = data.find("/Prev");
        if (it == data.end()) break;
        if (it->second.second != VALUE) throw pdf_error(FUNC_STRING + "/Prev value is not PDF VALUE type");
        cross_ref_offset = strict_stoul(it->second.first);
    }

    return trailer_offsets;
}

void get_object_offsets(const string &buffer, size_t offset, vector<size_t> &result)
{
    offset = skip_comments(buffer, offset);
    if (is_prefix(buffer.data() + offset, "xref")) return get_object_offsets_old(buffer, offset, result);
    get_object_offsets_new(buffer, offset, result);
}

array<unsigned int, 3> get_w(const map<string, pair<string, pdf_object_t>> &dictionary_data)
{
    auto it = dictionary_data.find("/W");
    if (it == dictionary_data.end()) throw pdf_error("can`t find /W");
    if (it->second.second != ARRAY) throw pdf_error("/W value must have ARRAY type");
    const string &str = it->second.first;
    if (str.at(0) != '[') throw pdf_error(FUNC_STRING + "str must be started with '['");
    if (str[str.length() - 1] != ']') throw pdf_error(FUNC_STRING + "str must be finished with ']'");
    size_t i = 0;
    array<unsigned int, 3> result;
    for (size_t offset = find_number(str, 0); offset < str.length(); offset = find_number(str, offset))
    {
        size_t end_offset = efind_first(str, " \r\n]", offset);
        result[i] = strict_stoul(str.substr(offset, end_offset - offset));
        if (result[i] > sizeof(uint64_t))
        {
            throw pdf_error(FUNC_STRING + to_string(result[i]) + " is greater than max(uint64_t)");
        }
        ++i;
        offset = end_offset;
    }
    if (i != 3) throw pdf_error(FUNC_STRING + "/W array must contain 3 elements");
    return result;
}

uint64_t get_uint64(const string &src)
{
    union
    {
        uint64_t v;
        uint8_t a[sizeof(uint64_t)];
    } v{0};

    for (int j = src.length() - 1, i = 0; j >= 0; --j, ++i) v.a[i] = static_cast<unsigned char>(src[j]);
    return v.v;
}

array<uint64_t, 3> get_cross_reference_entry(const string &stream, size_t &offset, const array<unsigned int, 3> &w)
{
    array<uint64_t, 3> result;
    for (int i = 0; i < w.size(); ++i)
    {
        if (w[i] == 0)
        {
            //no data, set default value
            //7.5.8.2Cross-Reference Stream Dictionary. table 17. W section
            switch (i)
            {
            case 0:
                result[i] = 1;
                break;
            default:
                result[i] = 0;
                break;
            }
            continue;
        }
        if (offset + w[i] > stream.length()) throw pdf_error(FUNC_STRING + "not enough data in stream for entry");
        result[i] = get_uint64(stream.substr(offset, w[i]));
        offset += w[i];
    }
//7.5.8.3. Cross-Reference Stream Data
    return result;
}

unsigned int get_cross_ref_entries(const map<string, pair<string, pdf_object_t>> dictionary_data,
                                   const array<unsigned int, 3> &w,
                                   size_t length)
{
    auto it = dictionary_data.find("/Index");
    if (it == dictionary_data.end())
    {
        const pair<string, pdf_object_t> &val = dictionary_data.at("/Size");
        if (val.second != VALUE) throw pdf_error(FUNC_STRING + "/Size must have VALUE type");
        return strict_stoul(val.first);
    }
    const string &array = it->second.first;
    if (it->second.second != ARRAY) throw pdf_error("/Index must be ARRAY");
    if (count(array.begin(), array.end(), ']') != 1) throw pdf_error("/Index must be one dimensional array");
    unsigned int entries = 0;
    for (size_t offset = find_number(array, 0); offset < array.length(); offset = find_number(array, offset))
    {
        offset = efind_number(array, efind_first(array, " \r\t\n", offset));
        size_t end_offset = efind_first(array, " \r\t\n]", offset);
        entries += strict_stoul(array.substr(offset, end_offset - offset));
        if (array[end_offset] == ']') return entries;
        offset = end_offset;
    }
    return entries;
}

void get_offsets_internal_new(const string &stream,
                              const map<string, pair<string, pdf_object_t>> dictionary_data,
                              vector<size_t> &result)
{
    //7.5.8.3. Cross-Reference Stream Data
    array<unsigned int, 3> w = get_w(dictionary_data);
    size_t offset = 0;
    for (unsigned int i = 0, n = get_cross_ref_entries(dictionary_data, w, stream.length()); i < n; ++i)
    {
        array<uint64_t, 3> entry = get_cross_reference_entry(stream, offset, w);
        if (entry[0] == 1) result.push_back(entry[1]);
    }
}

void get_object_offsets_new(const string &buffer, size_t offset, vector<size_t> &result)
{
    offset = efind(buffer, "<<", offset);
    string dict = get_dictionary(buffer, offset);
    map<string, pair<string, pdf_object_t>> dictionary_data = get_dictionary_data(dict, 0);
    auto it = dictionary_data.find("/Length");
    if (it == dictionary_data.end()) throw pdf_error("can`t find /Length");
    if (it->second.second != VALUE) throw pdf_error("/Length value must have VALUE type");
    size_t length = strict_stoul(it->second.first);
    string content = get_content(buffer, length, offset);
    content = decode(content, dictionary_data);
    get_offsets_internal_new(content, dictionary_data, result);
}

void get_object_offsets_old(const string &buffer, size_t offset, vector<size_t> &result)
{
    offset = efind(buffer, "xref", offset);
    offset += LEN("xref");
    while (true)
    {
        offset = skip_comments(buffer, offset);
        if (is_prefix(buffer.data() + offset, "trailer")) return;
        size_t n = get_xref_number(buffer, offset);
        for (size_t i = 0 ; i < n; offset += CROSS_REFERENCE_LINE_SIZE, ++i)
        {
            offset = skip_comments(buffer, offset);
            if (get_object_status(buffer, offset) == 'n') append_object(buffer, offset, result);
        }
    }
}

void validate_offsets(const string &buffer, const vector<size_t> &offsets)
{
    for (size_t offset : offsets)
    {
        if (offset >= buffer.size()) throw pdf_error(FUNC_STRING + "offset is greater than pdf buffer");
    }
}

vector<size_t> get_all_object_offsets(const string &buffer,
                                      size_t cross_ref_offset,
                                      const vector<pair<size_t, size_t>> &trailer_offsets)
{
    vector<size_t> object_offsets;
    for (const pair<size_t, size_t> &p : trailer_offsets)
    {
        get_object_offsets(buffer, p.first, object_offsets);
    }
    validate_offsets(buffer, object_offsets);

    return object_offsets;
}

map<size_t, size_t> get_id2offsets(const string &buffer,
                                   size_t cross_ref_offset,
                                   const vector<pair<size_t, size_t>> &trailer_offsets)
{
    vector<size_t> offsets = get_all_object_offsets(buffer, cross_ref_offset, trailer_offsets);
    map<size_t, size_t> id2offsets;
    for (size_t offset : offsets)
    {
        size_t start_offset = efind_number(buffer, skip_comments(buffer, offset));
        size_t end_offset = efind_first(buffer, " \r\n\t", start_offset);
        id2offsets.insert(make_pair(strict_stoul(buffer.substr(start_offset, end_offset - start_offset)), offset));
    }

    return id2offsets;
}

void get_pages_id_fonts_int(const map<string, pair<string, pdf_object_t>> &parent_dict,
                            const map<string, pair<string, pdf_object_t>> &parent_fonts,
                            const ObjectStorage &storage,
                            pages_id_resources_t &result)
{
    auto it = parent_dict.find("/Type");
    if (it == parent_dict.end() || it->second.first != "/Pages") return;
    pair<string, pdf_object_t> kids = parent_dict.at("/Kids");
    if (kids.second != ARRAY) throw pdf_error(FUNC_STRING + "/Kids is not array");

    for (const pair<unsigned int, unsigned int> &page : get_set(kids.first))
    {
        unsigned int id = page.first;
        //avoid infinite recursion for 'bad' pdf
        if (find_if(result.begin(), result.end(),
                    [id](const pages_id_resources_t::value_type &v){ return id == v.first; }) == result.end())
        {
            const pair<string, pdf_object_t> page_dict = storage.get_object(id);
            if (page_dict.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
            const map<string, pair<string, pdf_object_t>> dict_data = get_dictionary_data(page_dict.first, 0);
            const map<string, pair<string, pdf_object_t>> fonts = get_fonts(dict_data, storage, parent_fonts);
            result.push_back(make_pair(id, fonts));
            get_pages_id_fonts_int(dict_data, fonts, storage, result);
        }
    }
}

map<string, pair<string, pdf_object_t>> get_fonts(const map<string, pair<string, pdf_object_t>> &dictionary,
                                                  const ObjectStorage &storage,
                                                  const map<string, pair<string, pdf_object_t>> &parent_fonts)
{
    auto it = dictionary.find("/Resources");
    if (it == dictionary.end()) return parent_fonts;
    const map<string, pair<string, pdf_object_t>> resources = get_dict_or_indirect_dict(it->second, storage);
    it = resources.find("/Font");
    if (it == resources.end()) return map<string, pair<string, pdf_object_t>>();
    return get_dict_or_indirect_dict(it->second, storage);
}

pages_id_resources_t get_pages_id_fonts(unsigned int catalog_pages_id, const ObjectStorage &storage)
{
    const pair<string, pdf_object_t> catalog_pair = storage.get_object(catalog_pages_id);
    if (catalog_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "catalog must be DICTIONARY");
    const map<string, pair<string, pdf_object_t>> data = get_dictionary_data(catalog_pair.first, 0);
    auto it = data.find("/Type");
    if (it == data.end() || it->second.first != "/Pages")
    {
        throw pdf_error("In root catalog type must be '/Type /Pages'");
    }
    //due to PDF official documentation /Resources is required;inheritable(Table 3.8) and must be dictionary
    //Tolerate this rule: root /Resources always exists
    const map<string, pair<string, pdf_object_t>> fonts = get_fonts(data,
                                                                    storage,
                                                                    map<string, pair<string, pdf_object_t>>());
    pages_id_resources_t result;
    get_pages_id_fonts_int(data, fonts, storage, result);

    return result;
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

string get_text(const string &buffer,
                size_t cross_ref_offset,
                const ObjectStorage &storage,
                map<string, pair<string, pdf_object_t>> &decrypt_data)
{
    size_t trailer_offset = cross_ref_offset;
    if (is_prefix(buffer.data() + cross_ref_offset, "xref"))
    {
        trailer_offset = efind(buffer, "trailer", trailer_offset);
        trailer_offset += LEN("trailer");
    }
    const map<string, pair<string, pdf_object_t>> trailer_data = get_dictionary_data(buffer, trailer_offset);
    const pair<string, pdf_object_t> root_pair = trailer_data.at("/Root");
    if (root_pair.second != INDIRECT_OBJECT) throw pdf_error(FUNC_STRING + "/Root value must be INDIRECT_OBJECT");
    const pair<string, pdf_object_t> real_root_pair = storage.get_object(get_id_gen(root_pair.first).first);
    if (real_root_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "/Root indirect object must be a dictionary");

    const map<string, pair<string, pdf_object_t>> root_data = get_dictionary_data(real_root_pair.first, 0);
    const pair<string, pdf_object_t> pages_pair = root_data.at("/Pages");
    if (pages_pair.second != INDIRECT_OBJECT) throw pdf_error(FUNC_STRING + "/Pages value must be INDRECT_OBJECT");

    unsigned int catalog_pages_id = get_id_gen(pages_pair.first).first;
    const pages_id_resources_t pages_id_resources = get_pages_id_fonts(catalog_pages_id, storage);
    string result;
    map<unsigned int, cmap_t> cmap_storage;
    map<string, unsigned int> width_storage;
    for (const pages_id_resources_t::value_type &page_id_resource : pages_id_resources)
    {
        unsigned int page_id = page_id_resource.first;
        vector<pair<unsigned int, unsigned int>> contents_id_gen = get_contents_id_gen(storage.get_object(page_id));
        string page_content;
        for (const pair<unsigned int, unsigned int> &id_gen : contents_id_gen)
        {
            page_content += output_content(buffer, storage, id_gen, decrypt_data);
        }
        result += extract_text(buffer,
                               page_content,
                               page_id_resource.second,
                               storage, decrypt_data,
                               cmap_storage,
                               width_storage);
    }

    return result;
}

pair<pdf_object_t, string> pop(stack<pair<pdf_object_t, string>> &st)
{
    if (st.empty()) throw pdf_error(FUNC_STRING + "stack is empty");
    pair<pdf_object_t, string> result = st.top();
    st.pop();
    return result;
}

unique_ptr<CharsetConverter> get_font_encoding(const string &doc,
                                               const string &font,
                                               const map<string, pair<string, pdf_object_t>> &fonts,
                                               const ObjectStorage &storage,
                                               const map<string, pair<string, pdf_object_t>> &decrypt_data,
                                               map<unsigned int, cmap_t> &cmap_storage,
                                               map<string, unsigned int> &width_storage)
{
    auto it = fonts.find(font);
    if (it == fonts.end()) return unique_ptr<CharsetConverter>(new CharsetConverter());

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

    it = font_dict.find("/ToUnicode");
    if (it != font_dict.end())
    {
        const pair<unsigned int, unsigned int> cmap_id_gen = get_id_gen(it->second.first);
        if (cmap_storage.count(cmap_id_gen.first) == 0)
        {
            cmap_storage.insert(make_pair(cmap_id_gen.first, get_cmap(doc, storage, cmap_id_gen, decrypt_data)));
        }
        return unique_ptr<CharsetConverter>(new CharsetConverter(&cmap_storage[cmap_id_gen.first], width));
    }
    it = font_dict.find("/Encoding");
    if (it == font_dict.end()) return unique_ptr<CharsetConverter>(new CharsetConverter());
    string encoding;
    switch (it->second.second)
    {
    case DICTIONARY:
    case INDIRECT_OBJECT:
        return CharsetConverter::get_from_dictionary(get_dict_or_indirect_dict(it->second, storage), width);
    case NAME_OBJECT:
        return unique_ptr<CharsetConverter>(new CharsetConverter(it->second.first, width));
    default:
        throw pdf_error(FUNC_STRING + "wrong /Encoding type: " + to_string(it->second.second));
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

string extract_text(const string &doc,
                    const string &page,
                    const map<string, pair<string, pdf_object_t>> &fonts,
                    const ObjectStorage &storage,
                    const map<string, pair<string, pdf_object_t>> &decrypt_data,
                    map<unsigned int, cmap_t> &cmap_storage,
                    map<string, unsigned int> &width_storage)
{
    unique_ptr<CharsetConverter> encoding(new CharsetConverter());
    string result;
    stack<pair<pdf_object_t, string>> st;
    bool in_text_block = false;
    for (size_t i = 0; i < page.length();)
    {
        i = skip_spaces(page, i, false);
        if (in_text_block && put2stack(st, page, i)) continue;
        size_t end = page.find_first_of(" \r\n\t/[(<", i + 1);
        if (end == string::npos) end = page.length();
        const string token = page.substr(i, end - i);
        i = end;
        if (token == "BT")
        {
            in_text_block = true;
            continue;
        }
        if (token == "ET")
        {
            in_text_block = false;
            continue;
        }
        if (!in_text_block) continue;
        if (token == "Tj")
        {
            const pair<pdf_object_t, string> el = pop(st);
            //wrong arg for Tj operator skipping..
            if (el.first != STRING) continue;
            result += encoding->get_string(decode_string(el.second));
        }
        else if (token == "'" || token == "\"")
        {
            const pair<pdf_object_t, string> el = pop(st);
            //wrong arg for '" operators skipping..
            if (el.first != STRING) continue;
            result += '\n' + encoding->get_string(decode_string(el.second));
        }
        else if (token == "TJ")
        {
            const pair<pdf_object_t, string> el = pop(st);
            //wrong arg for TJ operator skipping..
            if (el.first != ARRAY) continue;
            result += '\n' + encoding->get_strings_from_array(el.second);
        }
        else if (token == "T*")
        {
            result += '\n';
        }
        else if (token == "Td" || token == "TD")
        {
            string number_str = get_int(pop(st).second);
            if (strict_stol(number_str) > 0) result += '\n';
        }
        else if (token == "Tf")
        {
            pop(st);
            encoding = get_font_encoding(doc, pop(st).second, fonts, storage, decrypt_data, cmap_storage, width_storage);
        }
        else
        {
            st.push(make_pair(VALUE, token));
        }
    }
    return result;
}

string output_content(const string &buffer,
                      const ObjectStorage &storage,
                      const pair<unsigned int, unsigned int> &id_gen,
                      const map<string, pair<string, pdf_object_t>> &decrypt_data)
{
    const pair<string, pdf_object_t> content_pair = storage.get_object(id_gen.first);
    if (content_pair.second == ARRAY)
    {
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

pair<string, pair<string, pdf_object_t>> get_id(const string &buffer, size_t start, size_t end)
{
    size_t off = efind(buffer, "/ID", start);
    if (off >= end) throw pdf_error(FUNC_STRING + "Can`t find /ID key");
    off = efind(buffer, '[', off);
    if (off >= end) throw pdf_error(FUNC_STRING + "Can`t find /ID value");

    return make_pair(string("/ID"), make_pair(get_array(buffer, off), ARRAY));
}

map<string, pair<string, pdf_object_t>> get_encrypt_data(const string &buffer,
                                                         size_t start,
                                                         size_t end,
                                                         const map<size_t, size_t> &id2offsets)
{
    size_t off = buffer.find("/Encrypt", start);
    if (off == string::npos || off >= end) return map<string, pair<string, pdf_object_t>>();
    off += LEN("/Encrypt");
    pdf_object_t type = get_object_type(buffer, off);
    map<string, pair<string, pdf_object_t>> result;
    switch (type)
    {
    case DICTIONARY:
    {
        result = get_dictionary_data(buffer, off);
        break;
    }
    case INDIRECT_OBJECT:
    {
        size_t end_off = efind_first(buffer, "\r\t\n ", off);
        const pair<string, pdf_object_t> encrypt_pair = get_object(buffer,
                                                                   strict_stoul(buffer.substr(off, end_off - off)),
                                                                   id2offsets);
        if (encrypt_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "Encrypt indirect object must be DICTIONARY");
        result = get_dictionary_data(encrypt_pair.first, 0);
        break;
    }
    default:
    {
        throw pdf_error(FUNC_STRING + "wrong /Encrypt value: " + to_string(type));
        break;
    }
    }
    result.insert(get_id(buffer, start, end));

    return result;
}

string pdf2txt(const string &buffer)
{
    size_t cross_ref_offset = get_cross_ref_offset(buffer);
    vector<pair<size_t, size_t>> trailer_offsets = get_trailer_offsets(buffer, cross_ref_offset);
    map<size_t, size_t> id2offsets = get_id2offsets(buffer, cross_ref_offset, trailer_offsets);
    map<string, pair<string, pdf_object_t>> encrypt_data = get_encrypt_data(buffer,
                                                                            trailer_offsets.at(0).first,
                                                                            trailer_offsets.at(0).second,
                                                                            id2offsets);
    ObjectStorage storage(buffer, move(id2offsets), encrypt_data);
    return get_text(buffer, cross_ref_offset, storage, encrypt_data);
}

int main(int argc, char *argv[])
{
    std::ifstream t(argv[1]);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    cout << pdf2txt(str);

    return 0;
}
