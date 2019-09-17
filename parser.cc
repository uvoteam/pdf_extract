#include <string>
#include <cstring>
#include <map>
#include <utility>
#include <vector>
#include <algorithm>
#include <cstdint>

//for test
#include <fstream>
#include <iostream>


#include "pdf_internal.h"

using namespace std;

extern string flate_decode(const string&, const map<string, pair<string, pdf_object_t>>&);
extern string lzw_decode(const string&, const map<string, pair<string, pdf_object_t>>&);
extern string ascii85_decode(const string&, const map<string, pair<string, pdf_object_t>>&);
extern string ascii_hex_decode(const string&, const map<string, pair<string, pdf_object_t>>&);
extern string decrypt(unsigned int n,
                      unsigned int g,
                      const string &in,
                      const map<string, pair<string, pdf_object_t>> &decrypt_opts);


enum {SMALLEST_PDF_SIZE = 67 /*https://stackoverflow.com/questions/17279712/what-is-the-smallest-possible-valid-pdf*/,
      CROSS_REFERENCE_LINE_SIZE = 20,
      BYTE_OFFSET_LEN = 10, /* length for byte offset in cross reference record */
      GENERATION_NUMBER_LEN = 5 /* length for generation number */
};

class ObjectStorage;

bool is_prefix(const char *str, const char *pre);
uint64_t get_uint64(const string &src);
array<uint64_t, 3> get_cross_reference_entry(const string &stream, size_t &offset, const array<unsigned int, 3> &w);
pair<unsigned int, unsigned int> get_id_gen(const string &data);
void get_offsets_internal_new(const string &stream,
                              const map<string, pair<string, pdf_object_t>> dictionary_data,
                              vector<size_t> &result);
bool is_blank(char c);
vector<map<string, pair<string, pdf_object_t>>> get_decode_params(const map<string, pair<string, pdf_object_t>> &src,
                                                                  size_t filters);
size_t skip_comments(const string &buffer, size_t offset);
size_t get_cross_ref_offset(const string &buffer);
pair<string, pair<string, pdf_object_t>> get_id(const string &buffer, size_t start, size_t end);
void append_object(const string &buf, size_t offset, vector<size_t> &objects);
char get_object_status(const string &buffer, size_t offset);
size_t get_xref_number(const string &buffer, size_t &offset);
vector<pair<size_t, size_t>> get_trailer_offsets(const string &buffer, size_t cross_ref_offset);
vector<pair<size_t, size_t>> get_trailer_offsets_old(const string &buffer, size_t cross_ref_offset);
vector<pair<size_t, size_t>> get_trailer_offsets_new(const string &buffer, size_t cross_ref_offset);
void get_object_offsets(const string &buffer, size_t offset, vector<size_t> &result);
void get_object_offsets_new(const string &buffer, size_t offset, vector<size_t> &result);
void get_object_offsets_old(const string &buffer, size_t offset, vector<size_t> &result);
vector<pair<unsigned int, unsigned int>> get_contents_id_gen(const string &buffer,
                                                             size_t cross_ref_offset,
                                                             const ObjectStorage &storage);
void validate_offsets(const string &buffer, const vector<size_t> &offsets);
vector<size_t> get_all_object_offsets(const string &buffer,
                                      size_t cross_ref_offset,
                                      const vector<pair<size_t, size_t>> &trailer_offsets);
map<size_t, size_t> get_id2offsets(const string &buffer,
                                   size_t cross_ref_offset,
                                   const vector<pair<size_t, size_t>> &trailer_offsets);
size_t find_number(const string &buffer, size_t offset);
size_t efind_number(const string &buffer, size_t offset);
void append_set(const string &array, vector<pair<unsigned int, unsigned int>> &result);
unsigned int get_cross_ref_entries(const map<string, pair<string, pdf_object_t>> dictionary_data,
                                   const array<unsigned int, 3> &w, size_t length);
array<unsigned int, 3> get_w(const map<string, pair<string, pdf_object_t>> &dictionary_data);
void get_pages_id_gen_int(const string &buffer,
                          const pair<unsigned int, unsigned int> &page,
                          const ObjectStorage &storage,
                          vector<pair<unsigned int, unsigned int>> &result);
vector<pair<unsigned int, unsigned int>> get_pages_id_gen(const string &buffer,
                                                          const pair<unsigned int, unsigned int> &catalog_pages_id_gen,
                                                          const ObjectStorage &storage);
void append_content_offsets(const pair<string, pdf_object_t> &page_pair,
                            vector<pair<unsigned int, unsigned int>> &content_offsets);
size_t get_content_len(const ObjectStorage &storage, const map<string, pair<string, pdf_object_t>> &props);
string get_content(const string &buffer, size_t len, size_t offset);
vector<string> get_filters(const map<string, pair<string, pdf_object_t>> &props);
void decode(string &content,
            const vector<string> &filters,
            const vector<map<string, pair<string, pdf_object_t>>> &decode_params);
string output_content(const string &buffer,
                      const ObjectStorage &storage,
                      const pair<unsigned int, unsigned int> &id_gen,
                      const map<string, pair<string, pdf_object_t>> &encrypt_data);
map<string, pair<string, pdf_object_t>> get_encrypt_data(const string &buffer,
                                                         size_t start,
                                                         size_t end,
                                                         const map<size_t, size_t> &id2offsets);
pair<string, pdf_object_t> get_object(const string &buffer, size_t id, const map<size_t, size_t> &id2offsets);

const map<string, string (&)(const string&, const map<string, pair<string, pdf_object_t>>&)> FILTER2FUNC =
                                                           {{"/FlateDecode", flate_decode},
                                                            {"/LZWDecode", lzw_decode},
                                                            {"/ASCII85Decode", ascii85_decode},
                                                            {"/ASCIIHexDecode", ascii_hex_decode}};

class ObjectStorage
{
public:
    ObjectStorage(const string &doc_arg,
                  map<size_t, size_t> &&id2offsets_arg,
                  const map<string, pair<string, pdf_object_t>> &encrypt_data_arg) :
                  doc(doc_arg), id2offsets(move(id2offsets_arg)), encrypt_data(encrypt_data_arg)
    {
        for (const pair<size_t, size_t> &p : id2offsets) insert_obj_stream(p.first);
    }

    size_t get_offset(size_t id) const
    {
        return id2offsets.at(id);
    }

    pair<string, pdf_object_t> get_object(size_t id) const
    {
        auto it = id2offsets.find(id);
        //object is located inside object stream
        if (it == id2offsets.end()) return id2obj_stm.at(id);
        return ::get_object(doc, id, id2offsets);
    }
private:
    size_t get_gen_id(size_t offset)
    {
        offset = efind_first(doc, " \r\t\n", offset);
        offset = efind_number(doc, offset);
        size_t end_offset = efind_first(doc, " \r\t\n", offset);
        return strict_stoul(doc.substr(offset, end_offset - offset));
    }

    void insert_obj_stream(size_t id)
    {
        size_t offset = id2offsets.at(id);
        offset = skip_comments(doc, offset);
        size_t gen_id = get_gen_id(offset);
        offset = skip_comments(doc, offset);
        if (doc.at(offset) != '<' && doc.at(offset + 1) != '<') return;
        map<string, pair<string, pdf_object_t>> dictionary = get_dictionary_data(get_dictionary(doc, offset), 0);
        auto it = dictionary.find("/Type");
        if (it == dictionary.end() || it->second.first != "/ObjStm") return;
        unsigned int len = strict_stoul(dictionary.at("/Length").first);
        string content = get_content(doc, len, offset);
        content = decrypt(id, gen_id, content, encrypt_data);
        if (dictionary.count("/Filter") == 1)
        {
            vector<string> filters = get_filters(dictionary);
            vector<map<string, pair<string, pdf_object_t>>> decode_params = get_decode_params(dictionary, filters.size());
            decode(content, filters, decode_params);
        }
        vector<pair<size_t, size_t>> id2offsets_obj_stm = get_id2offsets_obj_stm(content, dictionary);
        offset = strict_stoul(dictionary.at("/First").first);
        for (const pair<size_t, size_t> &p : id2offsets_obj_stm)
        {
            size_t obj_offset = offset + p.second;
            pdf_object_t type = get_object_type(content, obj_offset);
            id2obj_stm.insert(make_pair(p.first, make_pair(TYPE2FUNC.at(type)(content, obj_offset), type)));
        }
    }

    vector<pair<size_t, size_t>> get_id2offsets_obj_stm(const string &content,
                                                        const map<string, pair<string, pdf_object_t>> &dictionary)

    {
        vector<pair<size_t, size_t>> result;
        size_t offset = 0;
        unsigned int n = strict_stoul(dictionary.at("/N").first);
        for (unsigned int i = 0; i < n; ++i)
        {
            offset = efind_number(content, offset);
            size_t end_offset = efind_first(content, " \r\n", offset);
            unsigned int id = strict_stoul(content.substr(offset, end_offset - offset));
            offset = efind_number(content, end_offset);
            end_offset = efind_first(content, " \r\n", offset);
            unsigned int obj_off = strict_stoul(content.substr(offset, end_offset - offset));
            result.push_back(make_pair(id, obj_off));
            offset = end_offset;
        }

        return result;
    }

private:
    map<size_t, size_t> id2offsets;
    //7.5.7. Object Streams
    map<size_t, pair<string, pdf_object_t>> id2obj_stm;
    const string &doc;
    const map<string, pair<string, pdf_object_t>> &encrypt_data;
};

bool is_prefix(const char *str, const char *pre)
{
    return strncmp(str, pre, strlen(pre)) == 0;
}

size_t skip_comments(const string &buffer, size_t offset)
{
    while (true)
    {
        offset = skip_spaces(buffer, offset);
        if (buffer[offset] != '%') return offset;
        while (offset < buffer.length() && buffer[offset] != '\r' && buffer[offset] != '\n') ++offset;
    }
}

pair<string, pdf_object_t> get_object(const string &buffer, size_t id, const map<size_t, size_t> &id2offsets)
{
    size_t offset = id2offsets.at(id);
    offset = skip_comments(buffer, offset);
    offset = efind(buffer, "obj", id2offsets.at(id));
    offset += LEN("obj");
    offset = skip_comments(buffer, offset);
    pdf_object_t type = get_object_type(buffer, offset);
    return make_pair(TYPE2FUNC.at(type)(buffer, offset), type);
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
    uint64_t val = 0;
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);
    for (int j = src.length() - 1; j >= 0; --j, ++p) *p = static_cast<unsigned char>(src[j]);
    return val;
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
    for (size_t offset = find_number(array, 0); offset < array.length(); ++offset)
    {
        offset = efind_number(array, efind_first(array, " \r\t\n", offset));
        size_t end_offset = efind_first(array, " \r\t\n]", offset);
        entries += strict_stoul(array.substr(offset, end_offset - offset));
        if (array[end_offset] == ']') return entries;
        offset = end_offset;
    }
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
    if (dictionary_data.count("/Filter") == 1)
    {
        vector<string> filters = get_filters(dictionary_data);
        vector<map<string, pair<string, pdf_object_t>>> decode_params = get_decode_params(dictionary_data, filters.size());
        decode(content, filters, decode_params);
    }
    get_offsets_internal_new(content, dictionary_data, result);
}

void get_object_offsets_old(const string &buffer, size_t offset, vector<size_t> &result)
{
    offset = efind(buffer, "xref", offset);
    offset += LEN("xref");
    while (true)
    {
        offset = skip_spaces(buffer, offset);
        if (is_prefix(buffer.data() + offset, "trailer")) return;
        size_t n = get_xref_number(buffer, offset);
        size_t end_offset = offset + n * CROSS_REFERENCE_LINE_SIZE;
        if (end_offset >= buffer.length()) throw pdf_error(FUNC_STRING + "pdf buffer has no data for indirect objects info");
        for ( ; offset < end_offset; offset += CROSS_REFERENCE_LINE_SIZE)
        {
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

size_t efind_number(const string &buffer, size_t offset)
{
    size_t result = find_number(buffer, offset);
    if (result >= buffer.length()) throw pdf_error(FUNC_STRING + "can`t find number");
    return result;
}

size_t find_number(const string &buffer, size_t offset)
{
    while (offset < buffer.length() && (strchr("0123456789", buffer[offset]) == NULL)) ++offset;
    return offset;
}

void append_set(const string &array, vector<pair<unsigned int, unsigned int>> &result)
{
    for (size_t offset = find_number(array, 0); offset < array.length(); offset = find_number(array, offset))
    {
        size_t end_offset = efind_first(array, "  \r\n\t", offset);
        unsigned int id = strict_stoul(array.substr(offset, end_offset - offset));
        offset = efind_number(array, end_offset);
        end_offset = efind_first(array, "  \r\n\t", offset);
        unsigned int gen = strict_stoul(array.substr(offset, end_offset - offset));
        result.push_back(make_pair(id, gen));
        offset = efind(array, 'R', end_offset);
    }
}

void get_pages_id_gen_int(const string &buffer,
                          const pair<unsigned int, unsigned int> &parent_page,
                          const ObjectStorage &storage,
                          vector<pair<unsigned int, unsigned int>> &result)
{
    const pair<string, pdf_object_t> parent_page_pair = storage.get_object(parent_page.first);
    if (parent_page_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be dictionary");
    map<string, pair<string, pdf_object_t>> data = get_dictionary_data(parent_page_pair.first, 0);
    auto it = data.find("/Type");
    if (it == data.end() || it->second.first != "/Pages") return;
    pair<string, pdf_object_t> p = data.at("/Kids");
    if (p.second != ARRAY) throw pdf_error(FUNC_STRING + "/Kids is not array");
    string kids_string = p.first;
    vector<pair<unsigned int, unsigned int>> pages;
    append_set(kids_string, pages);
    for (const pair<unsigned int, unsigned int> &page : pages)
    {
        //avoid infinite recursion for 'bad' pdf
        if (find(result.begin(), result.end(), page) == result.end())
        {
            get_pages_id_gen_int(buffer, page, storage, result);
            result.insert(result.end(), pages.begin(), pages.end());
        }
    }
}

vector<pair<unsigned int, unsigned int>> get_pages_id_gen(const string &buffer,
                                                          const pair<unsigned int, unsigned int> &catalog_pages_id_gen,
                                                          const ObjectStorage &storage)
{
    const pair<string, pdf_object_t> catalog_pair = storage.get_object(catalog_pages_id_gen.first);
    if (catalog_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "catalog must be DICTIONARY");
    map<string, pair<string, pdf_object_t>> data = get_dictionary_data(catalog_pair.first, 0);
    auto it = data.find("/Type");
    if (it == data.end() || it->second.first != "/Pages")
    {
        throw pdf_error("In root catalog type must be '/Type /Pages'");
    }
    vector<pair<unsigned int, unsigned int>> result;
    get_pages_id_gen_int(buffer, catalog_pages_id_gen, storage, result);

    return result;
}

pair<unsigned int, unsigned int> get_id_gen(const string &data)
{
    size_t offset = 0;
    size_t end_offset = efind_first(data, "\r\t\n ", offset);
    unsigned int id = strict_stoul(data.substr(offset, end_offset - offset));
    offset = efind_number(data, end_offset);
    end_offset = efind_first(data, "\r\t\n ", offset);
    unsigned int gen = strict_stoul(data.substr(offset, end_offset - offset));
    return make_pair(id, gen);
}

void append_content_offsets(const pair<string, pdf_object_t> &page_pair,
                            vector<pair<unsigned int, unsigned int>> &content_offsets)
{
    if (page_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "page must be DICTIONARY");
    const map<string, pair<string, pdf_object_t>> data = get_dictionary_data(page_pair.first, 0);
    auto it = data.find("/Contents");
    // "/Contents" key can be absent for Page. In this case Page is empty
    if (it == data.end()) return;
    const string &contents_data = it->second.first;
    switch (it->second.second)
    {
    case ARRAY:
        append_set(contents_data, content_offsets);
        break;
    case INDIRECT_OBJECT:
        content_offsets.push_back(get_id_gen(contents_data));
        break;
    default:
        throw pdf_error(FUNC_STRING + "/Contents type must be ARRAY or INDIRECT_OBJECT");
        break;
    }
}

vector<pair<unsigned int, unsigned int>> get_contents_id_gen(const string &buffer,
                                                             size_t cross_ref_offset,
                                                             const ObjectStorage &storage)
{
    vector<pair<unsigned int, unsigned int>> result;
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

    const pair<unsigned int, unsigned int> catalog_pages_data = get_id_gen(pages_pair.first);
    vector<pair<unsigned int, unsigned int>> pages_id_gen = get_pages_id_gen(buffer, catalog_pages_data, storage);
    for (const pair<unsigned int, unsigned int> &id_gen : pages_id_gen)
    {
        append_content_offsets(storage.get_object(id_gen.first), result);
    }

    return result;
}


size_t get_content_len(const ObjectStorage &storage, const map<string, pair<string, pdf_object_t>> &props)
{
    const pair<string, pdf_object_t> &r = props.at("/Length");
    if (r.second == VALUE)
    {
        return strict_stoul(r.first);
    }
    else if (r.second == INDIRECT_OBJECT)
    {
        size_t id = strict_stoul(r.first.substr(0, efind_first(r.first, " \r\n\t", 0)));
        const pair<string, pdf_object_t> content_len_pair = storage.get_object(id);
        if (content_len_pair.second != VALUE) throw pdf_error(FUNC_STRING + "content len indirect obj must be VALUE");
        return strict_stoul(content_len_pair.first);
    }
    else
    {
        throw pdf_error(FUNC_STRING + " wrong type for /Length");
    }
}

string get_content(const string &buffer, size_t len, size_t offset)
{
    offset = efind(buffer, "stream", offset);
    offset += LEN("stream");
    if (buffer[offset] == '\r') ++offset;
    if (buffer[offset] == '\n') ++offset;
    return buffer.substr(offset, len);
}

vector<string> get_filters(const map<string, pair<string, pdf_object_t>> &props)
{
    const pair<string, pdf_object_t> filters = props.at("/Filter");
    if (filters.second == NAME_OBJECT) return vector<string>{filters.first};
    if (filters.second != ARRAY) throw pdf_error(FUNC_STRING + "wrong filter type: " + to_string(filters.second));
    vector<string> result;
    const string &body = filters.first;
    if (body.at(0) != '[') throw pdf_error(FUNC_STRING + "filter body array must start with '['. Input: " + body);
    size_t offset = 1;
    while (true)
    {
        offset = skip_spaces(body, offset);
        if (body[offset] == ']') return result;
        size_t end_offset = efind_first(body, "\r\n\t ]", offset);
        result.push_back(body.substr(offset, end_offset - offset));
        offset = end_offset;
    }
}

void decode(string &content, const vector<string> &filters,
            const vector<map<string, pair<string, pdf_object_t>>> &decode_params)
{
    for (size_t i = 0; i < filters.size(); ++i) content = FILTER2FUNC.at(filters[i])(content, decode_params[i]);
}

vector<map<string, pair<string, pdf_object_t>>> get_decode_params(const map<string, pair<string, pdf_object_t>> &src,
                                                                  size_t filters)
{
    auto it = src.find("/DecodeParms");
    //default decode params for all filters
    if (it == src.end()) return vector<map<string, pair<string, pdf_object_t>>>(filters);
    const string &params_data = it->second.first;
    if (it->second.second == DICTIONARY)
    {
        return vector<map<string, pair<string, pdf_object_t>>>{get_dictionary_data(params_data, 0)};
    }
    if (it->second.second != ARRAY) throw pdf_error(FUNC_STRING + "wrong type for /DecodeParms");
    vector<map<string, pair<string, pdf_object_t>>> result;
    size_t offset = 0;
    while (true)
    {
        offset = params_data.find("<<");
        if (offset == string::npos) return result;
        result.push_back(get_dictionary_data(get_dictionary(params_data, offset), 0));
    }
    return result;
}

string output_content(const string &buffer,
                      const ObjectStorage &storage,
                      const pair<unsigned int, unsigned int> &id_gen,
                      const map<string, pair<string, pdf_object_t>> &encrypt_data)
{
    const pair<string, pdf_object_t> content_pair = storage.get_object(id_gen.first);
    if (content_pair.second == ARRAY)
    {
        vector<pair<unsigned int, unsigned int>> contents;
        append_set(content_pair.first, contents);
        string result;
        for (const pair<unsigned int, unsigned int> &p : contents)
        {
            result += output_content(buffer, storage, p, encrypt_data);
        }
        return result;
    }
    if (content_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "content must be dictionary");
    const map<string, pair<string, pdf_object_t>> props = get_dictionary_data(content_pair.first, 0);
    string content = get_content(buffer, get_content_len(storage, props), storage.get_offset(id_gen.first));
    content = decrypt(id_gen.first, id_gen.second, content, encrypt_data);
    if (props.count("/Filter") == 1)
    {
        vector<string> filters = get_filters(props);
        vector<map<string, pair<string, pdf_object_t>>> decode_params = get_decode_params(props, filters.size());
        if (filters.size() != decode_params.size())
        {
            throw pdf_error(FUNC_STRING + "different sizes for filters and decode_params");
        }
        decode(content, filters, decode_params);
    }

    return content;
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
    if (buffer.size() < SMALLEST_PDF_SIZE) throw pdf_error(FUNC_STRING + "pdf buffer is too small");
    size_t cross_ref_offset = get_cross_ref_offset(buffer);
    vector<pair<size_t, size_t>> trailer_offsets = get_trailer_offsets(buffer, cross_ref_offset);
    map<size_t, size_t> id2offsets = get_id2offsets(buffer, cross_ref_offset, trailer_offsets);
    map<string, pair<string, pdf_object_t>> encrypt_data = get_encrypt_data(buffer,
                                                                            trailer_offsets.at(0).first,
                                                                            trailer_offsets.at(0).second,
                                                                            id2offsets);
    ObjectStorage storage(buffer, move(id2offsets), encrypt_data);
    vector<pair<unsigned int, unsigned int>> contents_id_gen = get_contents_id_gen(buffer, cross_ref_offset, storage);
    string result;
    for (const pair<unsigned int, unsigned int> &id_gen : contents_id_gen)
    {
        result += output_content(buffer, storage, id_gen, encrypt_data);
    }
    cout << result;
    return result;
}

int main(int argc, char *argv[])
{
    std::ifstream t(argv[1]);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    pdf2txt(str);
    
    return 0;
}
