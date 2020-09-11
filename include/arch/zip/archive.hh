// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/archive.hh>
#include <vector>

typedef struct zip_source zip_source_t;

namespace arch::zip {
	class archive final : public base::archive {
	public:
		archive() = default;
		~archive() { close(); }

		static bool is_valid(io::seekable*);

		bool open(io::seekable::ptr) final;
		void close() final;
		size_t count() const final;
		base::entry::ptr entry(size_t) const final;

	private:
		static bool open(io::seekable*, void** handle, zip_source_t** source);
		io::seekable::ptr file_{};
		void* handle_{nullptr};
		zip_source_t* source_{nullptr};
		std::vector<char> buffer_;
	};
}  // namespace arch::zip
