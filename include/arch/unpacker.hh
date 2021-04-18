// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/archive.hh>

namespace arch {
	class unpacker {
	public:
		virtual ~unpacker();
		unpacker() = default;
		explicit unpacker(fs::path const& rootdir) : rootdir_{rootdir} {}

		bool unpack(fs::path const& path) const;
		bool unpack(base::archive& archive) const;

		virtual fs::path modify_path(fs::path const& filename) const;
		virtual void on_error(fs::path const& filename, char const* msg) const;
		virtual void on_note(char const* msg) const;

	private:
		fs::path filename(base::entry const&) const;
		bool expand(base::entry const&) const;
		bool expand_file(base::entry const&) const;
		bool make_directory(base::entry const&) const;
		bool make_link(base::entry const&) const;
		bool make_symlink(base::entry const&) const;

		fs::path rootdir_{};
	};

	class skip_root : public unpacker {
	public:
		skip_root(fs::path const& dst_root, fs::path const& arg_root)
		    : unpacker{dst_root}, root_{arg_root.begin(), arg_root.end()} {}

		fs::path modify_path(fs::path const& filename) const override;

	private:
		std::vector<fs::path> root_;
	};
}  // namespace arch
