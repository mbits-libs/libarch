// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "arch/lzma.hh"
#include <limits>
#include "decompress_impl.hh"

namespace arch::impl {
	template <>
	struct stream_traits<lzma_stream> : stream_traits_base<lzma_stream> {
		static inline lzma_ret decompress(lzma_stream* stream) noexcept {
			return lzma_code(stream, LZMA_RUN);
		}

		static inline constexpr bool meta_data(lzma_stream* stream,
		                                       lzma_ret ret) noexcept {
			if (ret == LZMA_GET_CHECK || ret == LZMA_NO_CHECK) {
				lzma_get_check(stream);
				return true;
			}
			return false;
		}

		static inline constexpr bool ok(lzma_ret ret) noexcept {
			return ret == LZMA_OK;
		}

		static inline constexpr bool stream_end(lzma_ret ret) noexcept {
			return ret == LZMA_STREAM_END;
		}
	};
}  // namespace arch::impl

namespace arch::lzma {
	decompressor::decompressor() {
		auto const lzret =
		    lzma_auto_decoder(&lzs_, std::numeric_limits<uint64_t>::max(),
		                      LZMA_TELL_ANY_CHECK | LZMA_TELL_NO_CHECK);
		is_initialised_ = lzret == LZMA_OK;
	}

	decompressor::~decompressor() {
		if (is_initialised_) lzma_end(&lzs_);
	}

	std::pair<size_t, size_t> decompressor::decompress(
	    std::span<std::byte> input,
	    std::span<std::byte> output) {
		return impl::decompress(input, output, lzs_);
	}
}  // namespace arch::lzma
