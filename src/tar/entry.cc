// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/tar/entry.hh>
#include <arch/tar/stream.hh>

namespace arch::tar {
	entry::entry(fs::path const& filename,
	             fs::path const& link,
	             io::seekable* proxied,
	             size_t offset,
	             io::status const& file_status,
	             io::status const& status)
	    : base::entry_mixin{file_status, status, link}
	    , filename_{filename}
	    , proxied_{proxied}
	    , offset_{offset} {}

	fs::path const& entry::filename() const { return filename_; }

	io::stream::ptr entry::file() const {
		return std::make_unique<stream>(proxied_, offset_, file_status(),
		                                linked_status(), linkname());
	}
}  // namespace arch::tar
