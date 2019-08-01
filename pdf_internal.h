#ifndef PDF_INTERNAL_H
#define PDF_INTERNAL_H

#include <string>

#define FUNC_STRING (std::string(__func__) + ": ")


size_t strict_stoul(const std::string &str);
long int strict_stol(const std::string &str);
#endif //PDF_INTERNAL
