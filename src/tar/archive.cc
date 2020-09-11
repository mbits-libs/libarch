// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/tar/archive.hh>
#include <arch/tar/entry.hh>
#include "check_signature.hh"

#include <sys/types.h>
#include <cctype>
#include <charconv>
#include <chrono>
#include <numeric>
#include <unordered_set>

#ifdef WIN32
#include <Windows.h>
#undef min
#undef max
#endif

namespace arch::tar {
	namespace {
		constexpr size_t RECORDSIZE = 512;

		constexpr auto REGTYPE = '0';           // regular file
		constexpr auto AREGTYPE = '\0';         // regular file
		constexpr auto LNKTYPE = '1';           // link (inside tarfile)
		constexpr auto SYMTYPE = '2';           // symbolic link
		constexpr auto CHRTYPE = '3';           // character special device
		constexpr auto BLKTYPE = '4';           // block special device
		constexpr auto DIRTYPE = '5';           // directory
		constexpr auto FIFOTYPE = '6';          // fifo special device
		constexpr auto CONTTYPE = '7';          // contiguous file
		constexpr auto GNUTYPE_LONGNAME = 'L';  // GNU tar longname
		constexpr auto GNUTYPE_LONGLINK = 'K';  // GNU tar longlink
		constexpr auto GNUTYPE_SPARSE = 'S';    // GNU tar sparse file

		std::string_view as_string_view(std::string_view str) {
			auto pos = str.find('\0');
			if (pos == std::string_view::npos) return str;
			return str.substr(0, pos);
		}

		std::string as_string(std::string_view str) {
			auto trimmed = as_string_view(str);
			return {trimmed.data(), trimmed.size()};
		}

		template <typename Value>
		bool as_num(Value& result, std::string_view num) {
			result = 0;
			auto const unum0 = static_cast<unsigned char>(num[0]);
			if (unum0 == 0200u || unum0 == 0377u) {
				std::make_unsigned_t<Value> number{};
				auto const is_neg = unum0 == 0377u;

				for (auto signed_char : num.substr(1)) {
					number <<= 8u;
					number += static_cast<unsigned char>(signed_char);
				}

#ifdef _MSC_VER
#pragma warning(push)
// unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable : 4146)
#endif
				// TODO: overflow...
				if (is_neg)
					result = static_cast<Value>(-number);
				else
					result = static_cast<Value>(number);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
				return true;
			}

			num = as_string_view(num);
			while (!num.empty() &&
			       std::isspace(static_cast<unsigned char>(num.front())))
				num = num.substr(1);

			while (!num.empty() &&
			       std::isspace(static_cast<unsigned char>(num.back())))
				num = num.substr(0, num.size() - 1);

			if (num.empty()) return {};

			auto begin = num.data();
			auto end = num.data() + num.size();

			auto [ptr, ec] = std::from_chars(begin, end, result, 8);
			return (ec == std::errc{} && ptr == end);
		}

		bool checksums(std::string_view header, int chksum) {
			int unsigned_sum = 256;
			int signed_sum = 256;

			auto first = header.substr(0, 148);
			auto second = header.substr(156);

			for (auto const* chunk : {&first, &second}) {
				signed_sum += std::accumulate(
				    chunk->begin(), chunk->end(), 0,
				    [](int ax, signed char c) { return ax + c; });
				unsigned_sum += std::accumulate(
				    chunk->begin(), chunk->end(), 0,
				    [](int ax, unsigned char c) { return ax + c; });
			}

			return unsigned_sum == chksum || signed_sum == chksum;
		}

		inline size_t block_size(size_t size) noexcept {
			return ((size + (RECORDSIZE - 1)) / RECORDSIZE) * RECORDSIZE;
		}

		fs::file_type fs_type(char type, bool& is_hardlink) {
			switch (type) {
				case REGTYPE:
				case AREGTYPE:
				case CONTTYPE:
					return fs::file_type::regular;
				case LNKTYPE:
				case SYMTYPE:
					is_hardlink = type == LNKTYPE;
					return fs::file_type::symlink;
				case CHRTYPE:
					return fs::file_type::character;
				case BLKTYPE:
					return fs::file_type::block;
				case DIRTYPE:
					return fs::file_type::directory;
				case FIFOTYPE:
					return fs::file_type::fifo;
				default:
					break;
			}
			return fs::file_type::unknown;
		}

		fs::perms fs_perms(unsigned mode) {
			// file permissions, take the rwx + stickies posix value
			static constexpr auto ALL = 07777u;
			return static_cast<fs::perms>(mode & ALL);
		}
	}  // namespace

#define HANDLE get_handle(handle_)

	bool archive::is_valid(io::seekable* file) {
		if (check_signature<'u', 's', 't', 'a', 'r'>(file, 257)) return true;

		Entry entry{};
		file->seek(0);
		return header(entry, file);
	}

	bool archive::open(io::seekable::ptr file) {
		file_ = std::move(file);
		if (!file_) return false;

		std::byte byte[1];
		auto const size = file_->read(std::span{byte});
		file_->seek(0);

		if (size) load_entries();

		return size == 1 && !entries_.empty();
	}

	void archive::close() { file_.reset(); }

	size_t archive::count() const { return entries_.size(); }

	base::entry::ptr archive::entry(size_t index) const {
		if (index >= entries_.size()) return {};
		auto const& ref = entries_[index];

		bool is_hardlink = false;
		auto const type = fs_type(ref.type, is_hardlink);
		io::status const file_status{ref.size,
		                             base::io::to_file_clock(ref.mtime), type,
		                             fs_perms(ref.mode), is_hardlink};
		io::status const status = [&, self = this]() -> io::status {
			auto const link =
			    is_hardlink ? self->hardlink_for(index) : self->realpath(index);
			if (link == index) return file_status;
			if (link == self->entries_.size())
				return {0, {}, fs::file_type::not_found, fs::perms::none};
			auto const& linked = self->entries_[link];

			bool link_is_also_hard = false;
			auto const type = fs_type(linked.type, link_is_also_hard);
			return {linked.size, base::io::to_file_clock(linked.mtime), type,
			        fs_perms(linked.mode), link_is_also_hard};
		}();

		return std::make_unique<tar::entry>(ref.name, ref.linkname, file_.get(),
		                                    ref.data_offset, file_status,
		                                    status);
	}

	std::string normlized(fs::path const& path) {
		auto const thisdir = fs::path{"."}.native();
		auto const updir = fs::path{".."}.native();
		auto const sep = fs::path{"/"}.native();

		std::vector<fs::path::string_type> segments{};
		size_t negative = 0;

		for (auto const& seg : path) {
			if (seg == thisdir) continue;
			if (seg == updir) {
				if (segments.empty())
					++negative;
				else
					segments.pop_back();
				continue;
			}
			segments.push_back(seg.native());
		}

		size_t length = segments.size() * sep.size() +
		                negative * (updir.size() + sep.size());
		if (!segments.empty()) length -= sep.size();
		for (auto const& seg : segments)
			length += seg.size();

		fs::path::string_type normalized{};
		normalized.reserve(length);

		for (size_t index = 0; index < negative; ++index) {
			normalized += updir;
			normalized += sep;
		}

		bool first = true;
		for (auto const& seg : segments) {
			if (first)
				first = false;
			else
				normalized += sep;
			normalized += seg;
		}

#ifdef WIN32
		size_t size = WideCharToMultiByte(CP_UTF8, 0, normalized.c_str(), -1,
		                                  nullptr, 0, nullptr, nullptr);
		std::unique_ptr<char[]> out{new char[size + 1]};
		WideCharToMultiByte(CP_UTF8, 0, normalized.c_str(), -1, out.get(),
		                    static_cast<int>(size + 1), nullptr, nullptr);
		return out.get();
#else
		return normalized;
#endif
	}

	template <typename EntryType>
	size_t realpath_impl_next(size_t index,
	                          std::vector<EntryType> const& entries) {
		auto const& ref = entries[index];
		if (ref.type != SYMTYPE) return index;

		auto path = normlized(fs::path{ref.name}.parent_path() / ref.linkname);
		auto it =
		    std::find_if(entries.begin(), entries.end(),
		                 [&](auto const& entry) { return entry.name == path; });
		if (it == entries.end()) return index;
		return static_cast<size_t>(std::distance(entries.begin(), it));
	}

	template <typename EntryType>
	size_t realpath_impl(size_t index,
	                     std::vector<EntryType> const& entries,
	                     std::unordered_set<size_t>& needles) {
		auto const next = realpath_impl_next(index, entries);
		if (next == index || needles.count(next)) return index;
		needles.insert(index);
		return realpath_impl(next, entries, needles);
	}

	size_t archive::realpath(size_t index) const {
		std::unordered_set<size_t> needles{};
		auto result = realpath_impl(index, entries_, needles);
		if (result < entries_.size() && entries_[result].type == SYMTYPE)
			return entries_.size();
		return result;
	}

	size_t archive::hardlink_for(size_t index) const {
		auto const& ref = entries_[index];

		if (ref.type != LNKTYPE) return index;

		auto it = std::find_if(
		    entries_.begin(), entries_.end(),
		    [&](auto const& entry) { return entry.name == ref.linkname; });
		if (it == entries_.end()) return index;
		return static_cast<size_t>(std::distance(entries_.begin(), it));
	}

	void archive::load_entries() {
		offset_ = file_->tell();

		while (true) {
			entries_.push_back({});
			if (next(entries_.back())) continue;
			entries_.pop_back();
			break;
		}
	}

	bool archive::next(Entry& entry) {
		if (offset_ != file_->tell()) {
			file_->seek(offset_ - 1);
			std::byte ignores[1];
			if (!file_->read(ignores)) return false;
		}

		if (!header(entry, file_.get())) return false;

		entry.data_offset = file_->tell();
		entry.offset = entry.data_offset - RECORDSIZE;
		auto offset = entry.data_offset;
		switch (entry.type) {
			// empties, do not even look at size here:
			case LNKTYPE:
			case SYMTYPE:
			case DIRTYPE:
			case FIFOTYPE:
			case CONTTYPE:
			case CHRTYPE:
			case BLKTYPE:
				break;

			// prefixes for actual entries, read the long names:
			case GNUTYPE_LONGNAME:
			case GNUTYPE_LONGLINK:
				// offset_ will be updated inside another next() here
				return apply_gnulong(entry);

			// everything else, see how much data is attach to the entry.
			default:
				offset += block_size(entry.size);
				break;
		};
		offset_ = offset;

		return true;
	}

	bool archive::header(Entry& entry, io::seekable* file) {
		std::byte header[RECORDSIZE];
		if (file->read(header) != RECORDSIZE) return false;

		std::string_view view{reinterpret_cast<char const*>(header),
		                      RECORDSIZE};

		if (!as_num(entry.chksum, view.substr(148, 8))) return false;
		if (!checksums(view, entry.chksum)) return false;

		entry.name = as_string(view.substr(0, 100));
		if (!as_num(entry.mode, view.substr(100, 8))) return false;
		if (!as_num(entry.size, view.substr(124, 12))) return false;
		if (!as_num(entry.mtime, view.substr(136, 12))) return false;
		entry.type = view[156];
		entry.linkname = as_string(view.substr(157, 100));
		entry.magic.assign(view.substr(257, 6));

		auto prefix = as_string(view.substr(345, 155));

		if (entry.type == AREGTYPE && entry.name.ends_with('/'))
			entry.type = DIRTYPE;

		if (entry.type == DIRTYPE) {
			while (entry.name.ends_with('/'))
				entry.name.pop_back();
		}

		if (!prefix.empty()) {
			switch (entry.type) {
				case GNUTYPE_LONGNAME:
				case GNUTYPE_LONGLINK:
				case GNUTYPE_SPARSE:
					break;
				default:
					entry.name = prefix + "/" + entry.name;
					break;
			}
		}

		return true;
	}

	bool archive::apply_gnulong(Entry& entry) {
		std::vector<std::byte> longname(block_size(entry.size));
		if (file_->read({longname.data(), longname.size()}) !=
		    longname.size()) {
			return false;
		}

		offset_ = file_->tell();

		auto const current_offset = entry.offset;
		auto const current_type = entry.type;
		if (!next(entry)) return false;

		std::string_view view{reinterpret_cast<char const*>(longname.data()),
		                      longname.size()};
		entry.offset = current_offset;
		if (current_type == GNUTYPE_LONGNAME)
			entry.name = as_string(view);
		else if (current_type == GNUTYPE_LONGLINK)
			entry.linkname = as_string(view);

		return true;
	}
}  // namespace arch::tar
