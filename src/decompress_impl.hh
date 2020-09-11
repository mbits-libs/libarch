// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <span>

namespace arch::impl {
	template <typename Stream>
	struct stream_traits_base {
		template <int Direction>
		static auto& next(Stream& stream) {
			if constexpr (Direction == 0)
				return (stream.next_in);
			else if constexpr (Direction == 1)
				return (stream.next_out);
		}

		template <int Direction>
		static auto next(Stream const& stream) {
			if constexpr (Direction == 0)
				return (stream.next_in);
			else if constexpr (Direction == 1)
				return (stream.next_out);
		}

		template <int Direction>
		static auto& avail(Stream& stream) {
			if constexpr (Direction == 0)
				return (stream.avail_in);
			else if constexpr (Direction == 1)
				return (stream.avail_out);
		}

		template <int Direction>
		static auto avail(Stream const& stream) {
			if constexpr (Direction == 0)
				return (stream.avail_in);
			else if constexpr (Direction == 1)
				return (stream.avail_out);
		}

		template <typename ResultInt>
		static inline constexpr bool meta_data(Stream*, ResultInt) noexcept {
			return false;
		}
	};

	template <typename Stream>
	struct stream_traits : stream_traits_base<Stream> {};

	template <int Direction>
	struct stream_stats {
		std::size_t length{};
		std::size_t chunk{};
		std::size_t used{};

		template <typename Stream>
		explicit stream_stats(std::span<std::byte> buffer, Stream& stream)
		    : length{buffer.size()} {
			using traits = stream_traits<Stream>;
			using data_ptr = std::remove_reference_t<decltype(
			    traits::template next<Direction>(stream))>;

			auto data = reinterpret_cast<data_ptr>(buffer.data());
			traits::template next<Direction>(stream) = data;
			traits::template avail<Direction>(stream) = 0;
		}

		template <typename StreamSizeType>
		void update_avail_(StreamSizeType& avail) noexcept {
			static constexpr auto uint_max =
			    static_cast<size_t>(std::numeric_limits<StreamSizeType>::max());
			if (!avail) {
				used += chunk;
				chunk = std::min(length, uint_max);
				avail = static_cast<StreamSizeType>(chunk);
				length -= chunk;
			}
		}

		template <typename Stream>
		void update_avail(Stream& stream) noexcept {
			auto& avail =
			    stream_traits<Stream>::template avail<Direction>(stream);
			update_avail_(avail);
		}

		template <typename Stream>
		bool empty(Stream const& stream) const noexcept {
			auto const avail =
			    stream_traits<Stream>::template avail<Direction>(stream);
			return !avail || !length;
		}

		template <typename Stream>
		void on_chunk_read(Stream const& stream) noexcept {
			auto const avail =
			    stream_traits<Stream>::template avail<Direction>(stream);
			used += chunk - avail;
		}
	};

	template <typename Stream>
	std::pair<size_t, size_t> decompress(std::span<std::byte> input,
	                                     std::span<std::byte> output,
	                                     Stream& stream) {
		stream_stats<0> in{input, stream};
		stream_stats<1> out{output, stream};

		using traits = stream_traits<Stream>;

		while (true) {
			in.update_avail(stream);
			out.update_avail(stream);

			if (!stream.avail_in || !stream.avail_out) break;

			auto const res = traits::decompress(&stream);
			if (traits::meta_data(&stream, res)) continue;
			if (!traits::ok(res) && !traits::stream_end(res)) break;

			if (traits::stream_end(res) || in.empty(stream) ||
			    out.empty(stream)) {
				in.on_chunk_read(stream);
				out.on_chunk_read(stream);
				break;
			}
		}

		return {out.used, in.used};
	}
}  // namespace arch::impl