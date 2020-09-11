// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/fs.hh>
#include <map>
#include <string>
#include <string_view>

#define FILE_TYPE(X) \
	X(none)          \
	X(not_found)     \
	X(regular)       \
	X(directory)     \
	X(symlink)       \
	X(block)         \
	X(character)     \
	X(fifo)          \
	X(socket)        \
	X(unknown)

#define LS_TYPE(X) \
	FILE_TYPE(X)   \
	X(reset)       \
	X(orphan)      \
	X(executable)

namespace arch::colors {
	template <typename Enum>
	using short_or = std::conditional_t<
	    sizeof(std::underlying_type_t<Enum>) < sizeof(short),
	    std::conditional_t<std::is_signed_v<std::underlying_type_t<Enum>>,
	                       signed short,
	                       unsigned short>,
	    std::underlying_type_t<Enum>>;

	template <typename Enum>
	requires std::is_enum_v<Enum> constexpr inline auto val(Enum e) {
		return std::underlying_type_t<Enum>(e);
	}

#define LAST(X)                                                                \
	(std::numeric_limits<std::underlying_type_t<fs::file_type>>::max() - (X) + \
	 1)

#define COPY_VAL(N) N = val(fs::file_type::N),
	enum class ls_type : short_or<fs::file_type> {
		FILE_TYPE(COPY_VAL)

		    reset = LAST(3),
		orphan,
		executable
	};
#undef COPY_VAL
#undef LAST

	class painter const& get_default();
	class painter {
	public:
		friend painter const& get_default();
		void present() const noexcept;
		char const* find_color(std::string_view out,
		                       ls_type type,
		                       fs::perms permissions) const noexcept;
		void print_color(FILE* outfile,
		                 std::string_view out,
		                 fs::file_type type,
		                 fs::perms permissions) const noexcept {
			print_color(outfile, out, static_cast<ls_type>(type), permissions);
		}
		void print_color(FILE* outfile,
		                 std::string_view out,
		                 ls_type type,
		                 fs::perms permissions) const noexcept {
			print_color(outfile, out, find_color(out, type, permissions));
		}
		void print_color(FILE* outfile,
		                 std::string_view out,
		                 char const* color) const noexcept;

	private:
		void parse_ls_colors(std::string_view env);

		std::map<ls_type, std::string> types_{};
		std::map<std::string, std::string> exts_{};
		std::string reset_{};
	};

}  // namespace arch::colors
