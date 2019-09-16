#include <stdlib.h>
#include <errno.h>

#include <limits>
#include <exception>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <regex>
#include <cctype>

#include "pdf_internal.h"

using namespace std;

namespace
{
    char get_octal_char(const string &str, size_t &i)
    {
        // PDF doc: The number ddd may consist of one, two, or three octal digits; high-order overflow shall be ignored.
        //          Three octal digits shall be used, with leading zeros as needed, if the next character of the string
        //          is also a digit.
        size_t j = i;
        for (j = i; j < str.size(); ++j)
            if (!isdigit(str[j])) break;
        size_t len = j - i;
        if (len >= 4)
        {
            i += len - 3;
            len = 3;
        }

        long int result = strtol(str.substr(i, len).c_str(), nullptr, 8);
        if (errno != 0) throw pdf_error(FUNC_STRING + "wrong octal number: " + str.substr(i, len));
        if (result > numeric_limits<unsigned char>::max())
        {
            throw pdf_error(FUNC_STRING + "octal number " + to_string(result) + " is larger than 8 bit");
        }
        i += len - 1;
        return static_cast<char>(result);
    }

    char get_unescaped_char(const string &str, size_t &i)
    {
        if (i == str.size() - 1) return char();
//Table 3 –  Escape sequences in literal strings
        ++i;
        switch (str[i])
        {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case ')':
        case '(':
        case '\\':
            return str[i];
        default:
            break;
        }
        if (isdigit(str[i])) return get_octal_char(str, i);
        return str[i];
    }

    string unescape_string(const string &str)
    {
        string result;
        for (size_t i = 0; i < str.size(); ++i)
        {
            if (str[i] == '\\') result += get_unescaped_char(str, i);
            else result += str[i];
        }
        return result;
    }

    string hex_decode(const string &hex)
    {
        std::string result;
        for (size_t i = 0; i < hex.length(); i += 2)
        {
            long int d = strtol(hex.substr(i, 2).c_str(), nullptr, 16);
            if (errno != 0) throw pdf_error(FUNC_STRING + "wrong input: " + hex);
            result.push_back(static_cast<char>(d));
        }

        return result;
    }

    bool is_blank(char c)
    {
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') return true;
        return false;
    }

    unsigned int get_decode_key(const map<string, pair<string, pdf_object_t>> &opts, const string &key, unsigned int def)
    {
        auto it = opts.find(key);
        if (it == opts.end()) return def;

        return strict_stoul(it->second.first);
    }

    size_t find_name_end_delimiter(const string &buffer, size_t offset)
    {
        size_t result = buffer.find_first_of("\r\t\n /", offset + 1);
        size_t dict_end_offset = buffer.find(">>", offset);
        if (result == string::npos || dict_end_offset < result) result = dict_end_offset;
        if (result == string::npos) throw pdf_error(FUNC_STRING + " can`t find end delimiter for value");
        return result;
    }

    size_t find_value_end_delimiter(const string &buffer, size_t offset)
    {
        size_t result = buffer.find_first_of("\r\t\n /", offset);
        size_t dict_end_offset = buffer.find(">>", offset);
        if (result == string::npos || dict_end_offset < result) result = dict_end_offset;
        if (result == string::npos) throw pdf_error(FUNC_STRING + " can`t find end delimiter for value");
        return result;
    }
}

const map<pdf_object_t, string (&)(const string&, size_t&)> TYPE2FUNC = {{DICTIONARY, get_dictionary},
                                                                         {ARRAY, get_array},
                                                                         {STRING, get_string},
                                                                         {VALUE, get_value},
                                                                         {INDIRECT_OBJECT, get_indirect_object},
                                                                         {NAME_OBJECT, get_name_object}};

size_t efind_first(const string &src, const string& str, size_t pos)
{
    size_t ret = src.find_first_of(str, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first(const string &src, const char* s, size_t pos)
{
    size_t ret = src.find_first_of(s, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first(const string &src, const char* s, size_t pos, size_t n)
{
    size_t ret = src.find_first_of(s, pos, n);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const string& str, size_t pos)
{
    size_t ret = src.find(str, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, const char* s, size_t pos)
{
    size_t ret = src.find(s, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind(const string &src, char c, size_t pos)
{
    size_t ret = src.find(c, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + c + " in pos " + to_string(pos) + " failed");
    return ret;
}

pdf_object_t get_object_type(const string &buffer, size_t &offset)
{
    offset = skip_spaces(buffer, offset);
    const string str = buffer.substr(offset, 2);
    if (str == "<<") return DICTIONARY;
    if (str[0] == '[') return ARRAY;
    if (str[0] == '<' || str[0] == '(') return STRING;
    if (str[0] == '/') return NAME_OBJECT;
    if (regex_search(buffer.substr(offset), regex("^[0-9]+[\r\t\n ]+[0-9]+[\r\t\n ]+R"))) return INDIRECT_OBJECT;
    return VALUE;
}

string get_dictionary(const string &buffer, size_t &offset)
{
    if (buffer.substr(offset, 2) != "<<") throw pdf_error(FUNC_STRING + "dictionary must be started with '<<'");
    stack<pdf_object_t> prevs;
    size_t end_offset = offset + 2;
    while (end_offset < buffer.length())
    {
        char c = buffer.at(end_offset);
        char c_next = buffer.at(end_offset + 1);
        if (c == '<' && c_next == '<')
        {
            prevs.push(DICTIONARY);
            end_offset += 2;
            continue;
        }
        if (c == '(' || c == '<')
        {
            get_string(buffer, end_offset);
            continue;
        }
        if (c == '>' && c_next == '>')
        {
            if (prevs.empty())
            {
                end_offset += 2;
                size_t start_offset = offset;
                offset = end_offset;
                return buffer.substr(start_offset, end_offset - start_offset);
            }
            prevs.pop();
            end_offset += 2;
            continue;
        }
        ++end_offset;
    }
    if (end_offset >= buffer.length()) throw pdf_error(FUNC_STRING + "can`t find dictionary end delimiter");
}

string get_name_object(const string &buffer, size_t &offset)
{
    size_t start_offset = offset;
    offset = find_name_end_delimiter(buffer, offset);

    return buffer.substr(start_offset, offset - start_offset);
}

string get_value(const string &buffer, size_t &offset)
{
    size_t start_offset = offset;
    offset = find_value_end_delimiter(buffer, offset);

    return buffer.substr(start_offset, offset - start_offset);
}

string get_indirect_object(const string &buffer, size_t &offset)
{
    size_t start_offset = offset;
    offset = efind(buffer, 'R', offset) + 1;

    return buffer.substr(start_offset, offset - start_offset);
}

string get_string(const string &buffer, size_t &offset)
{
    char delimiter = buffer.at(offset);
    if (delimiter != '(' && delimiter != '<') throw pdf_error(FUNC_STRING + "string must start with '(' or '<'");
    char end_delimiter = delimiter == '('? ')' : '>';
    stack<pdf_object_t> prevs;
    string result(1, delimiter);
    ++offset;
    for (bool is_escaped = false; ; ++offset)
    {
        if (buffer.at(offset) == '\\')
        {
            is_escaped = !is_escaped;
            result.push_back(buffer[offset]);
            continue;
        }
        result.push_back(buffer[offset]);
        if (is_escaped)
        {
            is_escaped = false;
            continue;
        }

        if (buffer[offset] == delimiter)
        {
            prevs.push(STRING);
        }
        if (buffer[offset] == end_delimiter)
        {
            if (prevs.empty())
            {
                ++offset;
                return result;
            }
            prevs.pop();
        }
    }
}

string decode_string(const string &str)
{
    if (str.size() < 3) return string();
    return (str[0] == '<')? hex_decode(str.substr(1, str.size() - 2)) : unescape_string(str.substr(1, str.size() - 2));
}

string get_array(const string &buffer, size_t &offset)
{
    if (buffer[offset] != '[') throw pdf_error(FUNC_STRING + "offset should point to '['");
    string result = "[";
    ++offset;
    stack<pdf_object_t> prevs;
    while (true)
    {
        switch (buffer[offset])
        {
        case '(':
        case '<':
            result += get_string(buffer, offset);
            continue;
            break;
        case '[':
            result.push_back(buffer.at(offset));
            prevs.push(ARRAY);
            break;
        case ']':
            result.push_back(buffer.at(offset));
            if (prevs.empty())
            {
                ++offset;
                return result;
            }
            prevs.pop();
            break;
        default:
            result.push_back(buffer.at(offset));
            break;
        }
        ++offset;
    }
    throw pdf_error(FUNC_STRING + " no array in " + to_string(offset));
}

map<string, pair<string, pdf_object_t>> get_dictionary_data(const string &buffer, size_t offset)
{
    offset = efind(buffer, "<<", offset);
    offset += LEN("<<");
    map<string, pair<string, pdf_object_t>> result;
    while (true)
    {
        offset = skip_spaces(buffer, offset);
        if (buffer.at(offset) == '>' && buffer.at(offset + 1) == '>') return result;
        if (buffer[offset] != '/') throw pdf_error(FUNC_STRING + "Can`t find name key");
        size_t end_offset = efind_first(buffer, "\r\t\n /<[(", offset + 1);
        const string key = buffer.substr(offset, end_offset - offset);
        offset = end_offset;
        pdf_object_t type = get_object_type(buffer, offset);
        const string val = TYPE2FUNC.at(type)(buffer, offset);
        result.insert(make_pair(key, make_pair(val, type)));
    }
}

size_t skip_spaces(const string &buffer, size_t offset)
{
    while (offset < buffer.length() && is_blank(buffer[offset])) ++offset;
    if (offset >= buffer.length()) throw pdf_error(FUNC_STRING + "no data after space");
    return offset;
}

string predictor_decode(const string &data, const map<string, pair<string, pdf_object_t>> &opts)
{
    unsigned int predictor = get_decode_key(opts, "/Predictor", 1);
    unsigned int colors = get_decode_key(opts, "/Colors", 1);
    unsigned int BPCs = get_decode_key(opts, "/BitsPerComponent", 8);
    unsigned int columns     = get_decode_key(opts, "/Columns", 1);
    unsigned int early_change = get_decode_key(opts, "/EarlyChange", 1);
    bool next_byte_is_predictor = predictor >= 10? true: false;
    unsigned int cur_predictor = predictor >= 10? -1 : predictor;
    unsigned int cur_row_index  = 0;
    unsigned int bpp  = (BPCs * colors) >> 3;
    unsigned int rows = (columns * colors * BPCs) >> 3;
    vector<char> prev(rows, 0);

    if (predictor == 1) return data;

    const char *p_buffer = data.c_str();
    size_t len = data.length();
    string result;
    while (len--)
    {
        if (next_byte_is_predictor)
        {
            cur_predictor = *p_buffer + 10;
            next_byte_is_predictor = false;
        }
        else
        {
            switch (cur_predictor )
            {
            case 2: // Tiff Predictor
            {
                if (BPCs == 8)
                {   // Same as png sub
                    int prev_local = cur_row_index - bpp < 0 ? 0 : prev[cur_row_index - bpp];
                    prev[cur_row_index] = *p_buffer + prev_local;
                    break;
                }

                throw pdf_error(FUNC_STRING + "tiff predictor other than 8 BPC is not implemented");
                break;
            }
            case 10: // png none
            {
                prev[cur_row_index] = *p_buffer;
                break;
            }
            case 11: // png sub
            {
                int local_prev = cur_row_index - bpp < 0? 0 : prev[cur_row_index - bpp];
                prev[cur_row_index] = *p_buffer + local_prev;
                break;
            }
            case 12: // png up
            {
                prev[cur_row_index] += *p_buffer;
                break;
            }
            case 13: // png average
            {
                int local_prev = cur_row_index - bpp < 0? 0 : prev[cur_row_index - bpp];
                prev[cur_row_index] = ((local_prev + prev[cur_row_index]) >> 1) + *p_buffer;
                break;
            }
            case 14: // png paeth
            case 15: // png optimum
            {
                throw pdf_error(FUNC_STRING + "predictor " + to_string(cur_predictor) + " is invalid");
                break;
            }
            default:
            {
                //    throw pdf_error(FUNC_STRING + "predictor " + to_string(cur_predictor) + " is invalid");
                break;
            }
            }
            ++cur_row_index;
        }
        ++p_buffer;

        if (cur_row_index >= rows )
        {   // One line finished
            cur_row_index  = 0;
            next_byte_is_predictor = (cur_predictor >= 10);
            result.append(prev.data(), rows);
        }
    }

    return result;
}

size_t strict_stoul(const string &str)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    if (str.find('-') != string::npos) throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    size_t val;
    try
    {
        val = stoul(str, &pos);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not unsigned number");

    return val;
}

long int strict_stol(const string &str)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    size_t val;
    try
    {
        val = stol(str, &pos);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not number");

    return val;
}
