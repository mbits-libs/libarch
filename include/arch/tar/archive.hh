// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/archive.hh>
#include <vector>

namespace arch::tar {
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
		struct Entry {
			std::string name{};
			unsigned mode{};
			size_t size{};
			time_t mtime{};
			int chksum{};
			char type{};
			std::string linkname{};
			std::string magic{};

			size_t offset{};
			size_t data_offset{};
		};

		size_t realpath(size_t) const;
		size_t hardlink_for(size_t) const;

		void load_entries();
		bool next(Entry&);
		static bool header(Entry&, io::seekable*);
		bool apply_gnulong(Entry&);

		io::seekable::ptr file_{};
		std::vector<Entry> entries_{};
		size_t offset_{};
	};
}  // namespace arch::tar
