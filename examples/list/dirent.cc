// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "dirent.hh"
#include "colors.hh"

#ifdef _WIN32
#else
#include <langinfo.h>
#endif
#include <set>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif

namespace arch::dirent {
	namespace {
		template <typename Class>
		struct alignas(Class) non_stg {
			char buffer[sizeof(Class)];
			Class* ptr() noexcept {
				return reinterpret_cast<Class*>(buffer + 0);
			}
			Class const* ptr() const noexcept {
				return reinterpret_cast<Class*>(buffer + 0);
			}
		};

		bool leading(char c) noexcept {
			auto const uc = static_cast<unsigned char>(c);
			static constexpr auto leading_mask = 0xC0u;
			static constexpr auto leading_value = 0x80u;
			return (uc & leading_mask) == leading_value;
		}

		size_t utf8len(std::string_view view) {
			size_t count{};

			for (auto c : view)
				count += !leading(c);

			return count;
		}

		non_stg<std::vector<std::string>> build_abbrev_months() {
			non_stg<std::vector<std::string>> result;
			new (result.ptr()) std::vector<std::string>(12);
			auto& months = *result.ptr();

			size_t max_len = 0;
			auto it = months.begin();
			for (int ndx = 0; ndx < 12; ++ndx) {
				auto& month = *it++;
#ifdef _WIN32
				struct tm tm = {0};
				static wchar_t buffer[200];
				tm.tm_mon = ndx;
				std::wcsftime(buffer, sizeof buffer, L"%b", &tm);
				month = []() -> std::string {
					size_t size = WideCharToMultiByte(
					    CP_ACP, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
					std::unique_ptr<char[]> out{new char[size + 1]};
					WideCharToMultiByte(CP_UTF8, 0, buffer, -1, out.get(),
					                    static_cast<int>(size + 1), nullptr,
					                    nullptr);
					return out.get();
				}();
#else
				month = nl_langinfo(ABMON_1 + ndx);
#endif
				max_len = std::max(max_len, utf8len(month));
			}

			for (auto& month : months) {
				auto const len = utf8len(month);
				if (len == max_len) continue;
				month.reserve(std::max(max_len, month.length()));
				month.append(max_len - len, ' ');
			}

			return result;
		}

		std::vector<std::string> const& abbrev_months() {
			static non_stg<std::vector<std::string>> leaky_list =
			    build_abbrev_months();
			return *leaky_list.ptr();
		}

		std::string const& abbrev_month(size_t index) {
			return abbrev_months()[index];
		}

		char type_to_char(fs::file_type type) {
			switch (type) {
				case fs::file_type::none:
					return 'n';
				case fs::file_type::not_found:
					return '*';
				case fs::file_type::regular:
					return '-';
				case fs::file_type::directory:
					return 'd';
				case fs::file_type::symlink:
					return 'l';
				case fs::file_type::block:
					return 'b';
				case fs::file_type::character:
					return 'c';
				case fs::file_type::fifo:
					return 'p';
				case fs::file_type::socket:
					return 's';
				default:
					return '?';
			}
		}

		inline void bit(unsigned val,
		                unsigned b,
		                char yes,
		                FILE* outfile) noexcept {
			fputc((val & b) == b ? yes : '-', outfile);
		}

		inline void rwx(unsigned bits, FILE* outfile) noexcept {
			bit(bits, 4, 'r', outfile);
			bit(bits, 2, 'w', outfile);
			bit(bits, 1, 'x', outfile);
		}

		inline void oga(unsigned rights, FILE* outfile) noexcept {
			rwx(rights >> 6, outfile);
			rwx(rights >> 3, outfile);
			rwx(rights, outfile);
		}

		inline void oga(fs::perms rights, FILE* outfile) noexcept {
			oga(static_cast<unsigned>(rights), outfile);
		}

		inline struct tm localtime(time_t const* time) {
#ifdef _WIN32
			struct tm result {};
			[[maybe_unused]] auto const _ = localtime_s(&result, time);
			return result;
#else
			return *std::localtime(time);
#endif
		}

		inline void print_time(fs::file_time_type last_write_time,
		                       FILE* outfile) {
			auto const now = std::time(nullptr);
			auto mtime = std::chrono::system_clock::to_time_t(
			    base::io::to_system_clock(last_write_time));
			if (mtime < 0) mtime = now;

			// COMMENT FROM LS.C: Consider a time to be recent if it is within
			// the past six months.  A Gregorian year has 365.2425 * 24 * 60 *
			// 60 == 31556952 seconds on the average.  Write this value as an
			// integer constant to avoid floating point hassles.
			auto const six_months_ago = now - 31556952 / 2;
			auto const recent = int((six_months_ago < mtime) && (mtime <= now));

			// ls's default file formats
			static constexpr char const* long_time_format[] = {" %e  %Y",
			                                                   " %e %H:%M"};
			auto fmt = long_time_format[recent];

			char date_buffer[512];
			struct tm tm = localtime(&mtime);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
			strftime(date_buffer, sizeof date_buffer, fmt, &tm);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

			fputs(abbrev_month(static_cast<size_t>(tm.tm_mon)).c_str(),
			      outfile);
			fputs(date_buffer, outfile);
		}

		inline size_t human_readable(uintmax_t size, std::span<char> output) {
			if (size < 1024) {
				auto ret =
				    std::snprintf(output.data(), output.size(), "%zu", size);
				if (ret < 0) return 0;
				return static_cast<size_t>(ret);
			}

			static constexpr char powers[] = {' ', 'k', 'M', 'G', 'T',
			                                  'P', 'E', 'Z', 'Y'};
			static constexpr auto max_power = std::size(powers) - 1;

			size *= 10;
			for (auto power : powers) {
				size += 5;
				if (size < 10245) {
					auto ret = std::snprintf(output.data(), output.size(),
					                         "%zu%c", size / 10, power);
					if (ret < 0) return 0;
					return static_cast<size_t>(ret);
				}
				size /= 1024;
			}

			size += 5;
			auto ret = std::snprintf(output.data(), output.size(), "%zu%c",
			                         size / 10, powers[max_power]);
			if (ret < 0) return 0;
			return static_cast<size_t>(ret);
		}

		template <typename String>
		using path_add_cref =
		    std::conditional_t<std::is_same_v<String, fs::path::string_type>,
		                       String const&,
		                       String>;

#ifdef _WIN32
		std::string narrow(std::wstring const& s) {
			size_t size = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)s.c_str(),
			                                  -1, nullptr, 0, nullptr, nullptr);
			std::unique_ptr<char[]> out{new char[size + 1]};
			WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)s.c_str(), -1, out.get(),
			                    static_cast<int>(size + 1), nullptr, nullptr);
			return out.get();
		}
#endif

		template <typename NativeString>
		path_add_cref<std::string> printable(NativeString const& native_path) {
			if constexpr (std::is_same_v<std::string, NativeString>) {
				return native_path;
			} else {
				return narrow(native_path);
			}
		}

		path_add_cref<std::string> printable(fs::path const& path) {
			return printable(path.native());
		}
	}  // namespace

	size_t size_from(fileinfo const& nfo) {
		if (nfo.status.type == fs::file_type::symlink)
			return nfo.status.hardlink ? nfo.symlink_size
			                           : nfo.linkname.length();
		return nfo.status.size;
	}

	void printdir(direntries const& entries, FILE* outfile) {
		char space_buffer[1024];
		size_t size_width = 0;
		for (auto const& [name, nfo] : entries) {
			auto const width = human_readable(size_from(nfo), space_buffer);
			size_width = std::max(size_width, width);
		}

		for (auto const& [name, nfo] : entries) {
			auto width = human_readable(size_from(nfo), space_buffer);

			fputc(type_to_char(nfo.status.type), outfile);
			oga(nfo.status.permissions, outfile);
			fputc(' ', outfile);
			while (width < size_width) {
				fputc(' ', outfile);
				++width;
			}
			fputs(space_buffer, outfile);
			fputc(' ', outfile);
			print_time(nfo.status.last_write_time, outfile);
			fputc(' ', outfile);

			if (nfo.status.type == fs::file_type::symlink &&
			    nfo.symlink_type == fs::file_type::not_found) {
				colors::get_default().print_color(outfile, name,
				                                  colors::ls_type::orphan,
				                                  nfo.status.permissions);
			} else if (nfo.status.type == fs::file_type::symlink &&
			           nfo.status.hardlink) {
				colors::get_default().print_color(outfile, name,
				                                  colors::ls_type::regular,
				                                  nfo.status.permissions);
			} else {
				colors::get_default().print_color(
				    outfile, name, nfo.status.type, nfo.status.permissions);
			}

			if (nfo.status.type == fs::file_type::symlink) {
				if (nfo.status.hardlink) {
					fprintf(outfile, " [%s]", nfo.linkname.c_str());
				} else {
					fputs(" -> ", outfile);
					auto symtype =
					    static_cast<colors::ls_type>(nfo.symlink_type);
					if (symtype == colors::ls_type::not_found)
						symtype = colors::ls_type::orphan;
					colors::get_default().print_color(outfile, nfo.linkname,
					                                  symtype,
					                                  nfo.symlink_permissions);
				}
			}

			fputc('\n', outfile);
		}
	}

	void dirnode::append(base::archive const& archive) {
		auto const entry_count = archive.count();
		for (std::size_t ndx = 0; ndx < entry_count; ++ndx) {
			auto entry = archive.entry(ndx);
			if (entry) append(*entry);
		}
	}

	void dirnode::append(base::entry const& entry) {
		auto&& path = entry.filename();
		auto dirname = path.parent_path();
		auto filename = path.filename();

		if (filename.empty() && !dirname.empty()) {
			filename = dirname.filename();
			dirname = dirname.parent_path();
		}

		fileinfo nfo{entry.file_status()};
		if (nfo.status.type == fs::file_type::symlink) {
			auto const& linked = entry.linked_status();
			nfo.symlink_type = linked.type;
			nfo.symlink_size = linked.size;
			nfo.linkname = printable(entry.linkname());
		}

		auto dir = navigate(dirname);
		dir->entries[printable(filename)] = std::move(nfo);
	}

	dirnode* dirnode::navigate(fs::path dirname) {
		auto dir = this;
		for (auto& seg : dirname) {
			auto sseg = printable(seg);
			auto it = dir->subnodes.find(sseg);
			if (it == dir->subnodes.end()) {
				auto pair = dir->subnodes.insert({sseg, dirnode{}});
				it = pair.first;
			}
			dir = &it->second;
		}
		return dir;
	}

	void dirnode::minimize() {
		for (auto& [name, dir] : subnodes) {
			dir.minimize();
		}

		std::set<std::string> remove{};
		std::map<std::string, dirnode> additional{};
		for (auto& [name, dir] : subnodes) {
			if (!dir.minimizable()) continue;

			remove.insert(name);
			auto prefix = name;
			prefix.push_back('/');

			for (auto& [file, nfo] : dir.entries) {
				entries[prefix + file] = std::move(nfo);
			}

			for (auto& [file, sub] : dir.subnodes) {
				additional[prefix + file] = std::move(sub);
			}
		}
		for (auto const& key : remove) {
			auto it = subnodes.find(key);
			if (it != subnodes.end()) subnodes.erase(it);
		}
		for (auto& [file, sub] : additional) {
			subnodes[file] = std::move(sub);
		}
	}

	bool dirnode::minimizable() const noexcept {
		return (subnodes.size() + entries.size()) < 2;
	}

	void dirnode::print(FILE* outfile,
	                    fs::file_time_type const& now_ish,
	                    bool& first,
	                    std::string const& prefix) const {
		if (!entries.empty() || !subnodes.empty()) {
			auto const was_first = first;
			if (!first) fputc('\n', stdout);
			first = false;
			if (prefix.empty()) {
				if (!was_first) fputs("<root>:\n", stdout);
			} else
				printf("%s:\n", prefix.c_str());
			auto copy = entries;
			for (auto const& pair : subnodes) {
				copy.insert({pair.first,
				             {{0, now_ish, fs::file_type::directory,
				               static_cast<fs::perms>(0755)},
				              {}}});
			}
			printdir(copy, outfile);
		}

		for (auto const& [name, subdir] : subnodes) {
			auto pre = prefix;
			if (!pre.empty()) pre.push_back('/');

			subdir.print(outfile, now_ish, first, pre + name);
		}
	}
}  // namespace arch::dirent
