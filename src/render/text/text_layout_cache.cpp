module;
#include <stdint.h>

module kwik.render.text.layout.cache;

import std;
import kwik.render.text.types;

LayoutCache::LayoutCache() {
    entries_.resize(kMaxEntries);
}

auto LayoutCache::getOrCreate(const TextLayoutKey& key, const TextLayoutResult& result) -> TextLayoutToken {
    auto it = keyToIndex_.find(key);
    if (it != keyToIndex_.end()) {
        auto& entry = entries_[it->second];
        entry.result = result;
        entry.alive = true;
        return {it->second, entry.gen};
    }

    // 环形覆写
    uint32_t idx = nextIndex_;
    nextIndex_ = (nextIndex_ + 1) % kMaxEntries;

    auto& entry = entries_[idx];
    if (entry.alive) {
        keyToIndex_.erase(entry.key);
    }
    entry.gen++;
    entry.key = key;
    entry.result = result;
    entry.alive = true;
    keyToIndex_[key] = idx;
    return {idx, entry.gen};
}

auto LayoutCache::getByToken(TextLayoutToken token) -> TextLayoutResult* {
    if (token.index >= kMaxEntries) return nullptr;
    auto& entry = entries_[token.index];
    if (!entry.alive || entry.gen != token.gen) return nullptr;
    return &entry.result;
}