
/*
 * filter.h
 *
 *  Created on: Feb 18, 2019
 *      Author: totoro
 */

#pragma once

#include <string>
#include <vector>

#include "accelerator/common.h"
#include "rocksdb/slice.h"

namespace avx {

int simpleIntFilter(std::vector<long> &source, accelerator::FilterContext ctx,
                    std::vector<long> &results);

int recordFilter( std::vector<rocksdb::Slice> &raw_records,
                    const rocksdb::SlicewithSchema &schema_key,
                    std::vector<rocksdb::PinnableSlice> &results);

int recordFilterWithKey( std::vector<rocksdb::PinnableSlice> &k_records, std::vector<rocksdb::Slice> &raw_records,
                    const rocksdb::SlicewithSchema &schema_key, std::vector<rocksdb::PinnableSlice> &keys,
                    std::vector<rocksdb::PinnableSlice> &results);

}  // namespace avx
