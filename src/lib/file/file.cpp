/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <platform/detect.h>
#include <file/file.h>
#include <unicode/utf/convert.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#ifdef UTILS_PLATFORM_WASI
#define _WASI_EMULATED_MMAN
#endif
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#endif

namespace utils::file {

#ifdef UTILS_PLATFORM_WINDOWS
    // from golang syscall.Open. see also https://cs.opensource.google/go/go/+/refs/tags/go1.21.3:src/syscall/syscall_windows.go;l=342
    file_result<File> File::open(const wrap::path_char* filename, Flag flag, Mode mode) {
        std::uint32_t access = 0;
        switch (flag.io()) {
            case IOMode::read:
                access = GENERIC_READ;
                break;
            case IOMode::write:
                access = GENERIC_WRITE;
                break;
            case IOMode::read_write:
                access = GENERIC_READ | GENERIC_WRITE;
                break;
            default:
                break;
        }
        if (flag.create()) {
            access |= GENERIC_WRITE;
        }
        if (flag.append()) {
            access &= ~GENERIC_WRITE;
            access |= FILE_APPEND_DATA;
        }
        std::uint32_t share = 0;
        if (flag.share_read()) {
            share |= FILE_SHARE_WRITE;
        }
        if (flag.share_write()) {
            share |= FILE_SHARE_READ;
        }
        if (flag.share_delete()) {
            share |= FILE_SHARE_DELETE;
        }
        std::uint32_t create = 0;
        if (flag.create() && flag.exclusive()) {
            create = CREATE_NEW;
        }
        else if (flag.create() && flag.truncate()) {
            create = CREATE_ALWAYS;
        }
        else if (flag.create()) {
            create = OPEN_ALWAYS;
        }
        else if (flag.truncate()) {
            create = TRUNCATE_EXISTING;
        }
        else {
            create = OPEN_EXISTING;
        }
        std::uint32_t attr = FILE_ATTRIBUTE_NORMAL;
        if (!mode.perm().owner_write()) {
            attr = FILE_ATTRIBUTE_READONLY;
        }
        SECURITY_ATTRIBUTES sec{}, *psec = nullptr;
        if (!flag.close_on_exec()) {
            sec.nLength = sizeof(sec);
            sec.bInheritHandle = true;
            psec = &sec;
        }
        if (flag.non_block()) {
            attr |= FILE_FLAG_OVERLAPPED;
        }
        if (flag.direct()) {
            attr |= FILE_FLAG_NO_BUFFERING;
        }
        if (flag.sync()) {
            attr |= FILE_FLAG_WRITE_THROUGH;
        }

        if (!mode.perm().owner_write()) {
            attr = FILE_ATTRIBUTE_READONLY;
            if (mode == CREATE_ALWAYS) {
                auto h = CreateFileW(filename, access, share, psec, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h == INVALID_HANDLE_VALUE) {
                    return helper::either::unexpected(FileError{.method = "CreateFileW", .err_code = GetLastError()});
                }
                return File{reinterpret_cast<std::uintptr_t>(h)};
            }
        }
        if (create == OPEN_EXISTING && access == GENERIC_READ) {
            attr |= FILE_FLAG_BACKUP_SEMANTICS;
        }
        auto h = CreateFileW(filename, access, share, psec, create, attr, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            return helper::either::unexpected(FileError{.method = "CreateFileW", .err_code = GetLastError()});
        }
        return File{reinterpret_cast<std::uintptr_t>(h)};
    }

    void STDCALL format_os_error(helper::IPushBacker<> pb, std::int64_t code) {
        LPWSTR buf = nullptr;
        auto len = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
        if (len == 0) {
            strutil::appends(pb, "FormatMessageW failed with code ");
            number::to_string(pb, GetLastError());
            return;
        }
        auto d = utils::helper::defer([&] { LocalFree(buf); });
        utf::convert<2, 1>(buf, pb);
    }

    std::int64_t STDCALL map_os_error_code(ErrorCode code) {
        switch (code) {
            case ErrorCode::already_open:
                return ERROR_ALREADY_EXISTS;
            default:
                return -1;
        }
    }

    Time filetime_to_Time(const FILETIME& ft) {
        std::uint64_t t = (std::uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        t -= 116444736000000000;
        t *= 100;
        return Time{.sec = t / 1000000000, .nsec = std::uint32_t(t % 1000000000)};
    }

    file_result<Stat> File::stat() const {
        Stat stat;
        auto h = reinterpret_cast<HANDLE>(handle);
        auto type = GetFileType(h);
        BY_HANDLE_FILE_INFORMATION info{};
        FILE_ATTRIBUTE_TAG_INFO tag_info{};
        FILE_IO_PRIORITY_HINT_INFO priority_hint_info{};
        switch (type) {
            case FILE_TYPE_PIPE:
                stat.mode.set_pipe(true);
                break;
            case FILE_TYPE_CHAR: {
                stat.mode.set_char_device(true);
                stat.mode.set_device(true);
                DWORD mode = 0;
                if (GetConsoleMode(h, &mode) != 0) {
                    stat.mode.set_terminal(true);
                }
                break;
            }
            default: {
                if (!GetFileInformationByHandle(h, &info)) {
                    return helper::either::unexpected(FileError{.method = "GetFileInformationByHandle", .err_code = GetLastError()});
                }
                if (!GetFileInformationByHandleEx(h, FileAttributeTagInfo, &tag_info, sizeof(tag_info))) {
                    if (GetLastError() != ERROR_INVALID_PARAMETER) {
                        return helper::either::unexpected(FileError{.method = "GetFileInformationByHandleEx", .err_code = GetLastError()});
                    }
                    tag_info.ReparseTag = 0;
                }
                if (!GetFileInformationByHandleEx(h, FileIoPriorityHintInfo, &priority_hint_info, sizeof(priority_hint_info))) {
                    if (GetLastError() != ERROR_INVALID_PARAMETER) {
                        return helper::either::unexpected(FileError{.method = "GetFileInformationByHandleEx", .err_code = GetLastError()});
                    }
                    priority_hint_info.PriorityHint = IoPriorityHintNormal;
                }
            }
        }
        Perm perm;
        if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
            perm.set_read(true);
        }
        else {
            perm.set_read(true);
            perm.set_write(true);
        }
        if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
            (tag_info.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT ||
             tag_info.ReparseTag == IO_REPARSE_TAG_SYMLINK)) {
            stat.mode.set_symlink(true);
            stat.mode.set_perm(perm);
            return stat;
        }
        if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            stat.mode.set_directory(true);
            perm.set_execute(true);
        }
        if (priority_hint_info.PriorityHint == IoPriorityHintVeryLow) {
            stat.flag.set_non_block(true);
        }
        stat.mode.set_perm(perm);
        stat.size = (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        stat.create_time = filetime_to_Time(info.ftCreationTime);
        stat.access_time = filetime_to_Time(info.ftLastAccessTime);
        stat.mod_time = filetime_to_Time(info.ftLastWriteTime);
        return stat;
    }

    bool File::is_directory() const {
        auto h = reinterpret_cast<HANDLE>(handle);
        auto type = GetFileType(h);
        if (type == FILE_TYPE_PIPE) {
            return false;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(h, &info)) {
            return false;
        }
        return (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool File::is_tty() const {
        auto h = reinterpret_cast<HANDLE>(handle);
        DWORD mode = 0;
        return GetConsoleMode(h, &mode) != 0;
    }

    size_t File::size() const {
        auto h = reinterpret_cast<HANDLE>(handle);
        LARGE_INTEGER li{};
        if (!GetFileSizeEx(h, &li)) {
            return 0;
        }
        return li.QuadPart;
    }

    file_result<void> File::close() {
        if (!CloseHandle(reinterpret_cast<HANDLE>(handle))) {
            return helper::either::unexpected(FileError{.method = "CloseHandle", .err_code = GetLastError()});
        }
        handle = ~0;
        return {};
    }

    file_result<MMap> File::mmap(Mode m) const {
        if (handle == ~0) {
            return helper::either::unexpected(FileError{.method = "File::mmap", .err_code = ERROR_INVALID_PARAMETER});
        }
        auto s = stat();
        if (!s) {
            return helper::either::unexpected(s.error());
        }
        if ((s->mode.perm() & m.perm()) != m.perm()) {
            return helper::either::unexpected(FileError{.method = "File::mmap", .err_code = ERROR_ACCESS_DENIED});
        }
        int page_flag = 0;
        int file_map_flag = 0;
        if (m.perm().owner_write()) {
            page_flag |= PAGE_READWRITE;
            file_map_flag |= FILE_MAP_WRITE;
        }
        else {
            page_flag |= PAGE_READONLY;
            file_map_flag |= FILE_MAP_READ;
        }
        auto h = reinterpret_cast<HANDLE>(handle);
        auto m_handle = CreateFileMappingW(h, nullptr, page_flag, 0, 0, nullptr);
        if (m_handle == nullptr) {
            return helper::either::unexpected(FileError{.method = "CreateFileMappingW", .err_code = GetLastError()});
        }
        auto d = utils::helper::defer([&] { CloseHandle(m_handle); });
        auto ptr = MapViewOfFile(m_handle, file_map_flag, 0, 0, 0);
        if (ptr == nullptr) {
            return helper::either::unexpected(FileError{.method = "MapViewOfFile", .err_code = GetLastError()});
        }
        d.cancel();
        return MMap(reinterpret_cast<byte*>(ptr), std::uintptr_t(m_handle), s->size, m);
    }

    file_result<void> MMap::unmap() {
        if (!ptr) {
            return helper::either::unexpected(FileError{.method = "MMap::unmap", .err_code = ERROR_INVALID_PARAMETER});
        }
        if (!UnmapViewOfFile(ptr)) {
            return helper::either::unexpected(FileError{.method = "UnmapViewOfFile", .err_code = GetLastError()});
        }
        if (!CloseHandle(reinterpret_cast<HANDLE>(os_spec))) {
            return helper::either::unexpected(FileError{.method = "CloseHandle", .err_code = GetLastError()});
        }
        ptr = nullptr;
        os_spec = 0;
        file_len = 0;
        return {};
    }

    static_assert(sizeof(OVERLAPPED) == sizeof(NonBlockContext), "OVERLAPPED and NonBlockContext must be same size");

    file_result<view::rvec> File::write_file(view::rvec r, NonBlockContext* n) const {
        auto h = reinterpret_cast<HANDLE>(handle);
        DWORD written = 0, *pwritten = nullptr;
        if (!n) {
            pwritten = &written;
        }
        if (!WriteFile(h, r.data(), r.size(), pwritten, reinterpret_cast<OVERLAPPED*>(n))) {
            return helper::either::unexpected(FileError{.method = "WriteFile", .err_code = GetLastError()});
        }
        return view::rvec(r.data() + written, r.size() - written);
    }

    file_result<view::basic_rvec<wrap::path_char>> File::write_console(view::basic_rvec<wrap::path_char> w) const {
        auto h = reinterpret_cast<HANDLE>(handle);
        DWORD written = 0;
        if (!WriteConsoleW(h, w.data(), w.size(), &written, nullptr)) {
            return helper::either::unexpected(FileError{.method = "WriteConsoleW", .err_code = GetLastError()});
        }
        return view::basic_rvec<wrap::path_char>(w.data() + written, w.size() - written);
    }

    file_result<view::wvec> File::read_file(view::wvec w, NonBlockContext* n) const {
        auto h = reinterpret_cast<HANDLE>(handle);
        DWORD read = 0, *pread = nullptr;
        if (!n) {
            pread = &read;
        }
        if (!ReadFile(h, w.data(), w.size(), pread, reinterpret_cast<OVERLAPPED*>(n))) {
            return helper::either::unexpected(FileError{.method = "ReadFile", .err_code = GetLastError()});
        }
        return view::wvec(w.data(), read);
    }

    file_result<view::basic_wvec<wrap::path_char>> File::read_console(view::basic_wvec<wrap::path_char> w) const {
        auto h = reinterpret_cast<HANDLE>(handle);
        DWORD read = 0;
        if (!ReadConsoleW(h, w.data(), w.size(), &read, nullptr)) {
            return helper::either::unexpected(FileError{.method = "ReadConsoleW", .err_code = GetLastError()});
        }
        return view::basic_wvec<wrap::path_char>(w.data(), read);
    }

    file_result<void> File::seek(std::int64_t offset, SeekPoint point) const {
        auto h = reinterpret_cast<HANDLE>(handle);
        LARGE_INTEGER li{};
        li.QuadPart = offset;
        DWORD method = 0;
        switch (point) {
            case SeekPoint::begin:
                method = FILE_BEGIN;
                break;
            case SeekPoint::current:
                method = FILE_CURRENT;
                break;
            case SeekPoint::end:
                method = FILE_END;
                break;
            default:
                return helper::either::unexpected(FileError{.method = "File::seek", .err_code = ERROR_INVALID_PARAMETER});
        }
        if (!SetFilePointerEx(h, li, nullptr, method)) {
            return helper::either::unexpected(FileError{.method = "SetFilePointerEx", .err_code = GetLastError()});
        }
        return {};
    }

    size_t File::pos() const {
        auto h = reinterpret_cast<HANDLE>(handle);
        LARGE_INTEGER li{}, zero{};
        li.QuadPart = 0;
        if (!SetFilePointerEx(h, li, &zero, FILE_CURRENT)) {
            return 0;
        }
        return zero.QuadPart;
    }

    file_result<void> File::ioctl(std::uint64_t code, void* in_arg, std::size_t in_len, void* out_arg, std::size_t out_len) const {
        auto h = reinterpret_cast<HANDLE>(handle);
        DWORD written = 0;
        if (!DeviceIoControl(h, code, in_arg, in_len, out_arg, out_len, &written, nullptr)) {
            return helper::either::unexpected(FileError{.method = "DeviceIoControl", .err_code = GetLastError()});
        }
        return {};
    }

    bool NonBlockContext::complete() const {
        return reinterpret_cast<const OVERLAPPED*>(this)->Internal != STATUS_PENDING;
    }

    size_t NonBlockContext::transferred() const {
        return reinterpret_cast<const OVERLAPPED*>(this)->InternalHigh;
    }

    file_result<bool> File::can_non_block_read() const {
        auto s = stat();
        if (!s) {
            return helper::either::unexpected(s.error());
        }
        if (s->mode.pipe()) {
            DWORD avail = 0;
            if (!PeekNamedPipe(reinterpret_cast<HANDLE>(handle), nullptr, 0, nullptr, &avail, nullptr)) {
                return helper::either::unexpected(FileError{.method = "PeekNamedPipe", .err_code = GetLastError()});
            }
            return avail != 0;
        }
        if (s->mode.regular()) {
            return s->flag.non_block();
        }
        if (s->mode.terminal()) {
            DWORD avail = 0;
            if (!GetNumberOfConsoleInputEvents(reinterpret_cast<HANDLE>(handle), &avail)) {
                return helper::either::unexpected(FileError{.method = "GetNumberOfConsoleInputEvents", .err_code = GetLastError()});
            }
            return avail != 0;
        }
        return false;
    }

    file_result<bool> File::can_non_block_write() const {
        auto s = stat();
        if (!s) {
            return helper::either::unexpected(s.error());
        }
        if (s->mode.regular()) {
            return s->flag.non_block();
        }
        return true;
    }

    const File& File::stdin_file() {
        static File f{reinterpret_cast<std::uintptr_t>(GetStdHandle(STD_INPUT_HANDLE))};
        return f;
    }

    const File& File::stdout_file() {
        static File f{reinterpret_cast<std::uintptr_t>(GetStdHandle(STD_OUTPUT_HANDLE))};
        return f;
    }

    const File& File::stderr_file() {
        static File f{reinterpret_cast<std::uintptr_t>(GetStdHandle(STD_ERROR_HANDLE))};
        return f;
    }

#else

    struct StrHolder {
       private:
        const char *buf;
        bool allocated;

       public:
        StrHolder(const char *buf, bool allocated)
            : buf(buf), allocated(allocated) {}

        ~StrHolder() {
            if (allocated) {
                free(const_cast<char *>(buf));
            }
        }

        const char *c_str() const {
            return buf;
        }

        StrHolder(StrHolder &&other)
            : buf(std::exchange(other.buf, nullptr)), allocated(std::exchange(other.allocated, false)) {}
    };

    template <class R, class... Arg>
    StrHolder strerror_wrap(R (*strerr)(Arg...), int n) noexcept {
        size_t size = 1024;
        auto buf = static_cast<char *>(malloc(size));
        if (!buf) {
            return StrHolder("<malloc for strerror failed>", false);
        }
        if constexpr (std::is_convertible_v<R, int>) {
            // XSI compatible
            for (;;) {
                auto r = strerr(n, buf, size);
                if (r == -1 && errno == ERANGE) {
                    size *= 2;
                    auto new_buf = static_cast<char *>(realloc(buf, size));
                    if (!new_buf) {
                        free(buf);
                        return StrHolder("<realloc for strerror failed>", false);
                    }
                    buf = new_buf;
                    continue;
                }
                else if (r == -1) {
                    free(buf);
                    if (errno == EINVAL) {
                        return StrHolder("invalid error number", false);
                    }
                    return StrHolder("<strerror failed>", false);
                }
                else {
                    return StrHolder(buf, true);
                }
            }
        }
        else {
            // GNU compatible
            for (;;) {
                errno = 0;
                auto r = strerr(n, buf, size);
                if (r != buf) {
                    free(buf);
                    return StrHolder(r, false);
                }
                else if (errno == 0) {
                    return StrHolder(buf, true);
                }
                else if (errno == ERANGE) {
                    size *= 2;
                    auto new_buf = static_cast<char *>(realloc(buf, size));
                    if (!new_buf) {
                        // at GNU, string error is omitted but appear with null terminated
                        // so use omited error message
                        return StrHolder(buf, true);
                    }
                    buf = new_buf;
                    continue;
                }
                else {
                    free(buf);
                    if (errno == EINVAL) {
                        return StrHolder("invalid error number", false);
                    }
                    return StrHolder("<strerror failed>", false);
                }
            }
        }
    }

    StrHolder all_strerror(int n) {
        return strerror_wrap(strerror_r, n);
    }

    void format_os_error(helper::IPushBacker<> pb, std::int64_t code) {
        auto str = all_strerror(code);
        strutil::appends(pb, str.c_str());
    }

    std::int64_t map_os_error_code(ErrorCode code) {
        switch (code) {
            case ErrorCode::already_open:
                return EEXIST;
            default:
                return -1;
        }
    }

    int flag_to_open_flag(Flag flag, Mode mode) {
        int flags = 0;
        if (flag.append()) {
            flags |= O_APPEND;
        }
        if (flag.create()) {
            flags |= O_CREAT;
        }
        if (flag.exclusive()) {
            flags |= O_EXCL;
        }
        if (flag.truncate()) {
            flags |= O_TRUNC;
        }
        if (flag.sync()) {
            flags |= O_SYNC;
        }
        if (flag.no_ctty()) {
            flags |= O_NOCTTY;
        }
        if (flag.no_follow()) {
            flags |= O_NOFOLLOW;
        }

        if (flag.non_block()) {
            flags |= O_NONBLOCK;
        }
        if (flag.close_on_exec()) {
            flags |= O_CLOEXEC;
        }
        if (flag.dsync()) {
            flags |= O_DSYNC;
        }
        switch (flag.io()) {
            case IOMode::read:
                flags |= O_RDONLY;
                break;
            case IOMode::write:
                flags |= O_WRONLY;
                break;
            case IOMode::read_write:
                flags |= O_RDWR;
                break;
            default:
                break;
        }

#ifdef UTILS_PLATFORM_LINUX
        if (flag.direct()) {
            flags |= O_DIRECT;
        }
        if (flag.no_access_time()) {
            flags |= O_NOATIME;
        }
        if (mode.temporary()) {
            flags |= O_TMPFILE;
        }
        flags |= O_LARGEFILE;
#endif
        return flags;
    }

    Flag open_flag_to_flag(int flags, Mode &mode) {
        Flag flag;
        if (flags & O_APPEND) {
            flag.set_append(true);
        }
        if (flags & O_CREAT) {
            flag.set_create(true);
        }
        if (flags & O_EXCL) {
            flag.set_exclusive(true);
        }
        if (flags & O_TRUNC) {
            flag.set_truncate(true);
        }
        if (flags & O_SYNC) {
            flag.set_sync(true);
        }
        if (flags & O_NOCTTY) {
            flag.set_no_ctty(true);
        }
        if (flags & O_NOFOLLOW) {
            flag.set_no_follow(true);
        }
        if (flags & O_NONBLOCK) {
            flag.set_non_block(true);
        }
        if (flags & O_CLOEXEC) {
            flag.set_close_on_exec(true);
        }
        if (flags & O_DSYNC) {
            flag.set_dsync(true);
        }
        switch (flags & O_ACCMODE) {
            case O_RDONLY:
                flag.set_io(IOMode::read);
                break;
            case O_WRONLY:
                flag.set_io(IOMode::write);
                break;
            case O_RDWR:
                flag.set_io(IOMode::read_write);
                break;
            default:
                break;
        }
#ifdef UTILS_PLATFORM_LINUX
        if (flags & O_DIRECT) {
            flag.set_direct(true);
        }
        if (flags & O_NOATIME) {
            flag.set_no_access_time(true);
        }
        if (flags & O_TMPFILE) {
            mode.set_temporary(true);
        }
#endif
        return flag;
    }

    file_result<File> File::open(const wrap::path_char *filename, Flag flag, Mode mode) {
        auto perm = Mode(mode.perm());
        perm.set_gid(mode.gid());
        perm.set_uid(mode.uid());
        perm.set_sticky(mode.sticky());
        auto flags = flag_to_open_flag(flag, mode);
        auto h = openat(AT_FDCWD, filename, flags, std::uint32_t(perm));
        if (h < 0) {
            return helper::either::unexpected(FileError{.method = "openat", .err_code = errno});
        }
        return File(h);
    }

    file_result<Stat> File::stat() const {
        Stat stat;
        struct stat64 st {};
        if (fstat64(handle, &st) < 0) {
            return helper::either::unexpected(FileError{.method = "fstat64", .err_code = errno});
        }
        auto flag = fcntl(handle, F_GETFL, 0);
        if (flag < 0) {
            return helper::either::unexpected(FileError{.method = "fcntl", .err_code = errno});
        }
        stat.flag = open_flag_to_flag(flag, stat.mode);
        if (S_ISBLK(st.st_mode)) {
            stat.mode.set_device(true);
        }
        if (S_ISCHR(st.st_mode)) {
            stat.mode.set_char_device(true);
            if (isatty(handle)) {
                stat.mode.set_terminal(true);
            }
        }
        if (S_ISDIR(st.st_mode)) {
            stat.mode.set_directory(true);
        }
        if (S_ISFIFO(st.st_mode)) {
            stat.mode.set_pipe(true);
        }
        if (S_ISLNK(st.st_mode)) {
            stat.mode.set_symlink(true);
        }
        if (S_ISSOCK(st.st_mode)) {
            stat.mode.set_socket(true);
        }
        if (S_ISREG(st.st_mode)) {
            // nothing
        }
        stat.mode.set_perm(Perm(st.st_mode & 0777));
        stat.mode.set_uid(st.st_mode & S_ISUID);
        stat.mode.set_gid(st.st_mode & S_ISGID);
        stat.mode.set_sticky(st.st_mode & S_ISVTX);
        stat.size = st.st_size;
        stat.create_time.sec = st.st_ctim.tv_sec;
        stat.create_time.nsec = st.st_ctim.tv_nsec;
        stat.access_time.sec = st.st_atim.tv_sec;
        stat.access_time.nsec = st.st_atim.tv_nsec;
        stat.mod_time.sec = st.st_mtim.tv_sec;
        stat.mod_time.nsec = st.st_mtim.tv_nsec;
        return stat;
    }

    bool File::is_directory() const {
        struct stat64 st {};
        if (fstat64(handle, &st) < 0) {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }

    bool File::is_tty() const {
        return isatty(handle);
    }

    size_t File::size() const {
        struct stat64 st {};
        if (fstat64(handle, &st) < 0) {
            return 0;
        }
        return st.st_size;
    }

    file_result<void> File::close() {
        if (::close(handle) < 0) {
            return helper::either::unexpected(FileError{.method = "close", .err_code = errno});
        }
        handle = ~0;
        return {};
    }

    file_result<MMap> File::mmap(Mode m) const {
        if (handle == ~0) {
            return helper::either::unexpected(FileError{.method = "File::mmap", .err_code = EBADF});
        }
        auto s = stat();
        if (!s) {
            return helper::either::unexpected(s.error());
        }
        if ((s->mode.perm() & m.perm()) != m.perm()) {
            return helper::either::unexpected(FileError{.method = "File::mmap", .err_code = EACCES});
        }
        int prot_flag = 0;
        if (m.perm().owner_write()) {
            prot_flag |= PROT_WRITE;
        }
        if (m.perm().owner_read()) {
            prot_flag |= PROT_READ;
        }
        if (m.perm().owner_execute()) {
            prot_flag |= PROT_EXEC;
        }
        int map_flag = MAP_SHARED;
        if (m.temporary()) {
            map_flag = MAP_PRIVATE;
        }
        auto page = getpagesize();
        auto size = (s->size / page + 1) * page;
        auto ptr = ::mmap(nullptr, size, prot_flag, map_flag, handle, 0);
        if (ptr == MAP_FAILED) {
            return helper::either::unexpected(FileError{.method = "mmap", .err_code = errno});
        }
        return MMap(reinterpret_cast<byte *>(ptr), size, s->size, m);
    }

    file_result<void> MMap::unmap() {
        if (!ptr) {
            return helper::either::unexpected(FileError{.method = "MMap::unmap", .err_code = EINVAL});
        }
        if (::munmap(ptr, os_spec) != 0) {
            return helper::either::unexpected(FileError{.method = "munmap", .err_code = errno});
        }
        ptr = nullptr;
        os_spec = 0;
        file_len = 0;
        return {};
    }

    file_result<view::rvec> File::write_file(view::rvec r, NonBlockContext *) const {
        auto written = ::write(handle, r.data(), r.size());
        if (written < 0) {
            return helper::either::unexpected(FileError{.method = "write", .err_code = errno});
        }
        return view::rvec(r.data() + written, r.size() - written);
    }

    file_result<view::basic_rvec<wrap::path_char>> File::write_console(view::basic_rvec<wrap::path_char> w) const {
        return write_file(view::rvec(w.data(), w.size())).transform([&](view::rvec r) {
            return view::basic_rvec<wrap::path_char>(r.data(), r.size());
        });
    }

    file_result<view::wvec> File::read_file(view::wvec w, NonBlockContext *) const {
        auto read = ::read(handle, w.data(), w.size());
        if (read < 0) {
            return helper::either::unexpected(FileError{.method = "read", .err_code = errno});
        }
        return view::wvec(w.data(), read);
    }

    file_result<view::basic_wvec<wrap::path_char>> File::read_console(view::basic_wvec<wrap::path_char> w) const {
        return read_file(view::wvec(w.data(), w.size())).transform([&](view::wvec r) {
            return view::basic_wvec<wrap::path_char>(r.data(), r.size());
        });
    }

    file_result<void> File::seek(std::int64_t offset, SeekPoint point) const {
        int seek_pos;
        switch (point) {
            case SeekPoint::begin:
                seek_pos = SEEK_SET;
                break;
            case SeekPoint::current:
                seek_pos = SEEK_CUR;
                break;
            case SeekPoint::end:
                seek_pos = SEEK_END;
                break;
            default:
                return helper::either::unexpected(FileError{.method = "File::seek", .err_code = EINVAL});
        }
        if (::lseek64(handle, offset, seek_pos) < 0) {
            return helper::either::unexpected(FileError{.method = "lseek64", .err_code = errno});
        }
        return {};
    }

    size_t File::pos() const {
        auto pos = ::lseek64(handle, 0, SEEK_CUR);
        if (pos < 0) {
            return 0;
        }
        return pos;
    }

    file_result<void> File::ioctl(std::uint64_t code, void *in_arg, std::size_t in_len, void *out_arg, std::size_t out_len) const {
        if (::ioctl(handle, code, in_arg) < 0) {
            return helper::either::unexpected(FileError{.method = "ioctl", .err_code = errno});
        }
        return {};
    }

    bool NonBlockContext::complete() const {
        return true;
    }

    size_t NonBlockContext::transferred() const {
        return 0;
    }

    file_result<bool> File::can_non_block_read() const {
        auto s = stat();
        if (!s) {
            return helper::either::unexpected(s.error());
        }
        if (s->mode.pipe()) {
            int avail = 0;
            if (::ioctl(handle, FIONREAD, &avail) < 0) {
                return helper::either::unexpected(FileError{.method = "ioctl", .err_code = errno});
            }
            return avail != 0;
        }
        if (s->mode.regular()) {
            return s->flag.non_block();
        }
        if (s->mode.terminal()) {
            int avail = 0;
            if (::ioctl(handle, FIONREAD, &avail) < 0) {
                return helper::either::unexpected(FileError{.method = "ioctl", .err_code = errno});
            }
            return avail != 0;
        }
        return false;
    }

    const File &File::stdin_file() {
        static File f{STDIN_FILENO};
        return f;
    }

    const File &File::stdout_file() {
        static File f{STDOUT_FILENO};
        return f;
    }

    const File &File::stderr_file() {
        static File f{STDERR_FILENO};
        return f;
    }

#endif

}  // namespace utils::file
