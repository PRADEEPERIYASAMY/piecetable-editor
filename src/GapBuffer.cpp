#include "GapBuffer.hpp"
#include <fstream>
#include <algorithm>

GapBuffer::GapBuffer() {
    buf_.resize(kDefaultGap);
    gapStart_ = 0;
    gapEnd_   = kDefaultGap;
}

void GapBuffer::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    std::string content;
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) { content += line; content += '\n'; }
    }
    buf_.resize(content.size() + kDefaultGap);
    std::copy(content.begin(), content.end(), buf_.begin());
    gapStart_ = content.size();
    gapEnd_   = buf_.size();
}

char GapBuffer::charAt(size_t index) const {
    // Logical indices before the gap map directly to physical indices.
    // Logical indices at or after the gap skip over the gap.
    if (index < gapStart_) return buf_[index];
    return buf_[gapEnd_ + (index - gapStart_)];
}

std::string GapBuffer::text() const {
    std::string result;
    result.reserve(size());
    result.append(buf_.data(), gapStart_);                        // pre-gap
    result.append(buf_.data() + gapEnd_, buf_.size() - gapEnd_); // post-gap
    return result;
}

std::string GapBuffer::substring(size_t start, size_t length) const {
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) result.push_back(charAt(start + i));
    return result;
}

void GapBuffer::moveGapTo(size_t pos) {
    if (pos == gapStart_) return;

    if (pos < gapStart_) {
        // Shift pre-gap text rightward into the gap:
        //   buf_[pos..gapStart_-1]  →  buf_[gapEnd_-count..gapEnd_-1]
        size_t count = gapStart_ - pos;
        std::copy_backward(buf_.data() + pos,
                           buf_.data() + gapStart_,
                           buf_.data() + gapEnd_);
        gapEnd_   -= count;
        gapStart_  = pos;
    } else {
        // Shift post-gap text leftward into the gap:
        //   buf_[gapEnd_..gapEnd_+count-1]  →  buf_[gapStart_..gapStart_+count-1]
        size_t count = pos - gapStart_;
        std::copy(buf_.data() + gapEnd_,
                  buf_.data() + gapEnd_ + count,
                  buf_.data() + gapStart_);
        gapStart_ += count;
        gapEnd_   += count;
    }
}

void GapBuffer::ensureGap(size_t needed) {
    if (gapSize() >= needed) return;

    // Double the gap (at minimum kDefaultGap) to give amortised O(1) insert.
    size_t newGap  = std::max(needed * 2, kDefaultGap);
    size_t docLen  = size();
    size_t postLen = buf_.size() - gapEnd_;

    std::vector<char> newBuf(docLen + newGap);
    std::copy(buf_.data(),            buf_.data() + gapStart_,   newBuf.data());
    std::copy(buf_.data() + gapEnd_,  buf_.data() + buf_.size(), newBuf.data() + gapStart_ + newGap);
    gapEnd_ = gapStart_ + newGap;
    buf_    = std::move(newBuf);
    (void)postLen; // silences unused-variable warning; postLen used implicitly via buf_.size()
}

void GapBuffer::insert(size_t position, const std::string& text) {
    if (text.empty()) return;
    if (position > size()) position = size();
    ensureGap(text.size());
    moveGapTo(position);
    std::copy(text.begin(), text.end(), buf_.data() + gapStart_);
    gapStart_ += text.size();
}

void GapBuffer::erase(size_t position, size_t length) {
    if (length == 0) return;
    size_t docLen = size();
    if (position >= docLen) return;
    if (position + length > docLen) length = docLen - position;
    moveGapTo(position);
    // Absorb the deleted characters by extending gapEnd_ — no copying needed.
    gapEnd_ += length;
}
