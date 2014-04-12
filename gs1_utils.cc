#include "gs1_utils.h"
#include "gs1.pb.h"
#include "string_utils.h"

#include <string>
using std::string;
#include <vector>
using std::vector;

namespace zns {

GS1Product PODRowToGS1Product(const vector<string>& row) {
   GS1Product product_entry;

   if (row.size() < 21) {
      // malformed row
      return product_entry;
   }

   if (!row[0].empty()) {
      long gtin = -1;
      if (util::StringToLong(row[0], &gtin)) {
         product_entry.set_global_trade_id(gtin);
      }
   }

   if (!row[2].empty()) {
      long gcp;
      if (util::StringToLong(row[2], &gcp)) {
         product_entry.set_global_company_pref(gcp);
      }
   }
   if (!row[3].empty()) {
      product_entry.mutable_brand()->set_id(row[3]);
   }
   if (!row[4].empty()) {
      product_entry.mutable_gpc()->set_segment(row[4]);
   }
   if (!row[5].empty()) {
      product_entry.mutable_gpc()->set_family(row[5]);
   }
   if (!row[6].empty()) {
      product_entry.mutable_gpc()->set_class_(row[6]);
   }
   if (!row[7].empty()) {
      product_entry.mutable_gpc()->set_brick(row[7]);
   }
   if (!row[8].empty()) {
      product_entry.set_name(row[8]);
   }
   if (!row[10].empty()) {
      double grams;
      if (util::StringToDouble(row[10], &grams)) {
         product_entry.set_amount_g(grams);
      }
   }
   if (!row[11].empty()) {
      double ounces;
      if (util::StringToDouble(row[11], &ounces)) {
         product_entry.set_amount_oz(ounces);
      }
   }
   if (!row[12].empty()) {
      double milli_litres;
      if (util::StringToDouble(row[12], &milli_litres)) {
         product_entry.set_amount_ml(milli_litres);
      }
   }
   if (!row[13].empty()) {
      double fluid_oz;
      if (util::StringToDouble(row[13], &fluid_oz)) {
         product_entry.set_amount_floz(fluid_oz);
      }
   }

   return product_entry;
}

Brand PODRowToBrand(const vector<string>& row) {
   Brand brand;

   if (row.size() < 4) {
      return brand;
   }

   if (!row[0].empty()) {
      brand.set_id(row[0]);
   }
   if (!row[1].empty()) {
      brand.set_name(row[1]);
   }
   if (!row[3].empty()) {
      brand.set_url(row[3]);
   }

   return brand;
}

};  // namespace zns
