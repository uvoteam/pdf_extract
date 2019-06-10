#include <stdexcept>
#include <string>
#include <cstring>
#include <vector>

//for test
#include <fstream>
#include <iostream>

using namespace std;

#define LEN(S) (sizeof(S) - 1)
#define FUNC_STRING (string(__func__) + ": ")
#define CROSS_REFERENCE_LINE_SIZE 20

enum {SMALLEST_PDF_SIZE = 67 /*https://stackoverflow.com/questions/17279712/what-is-the-smallest-possible-valid-pdf*/};


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
    size_t end_offset = buf.find(' ', offset);
    if (end_offset == string::npos) throw runtime_error(FUNC_STRING + "no space for node info");
    nodes.push_back(strict_stoul(buf.substr(offset, end_offset - offset)));
}

vector<size_t> get_nodes_offsets(const string &buffer, size_t cross_ref_offset)
{
    size_t offset = buffer.find("xref", cross_ref_offset);
    if (offset == string::npos) throw runtime_error(FUNC_STRING + "can`t find xref");
    offset += LEN("xref");
    if (offset >= buffer.size() - CROSS_REFERENCE_LINE_SIZE) throw runtime_error(FUNC_STRING + "No data for xref");
    offset = buffer.find(' ', offset);
    if (offset == string::npos) throw runtime_error(FUNC_STRING + "can`t find space after xref");
    ++offset;
    if (offset >= buffer.size()) throw runtime_error(FUNC_STRING + "no data for elements size");
    size_t end_offset = buffer.find_first_of("\r\n ", offset);
    if (end_offset == string::npos) throw runtime_error(FUNC_STRING + "can`t find space symbol for elements size");
    size_t elements_num = strict_stoul(buffer.substr(offset, end_offset - offset));
    size_t nodes_offset = end_offset;
    if (buffer[nodes_offset] == ' ') ++nodes_offset;
    if (buffer[nodes_offset] == '\r') ++nodes_offset;
    if (buffer[nodes_offset] == '\n') ++nodes_offset;
    size_t end_nodes_offset = nodes_offset + elements_num * CROSS_REFERENCE_LINE_SIZE;
    if (end_nodes_offset >= buffer.size())
    {
        throw runtime_error(FUNC_STRING + "pdf buffer has no data for nodes");
    }
    vector<size_t> ret;
    ret.reserve(elements_num);
    while (nodes_offset < end_nodes_offset)
    {
        append_node(buffer, nodes_offset, ret);
        nodes_offset += CROSS_REFERENCE_LINE_SIZE;
    }

    return ret;
}

string pdf2txt(const string &buffer)
{
    if (buffer.size() < SMALLEST_PDF_SIZE) throw runtime_error(FUNC_STRING + "pdf buffer is too small");
    size_t cross_ref_offset = get_cross_ref_offset(buffer);
    vector<size_t> offsets = get_nodes_offsets(buffer, cross_ref_offset);
    
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
