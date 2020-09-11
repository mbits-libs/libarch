// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/zip/entry.hh>
#include <arch/zip/stream.hh>

namespace arch::zip {
	namespace {
		io::status from_zip(zip_stat_t const& st) {
			return {st.size, base::io::to_file_clock(st.mtime),
			        fs::file_type::regular,
			        fs::perms::owner_read | fs::perms::owner_write |
			            fs::perms::group_read | fs::perms::others_read};
		}
	}  // namespace

	entry::entry(zip_t* handle, size_t index, zip_stat_t const& st)
	    : base::dos_entry_mixin{from_zip(st)}
	    , handle_{handle}
	    , index_{index}
	    , filename_{st.name} {}

	fs::path const& entry::filename() const { return filename_; }

	io::stream::ptr entry::file() const {
		auto file = stream::zip_file{
		    zip_fopen_index(handle_, index_, ZIP_FL_UNCHANGED)};
		if (!file) return {};
		return std::make_unique<stream>(std::move(file), linked_status());
	}
}  // namespace arch::zip
