// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/io/decoding_file.hh>
#include <cstring>

namespace arch::io {
	void decoding_file::close() { decompressor_.reset(); }

	io::status const& decoding_file::file_status() const {
		return file_->file_status();
	}

	io::status const& decoding_file::linked_status() const {
		return file_->linked_status();
	}

	fs::path const& decoding_file::linkname() const {
		return file_->linkname();
	}

	std::size_t decoding_file::read(std::span<std::byte> buffer) {
		if (buffer.empty() || eof()) return 0;

		size_t result{};

		std::vector<std::byte> raw(10 * 1024 * 1024);

		while (true) {
			if (result == buffer.size()) break;

			if (decompressor_->eof()) reset_decompressor();

			auto const read = read_lowlevel({raw.data(), raw.size()});
			auto const [decompressed, used] = decompressor_->decompress(
			    {raw.data(), read}, buffer.subspan(result));

			if (used < read) putback({raw.data() + used, read - used});

			if (!decompressed) break;

			if (decompressed) {
				result += decompressed;
				move_by(decompressed);
			}
		}

		if (result) return result;

		eof_reached();
		return 0;
	}

	std::size_t decoding_file::seek(std::size_t pos) {
		if (pos == pos_) return pos_;

		auto offset = pos;
		if (pos < pos_)
			rewind();
		else
			offset -= pos_;

		// skip
		std::byte buffer[10240];
		while (offset) {
			auto const needed = std::min(sizeof(buffer), offset);
			auto const decoded = read({buffer, needed});
			if (!decoded) break;
			offset -= decoded;
		}

		return pos_;
	}

	std::size_t decoding_file::seek_end() {
		std::byte buffer[10240];
		while (read({buffer}))
			;
		return pos_;
	}

	std::size_t decoding_file::tell() const { return pos_; }

	void decoding_file::rewind() {
		file_->seek(0);
		eof_ = false;
		pos_ = 0;
		reset_decompressor();
		putback_.clear();
	}

	void decoding_file::reset_decompressor() {
		decompressor_ = make_decompressor();
	}

	void decoding_file::move_by(size_t decompressed) noexcept {
		pos_ += decompressed;
	}

	size_t decoding_file::read_lowlevel(std::span<std::byte> buffer) {
		size_t read{}, buffered{};

		if (!putback_.empty()) {
			buffered = buffer.size();
			if (buffered > putback_.size()) buffered = putback_.size();
			std::memcpy(buffer.data(), putback_.data(), buffered);
			if (buffered == putback_.size())
				putback_.clear();
			else {
				auto const diff = static_cast<std::iterator_traits<decltype(
				    putback_.begin())>::difference_type>(buffered);
				putback_.erase(putback_.begin(),
				               std::next(putback_.begin(), diff));
			}
		}

		if (buffered < buffer.size())
			read = file_->read(buffer.subspan(buffered));

		return buffered + read;
	}

	bool decoding_file::read_ll_char(char& c) {
		return read_exactly(as_bytes(c));
	}

	bool decoding_file::read_exactly(std::span<std::byte> buffer) {
		return read_lowlevel(buffer) == buffer.size();
	}

	void decoding_file::eof_reached() noexcept {
		eof_ = true;
		size_ = pos_;
	}
}  // namespace arch::io
