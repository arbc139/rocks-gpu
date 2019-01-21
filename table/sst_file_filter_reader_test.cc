/*
 * sst_file_filter_reader_test.cc
 *
 *  Created on: Dec 10, 2018
 *      Author: wonki
 */



#include <inttypes.h>
#include <ctime>
#include <time.h>

#include "cuda/filter.h"
#include "rocksdb/sst_file_filter_reader.h"
#include "rocksdb/sst_file_writer.h"
#include "table/block.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "utilities/merge_operators.h"

namespace rocksdb {

template <typename T>
std::string serialize(const T * data) {
	std::string d(sizeof(T), L'\0');
	memcpy(&d[0], data, d.size());
	return d;
}

std::string serializeVector(std::vector<int> v) {
	std::ostringstream oss;
	std::copy(v.begin(), v.end() - 1, std::ostream_iterator<int>(oss, " "));
	oss << v.back();
	return oss.str();
}


template <typename T>
std::unique_ptr<T> deserialize(const std::string & data) {
	if(data.size() != sizeof(T))
		return nullptr;
	auto d = std::unique_ptr<T>();
	memcpy(d.get(), data.data(), data.size());
	return d;
}

std::vector<int> deserializeVector(std::string s) {
	std::istringstream is(s);
	std::vector<int> v((std::istream_iterator<int>(is)), std::istream_iterator<int>());
	return v;
}

std::string EncodeAsString(uint64_t v) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%08" PRIu64, v);
  return std::string(buf);
}

std::string EncodeAsUint64(uint64_t v) {
  std::string dst;
  PutFixed64(&dst, v);
  return dst;
}

int filterPredicateCPU(const int target, ruda::ConditionContext ctx) {
  switch (ctx._op) {
    case 0:
      return target == ctx._pivot ? 1 : 0;
    case 1:
      return target < ctx._pivot ? 1 : 0;
    case 2:
      return target > ctx._pivot ? 1 : 0;
    case 3:
      return target <= ctx._pivot ? 1 : 0;
    case 4:
      return target >= ctx._pivot ? 1 : 0;
    default:
      return 0;
  }
}

class SstFileFilterReaderTest : public testing::Test {
 public:
  SstFileFilterReaderTest() {
    options_.merge_operator = MergeOperators::CreateUInt64AddOperator();
    //sst_name_ = test::PerThreadDBPath("sst_file");
    sst_name_ = "sst_testfile_1";
    sst_name_second_ = "sst_testfile_2";
  }

  void CreateFileAndCheck(const std::vector<std::string>& keys) {
    SstFileWriter writer(soptions_, options_);
    ASSERT_OK(writer.Open(sst_name_));
    for (size_t i = 0; i + 2 < keys.size(); i += 3) {
      ASSERT_OK(writer.Put(keys[i], keys[i]));
      ASSERT_OK(writer.Merge(keys[i+1], EncodeAsUint64(i+1)));
      ASSERT_OK(writer.Delete(keys[i+2]));
    }
    ASSERT_OK(writer.Finish());

    ReadOptions ropts;
    SstFileFilterReader reader(options_);
    ASSERT_OK(reader.Open(sst_name_));
    ASSERT_OK(reader.VerifyChecksum());
    std::unique_ptr<Iterator> iter(reader.NewIterator(ropts));
    iter->SeekToFirst();
    for (size_t i = 0; i + 2 < keys.size(); i += 3) {
      ASSERT_TRUE(iter->Valid());
      ASSERT_EQ(iter->key().compare(keys[i]), 0);
      ASSERT_EQ(iter->value().compare(keys[i]), 0);
      iter->Next();
      ASSERT_TRUE(iter->Valid());
      ASSERT_EQ(iter->key().compare(keys[i+1]), 0);
      ASSERT_EQ(iter->value().compare(EncodeAsUint64(i+1)), 0);
      iter->Next();
    }
    ASSERT_FALSE(iter->Valid());
  }

  void FileWrite(uint64_t kNumKeys) {
	  SstFileWriter writer(soptions_, options_);
	  srand((unsigned int)time(NULL));
	  ASSERT_OK(writer.Open(sst_name_));
	  for (size_t i = 0; i < kNumKeys; i ++) {
	  	ASSERT_OK(writer.Put(EncodeAsUint64(i), EncodeAsUint64(rand() % 100)));
	  }
	  ASSERT_OK(writer.Finish());
  }

  void FileWriteVector(uint64_t kNumKeys) {
	  SstFileWriter writer(soptions_, options_);
	  srand((unsigned int)time(NULL));
	  ASSERT_OK(writer.Open(sst_name_second_));
	  uint64_t index = 0;
	  std::vector<int> temp;
	  for (size_t i = 0; i < kNumKeys; i ++) {
		temp.emplace_back(rand() % 100000);
		if (i % 999999 == 0) {
	    	ASSERT_OK(writer.Put(EncodeAsUint64(index), serializeVector(temp)));
	    	index++;
	    	temp.clear();
		}
		if (i == kNumKeys - 1) {
	        ASSERT_OK(writer.Put(EncodeAsUint64(index), serializeVector(temp)));
		}
	  }
	  ASSERT_OK(writer.Finish());
  }

  void FilterWithCPU(ruda::ConditionContext ctx, std::vector<int> &results) {
	clock_t begin, end;
	clock_t cbegin, cend;

	ReadOptions ropts;
    SstFileFilterReader reader(options_);
    reader.Open(sst_name_);
    reader.VerifyChecksum();

    std::vector<int> temp;
    std::unique_ptr<Iterator> iter(reader.NewIterator(ropts));
    begin = clock();
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
       temp.emplace_back(atoi(iter->value().data()));
    }
    end = clock();
    cbegin = clock();
    for(unsigned int j = 0; j < temp.size(); j++) {
	   results.emplace_back(filterPredicateCPU(temp[j], ctx));
    }
    cend = clock();
    std::cout << " [size : " << results.size() << "]" << std::endl;
    std::cout << " CPU iteration time in CPU test(s) : " << (end - begin) / CLOCKS_PER_SEC << std::endl;
    std::cout << " CPU filter time in CPU test(s) : " << (cend - cbegin) / CLOCKS_PER_SEC << std::endl;


  }

  void FilterWithCPUVector(ruda::ConditionContext ctx, std::vector<int> &results) {
	clock_t begin, end;

	ReadOptions ropts;
    SstFileFilterReader reader(options_);
    reader.Open(sst_name_second_);
    reader.VerifyChecksum();

    std::unique_ptr<Iterator> iter(reader.NewIterator(ropts));
    begin = clock();
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      std::vector<int> temp_values = deserializeVector(iter->value().data());
      for(unsigned int j = 0 ; j < temp_values.size(); j++) {
    	  results.emplace_back(filterPredicateCPU(temp_values[j], ctx));
      }
    }
    end = clock();

    std::cout << " [size : " << results.size() << "]" << std::endl;
    std::cout << " CPU Vector time in CPU test(s) : " << (end - begin) / CLOCKS_PER_SEC << std::endl;


  }

  void FilterWithGPU(ruda::ConditionContext ctx, std::vector<int> &results) {
	clock_t begin, end, gbegin, gend;

	ReadOptions ropts;
    SstFileFilterReader reader(options_);
    reader.Open(sst_name_);
    reader.VerifyChecksum();
    std::vector<int> values;
    std::unique_ptr<Iterator> iter(reader.NewIterator(ropts));
    begin = clock();
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    	values.emplace_back(atoi(iter->value().data()));
    }
    end = clock();
    gbegin = clock();
	ruda::sstIntFilter(values, ctx, results);
	gend = clock();

	std::cout << " [size: "<< results.size() << "]" << std::endl;
	std::cout << " CPU iteration time in GPU test(s) : " << (end-begin) / CLOCKS_PER_SEC << std::endl;
	std::cout << " GPU filter time in GPU test(s) : " << (gend-gbegin) / CLOCKS_PER_SEC  << std::endl;
  }

  void FilterWithGPUVector(ruda::ConditionContext ctx, std::vector<int> &results) {
	clock_t begin, end;

	ReadOptions ropts;
    SstFileFilterReader reader(options_);
    reader.Open(sst_name_second_);
    reader.VerifyChecksum();
    std::unique_ptr<Iterator> iter(reader.NewIterator(ropts));
    begin = clock();
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    	std::vector<int> temp_values = deserializeVector(iter->value().data());
        std::vector<int> temp_results;
    	ruda::sstIntFilter(temp_values, ctx, temp_results);
    	results.insert(results.end(), temp_results.begin(), temp_results.end());
    }
    end = clock();

/*	std::cout << " Properties(1) = " << reader.GetTableProperties().get()->data_size << std::endl;
	std::cout << " Properties(2) = " << reader.GetTableProperties().get()->fixed_key_len << std::endl;
	std::cout << " Properties(3) = " << reader.GetTableProperties().get()->index_size << std::endl;
    std::cout << " Properties(4) = " << reader.GetTableProperties().get()->num_data_blocks << std::endl;
	std::cout << " Properties(5) = " << reader.GetTableProperties().get()->raw_key_size << std::endl;
	std::cout << " Properties(6) = " << reader.GetTableProperties().get()->raw_value_size << std::endl; */

	std::cout << " [size: "<< results.size() << "]" << std::endl;
	std::cout << " GPU Vector time in GPU test(s) : " << (end-begin) / CLOCKS_PER_SEC << std::endl;

  }

  void GetDataBlocks(std::vector<char>& data,
                     std::vector<uint64_t>& seek_indices) {
    ReadOptions ropts;
    SstFileFilterReader reader(options_);
    reader.Open(sst_name_);
    reader.VerifyChecksum();
    std::cout << "[GetDataBlocks]" << std::endl;
    reader.GetDataBlocks(ropts, data, seek_indices);
    std::cout << "--------------------------------------------" << std::endl;
  }

  void DecodeDataBlocksOnCpu(std::vector<char>& data,
                             std::vector<uint64_t>& seek_indices) {
    // Decode
    // TODO(totoro): Implements this logics to DataBulkCpuIter.
    std::cout << "[DecodeDataBlocksOnCpu]" << std::endl;
    const char *start = &data[0];
    uint64_t count = 0;
    for (size_t i = 0; i < seek_indices.size(); ++i) {
      const char *limit = (i == seek_indices.size() - 1)
          ? start + data.size()
          : start + seek_indices[i + 1];
      const char *subblock = start + seek_indices[i];
      while (subblock < limit) {
        uint32_t shared, non_shared, value_length;
        Slice key;
        subblock = DecodeEntry()(subblock, limit, &shared, &non_shared,
                                &value_length);
        if (shared == 0) {
          key = Slice(subblock, non_shared);
        } else {
          // TODO(totoro): We need to consider 'shared' data within subblock.
          key = Slice(subblock, shared + non_shared);
        }

        Slice value = Slice(subblock + non_shared, value_length);

        // Next DataKey...
        uint64_t next_offset = static_cast<uint64_t>(
          (value.data() + value.size()) - start
        );

        if (count % 1000000 == 0) {
          std::cout << "key[" << DecodeFixed64(key.data()) << "] "
              << "value[" << DecodeFixed64(value.data()) << "] "
              << "next_offset[" << next_offset << "] "
              << "while_test: " << (void *) (start + next_offset)
                  << " < " << (void *) limit
                  << " = " << ((start + next_offset) < limit)
              << std::endl;
        }
        subblock = start + next_offset;
        count++;
      }
    }
  }

  virtual void SetUp() {
	options_.comparator = test::Uint64Comparator();
	// uint64_t kNumKeys = 1000000000;
  uint64_t kNumKeys = 1000000;
	FileWrite(kNumKeys);
	// FileWriteVector(kNumKeys);
  }

 protected:
  Options options_;
  EnvOptions soptions_;
  std::string sst_name_;
  std::string sst_name_second_;
};

/* Basic Test
TEST_F(SstFileFilterReaderTest, Basic) {
  std::vector<std::string> keys;
  for (uint64_t i = 0; i < 100000; i++) {
    keys.emplace_back(EncodeAsString(i));
  }
  CreateFileAndCheck(keys);
}

TEST_F(SstFileFilterReaderTest, Uint64Comparator) {
  options_.comparator = test::Uint64Comparator();
  std::vector<std::string> keys;
  for (uint64_t i = 0; i < 1000000; i++) {
    keys.emplace_back(EncodeAsUint64(i));
  }
  CreateFileAndCheck(keys);
} */

// TEST_F(SstFileFilterReaderTest, FilterTestWithCPU) {
//   options_.comparator = test::Uint64Comparator();
//   ruda::ConditionContext ctx = { ruda::EQ, 5,};
//   std::vector<int> results;
//   FilterWithCPU(ctx, results);

// }

// TEST_F(SstFileFilterReaderTest, FilterTestWithCPUVector) {
//   options_.comparator = test::Uint64Comparator();
//   ruda::ConditionContext ctx = { ruda::EQ, 5,};
//   std::vector<int> results;
//   FilterWithCPUVector(ctx, results);

// }

// TEST_F(SstFileFilterReaderTest, FilterTestWithGPU) {
//   options_.comparator = test::Uint64Comparator();
//   ruda::ConditionContext ctx = { ruda::EQ, 5,};
//   std::vector<int> results;
//   FilterWithGPU(ctx, results);

// }

// TEST_F(SstFileFilterReaderTest, FilterTestWithGPUVector) {
//   options_.comparator = test::Uint64Comparator();
//   ruda::ConditionContext ctx = { ruda::EQ, 5,};
//   std::vector<int> results;
//   FilterWithGPUVector(ctx, results);

// }

TEST_F(SstFileFilterReaderTest, GetDataBlocksOnCpu) {
  options_.comparator = test::Uint64Comparator();
  std::vector<char> data;
  std::vector<uint64_t> seek_indices;
  GetDataBlocks(data, seek_indices);
  DecodeDataBlocksOnCpu(data, seek_indices);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
