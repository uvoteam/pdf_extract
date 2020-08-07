#include <string>
#include <cstring>
#include <map>
#include <utility>
#include <vector>
#include <unordered_set>

#include "common.h"
#include "object_storage.h"
#include "pages_extractor.h"

using namespace std;

enum
{
    CROSS_REFERENCE_LINE_SIZE = 20,
    BYTE_OFFSET_LEN = 10, /* length for byte offset in cross reference record */
    GENERATION_NUMBER_LEN = 5 /* length for generation number */
};

void get_object_offsets_old(const string &buffer, size_t offset, vector<size_t> &result);
void get_object_offsets_new(const string &buffer, size_t offset, vector<size_t> &result);

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

vector<pair<size_t, size_t>> get_trailer_offsets_old(const string &buffer, size_t cross_ref_offset)
{
    vector<pair<size_t, size_t>> trailer_offsets;
    unordered_set<size_t> cross_ref_offsets{cross_ref_offset};
    while (true)
    {
        size_t end_offset;
        if ((end_offset = buffer.find("\r\nstartxref\r\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\nstartxref\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\rstartxref\r", cross_ref_offset)) == string::npos)
        {
            throw pdf_error(FUNC_STRING + "Can`t find startxref in pos: " + to_string(cross_ref_offset));
        }
        trailer_offsets.emplace_back(cross_ref_offset, end_offset);
        size_t trailer_offset = efind(buffer, "trailer", cross_ref_offset);
        trailer_offset += LEN("trailer");
        const dict_t data = get_dictionary_data(buffer, trailer_offset);
        auto it = data.find("/Prev");
        if (it == data.end()) break;
        if (it->second.second != VALUE) throw pdf_error(FUNC_STRING + "/Prev value is not PDF VALUE type");
        cross_ref_offset = strict_stoul(it->second.first);
        //protect from endless loop
        if (cross_ref_offsets.count(cross_ref_offset)) break;
        cross_ref_offsets.insert(cross_ref_offset);
    }

    return trailer_offsets;
}

vector<pair<size_t, size_t>> get_trailer_offsets_new(const string &buffer, size_t cross_ref_offset)
{
    vector<pair<size_t, size_t>> trailer_offsets;
    unordered_set<size_t> cross_ref_offsets{cross_ref_offset};
    while (true)
    {
        size_t end_offset;
        if ((end_offset = buffer.find("\r\nstartxref\r\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\nstartxref\n", cross_ref_offset)) == string::npos &&
            (end_offset = buffer.find("\rstartxref\r", cross_ref_offset)) == string::npos)
        {
            throw pdf_error(FUNC_STRING + "Can`t find startxref in pos: " + to_string(cross_ref_offset));
        }
        trailer_offsets.emplace_back(cross_ref_offset, end_offset);
        size_t dict_offset = efind(buffer, "<<", cross_ref_offset);
        const dict_t data = get_dictionary_data(buffer, dict_offset);
        auto it = data.find("/Prev");
        if (it == data.end()) break;
        if (it->second.second != VALUE) throw pdf_error(FUNC_STRING + "/Prev value is not PDF VALUE type");
        cross_ref_offset = strict_stoul(it->second.first);
        if (cross_ref_offsets.count(cross_ref_offset)) break;
        cross_ref_offsets.insert(cross_ref_offset);
    }

    return trailer_offsets;
}


vector<pair<size_t, size_t>> get_trailer_offsets(const string &buffer, size_t cross_ref_offset)
{
    if (is_prefix(buffer.data() + cross_ref_offset, "xref")) return get_trailer_offsets_old(buffer, cross_ref_offset);
    return get_trailer_offsets_new(buffer, cross_ref_offset);
}

void get_object_offsets(const string &buffer, size_t offset, vector<size_t> &result)
{
    offset = skip_comments(buffer, offset);
    if (is_prefix(buffer.data() + offset, "xref")) return get_object_offsets_old(buffer, offset, result);
    get_object_offsets_new(buffer, offset, result);
}

array<unsigned int, 3> get_w(const dict_t &dictionary_data)
{
    auto it = dictionary_data.find("/W");
    if (it == dictionary_data.end()) throw pdf_error("can`t find /W");
    if (it->second.second != ARRAY) throw pdf_error("/W value must have ARRAY type");
    const string &str = it->second.first;
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
    for (unsigned int i = 0; i < w.size(); ++i)
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

unsigned int get_cross_ref_entries(const dict_t dictionary_data,
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
    if (it->second.second != ARRAY) throw pdf_error("/Index must be ARRAY");
    unsigned int entries = 0;
    vector<pair<string, pdf_object_t>> array_data = get_array_data(it->second.first, 0);
    if (array_data.empty()) throw pdf_error(FUNC_STRING + "/Index array is empty");
    for (size_t i = 0; i < array_data.size() - 1; i += 2)
    {
        if (array_data[i + 1].second != VALUE) throw pdf_error(FUNC_STRING + "wrong type for /Index. type=" +
                                                               to_string(array_data[i + 1].second) +
                                                               " val=" + array_data[i + 1].first);
        entries += strict_stoul(array_data[i + 1].first);
    }
    return entries;
}

void get_offsets_internal_new(const string &stream,
                              const dict_t dictionary_data,
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
    dict_t dictionary_data = get_dictionary_data(dict, 0);
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
        id2offsets.emplace(strict_stoul(buffer.substr(start_offset, end_offset - start_offset)), offset);
    }

    return id2offsets;
}

string get_text(const string &buffer,
                size_t cross_ref_offset,
                const ObjectStorage &storage,
                const dict_t &decrypt_data)
{
    size_t trailer_offset = cross_ref_offset;
    if (is_prefix(buffer.data() + cross_ref_offset, "xref"))
    {
        trailer_offset = efind(buffer, "trailer", trailer_offset);
        trailer_offset += LEN("trailer");
    }
    const dict_t trailer_data = get_dictionary_data(buffer, trailer_offset);
    const pair<string, pdf_object_t> root_pair = trailer_data.at("/Root");
    if (root_pair.second != INDIRECT_OBJECT) throw pdf_error(FUNC_STRING + "/Root value must be INDIRECT_OBJECT");
    const pair<string, pdf_object_t> real_root_pair = storage.get_object(get_id_gen(root_pair.first).first);
    if (real_root_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "/Root indirect object must be a dictionary");

    const dict_t root_data = get_dictionary_data(real_root_pair.first, 0);
    const pair<string, pdf_object_t> pages_pair = root_data.at("/Pages");
    if (pages_pair.second != INDIRECT_OBJECT) throw pdf_error(FUNC_STRING + "/Pages value must be INDRECT_OBJECT");

    return PagesExtractor(get_id_gen(pages_pair.first).first, storage, decrypt_data, buffer).get_text();
}

pair<string, pair<string, pdf_object_t>> get_id(const string &buffer, size_t start, size_t end)
{
    size_t off = efind(buffer, "/ID", start);
    if (off >= end) throw pdf_error(FUNC_STRING + "Can`t find /ID key");
    off = efind(buffer, '[', off);
    if (off >= end) throw pdf_error(FUNC_STRING + "Can`t find /ID value");

    return make_pair(string("/ID"), make_pair(get_array(buffer, off), ARRAY));
}

dict_t get_encrypt_data(const string &buffer, size_t start, size_t end, const map<size_t, size_t> &id2offsets)
{
    size_t off = buffer.find("/Encrypt", start);
    if (off == string::npos || off >= end) return dict_t();
    off += LEN("/Encrypt");
    pdf_object_t type = get_object_type(buffer, off);
    dict_t result;
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
    const vector<pair<size_t, size_t>> trailer_offsets = get_trailer_offsets(buffer, cross_ref_offset);
    map<size_t, size_t> id2offsets = get_id2offsets(buffer, cross_ref_offset, trailer_offsets);
    const dict_t encrypt_data = get_encrypt_data(buffer,
                                                 trailer_offsets.at(0).first,
                                                 trailer_offsets.at(0).second,
                                                 id2offsets);
    ObjectStorage storage(buffer, std::move(id2offsets), encrypt_data);
    return get_text(buffer, cross_ref_offset, storage, encrypt_data);
}
