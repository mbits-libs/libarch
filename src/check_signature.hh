// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/io/seekable.hh>
#include <cstring>

namespace arch {
	template <size_t N>
	inline bool check_signature(io::seekable* file,
	                            unsigned char const (&magic)[N],
	                            size_t offset = 0) {
		auto pos = file->seek(offset);
		if (pos != offset) return false;

		std::byte buffer[N];
		if (file->read(buffer) != sizeof(buffer)) return false;
		return !std::memcmp(buffer, magic, sizeof(buffer));
	}

	template <unsigned char... Chars>
	inline bool check_signature(io::seekable* file, size_t offset = 0) {
		static constexpr unsigned char magic[] = {Chars...};
		return check_signature(file, magic, offset);
	}
}  // namespace arch