#include "file_utils.h"
#include "string_utils.h"

#include <string>
using std::string;
#include <fstream>
using std::ifstream;
using std::ofstream;
#include <memory>
using std::unique_ptr;
#include <vector>
using std::vector;
#include <stdio.h>

namespace util {

CsvFileReader::CsvFileReader() 
      : field_term_(','),
        line_term_('\n'),
        enclosure_('"') {
}

CsvFileReader::~CsvFileReader() {
}

bool CsvFileReader::Init(const string& fname) {
   file_stream_.reset(new ifstream(fname, ifstream::in | ifstream::binary));
   return !file_stream_->fail();
}

// static
CsvFileReader* CsvFileReader::ConstructNew(const string& fname) {
   unique_ptr<CsvFileReader> reader(new CsvFileReader);
   if (reader->Init(fname)) {
      return reader.release();
   } else {
      return 0;
   }
}

void CsvFileReader::set_field_term(char terminator) {
   field_term_ = terminator;
}

char CsvFileReader::field_term() const {
   return field_term_;
}

void CsvFileReader::set_line_term(char terminator) {
   line_term_ = terminator;
}

char CsvFileReader::line_term() const {
   return line_term_;
}

void CsvFileReader::set_enclosure(char enclosure_char) {
   enclosure_ = enclosure_char;
}

char CsvFileReader::enclosure() const {
   return enclosure_;
}

bool CsvFileReader::HasMoreRows() {
   return !file_stream_->eof();
}

vector<string> CsvFileReader::GetNextRow() {
   vector<string> row;
   GetNextRow(&row);
   return row;
}

size_t CsvFileReader::FindNextTokenBegin(const string& row, size_t pos) {
   size_t quoted_pos = row.find(string(1, field_term_) + string(1, enclosure_), pos);
   size_t null_pos = row.find(string(1, field_term_) + "\\N", pos);
   if (quoted_pos != string::npos) quoted_pos++;
   if (null_pos != string::npos) null_pos++;

   if (quoted_pos == string::npos) {
      return null_pos;  // pos of \ in ,\N
   } else if (null_pos == string::npos) {
      return quoted_pos;  // pos of the " in ,"
   } else {
      return (null_pos < quoted_pos ? null_pos : quoted_pos);
   }
}

size_t CountConsecPreceding(const string& row, size_t pos, char c) {
   if (pos <= 0) return 0;
   for (size_t i = pos-1; i >= 0; --i) {
      if (row[i] != c) {
         return pos - i - 1;
      }
   }
   return pos;
}

size_t CsvFileReader::FindNextTokenEnd(const string& row, size_t pos) {
   size_t end_quote = row.find(string(1, enclosure_) + string(1, field_term_), pos);
   while (end_quote > 0 && end_quote != string::npos
          && CountConsecPreceding(row, end_quote, '\\')%2 == 1) {
      end_quote = row.find(string(1, enclosure_) + string(1, field_term_), end_quote+1);
   }
   size_t null_pos = row.find("\\N" + string(1, field_term_), pos);
   if (null_pos != string::npos) null_pos++;

   if (end_quote == string::npos) {
      return null_pos;  // pos of "N" in "\N"
   } else if (null_pos == string::npos) {
      return end_quote;   // pos of the "
   } else {
      return (null_pos < end_quote ? null_pos : end_quote);
   }
}

void CsvFileReader::GetNextRow(vector<string>* row_out) {
   row_out->clear();

   string row;
   std::getline(*file_stream_, row, line_term_);
   if (row.empty()) {
      return;
   }

   size_t idx = 0;
   while (idx < row.size() && idx != string::npos) {
      size_t jdx = FindNextTokenEnd(row, idx);
      if (jdx == string::npos) {
         jdx = row.size()-1;
      }

      size_t len = jdx - idx + 1;
      string str = row.substr(idx, len);
      if (str == "\\N") {
         str = "";
      } else if (str.front() == '\"' && str.back() == '\"') {
         str = str.substr(1, str.size()-2);
      }
      row_out->push_back(str);

      idx = FindNextTokenBegin(row, jdx);
   }
}

bool DoesFileExist(const string& fname) {
   ifstream in_stream(fname);
   return in_stream;
}

void DeleteFile(const string& fname) {
   remove(fname.c_str());
}

void WriteStringToFile(const std::string& fname, const std::string& str) {
   ofstream out_stream(
         fname, ofstream::out | ofstream::binary | ofstream::trunc);
   out_stream.write(str.data(), str.size());
   out_stream.close();
}

void AppendStringToFile(const std::string& fname, const std::string& str) {
   ofstream out_stream(fname, ofstream::out | ofstream::binary | ofstream::app);
   out_stream.write(str.data(), str.size());
   out_stream.close();
}

string ReadFileToString(const string& fname) {
   ifstream in_stream(fname, ifstream::in | ifstream::binary);
   if (in_stream) {
      string contents;
      in_stream.seekg(0, std::ios::end);
      contents.resize(in_stream.tellg());
      in_stream.seekg(0, std::ios::beg);
      in_stream.read(&contents[0], contents.size());
      in_stream.close();
      return contents;
   } else {
      return string();
   }
}

};  // namespace util
