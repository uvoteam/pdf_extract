#ifndef PDF_INTERNAL_H
#define PDF_INTERNAL_H

#include <string>
#include <map>
#include <stdexcept>

enum pdf_object_t {DICTIONARY = 1, ARRAY = 2, STRING = 3, VALUE = 4, INDIRECT_OBJECT = 5, NAME_OBJECT = 6};

#define FUNC_STRING (std::string(__func__) + ": ")

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


size_t strict_stoul(const std::string &str);
long int strict_stol(const std::string &str);
std::string predictor_decode(const std::string &data,
                             const std::map<std::string,
                             std::pair<std::string, pdf_object_t>> &opts);
#endif //PDF_INTERNAL
