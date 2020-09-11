// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <zip.h>
#include <arch/base/io/stream.hh>

namespace arch::zip {
	class stream final : public io::dos_stream_mixin {
	public:
		struct file_closer {
			void operator()(zip_file_t* file) { zip_fclose(file); }
		};

		using zip_file = std::unique_ptr<zip_file_t, file_closer>;

		stream(zip_file&& handle, io::status const& status);
		~stream();

		void close() final;
		std::size_t read(std::span<std::byte> bytes) final;

	private:
		zip_file handle_;
		size_t size_;
		time_t mtime_;
	};
}  // namespace arch::zip
