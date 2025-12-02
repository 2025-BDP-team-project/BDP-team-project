#pragma once
#include <string>
#include <sndfile.h>

class FileSource {
public:
    FileSource() = default;
    ~FileSource() { close(); }

    // 리소스를 안전하게 관리하기 위해 복사는 금지하고, 이동만 허용
    FileSource(const FileSource&) = delete;
    FileSource& operator=(const FileSource&) = delete;

    FileSource(FileSource&& other) noexcept;
    FileSource& operator=(FileSource&& other) noexcept;

    bool open(const std::string& path);
    void close();
    int  sampleRate() const { return info_.samplerate; }
    int  channels()   const { return info_.channels; }
    long long frames() const { return info_.frames; }
    size_t readFrames(float* interleaved, size_t frames);
private:
    SNDFILE* f_ = nullptr;
    SF_INFO  info_{};  // libsndfile 메타데이터
};
