// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/zip/stream.hh>

namespace arch::zip {
	stream::stream(zip_file&& handle, io::status const& status)
	    : io::dos_stream_mixin{status}, handle_{std::move(handle)} {}

	stream::~stream() { close(); }

	void stream::close() { handle_.reset(); }

	std::size_t stream::read(std::span<std::byte> bytes) {
		if (bytes.empty()) return 0;

		auto ret = zip_fread(handle_.get(), bytes.data(), bytes.size());
		if (ret < 0) return 0;
		return static_cast<size_t>(ret);
	}
}  // namespace arch::zip
