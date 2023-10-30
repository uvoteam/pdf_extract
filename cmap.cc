#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include <boost/optional.hpp>

#include "object_storage.h"
#include "common.h"
#include "cmap.h"

using namespace std;

namespace
{
    enum State_t { NONE, BFCHAR, BFRANGE, WMODE };
    const char *hex_digits = "01234567890abcdefABCDEF";
    struct token_t
    {
        enum token_type_t { DEC, HEX, ARRAY, NONE };
        token_t(token_type_t type_arg, string &&val_arg) : type(type_arg), val(std::move(val_arg))
        {
            if (val.empty()) throw pdf_error(FUNC_STRING + "string is empty");
        }
        token_type_t type;
        string val;
    };

    void get_sizes(vector<unsigned char> &sizes)
    {
        sizes[0] = 0;
        size_t i = 0;
        for (size_t j = 1; j < cmap_t::MAX_CODE_LENGTH + 1; ++j)
        {
            if (!sizes[j]) continue;
            sizes[i] = j;
            sizes[j] = 0;
            i = find(sizes.begin() + i, sizes.end(), 0) - sizes.begin();
        }
        sizes.resize(i);
    }

    size_t trim_leading_zeroes(const string &s)
    {
        for (size_t i = 0; i < s.length(); i++)
        {
            if (s[i] != 0) return i;
        }
        return s.length();
    }

    bool is_less_equal(const string &s1, const string &s2)
    {
        size_t s1_i = trim_leading_zeroes(s1);
        size_t s2_i = trim_leading_zeroes(s2);
        size_t s1_length = s1.length() - s1_i;
        size_t s2_length = s2.length() - s2_i;
        if (s1_length < s2_length) return true;
        if (s1_length > s2_length) return false;
        for (size_t i1 = s1_i, i2 = s2_i; i1 < s1.length() && i2 < s2.length(); ++i1, ++i2)
        {
            unsigned char c1 = s1[i1];
            unsigned char c2 = s2[i2];
            if (c1 < c2) return true;
            if (c1 > c2) return false;
        }
        return true;
    }

    token_t get_token(const string &line, size_t &offset)
    {
        size_t start = line.find_first_of("<[", offset);
        token_t::token_type_t type = token_t::NONE;
        if (start == string::npos)
        {
            start = efind_number(line, offset);
            type = token_t::DEC;
        }
        else if (line[start] == '<')
        {
            type = token_t::HEX;
            ++start;
        }
        else if (line[start] == '[')
        {
            type = token_t::ARRAY;
            ++start;
        }
        size_t end;
        switch (type)
        {
        case token_t::HEX:
            end = efind(line, '>', start);
            break;
        case token_t::ARRAY:
            end = efind(line, ']', start);
            break;
        case token_t::DEC:
            end = line.find_first_of(" \t\n", start);
            break;
        default:
            throw pdf_error(FUNC_STRING + "wrong type=" + to_string(type));
        }
        if (end == string::npos) end = line.length();
        offset = end;

        return token_t(type, line.substr(start, end - start));
    }

    //get utf16be symbols from hex
    string get_hex_val(const string &hex_str)
    {
        size_t n = hex_str.length() / 2 + (hex_str.length() % 2);
        string result(n, 0);
        for (size_t j = 0, i = 0; j < result.length(); ++j, i += 2) result[j] = strict_stoul(hex_str.substr(i, 2), 16);
        return result;
    }

    string convert2string(const token_t &token)
    {
        switch (token.type)
        {
        case token_t::HEX:
        {
            string result;
            for(size_t i = efind_first(token.val, hex_digits, 0), end = token.val.find_first_of(" \t", i);
                i != string::npos;
                i = token.val.find_first_of(hex_digits, end), end = token.val.find_first_of(" \t", i))
            {
                if (end == string::npos) end = token.val.length();
                result.append(get_hex_val(token.val.substr(i, end - i)));
            }
            return result;
        }
        case token_t::DEC:
            return num2string(strict_stoul(token.val, 10));
        default:
            throw pdf_error(FUNC_STRING + "wrong token type. val =" + token.val);
        }
    }

    void inc(string& n)
    {
        if (n.empty()) throw pdf_error(FUNC_STRING + "string is empty");
        char byte;
        for (int i = n.length() - 1; i >= 0; --i)
        {
            ++n[i];
            byte = n[i];
            if (byte != 0) break;
        }
        if (byte == 0) n = '\x01' + n;
    }

    size_t get_bfrange(const string &stream, size_t offset, cmap_t &cmap)
    {
        const string first = convert2string(get_token(stream, offset));
        const string second = convert2string(get_token(stream, offset));
        const token_t third_token = get_token(stream, offset);
        switch (third_token.type)
        {
        case token_t::HEX:
        case token_t::DEC:
        {
            string third = convert2string(third_token);
            for (string n = first; is_less_equal(n, second); inc(n), inc(third))
            {
                cmap.utf_map.emplace(n, make_pair(cmap_t::NOT_CONVERTED, third));
                cmap.sizes[n.length()] = 1;
            }
            break;
        }
        case token_t::ARRAY:
        {
            size_t token_offset = 0;
            for (string n = first; is_less_equal(n, second); inc(n))
            {
                cmap.utf_map.emplace(n, make_pair(cmap_t::NOT_CONVERTED,
                                                  convert2string(get_token(third_token.val, token_offset))));
                cmap.sizes[n.length()] = 1;
            }
            break;
        }
        default:
            throw pdf_error(FUNC_STRING + "wrong type=" + to_string(third_token.type));
        }
        return offset + 1;
    }

    size_t get_wmode(const string &stream, size_t offset, bool &is_vertical)
    {
        is_vertical = (strict_stoul(get_value(stream, offset)) == 1)? true : false;
        return offset;
    }

    boost::optional<string> try_get_string(const string &stream, size_t &offset)
    {
        try
        {
            return convert2string(get_token(stream, offset));
        }
        catch (...)
        {
            return boost::none;
        }
        return boost::none;
    }

    size_t get_bfchar(const string &stream, size_t offset, cmap_t &cmap)
    {
        const boost::optional<string> src = try_get_string(stream, offset);
        const boost::optional<string> dst = try_get_string(stream, offset);
        if (!src || !dst) return offset + 1;
        cmap.utf_map.emplace(*src, make_pair(cmap_t::NOT_CONVERTED, *dst));
        cmap.sizes[src->length()] = 1;
        return offset + 1;
    }

    boost::optional<State_t> get_state(const string &token)
    {
        if (token == "beginbfchar") return BFCHAR;
        if (token == "beginbfrange") return BFRANGE;
        if (token == "endbfchar" || token == "endbfrange") return NONE;
        if (token == "/WMode") return WMODE;

        return boost::none;
    }
}

cmap_t get_cmap(const string &doc,
                const ObjectStorage &storage,
                const pair<unsigned int, unsigned int> &cmap_id_gen,
                const dict_t &decrypt_data)
{
    State_t state = NONE;
    const string stream = get_stream(doc, cmap_id_gen, storage, decrypt_data);
    cmap_t result;
    result.is_vertical = false;
    for (size_t start = stream.find_first_not_of(" \t\n\r"), end = stream.find_first_of(" \t\n\r", start);
         start != string::npos;
         start = stream.find_first_not_of(" \t\n\r", end), end = stream.find_first_of(" \t\n\r", start))
    {
        if (end == string::npos) end = stream.length();
        if (stream[start] == '%')
        {
            end = stream.find('\n', start);
            if (end == string::npos) break;
        }
        string token = stream.substr(start, end - start);
        boost::optional<State_t> r = get_state(token);
        if (r)
        {
            state = *r;
            continue;
        }
        switch (state)
        {
        case NONE:
            continue;
        case BFCHAR:
            end = get_bfchar(stream, start, result);
            break;
        case BFRANGE:
            end = get_bfrange(stream, start, result);
            break;
        case WMODE:
            end = get_wmode(stream, start, result.is_vertical);
            state = NONE;
            break;
        }
        if (end == string::npos || end > (stream.length() - 2)) break;
        start = end + 1;
    }
    get_sizes(result.sizes);
    return result;
}
