#include "file_utils.h"
using namespace util;

#include <gtest/gtest.h>
#include <memory>
using std::unique_ptr;
#include <string>
using std::string;
#include <vector>
using std::vector;

const char kHelloFile[] = "testing/hello.txt";
const char kHelloWorld[] = "Hello, world!\n";
const char kDoesntExist[] = "testing/doesnt.exist";

const char kTempFile[] = "testing/tmp.txt";

const char kCsvFile[] = "testing/csv.txt";

TEST(TestFileUtils, FileExists) {
   EXPECT_TRUE(DoesFileExist(kHelloFile));
   EXPECT_FALSE(DoesFileExist(kDoesntExist));
}

TEST(TestFileUtils, ReadFileToString) {
   string hello_world = ReadFileToString(kHelloFile);
   EXPECT_EQ(hello_world, kHelloWorld);
}

TEST(TestFileUtils, WriteStringToFile) {
   EXPECT_FALSE(DoesFileExist(kTempFile));
   WriteStringToFile(kTempFile, kHelloWorld);

   string contents = ReadFileToString(kTempFile);
   EXPECT_EQ(contents, kHelloWorld);

   AppendStringToFile(kTempFile, kHelloWorld);
   contents = ReadFileToString(kTempFile);
   EXPECT_EQ(contents, string(kHelloWorld) + string(kHelloWorld));

   DeleteFile(kTempFile);
   EXPECT_FALSE(DoesFileExist(kTempFile));
}

TEST(TestFileUtils, ReadCsvFile) {
   unique_ptr<CsvFileReader> reader(CsvFileReader::ConstructNew(kCsvFile));
   ASSERT_TRUE(reader.get());

   reader->set_field_term(',');
   reader->set_line_term('\n');
   reader->set_enclosure('"');

   vector<vector<string>> rows;
   while (reader->HasMoreRows()) {
      vector<string> row = reader->GetNextRow();
      if (!row.empty()) {
         rows.push_back(row);
      }
   }

   vector<vector<string>> actual = {
      {"1", "2", "3"},
      {"\\\"4\\\"44", "5\\\",\\\"", "testies, testies"},
      {"", "", " "},
      {"", "", "A", "", "", "B", "", ""},
      {"\\\\", "A", "\\\\\\\\\\\"", "B", "\\\"\\\\\\\"\\\\\\\\\\\""}};

   EXPECT_EQ(actual, rows);
}
