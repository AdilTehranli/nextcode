/*
 * Copyright (c) 2024, ITGSS Corporation. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
  *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * Contact with ITGSS, 651 N Broad St, Suite 201, in the
 * city of Middletown, zip code 19709, and county of New Castle, state of Delaware.
 * or visit www.it-gss.com if you need additional information or have any
 * questions.
 *
 */

#include "src/datastream.h"

#include "src/compiler/runtime_api.h"
#include "src/zone.h"

namespace Code {

void BaseWriteStream::WriteTargetWord(word value) {
  ASSERT(Utils::BitLength(value) <= compiler::target::kBitsPerWord);
  WriteFixed(static_cast<compiler::target::word>(value));
}

MallocWriteStream::~MallocWriteStream() {
  free(buffer_);
}

void MallocWriteStream::Realloc(intptr_t new_size) {
  const intptr_t old_offset = current_ - buffer_;
  buffer_ = reinterpret_cast<uint8_t*>(realloc(buffer_, new_size));
  capacity_ = buffer_ != nullptr ? new_size : 0;
  current_ = buffer_ != nullptr ? buffer_ + old_offset : nullptr;
}

void ZoneWriteStream::Realloc(intptr_t new_size) {
  const intptr_t old_offset = current_ - buffer_;
  buffer_ = zone_->Realloc(buffer_, capacity_, new_size);
  capacity_ = buffer_ != nullptr ? new_size : 0;
  current_ = buffer_ != nullptr ? buffer_ + old_offset : nullptr;
}

StreamingWriteStream::~StreamingWriteStream() {
  Flush();
  free(buffer_);
}

void StreamingWriteStream::Realloc(intptr_t new_size) {
  Flush();
  // Check whether resetting the internal buffer by flushing gave enough space.
  if (new_size <= capacity_) {
    return;
  }
  const intptr_t new_capacity = Utils::RoundUp(new_size, 64 * KB);
  buffer_ = reinterpret_cast<uint8_t*>(realloc(buffer_, new_capacity));
  capacity_ = buffer_ != nullptr ? new_capacity : 0;
  current_ = buffer_;  // Flushing reset the internal buffer offset to 0.
}

void StreamingWriteStream::Flush() {
  intptr_t size = current_ - buffer_;
  callback_(callback_data_, buffer_, size);
  flushed_size_ += size;
  current_ = buffer_;
}

}  // namespace dart
