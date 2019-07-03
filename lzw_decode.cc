#include <string>
#include <vector>
#include <cstdint>

#include "pdf_internal.h"
#include "pdf_extractor.h"

using namespace std;

struct TLzwItem {
    vector<unsigned char> value;
};

typedef vector<TLzwItem>     TLzwTable;
enum { LZW_TABLE_SIZE = 4096 };
const unsigned short s_masks[4] = { 0x01FF, 0x03FF, 0x07FF, 0x0FFF };
const unsigned short s_clear = 0x0100;
const unsigned short s_eod = 0x0101;


TLzwTable init_table()
{
    TLzwTable m_table;
    m_table.reserve(LZW_TABLE_SIZE);
    for(int i = 0; i <= 255; i++)
    {
        TLzwItem item;
        item.value.push_back(static_cast<unsigned char>(i));
        m_table.push_back(item);
    }
    // Add dummy entry, which is never used by decoder
    TLzwItem item;
    m_table.push_back(item);

    return m_table;
}

string lzw_decode(const string& buf)
{
    unsigned int  m_mask = 0;
    unsigned int  m_code_len = 9;
    unsigned char m_character = 0;

    TLzwTable m_table = init_table();
    unsigned int       buffer_size = 0;
    const unsigned int buffer_max  = 24;

    uint32_t         old         = 0;
    uint32_t         code        = 0;
    uint32_t         buffer      = 0;

    TLzwItem           item;

    vector<unsigned char> data;
    string result;
    size_t lLen = buf.length();
    const char *pBuffer = buf.data();
    m_character = *pBuffer;
    while( lLen )
    {
        // Fill the buffer
        while( buffer_size <= (buffer_max-8) && lLen )
        {
            buffer <<= 8;
            buffer |= static_cast<uint32_t>(static_cast<unsigned char>(*pBuffer));
            buffer_size += 8;

            ++pBuffer;
            lLen--;
        }
        // read from the buffer
        while( buffer_size >= m_code_len )
        {
            code         = (buffer >> (buffer_size - m_code_len)) & s_masks[m_mask];
            buffer_size -= m_code_len;

            if( code == s_clear )
            {
                m_mask     = 0;
                m_code_len = 9;

                m_table = init_table();
            }
            else if( code == s_eod )
            {
                lLen = 0;
                break;
            }
            else
            {
                if( code >= m_table.size() )
                {
                    if (old >= m_table.size())
                    {
                        throw pdf_error(FUNC_STRING + "value out of range");
                    }
                    data = m_table[old].value;
                    data.push_back( m_character );
                }
                else
                    data = m_table[code].value;
                result.append(reinterpret_cast<char*>(data.data()), data.size());
                m_character = data[0];
                if( old < m_table.size() ) // fix the first loop
                    data = m_table[old].value;
                data.push_back( m_character );

                item.value = data;
                m_table.push_back( item );

                old = code;

                switch( m_table.size() )
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

