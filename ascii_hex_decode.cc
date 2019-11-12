#include <string>
#include <map>
#include <set>

#include "common.h"

using namespace std;

namespace
{
    const set<unsigned int> white_chars{
        0x00, // NULL
        0x09, // TAB
        0x0A, // Line Feed
        0x0C, // Form Feed
        0x0D, // Carriage Return
        0x20, // White Space
        0x00  // end marker
    };

    const map<unsigned char, unsigned char> hex_map = {{0x30, 0x0},//0-9
                                                       {0x31, 0x1},
                                                       {0x32, 0x2},
                                                       {0x33, 0x3},
                                                       {0x34, 0x4},
                                                       {0x35, 0x5},
                                                       {0x36, 0x6},
                                                       {0x37, 0x7},
                                                       {0x38, 0x8},
                                                       {0x39, 0x9},
                                                       {0x61, 0xA},//a-f
                                                       {0x62, 0xB},
                                                       {0x63, 0xC},
                                                       {0x64, 0xD},
                                                       {0x65, 0xE},
                                                       {0x66, 0xF},
                                                       {0x41, 0xA},//A-F
                                                       {0x42, 0xB},
                                                       {0x43, 0xC},
                                                       {0x44, 0xD},
                                                       {0x45, 0xE},
                                                       {0x46, 0xF}};
}

string ascii_hex_decode(const string &buf, const map<string, pair<string, pdf_object_t>> &dict)
{
    bool low = true;
    const char *buffer = buf.c_str();
    size_t len = buf.length();
    string result;
    unsigned char decoded_byte = 0;

    while (len--)
    {
        if (*buffer == '>')
        {
            if (!low) result.append(1, (decoded_byte << 4) | 0);
            return result;
        }
        if (white_chars.count(*buffer))
        {
            ++buffer;
            continue;
        }

        unsigned char val = hex_map.at(static_cast<unsigned char>(*buffer));
        if (low)
        {
            decoded_byte = val;
            low = false;
        }
        else
        {
            decoded_byte = ((decoded_byte << 4) | val);
            low = true;
            result.append(1, decoded_byte);
            decoded_byte = 0;
        }
        ++buffer;
    }

    if (!low) result.append(1, (decoded_byte << 4) | 0);

    return result;
}
