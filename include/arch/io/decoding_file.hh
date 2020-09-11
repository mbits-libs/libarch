// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <arch/base/decompressor.hh>
#include <arch/base/io/seekable.hh>
#include <vector>

namespace arch::io {
	class decoding_file : public seekable {
	public:
		void close() override;
		io::status const& file_status() const final;
		io::status const& linked_status() const final;
		fs::path const& linkname() const final;
		std::size_t read(std::span<std::byte>) override;
		std::size_t seek(std::size_t pos) final;
		std::size_t seek_end() final;
		std::size_t tell() const final;

	protected:
		template <typename Final, typename... Args>
		static io::seekable::ptr wrap_impl(io::seekable::ptr&& file,
		                                   Args&&... args) {
			if (!file) return {};

			auto stream = std::make_unique<Final>(std::forward<Args>(args)...);
			stream->set_file(std::move(file));
			return stream;
		}

		void set_file(io::seekable::ptr&& file) {
			file_ = std::move(file);
			rewind();

			seek_end();
			seek(0);
		}

		inline void putback(std::span<std::byte> unread) {
			putback_.insert(putback_.end(), unread.begin(), unread.end());
		}

		template <typename POD>
		static inline std::span<POD> as_span(POD& input) noexcept {
			return {&input, 1};
		}

		template <typename POD>
		static inline std::span<std::byte> as_bytes(POD& input) noexcept {
			return std::as_writable_bytes(as_span(input));
		}

		virtual base::decompressor::ptr make_decompressor() = 0;
		virtual void rewind();
		void reset_decompressor();
		void move_by(size_t) noexcept;
		size_t read_lowlevel(std::span<std::byte> buffer);
		bool read_ll_char(char&);
		bool read_exactly(std::span<std::byte> buffer);

		bool eof() const noexcept { return eof_; }
		void eof_reached() noexcept;
		base::decompressor* decompressor() const noexcept {
			return decompressor_.get();
		}

	private:
		seekable::ptr file_{};
		bool eof_{false};
		size_t pos_{};
		size_t size_{};
		base::decompressor::ptr decompressor_{};
		std::vector<std::byte> putback_{};
	};
}  // namespace arch::io
