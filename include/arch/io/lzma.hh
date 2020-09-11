// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/io/decoding_file.hh>

namespace arch::io {
	class lzma final : public decoding_file {
		class wrapper_tag {};

	public:
		explicit lzma(wrapper_tag);
		static bool is_valid(io::seekable* file);
		static io::seekable::ptr wrap(io::seekable::ptr&& file);

	private:
		base::decompressor::ptr make_decompressor() final;
	};
}  // namespace arch::io
