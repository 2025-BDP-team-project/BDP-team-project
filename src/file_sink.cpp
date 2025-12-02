#include "file_sink.hpp"
#include <cstring>
#include <utility>

FileSink::FileSink(FileSink&& other) noexcept {
    *this = std::move(other);
}

FileSink& FileSink::operator=(FileSink&& other) noexcept {
    if (this != &other) {
        close();
        f_    = other.f_;
        info_ = other.info_;
        other.f_ = nullptr;
        std::memset(&other.info_, 0, sizeof(other.info_));
    }
    return *this;
}

bool FileSink::open(const std::string& path, int sampleRate, int channels) {
    close();
    std::memset(&info_, 0, sizeof(info_));
    info_.samplerate = sampleRate;
    info_.channels   = channels;
    // 분석용 하네스이므로 32-bit float WAV로 저장하여 양자화 손실을 줄인다.
    info_.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    f_ = sf_open(path.c_str(), SFM_WRITE, &info_);
    return f_ != nullptr;
}

void FileSink::close() {
    if (f_) {
        sf_close(f_);
        f_ = nullptr;
    }
}

void FileSink::writeFrames(const float* interleaved, size_t frames) {
    if (!f_ || !interleaved || frames == 0) {
        return;
    }
    sf_writef_float(f_, interleaved, static_cast<sf_count_t>(frames));
}
