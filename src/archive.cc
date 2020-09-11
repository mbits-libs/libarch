// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/archive.hh>

#include <arch/io/bzip2.hh>
#include <arch/io/gzip.hh>
#include <arch/io/lzma.hh>
#include <arch/tar/archive.hh>
#include <arch/zip/archive.hh>

namespace arch {
	namespace {
		struct filter_info {
			bool (*is_valid)(io::seekable* file);
			io::seekable::ptr (*wrap)(io::seekable::ptr&& file);

			template <typename Filter>
			struct from {
				static bool is_valid(io::seekable* file) {
					return Filter::is_valid(file);
				}

				static io::seekable::ptr wrap(io::seekable::ptr&& file) {
					return Filter::wrap(std::move(file));
				}

				constexpr operator filter_info() const {
					return {is_valid, wrap};
				}
			};
		};

		struct archive_info {
			bool (*is_valid)(io::seekable* file);
			base::archive::ptr (*open)(io::seekable::ptr&& file);

			template <typename Archive>
			struct from {
				static bool is_valid(io::seekable* file) {
					return Archive::is_valid(file);
				}

				static base::archive::ptr open(io::seekable::ptr&& file) {
					auto ptr = std::make_unique<Archive>();
					if (!ptr->open(std::move(file))) ptr.reset();
					return ptr;
				}

				constexpr operator archive_info() const {
					return {is_valid, open};
				}
			};
		};

		open_status wrap(io::seekable::ptr& file) {
			static constexpr filter_info all_filters[] = {
			    filter_info::from<io::gzip>{},
			    filter_info::from<io::bzip2>{},
			    filter_info::from<io::lzma>{},
			};

			bool modified = true;
			while (modified) {
				modified = false;
				for (auto const& nfo : all_filters) {
					file->seek(0);
					if (!nfo.is_valid(file.get())) continue;

					file->seek(0);
					file = nfo.wrap(std::move(file));
					if (!file) return open_status::compression_damaged;
					modified = true;
					break;
				}
			}

			return open_status::ok;
		}
	}  // namespace

	open_status open(io::seekable::ptr file, base::archive::ptr& archive) {
		auto result = wrap(file);
		if (result != open_status::ok) return result;

		static constexpr archive_info all_archives[] = {
		    archive_info::from<zip::archive>{},
		    archive_info::from<tar::archive>{},
		};

		for (auto const& nfo : all_archives) {
			file->seek(0);
			if (!nfo.is_valid(file.get())) continue;
			file->seek(0);
			archive = nfo.open(std::move(file));
			return archive ? open_status::ok : open_status::archive_damaged;
		}

		return open_status::archive_unknown;
	}
}  // namespace arch
