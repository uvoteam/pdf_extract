#include <limits>
#include <exception>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <cctype>
#include <utility>
#include <stack>
#include <array>

#include <boost/optional.hpp>

#include "common.h"
#include "charset_converter.h"
#include "object_storage.h"

using namespace std;

extern string decrypt(unsigned int n, unsigned int g, const string &in, const dict_t &decrypt_opts);
extern string flate_decode(const string&, const dict_t&);
extern string lzw_decode(const string&, const dict_t&);
extern string ascii85_decode(const string&, const dict_t&);
extern string ascii_hex_decode(const string&, const dict_t&);

namespace
{
    char get_octal_char(const string &str, size_t &i)
    {
        // PDF doc: The number ddd may consist of one, two, or three octal digits; high-order overflow shall be ignored.
        //          Three octal digits shall be used, with leading zeros as needed, if the next character of the string
        //          is also a digit.
        size_t j = i;
        for (j = i; j < str.size(); ++j)
        {
            if (!isdigit(str[j])) break;
        }
        size_t len = j - i;
        if (len > 3) len = (str[i] == 0)? 4 : 3; //leading zero as oct mark

        size_t result = strict_stoul(str.substr(i, len).c_str(), 8);
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

    string hex_decode(const string &arg)
    {
        string hex;
        for (char c : arg)
        {
            if (c != '\n' && c != '\r' && c != ' ') hex += c;
        }
        string result;
        for (size_t i = 0; i < hex.length(); i += 2) result.append(1, static_cast<char>(strict_stoul(hex.substr(i, 2), 16)));

        return result;
    }

    unsigned int get_decode_key(const dict_t &opts, const string &key, unsigned int def)
    {
        auto it = opts.find(key);
        if (it == opts.end()) return def;

        return strict_stoul(it->second.first);
    }

    size_t find_name_end_delimiter(const string &buffer, size_t offset)
    {
        return efind_first(buffer, "\r\t\n /](<>", offset + 1);
    }

    size_t find_value_end_delimiter(const string &buffer, size_t offset)
    {
        return efind_first(buffer, "\r\t\n /][(<>", offset);
    }

    vector<string> get_filters(const dict_t &props)
    {
        const pair<string, pdf_object_t> filters = props.at("/Filter");
        if (filters.second == NAME_OBJECT) return vector<string>{filters.first};
        if (filters.second != ARRAY) throw pdf_error(FUNC_STRING + "wrong filter type: " + to_string(filters.second));
        vector<string> result;
        for (const array_t::value_type &p : get_array_data(filters.first, 0)) result.push_back(p.first);
        return result;
    }

    vector<dict_t> get_decode_params(const dict_t &src, size_t filters)
    {
        auto it = src.find("/DecodeParms");
        //default decode params for all filters
        if (it == src.end()) return vector<dict_t>(filters);
        const string &params_data = it->second.first;
        if (it->second.second == DICTIONARY)
        {
            return vector<dict_t>{get_dictionary_data(params_data, 0)};
        }
        if (it->second.second != ARRAY) throw pdf_error(FUNC_STRING + "wrong type for /DecodeParms");
        vector<dict_t> result;
        size_t offset = 0;
        while (true)
        {
            offset = params_data.find("<<");
            if (offset == string::npos)
            {
                //7.3.8.2.Stream Extent
                if (result.empty())
                {
                    throw pdf_error(FUNC_STRING + "/DecodeParms must be dictionary or an array of dictionaries");
                }
                return result;
            }
            result.push_back(get_dictionary_data(get_dictionary(params_data, offset), 0));
        }
        return result;
    }
    bool is_indirect_number(const string &s, size_t &offset)
    {
        static const char* DIGITS = "0123456789";
        static const char* SPACE = "\n\t\r ";

        if (!isdigit(s[offset])) return false;
        offset = s.find_first_not_of(DIGITS, offset);
        if (offset == string::npos) return false;
        if (!isspace(s[offset])) return false;
        offset = s.find_first_not_of(SPACE, offset);
        if (offset == string::npos) return false;
        return true;
    }

    bool is_indirect_object(const string &s, size_t offset)
    {
        static const unsigned int INDIRECT_NUMBERS = 2;
        for (unsigned int i = 0; i < INDIRECT_NUMBERS; ++i)
        {
            if (!is_indirect_number(s, offset)) return false;
        }
        return (s[offset] == 'R')? true : false;
    }

    const map<string, string (&)(const string&, const map<string, pair<string, pdf_object_t>>&)> FILTER2FUNC =
                                                           {{"/FlateDecode", flate_decode},
                                                            {"/LZWDecode", lzw_decode},
                                                            {"/ASCII85Decode", ascii85_decode},
                                                            {"/ASCIIHexDecode", ascii_hex_decode}};

}

const map<pdf_object_t, string (&)(const string&, size_t&)> TYPE2FUNC = {{DICTIONARY, get_dictionary},
                                                                         {ARRAY, get_array},
                                                                         {STRING, get_string},
                                                                         {VALUE, get_value},
                                                                         {INDIRECT_OBJECT, get_indirect_object},
                                                                         {NAME_OBJECT, get_name_object}};

bool is_blank(char c)
{
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') return true;
    return false;
}

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

size_t efind_first_not(const string &src, const string& str, size_t pos)
{
    size_t ret = src.find_first_not_of(str, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + str + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first_not(const string &src, const char* s, size_t pos)
{
    size_t ret = src.find_first_not_of(s, pos);
    if (ret == string::npos) throw pdf_error(FUNC_STRING + "for " + s + " in pos " + to_string(pos) + " failed");
    return ret;
}

size_t efind_first_not(const string &src, const char* s, size_t pos, size_t n)
{
    size_t ret = src.find_first_not_of(s, pos, n);
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

size_t skip_spaces(const string &buffer, size_t offset, bool validate /*= true */)
{
    while (offset < buffer.length() && is_blank(buffer[offset])) ++offset;
    if (validate && offset >= buffer.length()) throw pdf_error(FUNC_STRING + "no data after space");
    return offset;
}

size_t skip_comments(const string &buffer, size_t offset, bool validate /*= true */)
{
    while (true)
    {
        offset = skip_spaces(buffer, offset, validate);
        if (offset >= buffer.length() || buffer[offset] != '%') return offset;
        while (offset < buffer.length() && buffer[offset] != '\r' && buffer[offset] != '\n') ++offset;
    }
}

pdf_object_t get_object_type(const string &buffer, size_t &offset)
{
    offset = skip_comments(buffer, offset);
    const string str = buffer.substr(offset, 2);
    if (str == "<<") return DICTIONARY;
    if (str[0] == '[') return ARRAY;
    if (str[0] == '<' || str[0] == '(') return STRING;
    if (str[0] == '/') return NAME_OBJECT;
    if (is_indirect_object(buffer, offset)) return INDIRECT_OBJECT;
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
    return string(); //supress compiler warning
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
    return (str[0] == '<')? hex_decode(str.substr(1, str.size() - 2)) :
                            unescape_string(str.substr(1, str.size() - 2));
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
            result += get_string(buffer, offset);
            continue;
            break;
        case '<':
            result += buffer.at(offset + 1) == '<'? get_dictionary(buffer, offset) : get_string(buffer, offset);
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

dict_t get_dictionary_data(const string &buffer, size_t offset)
{
    offset = efind(buffer, "<<", offset);
    offset += LEN("<<");
    dict_t result;
    while (true)
    {
        offset = skip_comments(buffer, offset);
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

array_t get_array_data(const string &buffer, size_t offset)
{
    offset = efind(buffer, '[', offset);
    ++offset;
    vector<pair<string, pdf_object_t>> result;
    while (true)
    {
        offset = skip_comments(buffer, offset);
        if (buffer.at(offset) == ']') return result;
        pdf_object_t type = get_object_type(buffer, offset);
        string val = TYPE2FUNC.at(type)(buffer, offset);
        result.push_back(make_pair(std::move(val), type));
    }
    return result;
}

string predictor_decode(const string &data, const dict_t &opts)
{
    unsigned int predictor = get_decode_key(opts, "/Predictor", 1);
    unsigned int colors = get_decode_key(opts, "/Colors", 1);
    unsigned int BPCs = get_decode_key(opts, "/BitsPerComponent", 8);
    unsigned int columns     = get_decode_key(opts, "/Columns", 1);
    bool next_byte_is_predictor = predictor >= 10? true: false;
    unsigned int cur_predictor = predictor >= 10? -1 : predictor;
    unsigned int cur_row_index = 0;
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
                    unsigned int prev_local = cur_row_index - bpp < 0 ? 0 : prev[cur_row_index - bpp];
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
                unsigned int local_prev = cur_row_index - bpp < 0? 0 : prev[cur_row_index - bpp];
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

size_t strict_stoul(const string &str, int base /*= 10*/)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    if (str.find('-') != string::npos) throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    size_t val;
    try
    {
        val = stoul(str, &pos, base);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not unsigned number");

    return val;
}

long int strict_stol(const string &str, int base /*= 10*/)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    size_t val;
    try
    {
        val = stol(str, &pos, base);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not number");

    return val;
}

vector<pair<unsigned int, unsigned int>> get_set(const string &array)
{
    vector<pair<unsigned int, unsigned int>> result;
    for (size_t offset = find_number(array, 0); offset < array.length(); offset = find_number(array, offset))
    {
        size_t end_offset = efind_first(array, "  \r\n\t", offset);
        unsigned int id = strict_stoul(array.substr(offset, end_offset - offset));
        offset = efind_number(array, end_offset);
        end_offset = efind_first(array, "  \r\n\t", offset);
        unsigned int gen = strict_stoul(array.substr(offset, end_offset - offset));
        result.push_back(make_pair(id, gen));
        offset = efind(array, 'R', end_offset);
    }
    return result;
}

pair<string, pdf_object_t> get_object(const string &buffer, size_t id, const map<size_t, size_t> &id2offsets)
{
    size_t offset = id2offsets.at(id);
    offset = skip_comments(buffer, offset);
    offset = efind(buffer, "obj", id2offsets.at(id));
    offset += LEN("obj");
    offset = skip_comments(buffer, offset);
    pdf_object_t type = get_object_type(buffer, offset);
    return make_pair(TYPE2FUNC.at(type)(buffer, offset), type);
}

string get_stream(const string &doc,
                  const pair<unsigned int, unsigned int> &id_gen,
                  const ObjectStorage &storage,
                  const dict_t &decrypt_data)
{
    const pair<string, pdf_object_t> stream_pair = storage.get_object(id_gen.first);
    if (stream_pair.second != DICTIONARY) throw pdf_error(FUNC_STRING + "stream must be a dictionary");
    const dict_t props = get_dictionary_data(stream_pair.first, 0);
    const map<size_t, size_t> &id2offsets = storage.get_id2offsets();
    size_t offset = efind(doc, "<<", id2offsets.at(id_gen.first));
    get_dictionary(doc, offset);
    string content = get_content(doc, get_length(doc, storage, props), offset);
    content = decrypt(id_gen.first, id_gen.second, content, decrypt_data);

    return decode(content, props);
}

string get_content(const string &buffer, size_t len, size_t offset)
{
    offset = efind(buffer, "stream", offset);
    offset += LEN("stream");
    if (buffer[offset] == '\r') ++offset;
    if (buffer[offset] == '\n') ++offset;
    return buffer.substr(offset, len);
}

string decode(const string &content, const dict_t &props)
{
    if (!props.count("/Filter")) return content;
    vector<string> filters = get_filters(props);
    vector<dict_t> decode_params = get_decode_params(props, filters.size());
    if (filters.size() != decode_params.size())
    {
        throw pdf_error(FUNC_STRING + "different sizes for filters and decode_params");
    }
    string result = content;
    for (size_t i = 0; i < filters.size(); ++i) result = FILTER2FUNC.at(filters[i])(result, decode_params[i]);
    return result;
}

size_t find_number(const string &buffer, size_t offset)
{
    while (offset < buffer.length() && !isdigit(buffer[offset])) ++offset;
    return offset;
}

size_t efind_number(const string &buffer, size_t offset)
{
    size_t result = find_number(buffer, offset);
    if (result >= buffer.length()) throw pdf_error(FUNC_STRING + "can`t find number");
    return result;
}

pair<unsigned int, unsigned int> get_id_gen(const string &data)
{
    size_t offset = 0;
    size_t end_offset = efind_first(data, "\r\t\n ", offset);
    unsigned int id = strict_stoul(data.substr(offset, end_offset - offset));
    offset = efind_number(data, end_offset);
    end_offset = efind_first(data, "\r\t\n ", offset);
    unsigned int gen = strict_stoul(data.substr(offset, end_offset - offset));
    return make_pair(id, gen);
}

pair<string, pdf_object_t> get_indirect_object_data(const string &indirect_object,
                                                    const ObjectStorage &storage,
                                                    boost::optional<pdf_object_t> type /*=boost::none*/)
{
    pair<string, pdf_object_t> r = storage.get_object(strict_stoul(indirect_object.substr(0,
                                                                                          efind_first(indirect_object,
                                                                                                      " \r\n\t", 0))));
    if (type && r.second != *type) throw pdf_error(FUNC_STRING + "wrong type=" + to_string(*type) + " val=" + r.first);
    return r;
}

pair<float, float> apply_matrix_norm(const matrix_t &matrix, float x, float y)
{
    return make_pair(matrix.at(0) * x + matrix.at(2) * y, matrix.at(1) * x + matrix.at(3) * y);
}

std::string get_dict_val(const dict_t &dict, const string &key, const string &def)
{
    auto it = dict.find(key);
    return (it == dict.end())? def : it->second.first;
}

size_t utf8_length(const string &s)
{
    size_t len = 0;
    //Count all first-bytes (the ones that don't match 10xxxxxx).
    for (char c : s) len += (c & 0xc0) != 0x80;
    return len;
}

matrix_t operator*(const matrix_t &m1, const matrix_t &m2)
{
    return matrix_t{m2[0] * m1[0] + m2[2] * m1[1],
                    m2[1] * m1[0] + m2[3] * m1[1],
                    m2[0] * m1[2] + m2[2] * m1[3],
                    m2[1] * m1[2] + m2[3] * m1[3],
                    m2[0] * m1[4] + m2[2] * m1[5] + m2[4],
                    m2[1] * m1[4] + m2[3] * m1[5] + m2[5]};
}

dict_t get_dict_or_indirect_dict(const pair<string, pdf_object_t> &data, const ObjectStorage &storage)
{
    switch (data.second)
    {
    case DICTIONARY:
        return get_dictionary_data(data.first, 0);
    case INDIRECT_OBJECT:
        return get_dictionary_data(get_indirect_object_data(data.first, storage, DICTIONARY).first, 0);
    default:
        throw pdf_error(FUNC_STRING + "wrong object type " + to_string(data.second));
    }
}

array_t get_array_or_indirect_array(const pair<string, pdf_object_t> &data, const ObjectStorage &storage)
{
    switch (data.second)
    {
    case ARRAY:
        return get_array_data(data.first, 0);
    case INDIRECT_OBJECT:
        return get_array_data(get_indirect_object_data(data.first, storage, ARRAY).first, 0);
    default:
        throw pdf_error(FUNC_STRING + "wrong object type " + to_string(data.second));
    }
}

unsigned int string2num(const std::string &s)
{
    if (s.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    unsigned int result = 0;
    for (int i = s.length() - 1, j = 0; i >= 0; --i, ++j) result |= static_cast<unsigned char>(s[i]) << (8 * j);
    return result;
}

pair<string, pdf_object_t> get_content_len_pair(const string &buffer, size_t id, const map<size_t, size_t> &id2offsets)
{
    return get_object(buffer, id, id2offsets);
}

pair<string, pdf_object_t> get_content_len_pair(const string &buffer, size_t id, const ObjectStorage &storage)
{
    return storage.get_object(id);
}

const matrix_t IDENTITY_MATRIX = matrix_t{1, 0, 0, 1, 0, 0};
