// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "colors.hh"
namespace arch::colors {
	namespace {
		using namespace std::literals;

		auto constexpr LS_COLORS_builtin =
		    "rs=0:di=01;34:ln=01;36:mh=00:pi=40;33:so=01;35:do=01;35:bd=40;33;"
		    "01:cd=40;33;01:"
		    "or=40;31;01:mi=00:su=37;41:sg=30;43:ca=30;41:tw=30;42:ow=34;42:st=37;44:ex=01;32:"sv;

		std::map<std::string_view, ls_type> ls_to_enum = {
		    {"bd"sv, ls_type::block},     {"cd"sv, ls_type::character},
		    {"di"sv, ls_type::directory}, {"ex"sv, ls_type::executable},
		    {"ln"sv, ls_type::symlink},   {"mi"sv, ls_type::not_found},
		    {"no"sv, ls_type::regular},   {"or"sv, ls_type::orphan},
		    {"pi"sv, ls_type::fifo},      {"rs"sv, ls_type::reset},
		    {"so"sv, ls_type::socket},
		};
	}  // namespace

	painter const& get_default() {
		static painter instance = [] {
			painter result{};
			auto env = std::getenv("LS_COLORS");
			if (env)
				result.parse_ls_colors(env);
			else
				result.parse_ls_colors(LS_COLORS_builtin);

			result.reset_ = [&] {
				auto it = result.types_.find(ls_type::reset);
				if (it != result.types_.end())
					return "\033[" + it->second + "m";
				return std::string{""};
			}();

			return result;
		}();
		return instance;
	}

	void painter::present() const noexcept {
#define X(N) {ls_type::N, #N##sv},
		static constexpr struct {
			ls_type type;
			std::string_view name;
		} types[] = {LS_TYPE(X)};

		for (auto [type, name] : types) {
			auto it = types_.find(type);
			auto color = (it == types_.end()) ? nullptr : it->second.c_str();
			if (color) printf("\033[%sm", color);
			fputs(name.data(), stdout);
			if (color) fputs(reset_.c_str(), stdout);
			fputc('\n', stdout);
		}

		bool first = true;
		for (auto [ext, color] : exts_) {
			if (first)
				first = false;
			else
				fputs(", ", stdout);
			print_color(stdout, "*" + ext, fs::file_type::regular,
			            fs::perms::none);
		}
		if (!first) fputc('\n', stdout);
	}

	char const* painter::find_color(std::string_view printed,
	                                ls_type type,
	                                fs::perms permissions) const noexcept {
		auto const has_any_exec =
		    (0111u &
		     static_cast<std::underlying_type_t<fs::perms>>(permissions));

		auto it = types_.find(type);
		if (it == types_.end() && has_any_exec)
			it = types_.find(ls_type::executable);
		if (it != types_.end()) return it->second.c_str();

		auto dot = printed.rfind('.');
		auto slash = printed.rfind('/');

		if (dot == std::string_view::npos) return nullptr;

		// .files
		if (slash == std::string_view::npos) {
			if (dot == 0) return nullptr;
		} else {
			if (dot == slash + 1) return nullptr;
		}

		auto const ext = printed.substr(dot);
		std::string const ext_s{ext.data(), ext.size()};
		auto const ext_it = exts_.find(ext_s);
		if (ext_it != exts_.end()) return ext_it->second.c_str();

		return nullptr;
	}

	void painter::print_color(FILE* outfile,
	                          std::string_view printed,
	                          char const* color) const noexcept {
		if (!color) {
			fwrite(printed.data(), 1, printed.size(), outfile);
			return;
		}

		fputs("\033[", outfile);
		fputs(color, outfile);
		fputs("m", outfile);
		fwrite(printed.data(), 1, printed.size(), outfile);
		if (!reset_.empty()) fputs(reset_.c_str(), outfile);
	}

	void painter::parse_ls_colors(std::string_view env) {
		while (!env.empty()) {
			auto const pos = env.find(':');
			auto const color =
			    pos == std::string_view::npos ? env : env.substr(0, pos);
			env = env.substr(pos == std::string_view::npos ? pos : pos + 1);

			auto const eq = color.find('=');
			if (eq == std::string_view::npos) continue;
			auto name = color.substr(0, eq);
			auto value = color.substr(eq + 1);
			while (!name.empty() &&
			       std::isspace(static_cast<unsigned>(name.front())))
				name = name.substr(1);
			while (!name.empty() &&
			       std::isspace(static_cast<unsigned>(name.back())))
				name = name.substr(0, name.size() - 1);
			while (!value.empty() &&
			       std::isspace(static_cast<unsigned>(value.front())))
				value = value.substr(1);
			while (!value.empty() &&
			       std::isspace(static_cast<unsigned>(value.back())))
				value = value.substr(0, value.size() - 1);

			if (name.empty() || value.empty()) continue;

			auto it = ls_to_enum.find(name);
			if (it != ls_to_enum.end()) {
				types_[it->second] = value;
				continue;
			}

			if (name.front() == '*') {
				name = name.substr(1);
				if (name.empty()) continue;

				if (name.front() != '.') continue;
				exts_[{name.data(), name.size()}] = value;
			}
		}
	}
}  // namespace arch::colors
