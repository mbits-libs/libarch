// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/io/decoding_file.hh>

namespace arch::io {
	class gzip final : public decoding_file {
		class wrapper_tag {};

	public:
		explicit gzip(wrapper_tag);
		static bool is_valid(io::seekable* file);
		static io::seekable::ptr wrap(io::seekable::ptr&& file);
		std::size_t read(std::span<std::byte>) final;

	private:
		base::decompressor::ptr make_decompressor() final;
		void rewind() final;
		void init_read();
		bool read_gzip_header();
		bool read_eof();
		void skip_asciiz();

		bool new_member_{true};
		size_t stream_size_{};
		unsigned long crc32_{};
	};
}  // namespace arch::io
