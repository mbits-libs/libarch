// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/io/seekable.hh>
#include <arch/base/io/stream.hh>

namespace arch::tar {
	class stream final : public io::stream_mixin {
	public:
		stream(io::seekable* proxied,
		       size_t offset,
		       io::status const& file_status,
		       io::status const& status,
		       fs::path const& link);
		~stream();

		void close() final;
		std::size_t read(std::span<std::byte> bytes) final;

	private:
		io::seekable* proxied_{};
		size_t pos_{};
		size_t offset_{};
	};
}  // namespace arch::tar
