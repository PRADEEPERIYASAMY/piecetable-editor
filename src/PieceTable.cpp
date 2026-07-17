#include "PieceTable.hpp"
#include <fstream>
#include <algorithm>

PieceTable::PieceTable() {
    appendedText_.reserve(1 << 20); // 1 MB headroom avoids early reallocation churn
}

void PieceTable::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    originalText_.clear();
    appendedText_.clear();
    pieces_.clear();
    documentLength_ = 0;

    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        originalText_ += line;
        originalText_ += '\n';
    }

    if (!originalText_.empty()) {
        pieces_.push_back({SourceBuffer::Original, 0, originalText_.size()});
    }
    documentLength_ = originalText_.size();
}

std::string PieceTable::text() const {
    std::string result;
    result.reserve(documentLength_);
    for (const auto& piece : pieces_) {
        const std::string& buffer =
            (piece.source == SourceBuffer::Original) ? originalText_ : appendedText_;
        result.append(buffer, piece.offset, piece.length);
    }
    return result;
}

Piece PieceTable::appendAndTrackPiece(const std::string& text) {
    size_t insertedAt = appendedText_.size();
    appendedText_ += text;
    return {SourceBuffer::Appended, insertedAt, text.size()};
}

void PieceTable::insert(size_t position, const std::string& text) {
    if (text.empty()) return;
    if (position > documentLength_) position = documentLength_;

    // Fast path: typing at the end of the document (by far the most common
    // case) just extends the append buffer and either grows the last piece
    // in place or appends a new one — O(1) amortized, no piece splitting.
    if (position == documentLength_) {
        size_t appendStart = appendedText_.size();
        appendedText_ += text;
        documentLength_ += text.size();

        if (!pieces_.empty()) {
            Piece& last = pieces_.back();
            if (last.source == SourceBuffer::Appended &&
                last.offset + last.length == appendStart) {
                last.length += text.size();
                return;
            }
        }
        pieces_.push_back({SourceBuffer::Appended, appendStart, text.size()});
        return;
    }

    // General path: find the piece straddling `position` and split it.
    Piece inserted = appendAndTrackPiece(text);
    documentLength_ += text.size();

    size_t consumed = 0;
    for (size_t i = 0; i < pieces_.size(); ++i) {
        if (consumed + pieces_[i].length >= position) {
            size_t splitOffset = position - consumed;
            Piece original = pieces_[i];
            pieces_.erase(pieces_.begin() + i);

            size_t insertAt = i;
            if (splitOffset > 0) {
                pieces_.insert(pieces_.begin() + insertAt,
                                {original.source, original.offset, splitOffset});
                ++insertAt;
            }
            pieces_.insert(pieces_.begin() + insertAt, inserted);
            ++insertAt;
            if (splitOffset < original.length) {
                pieces_.insert(pieces_.begin() + insertAt,
                                {original.source,
                                 original.offset + splitOffset,
                                 original.length - splitOffset});
            }
            return;
        }
        consumed += pieces_[i].length;
    }
}

void PieceTable::erase(size_t position, size_t length) {
    if (length == 0) return;

    size_t consumed = 0;
    for (size_t i = 0; i < pieces_.size() && length > 0;) {
        if (consumed + pieces_[i].length > position) {
            size_t startWithinPiece = position - consumed;
            size_t removable = std::min(length, pieces_[i].length - startWithinPiece);

            Piece original = pieces_[i];
            pieces_.erase(pieces_.begin() + i);

            size_t insertAt = i;
            if (startWithinPiece > 0) {
                pieces_.insert(pieces_.begin() + insertAt,
                                {original.source, original.offset, startWithinPiece});
                ++insertAt;
            }
            if (startWithinPiece + removable < original.length) {
                pieces_.insert(pieces_.begin() + insertAt,
                                {original.source,
                                 original.offset + startWithinPiece + removable,
                                 original.length - startWithinPiece - removable});
            }

            length -= removable;
            documentLength_ -= removable;
            // Don't advance `i`: the piece(s) left behind at this index
            // need to be re-examined if more of `length` remains.
        } else {
            consumed += pieces_[i].length;
            ++i;
        }
    }

    coalesceAdjacentPieces();
}

void PieceTable::coalesceAdjacentPieces() {
    if (pieces_.size() < 2) return;
    std::vector<Piece> merged;
    merged.reserve(pieces_.size());
    merged.push_back(pieces_.front());

    for (size_t i = 1; i < pieces_.size(); ++i) {
        Piece& last = merged.back();
        const Piece& next = pieces_[i];
        if (last.source == next.source && last.offset + last.length == next.offset) {
            last.length += next.length; // contiguous — merge into one piece
        } else {
            merged.push_back(next);
        }
    }
    pieces_ = std::move(merged);
}

char PieceTable::charAt(size_t index) const {
    size_t consumed = 0;
    for (const Piece& piece : pieces_) {
        if (index < consumed + piece.length) {
            size_t offset = index - consumed;
            const std::string& buffer =
                (piece.source == SourceBuffer::Original) ? originalText_ : appendedText_;
            return buffer[piece.offset + offset];
        }
        consumed += piece.length;
    }
    return '\0';
}

std::string PieceTable::substring(size_t start, size_t length) const {
    std::string result;
    result.reserve(length);

    size_t consumed = 0;
    size_t end = start + length;

    for (const Piece& piece : pieces_) {
        size_t pieceStart = consumed;
        size_t pieceEnd = consumed + piece.length;

        if (pieceEnd <= start) { consumed += piece.length; continue; }
        if (pieceStart >= end) break;

        size_t from = std::max(start, pieceStart);
        size_t to = std::min(end, pieceEnd);
        size_t offsetInPiece = from - pieceStart;
        size_t count = to - from;

        const std::string& buffer =
            (piece.source == SourceBuffer::Original) ? originalText_ : appendedText_;
        result.append(buffer, piece.offset + offsetInPiece, count);

        consumed += piece.length;
    }
    return result;
}
