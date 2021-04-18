// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/archive.hh>
#include <string_view>
#include <vector>

namespace arch {
	enum class open_status {
		ok,
		compression_damaged,
		archive_damaged,
		archive_unknown
	};

	open_status open(io::seekable::ptr file, base::archive::ptr& archive);
	std::vector<std::string_view> known_extentions();
}  // namespace arch
