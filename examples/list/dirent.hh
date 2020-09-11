// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/archive.hh>
#include <arch/base/io/stream.hh>
#include <map>

namespace arch::dirent {
	struct fileinfo {
		io::status status{};
		fs::file_type symlink_type{fs::file_type::none};
		fs::perms symlink_permissions{fs::perms::none};
		std::string linkname{};
		size_t symlink_size{};
	};

	using direntries = std::map<std::string, fileinfo>;

	void printdir(direntries const& entries, FILE* outfile);

	struct dirnode {
		direntries entries{};
		std::map<std::string, dirnode> subnodes{};

		void append(base::archive const& archive);
		void append(base::entry const& entry);
		void minimize();
		void print(FILE* outfile, fs::file_time_type const& now_ish) const {
			bool first = true;
			print(outfile, now_ish, first);
		}

	private:
		dirnode* navigate(fs::path dirname);
		bool minimizable() const noexcept;
		void print(FILE* outfile,
		           fs::file_time_type const& now_ish,
		           bool& first,
		           std::string const& prefix = {}) const;
	};
}  // namespace arch::dirent
