#include <string>
#include <utility>
#include <vector>

#include "object_storage.h"
#include "common.h"
#include "cmap.h"

using namespace std;

//https://docs.microsoft.com/en-us/typography/opentype/spec/otff
//https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6cmap.html
void get_format4_data(cmap_t &cmap, const string &stream, size_t off);

cmap_t get_FontFile2(const string &doc,
                     const ObjectStorage &storage,
                     const pair<unsigned int, unsigned int> &cmap_id_gen,
                     const dict_t &decrypt_data)
{
    enum { TAG_SIZE = 4 };
    const string stream = get_stream(doc, cmap_id_gen, storage, decrypt_data);
    uint16_t tables_num = get_integer<uint16_t>(stream, sizeof(uint32_t));
    uint16_t i = 0;
    for (i = 0; i < tables_num; ++i)
    {
        if (stream.substr(i * (TAG_SIZE + sizeof(uint32_t) * 3) + (sizeof(uint32_t) + sizeof(uint16_t) * 4),
                          TAG_SIZE) == "cmap") break;
    }
    if (i >= tables_num) return cmap_t();
    uint32_t table_offset = get_integer<uint32_t>(stream, i * 16 + 20);
    size_t offset = table_offset + sizeof(uint16_t);
    uint16_t subtables_num = get_integer<uint16_t>(stream, offset);
    vector<size_t> mapping_offsets;
    mapping_offsets.reserve(subtables_num);
    offset += sizeof(uint16_t) * 3;
    for (uint16_t i = 0; i < subtables_num; ++i, offset += (sizeof(uint16_t) * 2) + sizeof(uint32_t))
    {
        mapping_offsets.push_back(table_offset + get_integer<uint32_t>(stream, offset));
    }
    cmap_t result;
    result.sizes[0] = sizeof(uint16_t);
    result.is_vertical = false;
    for (size_t off : mapping_offsets)
    {
        uint16_t format_id = get_integer<uint16_t>(stream, off);
        if (format_id == 4) get_format4_data(result, stream, off);
    }
    return result;
}

template <class T> vector<T> get_array(const string &stream, size_t &off, uint16_t num)
{
    vector<T> result;
    result.reserve(num);
    for (uint16_t i = 0; i < num; ++i, off += sizeof(T)) result.push_back(get_integer<T>(stream, off));
    return result;
}

string get_utf8(uint32_t c)
{
    union
    {
        uint32_t v;
        char str[sizeof(uint32_t)];
    } u;
    u.v = c;
    return string(u.str, sizeof(uint32_t));
}

void get_format4_data(cmap_t &cmap, const string &stream, size_t off)
{
    enum { FINAL_ENC_VAL = 0xFFFF };
    off += sizeof(uint16_t) * 3;
    uint16_t seg_count = get_integer<uint16_t>(stream, off) / 2;
    off += sizeof(uint16_t) * 4;
    vector<uint16_t> ecs = get_array<uint16_t>(stream, off, seg_count);
    off += sizeof(uint16_t);
    vector<uint16_t> scs = get_array<uint16_t>(stream, off, seg_count);
    vector<int16_t> idds = get_array<int16_t>(stream, off, seg_count);
    size_t pos = off;
    vector<uint16_t> idrs = get_array<uint16_t>(stream, off, seg_count);
    for (uint16_t i = 0; i < seg_count; ++i)
    {
        if (ecs[i] == FINAL_ENC_VAL) continue;
        if (idrs[i])
        {
            size_t off2 = pos + idrs[i];
            for (uint32_t c = scs[i]; c <= ecs[i]; ++c, off2 += sizeof(uint16_t))
            {
                cmap.utf_map.emplace(num2string(get_integer<uint16_t>(stream, off2) + idds[i]),
                                     make_pair(cmap_t::CONVERTED, string(1, c & 0xFF)));
            }
        }
        else
        {
            for (uint32_t c = scs[i]; c <= ecs[i]; ++c)
            {
                cmap.utf_map.emplace(num2string(c + idds[i]), make_pair(cmap_t::CONVERTED, string(1, c & 0xFF)));
            }
        }
    }
}
