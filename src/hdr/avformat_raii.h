#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

#include <memory>
#include <string>
#include <stdexcept>

namespace hdr {

/**
 * RAII wrapper for FFmpeg's AVFormatContext
 * Ensures proper resource cleanup and exception safety
 */
class AVFormatContextRAII {
public:
    /**
     * Custom deleter for AVFormatContext
     */
    struct AVFormatDeleter {
        void operator()(AVFormatContext* ctx) noexcept {
            if (ctx) {
                avformat_close_input(&ctx);
            }
        }
    };
    
    using ContextPtr = std::unique_ptr<AVFormatContext, AVFormatDeleter>;

private:
    ContextPtr context_;
    std::string filename_;

public:
    /**
     * Default constructor - creates empty context
     */
    AVFormatContextRAII() = default;
    
    /**
     * Constructor that opens a media file
     * @param filename Path to media file
     * @throws std::runtime_error if file cannot be opened
     */
    explicit AVFormatContextRAII(const std::string& filename) 
        : filename_(filename) {
        open(filename);
    }
    
    // Move semantics
    AVFormatContextRAII(AVFormatContextRAII&&) = default;
    AVFormatContextRAII& operator=(AVFormatContextRAII&&) = default;
    
    // Disable copy semantics
    AVFormatContextRAII(const AVFormatContextRAII&) = delete;
    AVFormatContextRAII& operator=(const AVFormatContextRAII&) = delete;
    
    /**
     * Open a media file
     * @param filename Path to media file
     * @throws std::runtime_error if file cannot be opened or analyzed
     */
    void open(const std::string& filename) {
        // Pass nullptr — avformat_open_input allocates its own context.
        // On failure it leaves the pointer null; no manual cleanup needed.
        AVFormatContext* raw_ctx = nullptr;
        if (avformat_open_input(&raw_ctx, filename.c_str(), nullptr, nullptr) < 0)
            throw std::runtime_error("Failed to open file: " + filename);

        if (avformat_find_stream_info(raw_ctx, nullptr) < 0) {
            avformat_close_input(&raw_ctx);
            throw std::runtime_error("Failed to analyze streams in: " + filename);
        }

        // Update state only after both calls succeed — preserves old state on throw.
        context_.reset(raw_ctx);
        filename_ = filename;
    }
    
    /**
     * Close current context and release resources
     */
    void reset() noexcept {
        context_.reset();
        filename_.clear();
    }
    
    explicit operator bool() const noexcept { return is_open(); }
    
    /**
     * Get raw pointer to AVFormatContext
     */
    AVFormatContext* get() const noexcept {
        return context_.get();
    }
    
    /**
     * Arrow operator for direct access to AVFormatContext members
     */
    AVFormatContext* operator->() const noexcept {
        return context_.get();
    }
    
    /**
     * Get the filename of currently opened file
     */
    const std::string& filename() const noexcept {
        return filename_;
    }
    
    // is_open() is the canonical check; operator bool() delegates to it.
    bool is_open() const noexcept { return static_cast<bool>(context_); }
};

} // namespace hdr