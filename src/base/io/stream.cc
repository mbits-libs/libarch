// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#include "arch/base/io/stream.hh"

namespace arch::base::io {
#if __cpp_lib_chrono >= 201907L
#define HAS_CXX20_FILE_CLOCK
#elif defined(__GNUC__)
#define GCC_VER (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VER >= 100100
#define HAS_CXX20_FILE_CLOCK
#endif
#endif

#if defined(HAS_CXX20_FILE_CLOCK)
	std::chrono::system_clock::time_point to_system_clock(
	    fs::file_time_type last_write_time) {
		return std::chrono::file_clock::to_sys(last_write_time);
	}

	fs::file_time_type to_file_clock(
	    std::chrono::system_clock::time_point last_write_time) {
		return std::chrono::file_clock::from_sys(last_write_time);
	}
#else
#ifdef _WIN32
	// on windows, both sys clock and file clock use the same duration, but the
	// epochs are moved apart, by the value below (taken from xfilesystem_abi.h)
	inline static constexpr long long __std_fs_file_time_epoch_adjustment =
	    0x19DB1DED53E8000LL;

	std::chrono::system_clock::time_point to_system_clock(
	    fs::file_time_type last_write_time) {
		using tp = std::chrono::system_clock::time_point;
		auto const ticks = last_write_time.time_since_epoch().count() -
		                   __std_fs_file_time_epoch_adjustment;
		auto const since_epoch = tp::duration{ticks};
		return tp{since_epoch};
	}

	fs::file_time_type to_file_clock(
	    std::chrono::system_clock::time_point last_write_time) {
		auto const ticks = last_write_time.time_since_epoch().count() +
		                   __std_fs_file_time_epoch_adjustment;
		auto const since_epoch = fs::file_time_type::duration{ticks};
		return fs::file_time_type{since_epoch};
	}
#else
	std::chrono::system_clock::time_point to_system_clock(
	    fs::file_time_type last_write_time) {}

	fs::file_time_type to_file_clock(
	    std::chrono::system_clock::time_point last_write_time) {}
#endif
#endif

	stream::stream() = default;
	stream::~stream() = default;

	std::size_t stream::read(std::span<std::byte>) { return 0; }
}  // namespace arch::base::io