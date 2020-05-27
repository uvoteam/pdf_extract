#include <string>
#include <utility>
#include <unordered_map>
#include <algorithm>

#include <boost/optional.hpp>

#include "object_storage.h"
#include "common.h"
#include "cmap.h"


using namespace std;

namespace
{
    enum { CODE_SPACE_RANGE_TOKEN_NUM = 2 };
    enum State_t { NONE, BFCHAR, BFRANGE, CODE_SPACE_RANGE, WMODE };
    const char *hex_digits = "01234567890abcdefABCDEF";
    struct token_t
    {
        enum token_type_t { DEC, HEX, ARRAY};
        token_t(token_type_t type_arg, string &&val_arg) : type(type_arg), val(std::move(val_arg))
        {
            if (val.empty()) throw pdf_error(FUNC_STRING + "string is empty");
        }
        token_type_t type;
        string val;
    };

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
        token_t::token_type_t type;
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
        case ARRAY:
            end = efind(line, ']', start);
            break;
        case token_t::DEC:
            end = line.find_first_of(" \t\n", start);
            break;
        }
        if (end == string::npos) end = line.length();
        offset = end;

        return token_t(type, line.substr(start, end - start));
    }

    //convert to utf16-be symbol
    string num2string(unsigned int n)
    {
        if (n == 0) return string(2, 0);
        string result;
        while (n)
        {
            result = static_cast<char>(n & 0xFF) + result;
            n >>= 8;
        }
        if (result.length() == 1) result = '\x00' + result;
        return result;
    }

    //get utf16be symbols from hex
    string get_hex_val(const string &hex_str)
    {
        size_t n = hex_str.length() / 2 + (hex_str.length() % 2);
        string result(n, 0);
        for (int j = 0, i = 0; j < result.length(); ++j, i += 2) result[j] = strict_stoul(hex_str.substr(i, 2), 16);
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

    size_t get_bfrange(const string &stream, size_t offset, unordered_map<string, string> &utf16_map)
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
            for (string n = first; is_less_equal(n, second); inc(n), inc(third)) utf16_map.insert(make_pair(n, third));
            break;
        }
        case token_t::ARRAY:
        {
            size_t token_offset = 0;
            for (string n = first; is_less_equal(n, second); inc(n))
            {
                utf16_map.insert(make_pair(n, convert2string(get_token(third_token.val, token_offset))));
            }
            break;
        }
        }
        return offset + 1;
    }

    size_t get_wmode(const string &stream, size_t offset, bool &is_vertical)
    {
        is_vertical = (strict_stoul(get_value(stream, offset)) == 1)? true : false;
        return offset;
    }

    size_t get_bfchar(const string &stream, size_t offset, unordered_map<string, string> &utf16_map)
    {
        const string src = convert2string(get_token(stream, offset));
        const string dst = convert2string(get_token(stream, offset));
        utf16_map.insert(make_pair(src, dst));
        return offset + 1;
    }

    unsigned char get_nbytes(unsigned int a)
    {
        if (a == 0) return 1;
        unsigned char result = 0;
        for (; a > 0; a = (a >> 8)) ++result;

        return result;
    }

//return sizes in bytes for code space range
    size_t get_code_space_range(const string &stream, size_t offset, vector<unsigned char> &sizes)
    {
        int base = 0;
        unsigned char max = 0;
        for (size_t j = 0; j < CODE_SPACE_RANGE_TOKEN_NUM; ++j)
        {
            const token_t token = get_token(stream, offset);
            switch (token.type)
            {
            case token_t::HEX:
                base = 16;
                break;
            case token_t::DEC:
                base = 10;
                break;
            default:
                throw pdf_error(FUNC_STRING + "wrong token type.");
            }
            unsigned char v = base == 10? get_nbytes(strict_stoul(token.val)) : token.val.size() / 2;
            if (v > sizeof(unsigned int)) throw pdf_error(FUNC_STRING + "wrong size number. val= " + token.val);
            if (v > max) max = v;
        }

        for (unsigned char i = 1; i <= max; ++i) sizes.push_back(i);

        return offset + 1;
    }

    boost::optional<State_t> get_state(const string &token)
    {
        if (token == "beginbfchar") return BFCHAR;
        if (token == "beginbfrange") return BFRANGE;
        if (token == "begincodespacerange") return CODE_SPACE_RANGE;
        if (token == "endbfchar" || token == "endbfrange" || token == "endcodespacerange") return NONE;
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
            end = get_bfchar(stream, start, result.utf16_map);
            break;
        case BFRANGE:
            end = get_bfrange(stream, start, result.utf16_map);
            break;
        case CODE_SPACE_RANGE:
            end = get_code_space_range(stream, start, result.sizes);
            break;
        case WMODE:
            end = get_wmode(stream, start, result.is_vertical);
            state = NONE;
            break;
        }
        if (end == string::npos || end > (stream.length() - 2)) break;
        start = end + 1;
    }
    if (result.sizes.empty()) throw pdf_error(FUNC_STRING + "no codespacerange in cmap");
    sort(result.sizes.begin(), result.sizes.end());
    result.sizes.erase(unique(result.sizes.begin(), result.sizes.end()), result.sizes.end());
    return result;
}
