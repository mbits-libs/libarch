// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/io/gzip.hh>
#include <arch/zlib.hh>
#include "check_signature.hh"

namespace arch::io {
	namespace {
		struct Header {
			uint16_t magic;
			uint8_t compression;
			uint8_t file_flags;
			uint16_t timestamp_hi;
			uint16_t timestamp_lo;
			uint8_t compression_flags;
			uint8_t os_id;
		};
		static_assert(sizeof(Header) == 10);

		constexpr uint8_t FHCRC = 2;
		constexpr uint8_t FEXTRA = 4;
		constexpr uint8_t FNAME = 8;
		constexpr uint8_t FCOMMENT = 16;
	}  // namespace

	gzip::gzip(wrapper_tag) {}

	bool gzip::is_valid(io::seekable* file) {
		// magic + deflate
		return check_signature<0x1F, 0x8B, 0x08>(file);
	}

	io::seekable::ptr gzip::wrap(io::seekable::ptr&& file) {
		return wrap_impl<gzip>(std::move(file), wrapper_tag{});
	}

	std::size_t gzip::read(std::span<std::byte> buffer) {
		if (buffer.empty() || eof()) return 0;

		size_t result{};

		std::vector<std::byte> raw(32 * 1024);

		while (true) {
			if (result == buffer.size()) break;

			if (decompressor()->eof()) {
				if (!read_eof()) break;
				new_member_ = true;
				reset_decompressor();
			}
			if (new_member_) {
				init_read();
				if (!read_gzip_header()) {
					eof_reached();
					return 0;
				}
				new_member_ = false;
			}

			auto const read = read_lowlevel({raw.data(), raw.size()});
			auto const [decompressed, used] = decompressor()->decompress(
			    {raw.data(), read}, buffer.subspan(result));

			if (used < read) putback({raw.data() + used, read - used});

			if (!decompressed) break;

			if (decompressed) {
				static constexpr auto uint_max =
				    static_cast<size_t>(std::numeric_limits<uInt>::max());

				auto size = decompressed;
				auto data = reinterpret_cast<Bytef*>(buffer.data());
				while (size) {
					auto chunk = std::min(size, uint_max);
					crc32_ = crc32(crc32_, data, static_cast<uInt>(chunk));
					size -= chunk;
					data += chunk;
				}

				stream_size_ += decompressed;
				result += decompressed;
				move_by(decompressed);
			}
		}

		if (result) return result;

		eof_reached();
		return 0;
	}

	base::decompressor::ptr gzip::make_decompressor() {
		return std::make_unique<zlib::decompressor>(-MAX_WBITS);
	}

	void gzip::rewind() {
		decoding_file::rewind();
		new_member_ = true;
	}

	void gzip::init_read() {
		crc32_ = crc32(0, nullptr, 0);
		stream_size_ = 0;
	}

	bool gzip::read_gzip_header() {
		Header hdr;

		if (!read_exactly(as_bytes(hdr))) return false;
		if (hdr.magic != 0x8b1f) return false;

		if (hdr.file_flags & FEXTRA) {
			uint16_t size{};
			if (!read_exactly(as_bytes(size))) return false;
			std::vector<std::byte> buffer(size);
			if (!read_exactly({buffer.data(), buffer.size()})) return false;
		}

		if (hdr.file_flags & FNAME) {
			skip_asciiz();
		}

		if (hdr.file_flags & FCOMMENT) {
			skip_asciiz();
		}

		if (hdr.file_flags & FHCRC) {
			uint16_t crc16{};
			if (!read_exactly(as_bytes(crc16))) return false;
		}

		return true;
	}

	bool gzip::read_eof() {
		struct GzipEof {
			uint32_t crc;
			uint32_t stream_size;
		} eof{};
		static_assert(sizeof(GzipEof) == 8);

		if (!read_exactly(as_bytes(eof))) return false;

		if (eof.crc != crc32_) return false;
		if (eof.stream_size != (stream_size_ & 0xffff'ffff)) return false;

		char c = 0;
		bool padding_eof = false;
		while (c == 0 && !padding_eof) {
			padding_eof = !read_ll_char(c);
		}
		if (!padding_eof && c) {
			putback(as_bytes(c));
		}
		return true;
	}

	void gzip::skip_asciiz() {
		char c = 'a';
		while (c != 0) {
			auto const read = read_ll_char(c);
			if (!read) break;
		}
	}
}  // namespace arch::io
