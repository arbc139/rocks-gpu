/*
 * sst_file_filter_reader.h
 *
 *  Created on: Dec 10, 2018
 *      Author: wonki
 */


#pragma once

#include "rocksdb/slice.h"
#include "rocksdb/options.h"
#include "rocksdb/iterator.h"
#include "rocksdb/table_properties.h"
#include "cuda/filter.h"

namespace rocksdb {

// SstFileReader is used to read sst files that are generated by DB or
// SstFileWriter.
class SstFileFilterReader {
 public:
  SstFileFilterReader(const Options& options);

  ~SstFileFilterReader();

  // Prepares to read from the file located at "file_path".
  Status Open(const std::string& file_path);
  Status BulkReturn(const std::string& file_path, char * scratch);
  // Returns a new iterator over the table contents.
  // Most read options provide the same control as we read from DB.
  // If "snapshot" is nullptr, the iterator returns only the latest keys.
  Iterator* NewIterator(const ReadOptions& options);

  Status filterWithCPU();
  Status filterWithGPU();

  std::shared_ptr<const TableProperties> GetTableProperties() const;

  // Verifies whether there is corruption in this table.
  Status VerifyChecksum();

 private:
  struct Rep;
  std::unique_ptr<Rep> rep_;
};

}  // namespace rocksdb
