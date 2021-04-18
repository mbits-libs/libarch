// Copyright (c) 2020 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <zip.h>

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic pop
#endif

#include <arch/zip/archive.hh>
#include <arch/zip/entry.hh>
#include "check_signature.hh"

namespace arch::zip {
	namespace {
		size_t safe_sum(size_t pos, int64_t offset) {
			if (offset < 0) {
				auto const diff = [=] {
					static constexpr auto max_ =
					    (std::numeric_limits<long long>::max)();
					static constexpr auto min_ =
					    (std::numeric_limits<long long>::min)();

					static_assert(!std::is_same_v<size_t, long long>);
					static_assert(!std::is_signed_v<size_t>);
					static_assert(static_cast<size_t>(max_) <
					              (std::numeric_limits<size_t>::max)());

					if (offset == min_) {
						return static_cast<size_t>(max_) + 1;
					}
					return static_cast<size_t>(-offset);
				}();

				if (diff > pos) return 0;
				return pos - diff;
			}

			return pos + static_cast<size_t>(offset);
		}

		struct cb_data {
			io::seekable* file{};
			io::status status{};
			zip_error_t error{};
		};
		static zip_int64_t source_callback(void* user,
		                                   void* data,
		                                   zip_uint64_t size,
		                                   zip_source_cmd_t cmd) {
			auto ctx = reinterpret_cast<cb_data*>(user);

			switch (cmd) {
				case ZIP_SOURCE_CLOSE:
				case ZIP_SOURCE_COMMIT_WRITE:
				case ZIP_SOURCE_REMOVE:
				case ZIP_SOURCE_ROLLBACK_WRITE:
				case ZIP_SOURCE_SEEK_WRITE:
				case ZIP_SOURCE_TELL_WRITE:
				case ZIP_SOURCE_WRITE:
					return 0;

				case ZIP_SOURCE_BEGIN_WRITE:
				case ZIP_SOURCE_BEGIN_WRITE_CLONING:
					return -1;

				case ZIP_SOURCE_FREE:
					delete ctx;
					return 0;

				case ZIP_SOURCE_ERROR:
					return zip_error_to_data(&ctx->error, data, size);

				case ZIP_SOURCE_OPEN:
					ctx->file->seek(0);
					return 0;

				case ZIP_SOURCE_READ:
					return static_cast<zip_int64_t>(ctx->file->read(
					    {reinterpret_cast<std::byte*>(data), size}));

				case ZIP_SOURCE_SEEK: {
#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

					auto* seek = ZIP_SOURCE_GET_ARGS(zip_source_args_seek_t,
					                                 data, size, nullptr);
					if (!seek) return -1;

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic pop
#endif

					auto const newpos = [whence = seek->whence,
					                     off = seek->offset, file = ctx->file] {
						switch (whence) {
							case SEEK_SET:
								return safe_sum(0, off);
							case SEEK_CUR:
								return safe_sum(file->tell(), off);
							case SEEK_END:
								return safe_sum(file->seek_end(), off);
							default:
								break;
						}
						return file->tell();
					}();

					auto const actual = ctx->file->seek(newpos);
					return actual == newpos ? 0 : -1;
				}

				case ZIP_SOURCE_STAT: {
#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

					auto* st =
					    ZIP_SOURCE_GET_ARGS(zip_stat_t, data, size, nullptr);
					if (!st) return 0;

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic pop
#endif

					zip_stat_init(st);
					auto const last_write_time =
					    base::io::to_system_clock(ctx->status.last_write_time);
					st->mtime =
					    std::chrono::system_clock::to_time_t(last_write_time);
					st->size = ctx->status.size;
					st->valid = ZIP_STAT_MTIME | ZIP_STAT_SIZE;
					return sizeof(*st);
				}

				case ZIP_SOURCE_SUPPORTS:
#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

					return ZIP_SOURCE_SUPPORTS_SEEKABLE;

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic pop
#endif

				case ZIP_SOURCE_TELL:
					return static_cast<zip_int64_t>(ctx->file->tell());

				default:
					zip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
					return -1;
			}
		}

		zip_t*& get_handle(void*& handle) {
			return reinterpret_cast<zip_t*&>(handle);
		}
		zip_t* get_handle(void const* handle) {
			return const_cast<zip_t*>(reinterpret_cast<zip_t const*>(handle));
		}
	}  // namespace

#define HANDLE get_handle(handle_)

	bool archive::is_valid(io::seekable* file) {
		if (check_signature<'P', 'K', 0x03, 0x04>(file)) return true;

		file->seek(0);

		void* handle;
		zip_source_t* source;

		auto result = open(file, &handle, &source);
		if (handle) zip_close(get_handle(handle));
		if (source) zip_source_close(source);
		return result;
	}

	bool archive::open(io::seekable::ptr file) {
		close();

		file_ = std::move(file);
		if (!file_) return false;

		return open(file_.get(), &handle_, &source_);
	}

	bool archive::open(io::seekable* file,
	                   void** handle,
	                   zip_source_t** source) {
		*handle = nullptr;
		*source = nullptr;

		auto cb = std::make_unique<cb_data>();
		cb->file = file;
		cb->error = {};
		cb->status = cb->file->linked_status();

		*source =
		    zip_source_function_create(source_callback, cb.get(), nullptr);
		if (!*source) return false;
		cb.release();

		get_handle(*handle) =
		    zip_open_from_source(*source, ZIP_CHECKCONS | ZIP_RDONLY, nullptr);
		return !!*handle;
	}

	void archive::close() {
		if (HANDLE) zip_close(HANDLE);
		if (source_) zip_source_close(source_);
		HANDLE = nullptr;
		source_ = nullptr;
		file_ = nullptr;
	}

	size_t archive::count() const {
		auto const ret = zip_get_num_entries(HANDLE, ZIP_FL_UNCHANGED);
		if (ret < 0) return 0;
		return static_cast<size_t>(ret);
	}

	base::entry::ptr archive::entry(size_t index) const {
		static constexpr auto expected =
		    ZIP_STAT_SIZE | ZIP_STAT_MTIME | ZIP_STAT_NAME;
		zip_stat_t st{};
		if (zip_stat_index(HANDLE, index, ZIP_FL_UNCHANGED, &st) ||
		    (st.valid & expected) != expected)
			return {};
		return std::make_unique<zip::entry>(HANDLE, index, st);
	}
}  // namespace arch::zip
