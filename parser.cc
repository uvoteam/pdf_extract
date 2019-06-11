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

size_t get_root_id(const string &buffer, size_t cross_ref_offset)
{
    size_t start_offset = buffer.find("/Root ", cross_ref_offset);
    if (start_offset == string::npos) throw runtime_error(FUNC_STRING + "Can`t find /Root object");
    start_offset += LEN("/Root ");
    if (start_offset >= buffer.length()) throw runtime_error(FUNC_STRING + "No data for /Root object");
    size_t end_offset = buffer.find(' ', start_offset);

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

void append_node(const string &buf, size_t offset, vector<size_t> &nodes)
{
    if (offset + BYTE_OFFSET_LEN >= buf.length()) throw runtime_error(FUNC_STRING + "node info record is too small");
    if (buf[offset + BYTE_OFFSET_LEN] != ' ') throw runtime_error(FUNC_STRING + "no space for node info");
    nodes.push_back(strict_stoul(buf.substr(offset, BYTE_OFFSET_LEN)));
}

char get_node_status(const string &buffer, size_t offset)
{
    size_t start_offset = offset + BYTE_OFFSET_LEN + GENERATION_NUMBER_LEN + 1;
    if (start_offset + 2 >= buffer.length()) throw runtime_error(FUNC_STRING + "node info record is too small");
    if (buffer[start_offset] != ' ') throw runtime_error(FUNC_STRING + "no space for node info record");
    if (strchr("\r\n ", buffer[start_offset + 2]) == NULL)
    {
        throw runtime_error(FUNC_STRING + "no newline for node info record");
    }
    char ret = buffer[start_offset + 1];
    if (ret != 'n' && ret != 'f') throw runtime_error(FUNC_STRING + "info node record status entry must be 'n' or 'f'");

    return ret;
}

size_t get_start_offset(const string &buffer, size_t offset)
{
    offset = buffer.find(' ', offset);
    if (offset == string::npos) throw runtime_error(FUNC_STRING + "can`t find space after xref");
    ++offset;
    if (offset >= buffer.size()) throw runtime_error(FUNC_STRING + "no data for elements size");
    return offset;
}

tuple<size_t, size_t, bool> get_node_info_data(const string &buffer, size_t offset)
{
    if (prefix("trailer", buffer.data() + offset)) return make_tuple(0, 0, false);
    offset = get_start_offset(buffer, offset);
    size_t end_offset = buffer.find_first_of("\r\n ", offset);
    if (end_offset == string::npos) throw runtime_error(FUNC_STRING + "can`t find space symbol for elements size");
    size_t nodes_offset = end_offset;
    if (buffer.at(nodes_offset) == ' ') ++nodes_offset;
    if (buffer.at(nodes_offset) == '\r') ++nodes_offset;
    if (buffer.at(nodes_offset) == '\n') ++nodes_offset;

    size_t elements_num = strict_stoul(buffer.substr(offset, end_offset - offset));
    if (elements_num == 0) throw runtime_error(FUNC_STRING + "number of elements in cross ref table can`t be zero.");
    
    return make_tuple(elements_num, nodes_offset, true);
}

vector<size_t> get_nodes_offsets(const string &buffer, size_t cross_ref_offset)
{
    size_t offset = buffer.find("xref", cross_ref_offset);
    if (offset == string::npos) throw runtime_error(FUNC_STRING + "can`t find xref");
    offset += LEN("xref");
    size_t elements_num, nodes_offset;
    bool is_success;
    tie (elements_num, nodes_offset, is_success) = get_node_info_data(buffer, offset);
    if (!is_success) throw runtime_error(FUNC_STRING + "no size data for cross reference table");
    vector<size_t> ret;
    ret.reserve(elements_num);
    while (is_success)
    {
        size_t end_nodes_offset = nodes_offset + elements_num * CROSS_REFERENCE_LINE_SIZE;
        if (end_nodes_offset >= buffer.size()) throw runtime_error(FUNC_STRING + "pdf buffer has no data for nodes");
        while (nodes_offset < end_nodes_offset)
        {
            if (get_node_status(buffer, nodes_offset) == 'n') append_node(buffer, nodes_offset, ret);
            nodes_offset += CROSS_REFERENCE_LINE_SIZE;
        }
        tie (elements_num, nodes_offset, is_success) = get_node_info_data(buffer, nodes_offset);
    }
    return ret;
}

map<size_t, size_t> get_id2offset(const string &buffer, const vector<size_t> &offsets)
{
    map<size_t, size_t> ret;
    for (size_t offset : offsets)
    {
        size_t start_offset = buffer.find_first_of("0123456789", offset);
        if (start_offset == string::npos) throw runtime_error(FUNC_STRING + "Can`t find start offset for object id");
        size_t end_offset = buffer.find(' ', offset);
        if (start_offset == string::npos) throw runtime_error(FUNC_STRING + "Can`t find end offset for object id");
        ret.insert(make_pair(strict_stoul(buffer.substr(start_offset, end_offset - start_offset)), offset));
    }
    return ret;
}

void validate_offsets(const string &buffer, const vector<size_t> &offsets)
{
    for (size_t offset : offsets)
    {
        if (offset >= buffer.size()) throw runtime_error(FUNC_STRING + "offset is greater than pdf buffer");
    }
}

string pdf2txt(const string &buffer)
{
    if (buffer.size() < SMALLEST_PDF_SIZE) throw runtime_error(FUNC_STRING + "pdf buffer is too small");
    size_t cross_ref_offset = get_cross_ref_offset(buffer);
    vector<size_t> offsets = get_nodes_offsets(buffer, cross_ref_offset);
    validate_offsets(buffer, offsets);
    map<size_t, size_t> id2offset = get_id2offset(buffer, offsets);
    size_t root_id = get_root_id(buffer, cross_ref_offset);
    cout << root_id << endl;

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
