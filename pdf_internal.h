#ifndef PDF_INTERNAL_H
#define PDF_INTERNAL_H

#include <string>
#include <map>
#include <stdexcept>
#include <utility>

#define LEN(S) (sizeof(S) - 1)

enum pdf_object_t {DICTIONARY = 1, ARRAY = 2, STRING = 3, VALUE = 4, INDIRECT_OBJECT = 5, NAME_OBJECT = 6};

#define FUNC_STRING (std::string(__func__) + ": ")
extern const std::map<pdf_object_t, std::string (&)(const std::string&, size_t&)> TYPE2FUNC;

class pdf_error : public std::runtime_error
{
public:
    pdf_error(const char* what) : runtime_error(what)
    {
    }

    pdf_error(const std::string &what) : runtime_error(what)
    {
    }
};

size_t efind_first(const std::string &src, const std::string& str, size_t pos);
size_t efind_first(const std::string &src, const char* s, size_t pos);
size_t efind_first(const std::string &src, const char* s, size_t pos, size_t n);
size_t efind(const std::string &src, const std::string& str, size_t pos);
size_t efind(const std::string &src, const char* s, size_t pos);
size_t efind(const std::string &src, char c, size_t pos);
size_t skip_spaces(const std::string &buffer, size_t offset);
pdf_object_t get_object_type(const std::string &buffer, size_t &offset);
std::string get_value(const std::string &buffer, size_t &offset);
std::string get_array(const std::string &buffer, size_t &offset);
std::string get_name_object(const std::string &buffer, size_t &offset);
std::string get_indirect_object(const std::string &buffer, size_t &offset);
std::string get_string(const std::string &buffer, size_t &offset);
std::string get_dictionary(const std::string &buffer, size_t &offset);
std::string decode_string(const std::string &str);
size_t strict_stoul(const std::string &str);
long int strict_stol(const std::string &str);
std::string predictor_decode(const std::string &data,
                             const std::map<std::string,
                             std::pair<std::string, pdf_object_t>> &opts);
std::map<std::string, std::pair<std::string, pdf_object_t>> get_dictionary_data(const std::string &buffer, size_t offset);
#endif //PDF_INTERNAL
