// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/bzlib.hh>
#include <arch/io/bzip2.hh>
#include <cstring>
#include "check_signature.hh"

namespace arch::io {
	bzip2::bzip2(wrapper_tag) {}

	bool bzip2::is_valid(io::seekable* file) {
		return check_signature<'B', 'Z', 'h'>(file);
	}

	io::seekable::ptr bzip2::wrap(io::seekable::ptr&& file) {
		return wrap_impl<bzip2>(std::move(file), wrapper_tag{});
	}

	base::decompressor::ptr bzip2::make_decompressor() {
		return std::make_unique<bzlib::decompressor>();
	}
}  // namespace arch::io
