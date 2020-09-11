// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/fs.hh>
#include <arch/base/io/writeable.hh>
#include <cstdio>
#include <memory>

namespace arch::io {

	class file final : public io::status_mixin<writeable> {
	private:
		struct closer {
			void operator()(FILE* file) { std::fclose(file); }
		};
		using fptr = std::unique_ptr<FILE, closer>;

		class private_tag {};

	public:
		static std::unique_ptr<file> open(fs::path const& path,
		                                  const char* mode = "rb");

		file(private_tag,
		     fptr&&,
		     io::status const&,
		     io::status const&,
		     fs::path&&);
		file();
		~file();

		void close() final;
		std::size_t write(std::span<const std::byte>) final;
		std::size_t read(std::span<std::byte>) final;
		std::size_t seek(std::size_t pos) final;
		std::size_t seek_end() final;
		std::size_t tell() const final;

	private:
		static fptr fopen(std::string const& utf8path, const char* mode);
#ifdef WIN32
		static fptr fopen(std::wstring const& path, const char* mode);
#endif

		static fptr fopen(fs::path const& path, const char* mode) {
			return fopen(path.native(), mode);
		}

		fptr file_{};
		fs::path path_{};
	};
}  // namespace arch::io
