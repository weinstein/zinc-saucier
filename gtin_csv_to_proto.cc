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
#include <string>
using std::string;
#include <vector>
using std::vector;

#include <fcntl.h>
#include <unistd.h>

#include <gflags/gflags.h>

DEFINE_string(csv_in, "", "File from which to load database of product keyed"
                          " by GTIN");
DEFINE_string(brand_csv_in, "", "Optional file from which to load brand data keyed"
                                " by BSIN");
DEFINE_string(proto_bin_out, "", "Dump loaded product entries protobuf serialized here");

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

int main(int argc, char** argv) {
   google::ParseCommandLineFlags(&argc, &argv, true);

   if (FLAGS_csv_in.empty() || FLAGS_brand_csv_in.empty()) {
      cerr << "Must specify csv_in and brand_csv_in source files.\n";
      return 1;
   }

   unique_ptr<CsvFileReader> brand_reader(
         CsvFileReader::ConstructNew(FLAGS_brand_csv_in));
   if (!brand_reader.get()) {
      cerr << "Error opening brand csv file \""
            << FLAGS_brand_csv_in << "\" for parsing.\n";
      return 1;
   }

   cerr << "Loading brands...\n";
   unordered_map<string, Brand> brand_id_to_data;
   int row_num = 0;
   vector<string> cur_row;
   while (brand_reader->HasMoreRows()) {
      brand_reader->GetNextRow(&cur_row);
      Brand brand_entry = zns::PODRowToBrand(cur_row);
      if (brand_entry.has_id() && !brand_entry.id().empty()) {
         const auto& ret = brand_id_to_data.insert({brand_entry.id(), brand_entry});
         if (!ret.second) {
            cerr << "on row " << row_num 
                  << ": brand id " << brand_entry.id() << " is a duplicate\n";
         }
      } else {
         cerr << "on row " << row_num << ": bad brand row\n";
      }
      row_num++;
   }
   cerr << "Loaded " << brand_id_to_data.size() << " brand entries.\n";

   unique_ptr<CsvFileReader> product_reader(
         CsvFileReader::ConstructNew(FLAGS_csv_in));
   if (!product_reader.get()) {
      cerr << "Error opening input gtin csv file \""
            << FLAGS_csv_in << "\" for parsing.\n";
      return 1;
   }

   cerr << "Loading products...\n";
   unordered_map<long, GS1Product> gtin_to_product;
   row_num = 0;
   while (product_reader->HasMoreRows()) {
      product_reader->GetNextRow(&cur_row);
      GS1Product product_entry = zns::PODRowToGS1Product(cur_row);
      if (product_entry.has_brand() && !product_entry.brand().id().empty()) {
         unordered_map<string, Brand>::const_iterator iter =
               brand_id_to_data.find(product_entry.brand().id());
         if (iter != brand_id_to_data.end()) {
            *product_entry.mutable_brand() = iter->second;
         } else {
            cerr << "on row " << row_num << ": "
                  "product with unknown brand id " << product_entry.brand().id() << "\n";
         }
      }
      if (product_entry.has_global_trade_id() && product_entry.has_name()) {
         const auto& ret = gtin_to_product.insert(
               {product_entry.global_trade_id(), product_entry});
         if (!ret.second) {
            cerr << "on row " << row_num << ": product id "
                  << product_entry.global_trade_id() << " is a duplicate\n";
         }
      } else {
         cerr << "on row " << row_num << ": bad product row\n";
      }
      row_num++;
   }
   cerr << "Loaded " << gtin_to_product.size() << " product entries.\n";

   cerr << "Serializing products...\n";
   int fd = open(FLAGS_proto_bin_out.c_str(), O_WRONLY | O_CREAT);
   if (fd < 0) {
      cerr << "Error opening output file " << FLAGS_proto_bin_out << "\n";
      return 1;
   }
   zns::GS1ProductList product_list;
   for (const auto& kv : gtin_to_product) {
      *product_list.add_entries() = kv.second;
   }
   if (!product_list.SerializeToFileDescriptor(fd)) {
      cerr << "Error writing serialized data to file descriptor for file "
            << FLAGS_proto_bin_out << "\n";
      return 1;
   }

/*
   while (true) {
      string input_line;
      std::getline(std::cin, input_line);
      if (input_line.size() >= 12 &&
               std::all_of(input_line.begin(), input_line.end(), ::isdigit)) {
         long int gtin = -1;
         if (!util::StringToLong(input_line, &gtin) || gtin < 0) {
            PrintError(input_line, "not a valid gtin");
            continue;
         }
         unordered_map<long int, GS1Product>::const_iterator iter =
               gtin_keystore.find(gtin);
         if (iter == gtin_keystore.end()) {
            cout << "gtin #" << input_line << " not found.\n";
            continue;
         } else {
            const GS1Product& prod = iter->second;
            if (!brand_keystore.empty()) {
               unordered_map<string, Brand>::const_iterator biter =
                     brand_keystore.find(prod.brand_id());
               if (biter != brand_keystore.end()) {
                  cout << biter->second.DebugString();
               }
            }
            cout << iter->second.DebugString();
            continue;
         }
      }

      vector<string> split = util::SplitString(input_line, " ");
      if (split.size() < 1) {
         PrintError(input_line, "");
         continue;
      }
      string command = split[0];
      if (command == "bsin") {
         if (split.size() < 2) {
            PrintError(input_line, "must supply bsin");
            continue;
         }
         const string& bsin = split[1];
         unordered_map<string, Brand>::const_iterator iter =
               brand_keystore.find(bsin);
         if (iter == brand_keystore.end()) {
            cout << "bsin " << bsin << " not found.\n";
            continue;
         } else {
            cout << iter->second.DebugString();
            continue;  
         }
      } else if (command == "exit" || command == "quit" || command == "stop") {
         break;
      } else {
         PrintError(input_line, "unknown command");
         continue;
      }
   }
*/

   return 0;
}
