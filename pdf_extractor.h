#ifndef PDF_EXTRACTOR_H
#define PDF_EXTRACTOR_H

#include <string>

std::string pdf2txt(const std::string &buffer);
void pdf_extractor_init();
void pdf_extractor_deinit();

#endif //PDF_EXTRACTOR
