// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Slice is a simple structure containing a pointer into some external
// storage and a size.  The user of a Slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// Multiple threads can invoke const methods on a Slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Slice must use
// external synchronization.

#pragma once

#include <assert.h>
#include <cstdio>
#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef __cpp_lib_string_view
#include <string_view>
#endif

#include "rocksdb/cleanable.h"
#include "accelerator/common.h"

namespace rocksdb {

enum data_types { TYPE_DECIMAL, TYPE_TINY,
			TYPE_SHORT,   TYPE_LONG,
			 TYPE_FLOAT,   TYPE_DOUBLE,
			 TYPE_NULL,    TYPE_TIMESTAMP,
			 TYPE_LONGLONG, TYPE_INT24,
			 TYPE_DATE,    TYPE_TIME,
			 TYPE_DATETIME,  TYPE_YEAR,
			 TYPE_NEWDATE,  TYPE_VARCHAR,
			 TYPE_BIT,
			 TYPE_TIMESTAMP2,
			 TYPE_DATETIME2,
			 TYPE_TIME2,
			 TYPE_DOCUMENT,
			 TYPE_DOCUMENT_VALUE, // Used for DOCUMENT()
			 TYPE_DOCUMENT_UNKNOWN,
			 TYPE_NEWDECIMAL=246,
			 TYPE_ENUM=247,
			 TYPE_SET=248,
			 TYPE_TINY_BLOB=249,
			 TYPE_MEDIUM_BLOB=250,
			 TYPE_LONG_BLOB=251,
			 TYPE_BLOB=252,
			 TYPE_VAR_STRING=253,
			 TYPE_STRING=254,
			 TYPE_GEOMETRY=255
};

class Slice {
 public:
  // Create an empty slice.
  Slice() : data_(""), size_(0) { }

  // Create a slice that refers to d[0,n-1].
  Slice(const char* d, size_t n) : data_(d), size_(n) { }

  // Create a slice that refers to the contents of "s"
  /* implicit */
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

#ifdef __cpp_lib_string_view
  // Create a slice that refers to the same contents as "sv"
  /* implicit */
  Slice(std::string_view sv) : data_(sv.data()), size_(sv.size()) {}
#endif

  // Create a slice that refers to s[0,strlen(s)-1]
  /* implicit */
  Slice(const char* s) : data_(s) {
    size_ = (s == nullptr) ? 0 : strlen(s);
  }

  // Create a single slice from SliceParts using buf as storage.
  // buf must exist as long as the returned Slice exists.
  Slice(const struct SliceParts& parts, std::string* buf);

  // Return a pointer to the beginning of the referenced data
  const char* data() const { return data_; }

  // Return the length (in bytes) of the referenced data
  size_t size() const { return size_; }

  // Return true iff the length of the referenced data is zero
  bool empty() const { return size_ == 0; }

  // Return the ith byte in the referenced data.
  // REQUIRES: n < size()
  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

  // Change this slice to refer to an empty array
  void clear() { data_ = ""; size_ = 0; }

  // Drop the first "n" bytes from this slice.
  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  void remove_suffix(size_t n) {
    assert(n <= size());
    size_ -= n;
  }

  // Return a string that contains the copy of the referenced data.
  // when hex is true, returns a string of twice the length hex encoded (0-9A-F)
  std::string ToString(bool hex = false) const;

#ifdef __cpp_lib_string_view
  // Return a string_view that references the same data as this slice.
  std::string_view ToStringView() const {
    return std::string_view(data_, size_);
  }
#endif

  // Decodes the current slice interpreted as an hexadecimal string into result,
  // if successful returns true, if this isn't a valid hex string
  // (e.g not coming from Slice::ToString(true)) DecodeHex returns false.
  // This slice is expected to have an even number of 0-9A-F characters
  // also accepts lowercase (a-f)
  bool DecodeHex(std::string* result) const;

  // Three-way comparison.  Returns value:
  //   <  0 iff "*this" <  "b",
  //   == 0 iff "*this" == "b",
  //   >  0 iff "*this" >  "b"
  int compare(const Slice& b) const;

  // Return true iff "x" is a prefix of "*this"
  bool starts_with(const Slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_, x.data_, x.size_) == 0));
  }

  bool ends_with(const Slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_ + size_ - x.size_, x.data_, x.size_) == 0));
  }

  // Compare two slices and returns the first byte where they differ
  size_t difference_offset(const Slice& b) const;

 // private: make these public for rocksdbjni access
  const char* data_;
  size_t size_;
  // Intentionally copyable
};

/**
 * A Slice that can be pinned with some cleanup tasks, which will be run upon
 * ::Reset() or object destruction, whichever is invoked first. This can be used
 * to avoid memcpy by having the PinnableSlice object referring to the data
 * that is locked in the memory and release them after the data is consumed.
 */
class PinnableSlice : public Slice, public Cleanable {
 public:
  PinnableSlice() { buf_ = &self_space_; }
  explicit PinnableSlice(const char* data__, size_t size__) {
    buf_ = &self_space_;
    PinSelf(data__, size__);
  }
  explicit PinnableSlice(std::string* buf) { buf_ = buf; }

  // No copy constructor and copy assignment allowed.
  PinnableSlice(PinnableSlice&) = delete;
  PinnableSlice& operator=(PinnableSlice&) = delete;

  // Move constructor allowed.
  PinnableSlice(PinnableSlice&& other) {
    *this = std::move(other);
  }
  PinnableSlice & operator=(PinnableSlice&& other) {
	  if(this != &other) {
		  self_space_ = std::move(other.self_space_);
		  buf_ = other.buf_;
		  data_ = other.data_;
		  size_ = other.size_;
		  pinned_ = other.pinned_;
	  }
	  return *this;
  }

  inline void PinSlice(const Slice& s, CleanupFunction f, void* arg1,
                       void* arg2) {
    assert(!pinned_);
    pinned_ = true;
    data_ = s.data();
    size_ = s.size();
    RegisterCleanup(f, arg1, arg2);
    assert(pinned_);
  }

  inline void PinSlice(const Slice& s, Cleanable* cleanable) {
    assert(!pinned_);
    pinned_ = true;
    data_ = s.data();
    size_ = s.size();
    cleanable->DelegateCleanupsTo(this);
    assert(pinned_);
  }

  inline void PinSelf(const char *data__, size_t size__) {
    assert(!pinned_);
    buf_->assign(data__, size__);
    data_ = buf_->data();
    size_ = buf_->size();
    assert(!pinned_);
  }

  inline void PinSelf(const Slice& slice) {
    assert(!pinned_);
    buf_->assign(slice.data(), slice.size());
    data_ = buf_->data();
    size_ = buf_->size();
    assert(!pinned_);
  }

  inline void PinSelf() {
    assert(!pinned_);
    data_ = buf_->data();
    size_ = buf_->size();
    assert(!pinned_);
  }

  void remove_suffix(size_t n) {
    assert(n <= size());
    if (pinned_) {
      size_ -= n;
    } else {
      buf_->erase(size() - n, n);
      PinSelf();
    }
  }

  void remove_prefix(size_t /*n*/) {
    assert(0);  // Not implemented
  }

  void Reset() {
    Cleanable::Reset();
    pinned_ = false;
  }

  inline std::string* GetSelf() { return buf_; }

  inline bool IsPinned() { return pinned_; }

 private:
  friend class PinnableSlice4Test;
  std::string self_space_;
  std::string* buf_;
  bool pinned_ = false;
};

/* GPU Accelerator */
class SlicewithSchema : public Slice, public Cleanable {
 public:
  SlicewithSchema(const char* d, size_t n, accelerator::FilterContext ctx,
		  int idx, const std::vector<uint>& type, const std::vector<uint>& length,
      const std::vector<uint>& skip)
 : Slice(d, n) {
	  context = ctx;
	  target_idx = idx;
    std::copy(type.begin(), type.end(), std::back_inserter(field_type));
    std::copy(length.begin(), length.end(), std::back_inserter(field_length));
    std::copy(skip.begin(), skip.end(), std::back_inserter(field_skip));
  }

  SlicewithSchema* clone() const {
    return new SlicewithSchema(
        data_, size_, context, target_idx, field_type, field_length,
        field_skip);
  }

  // No copy constructor and copy assignment allowed.
  SlicewithSchema(SlicewithSchema&) = delete;
  SlicewithSchema& operator=(SlicewithSchema&) = delete;

  // Move constructor allowed.
  SlicewithSchema(SlicewithSchema&& other) {
    *this = std::move(other);
  }
  SlicewithSchema& operator=(SlicewithSchema&& other) {
	  if (this != &other) {
      data_ = other.data_;
      size_ = other.size_;
      target_idx = other.target_idx;
      context = other.context;
      field_type = std::move(other.field_type);
      field_length = std::move(other.field_length);
      field_skip = std::move(other.field_skip);
	  }
	  return *this;
  }

  unsigned int getType (unsigned int index) const  {
	  return field_type[index];
  }

  unsigned int getLength (unsigned int index) const  {
	  return field_length[index];
  }

  unsigned int getSkip (unsigned int index) const  {
      return field_skip[index];
  }

  int getTarget() const {
	  return target_idx;
  }

  accelerator::Operator getOp() const {
	  return context._op;
  }

  long getPivot() const {
	  return context._pivot;
  }

  accelerator::FilterContext context;

 //private:
  int target_idx;
  std::vector<uint> field_type;
  std::vector<uint> field_length;
  std::vector<uint> field_skip;
};

// A set of Slices that are virtually concatenated together.  'parts' points
// to an array of Slices.  The number of elements in the array is 'num_parts'.
struct SliceParts {
  SliceParts(const Slice* _parts, int _num_parts) :
      parts(_parts), num_parts(_num_parts) { }
  SliceParts() : parts(nullptr), num_parts(0) {}

  const Slice* parts;
  int num_parts;
};

inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) {
  return !(x == y);
}

inline int Slice::compare(const Slice& b) const {
  assert(data_ != nullptr && b.data_ != nullptr);
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}

inline size_t Slice::difference_offset(const Slice& b) const {
  size_t off = 0;
  const size_t len = (size_ < b.size_) ? size_ : b.size_;
  for (; off < len; off++) {
    if (data_[off] != b.data_[off]) break;
  }
  return off;
}

}  // namespace rocksdb
