#include <string>
#include <map>
#include <regex>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include <boost/optional.hpp>

#include "object_storage.h"
#include "common.h"
#include "cmap.h"


using namespace std;
using namespace boost;

namespace
{
    enum { CODE_SPACE_RANGE_TOKEN_NUM = 2 };
    enum State_t { NONE, BFCHAR, BFRANGE, CODE_SPACE_RANGE};
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

    unsigned int convert2number(const token_t &token)
    {
        switch (token.type)
        {
        case token_t::HEX:
            return strict_stoul(token.val, 16);
        case token_t::DEC:
            return strict_stoul(token.val, 10);
        default:
            throw pdf_error(FUNC_STRING + "wrong token type. val =" + token.val);
        }
    }
    //convert to utf16-le symbol
    string num2string(unsigned int n)
    {
        if (n == 0) return string(2, 0);
        string result;
        while (n)
        {
            result.push_back(n & 0xFF);
            n >>= 8;
        }
        if (result.length() == 1) result.push_back(0);
        return result;
    }

    //get utf16le symbols from hex
    string get_hex_val(const string &hex_str)
    {
        size_t n = hex_str.length() / 2 + (hex_str.length() % 2);
        string result(n, 0);
        for (int j = result.length() - 1, i = 0; j >= 0; --j, i += 2) result[j] = strict_stoul(hex_str.substr(i, 2), 16);
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

    size_t get_bfrange(const string &stream, size_t offset, unordered_map<unsigned int, string> &utf16_map)
    {
        unsigned int first = convert2number(get_token(stream, offset));
        unsigned int second = convert2number(get_token(stream, offset));
        token_t third_token = get_token(stream, offset);
        switch (third_token.type)
        {
        case token_t::HEX:
        case token_t::DEC:
        {
            unsigned int third = convert2number(third_token);
            for (unsigned int n = first; n <= second; ++n, ++third) utf16_map.insert(make_pair(n, num2string(third)));
            break;
        }
        case token_t::ARRAY:
        {
            size_t token_offset = 0;
            for (unsigned int n = first; n <= second; ++n)
            {
                utf16_map.insert(make_pair(n, num2string(convert2number(get_token(third_token.val, token_offset)))));
            }
            break;
        }
        }
        return offset + 1;
    }

    size_t get_bfchar(const string &stream, size_t offset, unordered_map<unsigned int, string> &utf16_map)
    {
        unsigned int num = convert2number(get_token(stream, offset));
        const string str = convert2string(get_token(stream, offset));
        utf16_map.insert(make_pair(num, str));
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
    size_t get_code_space_range(const string &stream, size_t offset, unordered_set<unsigned char> &sizes)
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
            unsigned char v = get_nbytes(strict_stoul(token.val, base));
            if (v > sizeof(unsigned int)) throw pdf_error(FUNC_STRING + "wrong size number. val= " + token.val);
            if (v > max) max = v;
        }

        for (unsigned char i = 1; i <= max; ++i) sizes.insert(i);

        return offset + 1;
    }

    bool is_line_empty(const string &s)
    {
        for (char c : s)
        {
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
        }
        return true;
    }

    optional<State_t> get_state(const string &line)
    {
        if (regex_search(line, regex("^[\t ]*[0-9]+[\t ]+beginbfchar[ \t\n]"))) return BFCHAR;
        if (regex_search(line, regex("^[\t ]*[0-9]+[\t ]+beginbfrange[ \t\n]"))) return BFRANGE;
        if (regex_search(line, regex("^[\t ]*[0-9]+[\t ]+begincodespacerange[ \t\n]"))) return CODE_SPACE_RANGE;
        if (regex_search(line, regex("^[\t ]*endbfchar[ \t\n]")) ||
            regex_search(line, regex("^[\t ]*endbfrange[ \t\n]")) ||
            regex_search(line, regex("^[\t ]*endcodespacerange[ \t\n]"))) return NONE;

        return boost::none;
    }
}

cmap_t get_cmap(const string &doc,
                const ObjectStorage &storage,
                const pair<unsigned int, unsigned int> &cmap_id_gen,
                const map<string, pair<string, pdf_object_t>> &decrypt_data)
{
    State_t state = NONE;
    const string stream = get_stream(doc, cmap_id_gen, storage, decrypt_data);
    cmap_t result;
    for (size_t end = stream.find('\n', 0), start = 0;
         start < stream.length();
         start = end + 1, end = stream.find('\n', start))
    {
        if (end == string::npos) end = stream.length() - 1;
        string line = stream.substr(start, end - start + 1);
        size_t comment_position = line.find('%');
        if (comment_position == 0) continue;
        if (comment_position != string::npos) line = line.substr(0, comment_position);
        if (is_line_empty(line)) continue;
        optional<State_t> r = get_state(line);
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
        }
    }
    return result;
}
