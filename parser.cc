#include <stdexcept>
#include <string>
#include <cstring>
#include <vector>
#include <tuple>
#include <map>
#include <utility>

//for test
#include <fstream>
#include <iostream>

using namespace std;

#define LEN(S) (sizeof(S) - 1)
#define FUNC_STRING (string(__func__) + ": ")

enum {SMALLEST_PDF_SIZE = 67 /*https://stackoverflow.com/questions/17279712/what-is-the-smallest-possible-valid-pdf*/,
      CROSS_REFERENCE_LINE_SIZE = 20,
      BYTE_OFFSET_LEN = 10, /* length for byte offset in cross reference record */
      GENERATION_NUMBER_LEN = 5 /* length for generation number */
};


size_t efind_first(const string &src, const string& str, size_t pos = 0)
{
    size_t ret = src.find_first_of(str, pos);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first(const string &src, const char* s, size_t pos = 0)
{
    size_t ret = src.find_first_of(s, pos);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first(const string &src, const char* s, size_t pos, size_t n)
{
    size_t ret = src.find_first_of(s, pos);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const string& str, size_t pos = 0)
{
    size_t ret = src.find(str, pos);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const char* s, size_t pos = 0)
{
    size_t ret = src.find(s, pos);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const char* s, size_t pos, size_t n)
{
    size_t ret = src.find(s, pos, n);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, char c, size_t pos = 0)
{
    size_t ret = src.find(c, pos);
    if (ret == string::npos) throw runtime_error(FUNC_STRING + "for " + c + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t strict_stoul(const string &str)
{
    if (str.empty()) throw runtime_error(FUNC_STRING + "string is empty");
    size_t pos;
    if (str.find('-') != string::npos) throw runtime_error(FUNC_STRING + str + " is not unsigned number");
    size_t val;
    try
    {
        val = stoul(str, &pos);
    }
    catch (const std::exception &e)
    {
        throw runtime_error(FUNC_STRING + str + " is not unsigned number");
    }

    if (pos != str.size()) throw runtime_error(FUNC_STRING + str + " is not unsigned number");

    return val;
}

bool prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

size_t get_cross_ref_offset_start(const string &buffer, size_t end)
{
    size_t start = buffer.find_last_of("\r\n", end);
    if (start == string::npos) throw runtime_error(FUNC_STRING + "can`t find start offset");
    ++start;
    
    return start;
}

size_t get_number(const string &buffer, size_t offset, const char *name)
{
    size_t start_offset = efind(buffer, name, offset);
    start_offset += strlen(name);
    if (start_offset >= buffer.length()) throw runtime_error(FUNC_STRING + "No data for " + name + " object");
    size_t end_offset = efind_first(buffer, "  \r\n", start_offset);

    return strict_stoul(buffer.substr(start_offset, end_offset - start_offset));
}

size_t get_cross_ref_offset_end(const string &buffer)
{
    size_t end = buffer.size() - 1;
    if (buffer[end] == '\n') --end;
    if (buffer[end] == '\r') --end;
    end -= LEN("%%EOF");
    if (!prefix("%%EOF", buffer.data() + end + 1)) throw runtime_error(FUNC_STRING + "can`t find %%EOF");
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
        throw runtime_error(FUNC_STRING + to_string(r) + " is larger than buffer size " + to_string(buffer.size()));
    }

    return r;
}

void append_object(const string &buf, size_t offset, vector<size_t> &objects)
{
    if (offset + BYTE_OFFSET_LEN >= buf.length()) throw runtime_error(FUNC_STRING + "object info record is too small");
    if (buf[offset + BYTE_OFFSET_LEN] != ' ') throw runtime_error(FUNC_STRING + "no space for object info");
    objects.push_back(strict_stoul(buf.substr(offset, BYTE_OFFSET_LEN)));
}

char get_object_status(const string &buffer, size_t offset)
{
    size_t start_offset = offset + BYTE_OFFSET_LEN + GENERATION_NUMBER_LEN + 1;
    if (start_offset + 2 >= buffer.length()) throw runtime_error(FUNC_STRING + "object info record is too small");
    if (buffer[start_offset] != ' ') throw runtime_error(FUNC_STRING + "no space for object info record");
    if (strchr("\r\n ", buffer[start_offset + 2]) == NULL)
    {
        throw runtime_error(FUNC_STRING + "no newline for object info record");
    }
    char ret = buffer[start_offset + 1];
    if (ret != 'n' && ret != 'f') throw runtime_error(FUNC_STRING + "info object record status entry must be 'n' or 'f'");

    return ret;
}

size_t get_start_offset(const string &buffer, size_t offset)
{
    offset = efind(buffer, ' ', offset);
    ++offset;
    if (offset >= buffer.size()) throw runtime_error(FUNC_STRING + "no data for elements size");
    return offset;
}

tuple<size_t, size_t, bool> get_object_info_data(const string &buffer, size_t offset)
{
    if (prefix("trailer", buffer.data() + offset)) return make_tuple(0, 0, false);
    offset = get_start_offset(buffer, offset);
    size_t end_offset = efind_first(buffer, "\r\n ", offset);
    size_t objects_offset = end_offset;
    if (buffer.at(objects_offset) == ' ') ++objects_offset;
    if (buffer.at(objects_offset) == '\r') ++objects_offset;
    if (buffer.at(objects_offset) == '\n') ++objects_offset;

    size_t elements_num = strict_stoul(buffer.substr(offset, end_offset - offset));
    if (elements_num == 0) throw runtime_error(FUNC_STRING + "number of elements in cross ref table can`t be zero.");
    
    return make_tuple(elements_num, objects_offset, true);
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

void get_object_offsets(const string &buffer, size_t cross_ref_offset, vector<size_t> &result)
{
    size_t offset = efind(buffer, "xref", cross_ref_offset);
    offset += LEN("xref");
    size_t elements_num, objects_offset;
    bool is_success;
    tie (elements_num, objects_offset, is_success) = get_object_info_data(buffer, offset);
    if (!is_success) throw runtime_error(FUNC_STRING + "no size data for cross reference table");
    while (is_success)
    {
        size_t end_objects_offset = objects_offset + elements_num * CROSS_REFERENCE_LINE_SIZE;
        if (end_objects_offset >= buffer.size()) throw runtime_error(FUNC_STRING + "pdf buffer has no data for objects");
        while (objects_offset < end_objects_offset)
        {
            if (get_object_status(buffer, objects_offset) == 'n') append_object(buffer, objects_offset, result);
            objects_offset += CROSS_REFERENCE_LINE_SIZE;
        }
        tie (elements_num, objects_offset, is_success) = get_object_info_data(buffer, objects_offset);
    }
}

void validate_offsets(const string &buffer, const vector<size_t> &offsets)
{
    for (size_t offset : offsets)
    {
        if (offset >= buffer.size()) throw runtime_error(FUNC_STRING + "offset is greater than pdf buffer");
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

void append_array(const string &buffer, size_t start_offset, const map<size_t, size_t> &id2offset, vector<size_t> &result)
{
    size_t array_end_offset = efind(buffer, ']', start_offset);
    start_offset = find_number(buffer, start_offset);
    for (;start_offset < array_end_offset; start_offset = find_number(buffer, start_offset))
    {
        size_t end_offset = buffer.find_first_of("  \r\n", start_offset);
        if (end_offset == string::npos || end_offset >= array_end_offset)
        {
            throw runtime_error(FUNC_STRING + "Can`t find end delimiter for number");
        }
        result.push_back(id2offset.at(strict_stoul(buffer.substr(start_offset, end_offset - start_offset))));
        start_offset = efind(buffer, 'R', end_offset);
    }
}

vector<size_t> get_pages_offsets(const string &buffer, size_t catalog_pages_id, const map<size_t, size_t> &id2offset)
{
    size_t offset = id2offset.at(catalog_pages_id);
    size_t kids_offset = efind(buffer, "/Kids", offset);
    kids_offset = efind(buffer, '[', kids_offset);
    vector<size_t> ret;
    append_array(buffer, kids_offset, id2offset, ret);

    return ret;
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
    while (strchr("\r\n ", buffer[start_offset]) != NULL) ++start_offset;
    if (buffer[start_offset] == '[')
    {
        append_array(buffer, start_offset, id2offset, content_offsets);
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
    vector<size_t> page_offsets = get_pages_offsets(buffer, catalog_pages_id, id2offset);
    for (size_t page_offset : page_offsets) append_content_offsets(buffer, page_offset, id2offset, result);

    return result;
}

string pdf2txt(const string &buffer)
{
    if (buffer.size() < SMALLEST_PDF_SIZE) throw runtime_error(FUNC_STRING + "pdf buffer is too small");
    size_t cross_ref_offset = get_cross_ref_offset(buffer);
    map<size_t, size_t> id2offset = get_id2offset(buffer, cross_ref_offset);
    vector<size_t> content_offsets = get_content_offsets(buffer, cross_ref_offset, id2offset);
//    for (size_t off : content_offsets) cout << off << endl;

    return string();
}

int main(int argc, char *argv[])
{
    std::ifstream t(argv[1]);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    pdf2txt(str);
    
    return 0;
}
