#pragma once
#include <string>
#include <sndfile.h>

class FileSink {
public:
    FileSink() = default;
    ~FileSink() { close(); }

    // 리소스를 안전하게 관리하기 위해 복사는 금지하고, 이동만 허용
    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    FileSink(FileSink&& other) noexcept;
    FileSink& operator=(FileSink&& other) noexcept;

    bool open(const std::string& path, int sampleRate, int channels);
    void close();
    void writeFrames(const float* interleaved, size_t frames);
private:
    SNDFILE* f_ = nullptr;
    SF_INFO  info_{};
};
