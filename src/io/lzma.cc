// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/io/lzma.hh>
#include <arch/lzma.hh>
#include <cstring>
#include "check_signature.hh"

namespace arch::io {
	lzma::lzma(wrapper_tag) {}

	bool lzma::is_valid(io::seekable* file) {
		return check_signature<0xFD, '7', 'z', 'X', 'Z', 0x00>(file);
	}

	io::seekable::ptr lzma::wrap(io::seekable::ptr&& file) {
		return wrap_impl<lzma>(std::move(file), wrapper_tag{});
	}

	base::decompressor::ptr lzma::make_decompressor() {
		return std::make_unique<arch::lzma::decompressor>();
	}
}  // namespace arch::io
