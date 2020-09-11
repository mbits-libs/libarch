// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/entry.hh>

typedef struct zip zip_t;
typedef struct zip_stat zip_stat_t;

namespace arch::zip {
	class entry final : public base::dos_entry_mixin {
	public:
		entry(zip_t* handle, size_t index, zip_stat_t const& st);

		fs::path const& filename() const final;
		io::stream::ptr file() const final;

	private:
		zip_t* handle_;
		size_t index_;
		fs::path filename_;
	};
}  // namespace arch::zip
