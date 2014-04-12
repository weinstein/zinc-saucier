#include "file_utils.h"
using util::CsvFileReader;
#include "string_utils.h"

#include "gs1_utils.h"
#include "gs1.pb.h"
using zns::GS1Product;
using zns::Brand;

#include <algorithm>
#include <iostream>
using std::cerr;
using std::cout;
#include <memory>
using std::unique_ptr;
#include <unordered_map>
using std::unordered_map;
#include <unordered_set>
using std::unordered_set;
#include <string>
using std::string;
#include <vector>
using std::vector;

#include <fcntl.h>
#include <unistd.h>

#include <gflags/gflags.h>
#include <simstring/simstring.h>
#include <google/protobuf/io/zero_copy_stream.h>
using google::protobuf::io::ZeroCopyInputStream;
#include <google/protobuf/io/zero_copy_stream_impl.h>
using google::protobuf::io::FileInputStream;
#include <google/protobuf/io/coded_stream.h>
using google::protobuf::io::CodedInputStream;

DEFINE_string(proto_bin_in, "", "Load serialized products from this file");
DEFINE_string(tmp_dir, "", "Save temporary simstring db to this directory");
DEFINE_string(extra_bin_in, "", "Optionally load more products from this file");
const char kSimstringFname[] = "simstring_prods.db";

vector<string> GetSimstrings(const string& query, int measure, double threshold,
                            simstring::reader* dbr) {
   vector<string> res;
   dbr->retrieve(query, measure, threshold, std::back_inserter(res));
   return res;
}

void PrintError(const string& line, const string& err_msg) {
   cout << "bad input: " << line << "\n";
   if (!err_msg.empty()) {
      cout << err_msg << "\n";
   }
}

void DebugPrintRow(const vector<string> cur_row) {
   cerr << "cur_row=[";
   for (const string& str : cur_row) {
      cerr << " " << (!str.empty() ? str : "(empty)");
   }
   cerr << " ]\n";
}

bool LoadProductListFromFD(int fd, zns::GS1ProductList* product_list) {
   struct stat infile_stat;
   if (fstat(fd, &infile_stat) < 0) {
      cerr << "Error stat-ing input file\n";
      return false;
   }
   size_t fsize = infile_stat.st_size;

   unique_ptr<ZeroCopyInputStream> zc_stream(new FileInputStream(fd));
   CodedInputStream in_stream(zc_stream.get());
   in_stream.SetTotalBytesLimit(fsize, fsize);
   if (!product_list->ParseFromCodedStream(&in_stream)) {
      cerr << "Error parsing serialized products\n";
      return false;
   }
   return true;
}

void WriteSimstringDB(const string& db_fname, const unordered_set<string>& keys) {
   simstring::ngram_generator gen(3, false);
   simstring::writer_base<string> dbw(gen, db_fname);
   for (const string& prod_name : keys) {
      dbw.insert(prod_name);
   }
   dbw.close();
}

int main(int argc, char** argv) {
   google::ParseCommandLineFlags(&argc, &argv, true);

   if (FLAGS_proto_bin_in.empty()) {
      cerr << "Must supply an input file proto_bin_in\n";
      return 1;
   }

   int fd = open(FLAGS_proto_bin_in.c_str(), O_RDONLY);
   if (fd < 0) {
      cerr << "Error opening proto_bin_in=\""
            << FLAGS_proto_bin_in << "\" for reading.\n";
      return 1;
   }

   cerr << "Loading gtin products...\n";
   zns::GS1ProductList product_list;
   if (!LoadProductListFromFD(fd, &product_list)) {
      return 1;
   }
   close(fd);

   if (!FLAGS_extra_bin_in.empty()) {
      fd = open(FLAGS_extra_bin_in.c_str(), O_RDONLY);
      if (fd < 0) {
         cerr << "Error opening extra_bin_in=\""
               << FLAGS_extra_bin_in << "\" for reading.\n";
      } else {
         cerr << "Loading extra products...\n";
         zns::GS1ProductList extra_list;
         if (LoadProductListFromFD(fd, &extra_list)) {
            product_list.MergeFrom(extra_list);
         }
      }
   }
   cerr << "Loaded " << product_list.entries().size() << " product entries.\n";

   cerr << "Mapping products...\n";
   unordered_map<long int, const zns::GS1Product*> gtin_to_product;
   unordered_map<string, const zns::GS1Product*> name_to_product;
   unordered_set<string> all_names;
   for (const zns::GS1Product& product : product_list.entries()) {
      if (!product.has_global_trade_id() || product.global_trade_id() < 0) {
         cerr << "Invalid product entry: missing or invalid gtin\n"
               << product.DebugString();
         continue;
      }
      if (!product.has_name() || product.name().empty()) {
         cerr << "Invalid product entry: missing name\n"
               << product.DebugString();
         continue;
      }
      string prod_name = util::StringToLower(product.name());
      gtin_to_product.insert({product.global_trade_id(), &product});
      name_to_product.insert({prod_name, &product});
      all_names.insert({prod_name});
   }

   cerr << "Creating simstring db...\n";
   const string simstring_db_fname =
         FLAGS_tmp_dir.append("/").append(kSimstringFname);
   WriteSimstringDB(simstring_db_fname, all_names);
   all_names.clear();

   cerr << "Reopening simstring db...\n";
   simstring::reader dbr;
   dbr.open(simstring_db_fname);

   double threshold = 0.6;
   while (true) {
      string input_line;
      cerr << ":";
      std::getline(std::cin, input_line);
      if (std::all_of(input_line.begin(), input_line.end(), ::isdigit)) {
         long int gtin;
         if (!util::StringToLong(input_line, &gtin)) {
            PrintError(input_line, "not a valid gtin");
            continue;
         }
         unordered_map<long int, const zns::GS1Product*>::const_iterator iter =
               gtin_to_product.find(gtin);
         if (iter == gtin_to_product.end()) {
            cout << "gtin " << input_line << " not found\n";
            continue;
         } else {
            const zns::GS1Product* prod = iter->second;
            cout << prod->DebugString();
            continue;
         }
      } else if (input_line == ".exit" || input_line == ".q") {
         break;
      } else if (input_line == ".threshold") {
         cerr << "threshold=" << threshold << "\nenter new threshold:";
         std::getline(std::cin, input_line);
         if (util::StringToDouble(input_line, &threshold)) {
            continue;
         } else {
            cerr << "invalid threshold value.\n";
            continue;
         }
      } else {
         // input line is search query
         vector<string> matches = GetSimstrings(
               util::StringToLower(input_line), simstring::cosine, threshold, &dbr);
         if (matches.empty()) {
            cout << "No good matches for search string: \""
                  << input_line << "\"\n";
            continue;
         }
         for (const string& prod_name : matches) {
            unordered_map<string, const zns::GS1Product*>::const_iterator iter =
               name_to_product.find(prod_name);
            if (iter == name_to_product.end()) {
               // This should never happen...
               cerr << "Product name not found: this shouldn't happen\n";
               continue;
            } else {
               const zns::GS1Product* prod = iter->second;
               cout << prod->DebugString();
            }
         }
      }
   }

   return 0;
}
