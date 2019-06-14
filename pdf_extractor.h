#ifndef PDF_EXTRACTOR_H
#define PDF_EXTRACTOR_H

#include <string>
#include <stdexcept>

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

std::string pdf2txt(const std::string &buffer);

#endif //PDF_EXTRACTOR_H
