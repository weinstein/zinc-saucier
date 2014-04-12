#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace util {

class CsvFileReader {
 public:
   ~CsvFileReader();
   static CsvFileReader* ConstructNew(const std::string& fname);

   void set_field_term(char terminator);
   char field_term() const;
   void set_line_term(char terminator);
   char line_term() const;
   void set_enclosure(char enclosure_char);
   char enclosure() const;

   bool HasMoreRows();
   std::vector<std::string> GetNextRow();
   void GetNextRow(std::vector<std::string>* row_out);

 private:
   char field_term_;
   char line_term_;
   char enclosure_;
   std::unique_ptr<std::ifstream> file_stream_;

   CsvFileReader();
   bool Init(const std::string& fname);
   size_t FindNextTokenBegin(const std::string& str, size_t pos);
   size_t FindNextTokenEnd(const std::string& str, size_t pos);
};

bool DoesFileExist(const std::string& fname);
void DeleteFile(const std::string& fname);
void WriteStringToFile(const std::string& fname, const std::string& str);
void AppendStringToFile(const std::string& fname, const std::string& str);
std::string ReadFileToString(const std::string& fname);

std::vector<std::string> GetFilesInDirectory(const std::string& dir);
std::vector<std::string> GetFilesInDirectory(const std::string& dir,
                                             const std::string& pref,
                                             const std::string& suff);

};  // namespace util

#endif
