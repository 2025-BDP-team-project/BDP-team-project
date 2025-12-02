#include "file_source.hpp"
#include <cstring>
#include <utility>

FileSource::FileSource(FileSource&& other) noexcept {
    *this = std::move(other);
}

FileSource& FileSource::operator=(FileSource&& other) noexcept {
    if (this != &other) {
        close();
        f_    = other.f_;
        info_ = other.info_;
        other.f_ = nullptr;
        std::memset(&other.info_, 0, sizeof(other.info_));
    }
    return *this;
}

bool FileSource::open(const std::string& path) {
    close();
    std::memset(&info_, 0, sizeof(info_));
    f_ = sf_open(path.c_str(), SFM_READ, &info_);
    return f_ != nullptr;
}

void FileSource::close() {
    if (f_) {
        sf_close(f_);
        f_ = nullptr;
    }
}

size_t FileSource::readFrames(float* interleaved, size_t frames) {
    if (!f_ || !interleaved || frames == 0) {
        return 0;
    }
    sf_count_t r = sf_readf_float(f_, interleaved, static_cast<sf_count_t>(frames));
    return static_cast<size_t>(r);
}
