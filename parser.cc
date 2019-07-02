#include <string>
#include <cstring>
#include <map>
#include <utility>
#include <vector>
#include <algorithm>
#include <stack>
#include <regex>

//for test
#include <fstream>
#include <iostream>


#include "pdf_extractor.h"
#include "pdf_internal.h"

using namespace std;

#define LEN(S) (sizeof(S) - 1)

extern string flate_decode(const string&);
extern string lzw_decode(const string&);

enum {SMALLEST_PDF_SIZE = 67 /*https://stackoverflow.com/questions/17279712/what-is-the-smallest-possible-valid-pdf*/,
      CROSS_REFERENCE_LINE_SIZE = 20,
      BYTE_OFFSET_LEN = 10, /* length for byte offset in cross reference record */
      GENERATION_NUMBER_LEN = 5 /* length for generation number */
};

enum pdf_object_t {DICTIONARY, ARRAY, STRING, VALUE, INDIRECT_OBJECT, NAME_OBJECT};

size_t efind_first(const string &src, const string& str, size_t pos)
{
    size_t ret = src.find_first_of(str, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first(const string &src, const char* s, size_t pos)
{
    size_t ret = src.find_first_of(s, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first(const string &src, const char* s, size_t pos, size_t n)
{
    size_t ret = src.find_first_of(s, pos, n);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const string& str, size_t pos)
{
    size_t ret = src.find(str, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const char* s, size_t pos)
{
    size_t ret = src.find(s, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, char c, size_t pos)
{
    size_t ret = src.find(c, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + c + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t strict_stoul(const string &str)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    if (str.find('-') != string::npos) throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    size_t val;
    try
    {
        val = stoul(str, &pos);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not unsigned number");

    return val;
}

bool prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

size_t get_cross_ref_offset_start(const string &buffer, size_t end)
{
    size_t start = buffer.find_last_of("\r\n", end);
    if (start == string::npos) throw pdf_error(FUNC_STRING + "can`t find start offset");
    ++start;
    
    return start;
}

bool is_blank(char c)
{
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') return true;
    return false;
}

size_t skip_spaces(const string &buffer, size_t offset)
{
    while (offset < buffer.length() && is_blank(buffer[offset])) ++offset;
    if (offset >= buffer.length()) throw pdf_error(FUNC_STRING + "no data after space");
    return offset;
}

string get_string(const string &buffer, size_t offset, const char *name, size_t end = string::npos)
{
    if (end == string::npos) end = buffer.length();
    size_t start_offset = efind(buffer, name, offset);
    start_offset += strlen(name);
    start_offset = skip_spaces(buffer, start_offset);
    size_t end_offset = efind_first(buffer, "  \r\n", start_offset);
    if (end_offset >= end) return string();
    return buffer.substr(start_offset, end_offset - start_offset);
}

size_t get_number(const string &buffer, size_t offset, const char *name, size_t end = string::npos)
{
    if (end == string::npos) end = buffer.length();
    const string str = get_string(buffer, offset, name, end);
    if (str.empty()) throw pdf_error(FUNC_STRING + "no number for " + name +
                                         " offset " + to_string(offset) + " end " + to_string(end)); 
    return strict_stoul(str);
}

size_t get_cross_ref_offset_end(const string &buffer)
{
    size_t end = buffer.size() - 1;
    if (buffer[end] == '\n') --end;
    if (buffer[end] == '\r') --end;
    end -= LEN("%%EOF");
    if (!prefix("%%EOF", buffer.data() + end + 1)) throw pdf_error(FUNC_STRING + "can`t find %%EOF");
    if (buffer[end] == '\n') --end;
    if (buffer[end] == '\r') --end;

    return end;
}

size_t get_cross_ref_offset(const string &buffer)
{
    size_t offset_end = get_cross_ref_offset_end(buffer);
    size_t offset_start = get_cross_ref_offset_start(buffer, offset_end);

    size_t r = strict_stoul(buffer.substr(offset_start, offset_end - offset_start + 1));
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

size_t get_start_offset(const string &buffer, size_t offset)
{
    offset = efind(buffer, ' ', offset);
    ++offset;
    if (offset >= buffer.size()) throw pdf_error(FUNC_STRING + "no data for elements size");
    return offset;
}

size_t get_xref_number(const string &buffer, size_t &offset)
{
    offset = skip_spaces(buffer, offset);
    offset = efind_first(buffer, "\r\t\n ", offset);
    offset = skip_spaces(buffer, offset);
    size_t end_offset = efind_first(buffer, "\r\t\n ", offset);
    size_t result = strict_stoul(buffer.substr(offset, end_offset - offset));
    offset = skip_spaces(buffer, end_offset);

    return result;
}

vector<size_t> get_trailer_offsets(const string &buffer, size_t cross_ref_offset)
{
    vector<size_t> trailer_offsets = {cross_ref_offset};
    while (true)
    {
        size_t end_offset = efind(buffer, "startxref", cross_ref_offset);
        size_t prev_offset = buffer.find("/Prev ", cross_ref_offset);
        if (prev_offset == string::npos || prev_offset >= end_offset) break;
        cross_ref_offset = get_number(buffer, prev_offset, "/Prev ");
        trailer_offsets.push_back(cross_ref_offset);
    }

    return trailer_offsets;
}

void get_object_offsets(const string &buffer, size_t offset, vector<size_t> &result)
{
    offset = efind(buffer, "xref", offset);
    offset += LEN("xref");
    size_t n = get_xref_number(buffer, offset);
    size_t end_offset = offset + n * CROSS_REFERENCE_LINE_SIZE;
    if (end_offset >= buffer.length()) throw pdf_error(FUNC_STRING + "pdf buffer has no data for indirect objects info");
    for ( ; offset < end_offset; offset += CROSS_REFERENCE_LINE_SIZE)
    {
        if (get_object_status(buffer, offset) == 'n') append_object(buffer, offset, result);
    }
}

void validate_offsets(const string &buffer, const vector<size_t> &offsets)
{
    for (size_t offset : offsets)
    {
        if (offset >= buffer.size()) throw pdf_error(FUNC_STRING + "offset is greater than pdf buffer");
    }
}

vector<size_t> get_all_object_offsets(const string &buffer, size_t cross_ref_offset)
{
    vector<size_t> trailer_offsets = get_trailer_offsets(buffer, cross_ref_offset);
    vector<size_t> object_offsets;
    for (size_t off : trailer_offsets)
    {
        get_object_offsets(buffer, off, object_offsets);
    }
    validate_offsets(buffer, object_offsets);

    return object_offsets;
}

map<size_t, size_t> get_id2offset(const string &buffer, size_t cross_ref_offset)
{
    vector<size_t> offsets = get_all_object_offsets(buffer, cross_ref_offset);
    map<size_t, size_t> ret;
    for (size_t offset : offsets)
    {
        size_t start_offset = efind_first(buffer, "0123456789", offset);
        size_t end_offset = efind(buffer, ' ', offset);
        ret.insert(make_pair(strict_stoul(buffer.substr(start_offset, end_offset - start_offset)), offset));
    }
    return ret;
}

size_t find_number(const string &buffer, size_t offset)
{
    while (offset < buffer.length() && (strchr("0123456789", buffer[offset]) == NULL)) ++offset;
    return offset;
}

void append_set(const string &buffer, size_t start_offset, const map<size_t, size_t> &id2offset, vector<size_t> &result)
{
    size_t array_end_offset = efind(buffer, ']', start_offset);
    start_offset = find_number(buffer, start_offset);
    for (;start_offset < array_end_offset; start_offset = find_number(buffer, start_offset))
    {
        size_t end_offset = buffer.find_first_of("  \r\n", start_offset);
        if (end_offset == string::npos || end_offset >= array_end_offset)
        {
            throw pdf_error(FUNC_STRING + "Can`t find end delimiter for number");
        }
        result.push_back(id2offset.at(strict_stoul(buffer.substr(start_offset, end_offset - start_offset))));
        start_offset = efind(buffer, 'R', end_offset);
    }
}

void get_pages_offsets_int(const string &buffer, size_t off, const map<size_t, size_t> &id2offset, vector<size_t> &result)
{
    size_t end_offset = efind(buffer, "endobj", off);
    if (get_string(buffer, off, "/Type ", end_offset) != "/Pages") return;
    size_t kids_offset = efind(buffer, "/Kids", off);
    kids_offset = efind(buffer, '[', kids_offset);
    vector<size_t> pages;
    append_set(buffer, kids_offset, id2offset, pages);
    for (size_t page : pages)
    {
        //avoid infinite recursion for 'bad' pdf
        if (find(result.begin(), result.end(), page) == result.end())
        {
            get_pages_offsets_int(buffer, page, id2offset, result);
            result.insert(result.end(), pages.begin(), pages.end());
        }
    }
}

pdf_object_t get_object_type(const string &buffer, size_t &offset)
{
    offset = skip_spaces(buffer, offset);
    const string str = buffer.substr(offset, 2);
    if (str == "<<") return DICTIONARY;
    if (str[0] == '[') return ARRAY;
    if (str[0] == '<' || str[0] == '(') return STRING;
    if (str[0] == '/') return NAME_OBJECT;
    if (regex_search(buffer.substr(offset), regex("^[0-9]+[\r\t\n ]+[0-9]+[\r\t\n ]+R"))) return INDIRECT_OBJECT;
    return VALUE;
}

size_t find_value_end_delimiter(const string &buffer, size_t offset)
{
    size_t result = buffer.find_first_of("\r\t\n ", offset);
    size_t dict_end_offset = buffer.find(">>", offset);
    if (result == string::npos || dict_end_offset < result) result = dict_end_offset;
    if (result == string::npos) throw pdf_error(FUNC_STRING + " can`t find end delimiter for value");
    return result;
}

string get_value(const string &buffer, size_t &offset)
{
    size_t start_offset = offset;
    offset = find_value_end_delimiter(buffer, offset);

    return buffer.substr(start_offset, offset - start_offset);
}

string get_array(const string &buffer, size_t &offset)
{
    if (buffer[offset] != '[') throw pdf_error(FUNC_STRING + "offset should point to '['");
    string result = "[";
    ++offset;
    stack<pdf_object_t> prevs;
    while (true)
    {
        result.push_back(buffer.at(offset));
        switch (buffer[offset])
        {
        case '[':
            prevs.push(ARRAY);
            break;
        case ']':
            if (prevs.empty())
            {
                ++offset;
                return result;
            }
            prevs.pop();
            break;
        }
        ++offset;
    }
    throw pdf_error(FUNC_STRING + " no array in " + to_string(offset));
}

string get_name_object(const string &buffer, size_t &offset)
{
    size_t start_offset = offset;
    offset = find_value_end_delimiter(buffer, offset);

    return buffer.substr(start_offset, offset - start_offset);
}

string get_indirect_object(const string &buffer, size_t &offset)
{
    size_t start_offset = offset;
    offset = efind(buffer, 'R', offset) + 1;

    return buffer.substr(start_offset, offset - start_offset);
}

string get_string(const string &buffer, size_t &offset)
{
    char delimiter = buffer.at(offset);
    if (delimiter != '(' && delimiter != '<') throw pdf_error(FUNC_STRING + "string must start with '(' or '<'");
    char end_delimiter = delimiter == '('? ')' : '>';
    stack<pdf_object_t> prevs;
    string result(1, delimiter);
    ++offset;
    while (true)
    {
        result.push_back(buffer.at(offset));
        if (buffer[offset] == delimiter)
        {
            prevs.push(STRING);
        }
        if (buffer[offset] == end_delimiter)
        {
            if (prevs.empty())
            {
                ++offset;
                return result;
            }
            prevs.pop();
        }
        ++offset;
    }
}

string get_dictionary(const string &buffer, size_t &offset)
{
    if (buffer.substr(offset, 2) != "<<") throw pdf_error(FUNC_STRING + "dictionary must start with '<<'");
    stack<pdf_object_t> prevs;
    size_t end_offset = offset;
    while (end_offset < buffer.length())
    {
        if (buffer.at(end_offset) == '<' && buffer.at(end_offset + 1) == '<')
        {
            prevs.push(DICTIONARY);
        }
        if (buffer.at(end_offset) == '>' && buffer.at(end_offset + 1) == '>')
        {
            if (prevs.empty())
            {
                end_offset += 2;
                size_t start_offset = offset;
                offset = end_offset;
                return buffer.substr(start_offset, end_offset - start_offset);
            }
            prevs.pop();
        }
        ++end_offset;
    }
    if (end_offset >= buffer.length()) throw pdf_error(FUNC_STRING + "can`t find dictionary end delimiter");
}

map<string, pair<string, pdf_object_t>> get_dictionary_data(const string &buffer, size_t offset)
{
    static const map<pdf_object_t, string (&)(const string&, size_t&)> type2func = {{DICTIONARY, get_dictionary},
                                                                                    {ARRAY, get_array},
                                                                                    {STRING, get_string},
                                                                                    {VALUE, get_value},
                                                                                    {INDIRECT_OBJECT, get_indirect_object},
                                                                                    {NAME_OBJECT, get_name_object}};

    offset = efind(buffer, "<<", offset);
    offset += LEN("<<");
    map<string, pair<string, pdf_object_t>> result;
    while (true)
    {
        offset = skip_spaces(buffer, offset);
        if (buffer.at(offset) == '>' && buffer.at(offset + 1) == '>') return result;
        if (buffer.at(offset) != '/') throw pdf_error(FUNC_STRING + "malformed dictionary");
        size_t end_offset = efind_first(buffer, "\r\t\n ", offset);
        const string key = buffer.substr(offset, end_offset - offset);
        offset = end_offset;
        pdf_object_t type = get_object_type(buffer, offset);
        const string val = type2func.at(type)(buffer, offset);
        result.insert(make_pair(key, make_pair(val, type)));
    }
}

vector<size_t> get_pages_offsets(const string &buffer, size_t offset, const map<size_t, size_t> &id2offset)
{
    size_t end_offset = efind(buffer, "endobj", offset);
    if (get_string(buffer, offset, "/Type ", end_offset) != "/Pages")
    {
        throw pdf_error("Root catalog type must be 'Pages'");
    }
    vector<size_t> result;
    get_pages_offsets_int(buffer, offset, id2offset, result);

    return result;
}

void append_content_offsets(const string &buffer,
                           size_t page_offset,
                           const map<size_t, size_t> &id2offset,
                           vector<size_t> &content_offsets)
{
    size_t end_offset = efind(buffer, "endobj", page_offset);
    size_t start_offset = buffer.find("/Contents", page_offset);
    // "/Contents" key can be absent for Page. In this case Page is empty
    if (start_offset == string::npos || start_offset >= end_offset) return;
    start_offset += LEN("/Contents");
    start_offset = skip_spaces(buffer, start_offset);
    if (buffer[start_offset] == '[')
    {
        append_set(buffer, start_offset, id2offset, content_offsets);
    }
    else
    {
        size_t space_pos = efind(buffer, ' ', start_offset);
        content_offsets.push_back(id2offset.at(strict_stoul(buffer.substr(start_offset, space_pos - start_offset))));
    }
}

vector<size_t> get_content_offsets(const string &buffer, size_t cross_ref_offset, const map<size_t, size_t> &id2offset)
{
    vector<size_t> result;
    size_t root_id = get_number(buffer, cross_ref_offset, "/Root ");
    size_t catalog_pages_id = get_number(buffer, id2offset.at(root_id), "/Pages ");
    vector<size_t> page_offsets = get_pages_offsets(buffer, id2offset.at(catalog_pages_id), id2offset);
    for (size_t page_offset : page_offsets) append_content_offsets(buffer, page_offset, id2offset, result);
    return result;
}

size_t get_content_len(const string &buffer,
                       const map<size_t, size_t> &id2offset,
                       const map<string, pair<string, pdf_object_t>> &props)
{
    const pair<string, pdf_object_t> &r = props.at("/Length");
    if (r.second == VALUE)
    {
        return strict_stoul(r.first);
    }
    else if (r.second == INDIRECT_OBJECT)
    {
        size_t end_offset = efind(r.first, ' ', 0);
        size_t id = strict_stoul(r.first.substr(0, end_offset));
        size_t start_offset = efind(buffer, "obj", id2offset.at(id));
        start_offset += LEN("obj");
        start_offset = skip_spaces(buffer, start_offset);
        end_offset = efind_first(buffer, "\r\n\t ", start_offset);

        return strict_stoul(buffer.substr(start_offset, end_offset - start_offset));
    }
    else
    {
        throw pdf_error(FUNC_STRING + " wrong type for /Length");
    }
}

string get_content(const string &buffer,
                   const map<size_t, size_t> &id2offset,
                   size_t offset,
                   const map<string, pair<string, pdf_object_t>> &props)
{
    size_t len = get_content_len(buffer, id2offset, props);
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

void decode(string &content, const vector<string> &filters)
{
    static const map<string, string (&)(const string&)> filter2func = {{"/FlateDecode", flate_decode},
                                                                       {"/LZWDecode", lzw_decode}};
    for (const string& filter : filters) content = filter2func.at(filter)(content);
    cout << content;
}

string output_content(const string &buffer, const map<size_t, size_t> &id2offset, size_t offset)
{
    const map<string, pair<string, pdf_object_t>> props = get_dictionary_data(buffer, offset);

    string content = get_content(buffer, id2offset, offset, props);
    if (props.count("/Filter") == 1)
    {
        vector<string> filters = get_filters(props);
        decode(content, filters);
    }
    
    
    return content;
}

string pdf2txt(const string &buffer)
{
    if (buffer.size() < SMALLEST_PDF_SIZE) throw pdf_error(FUNC_STRING + "pdf buffer is too small");
    size_t cross_ref_offset = get_cross_ref_offset(buffer);
    map<size_t, size_t> id2offset = get_id2offset(buffer, cross_ref_offset);
    vector<size_t> content_offsets = get_content_offsets(buffer, cross_ref_offset, id2offset);
    string result;
    for (size_t offset : content_offsets) result += output_content(buffer, id2offset, offset);

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
