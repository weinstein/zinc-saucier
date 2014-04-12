#include "cooking.pb.h"
#include "file_utils.h"
using util::CsvFileReader;
#include "string_utils.h"

#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

#include <gflags/gflags.h>
#include <simstring/simstring.h>

DEFINE_string(proto_db, "", "Load protobuf of list of ingredients from here");
DEFINE_string(tmp_dir, "/tmp", "Save/load temporary files here,"
                               " like simstring db");

const char kSimstringFile[] = "simstring.db";

int main(int argc, char** argv) {
   google::ParseCommandLineFlags(&argc, &argv, true);

   if (FLAGS_proto_db.empty()) {
      cerr << "Must specify proto_db source file.\n";
      return 1;
   }

   std::ifstream in_stream(
      FLAGS_proto_db, std::ifstream::in | std::ifstream::binary);
   zns::IngredientList ingred_list;
   if (ingred_list.ParseFromIstream(&in_stream)) {
      cerr << "Loaded ingredient list: "
            << ingred_list.entries_size() << " entries.\n";
   } else {
      cerr << "Parsing protobuf from proto_db file failed.\n";
      return 1;
   }

   unordered_map<string, zns::Ingredient> name_to_ingredient;

   simstring::ngram_generator gen(3, false);
   const string db_fname = FLAGS_tmp_dir.append("/").append(kSimstringFile);
   simstring::writer_base<string> dbw(gen, db_fname);
   for (const zns::Ingredient& ingredient : ingred_list.entries()) {
      const string name = ingredient.name();
      if (name.empty()) {
         continue;
      }
      dbw.insert(name);
      name_to_ingredient.insert({name, ingredient});
   }
   dbw.close();

   simstring::reader dbr;
   dbr.open(db_fname);
}
