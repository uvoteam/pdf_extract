#include <string>
#include <vector>

#include "pdf_internal.h"
#include "pdf_extractor.h"

using namespace std;

namespace
{
    const vector<unsigned long> powers85{85*85*85*85, 85*85*85, 85*85, 85, 1};

    string wide_put(unsigned long tuple, int bytes)
    {
        char data[4];
        
        switch (bytes) 
        {
        case 4:
            data[0] = static_cast<char>(tuple >> 24);
            data[1] = static_cast<char>(tuple >> 16);
            data[2] = static_cast<char>(tuple >>  8);
            data[3] = static_cast<char>(tuple);
            break;
        case 3:
            data[0] = static_cast<char>(tuple >> 24);
            data[1] = static_cast<char>(tuple >> 16);
            data[2] = static_cast<char>(tuple >>  8);
            break;
        case 2:
            data[0] = static_cast<char>(tuple >> 24);
            data[1] = static_cast<char>(tuple >> 16);
            break;
        case 1:
            data[0] = static_cast<char>(tuple >> 24);
            break;
        }
        return string(data, bytes);
    }
}

string ascii85_decode(const string &buf, const map<string, pair<string, pdf_object_t>> &opts)
{
    int count = 0;
    unsigned long tuple = 0;
    bool found_end_marker = false;
    const char* buffer = buf.data();
    size_t len = buf.length();
    string result;
    while (len && !found_end_marker)
    {
        switch (*buffer)
        {
        default:
            if (*buffer < '!' || *buffer > 'u') throw pdf_error(FUNC_STRING + "*buffer is out of range");
            tuple += (*buffer - '!') * powers85.at(count++);
            if (count == 5)
            {
                result += wide_put(tuple, 4);
                count = 0;
                tuple = 0;
            }
            break;
        case 'z':
            if (count != 0) throw pdf_error(FUNC_STRING + "count is not zero");
            result += wide_put(tuple, 4);
            break;
        case '~':
            ++buffer; 
            --len;
            if(len && *buffer != '>') throw pdf_error(FUNC_STRING + "buffer is not >");
            found_end_marker = true;
            break;
        case '\n': case '\r': case '\t': case ' ':
        case '\0': case '\f': case '\b': case 0177:
            break;
        }
        --len;
        ++buffer;
    }
}
