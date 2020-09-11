// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include <arch/tar/stream.hh>

namespace arch::tar {
	stream::stream(io::seekable* proxied,
	               size_t offset,
	               io::status const& file_status,
	               io::status const& status,
	               fs::path const& link)
	    : io::stream_mixin{file_status, status, link}
	    , proxied_{proxied}
	    , offset_{offset} {}

	stream::~stream() { close(); }

	void stream::close() {
		// noop
	}

	std::size_t stream::read(std::span<std::byte> bytes) {
		auto const newpos = proxied_->seek(pos_ + offset_);
		if (newpos != pos_ + offset_) return 0;

		// this will not be virtual call, if either mixin's file_status is
		// final, or stream is final; they ar both, so this is optimizable call
		// and mixin can hold its data private
		auto const rest = file_status().size - pos_;
		if (bytes.size() > rest) bytes = bytes.subspan(0, rest);

		auto const read = proxied_->read(bytes);
		pos_ += read;
		return read;
	}
}  // namespace arch::tar
