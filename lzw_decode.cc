#include <string>
#include <vector>
#include <cstdint>

#include "pdf_internal.h"
#include "pdf_extractor.h"

using namespace std;
namespace
{
    struct lzw_item_t
    {
        vector<unsigned char> value;
    };

    typedef vector<lzw_item_t> lzw_table_t;
    enum { LZW_TABLE_SIZE = 4096 };
    const unsigned short masks[4] = { 0x01FF, 0x03FF, 0x07FF, 0x0FFF };
    const unsigned short clear = 0x0100;
    const unsigned short eod = 0x0101;

    lzw_table_t init_table()
    {
        lzw_table_t table;
        table.reserve(LZW_TABLE_SIZE);
        for(int i = 0; i <= 255; i++)
        {
            lzw_item_t item;
            item.value.push_back(static_cast<unsigned char>(i));
            table.push_back(item);
        }
        // Add dummy entry, which is never used by decoder
        lzw_item_t item;
        table.push_back(item);

        return table;
    }
}

string lzw_decode(const string& buf)
{
    unsigned int  m_mask = 0;
    unsigned int  m_code_len = 9;
    unsigned char m_character = 0;

    lzw_table_t table = init_table();
    unsigned int       buffer_size = 0;
    const unsigned int buffer_max  = 24;

    uint32_t         old         = 0;
    uint32_t         code        = 0;
    uint32_t         buffer      = 0;

    lzw_item_t           item;

    vector<unsigned char> data;
    string result;
    size_t len = buf.length();
    const char *pBuffer = buf.data();
    m_character = *pBuffer;
    while (len)
    {
        // Fill the buffer
        while (buffer_size <= (buffer_max-8) && len)
        {
            buffer <<= 8;
            buffer |= static_cast<uint32_t>(static_cast<unsigned char>(*pBuffer));
            buffer_size += 8;

            ++pBuffer;
            len--;
        }
        // read from the buffer
        while( buffer_size >= m_code_len )
        {
            code         = (buffer >> (buffer_size - m_code_len)) & masks[m_mask];
            buffer_size -= m_code_len;

            if( code == clear )
            {
                m_mask     = 0;
                m_code_len = 9;

                table = init_table();
            }
            else if( code == eod )
            {
                len = 0;
                break;
            }
            else
            {
                if( code >= table.size() )
                {
                    if (old >= table.size())
                    {
                        throw pdf_error(FUNC_STRING + "value out of range");
                    }
                    data = table[old].value;
                    data.push_back( m_character );
                }
                else
                    data = table[code].value;
                result.append(reinterpret_cast<char*>(data.data()), data.size());
                m_character = data[0];
                if( old < table.size() ) // fix the first loop
                    data = table[old].value;
                data.push_back( m_character );

                item.value = data;
                table.push_back( item );

                old = code;

                switch(table.size())
                {
                case 511:
                case 1023:
                case 2047:
                    ++m_code_len;
                    ++m_mask;
                default:
                    break;
                }
            }
        }
    }
    return result;
}
