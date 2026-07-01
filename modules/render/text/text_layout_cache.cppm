module;
#include <stdint.h>

export module kwik.render.text.layout.cache;

import std;
import kwik.render.text.types;

/**
 * @brief 全局布局缓存
 *
 * Key: (textHash + styleHash + maxWidth) → TextLayoutResult
 * 组件持有 TextLayoutToken (index + gen) 轻量引用。
 *
 * 缓存条目 LRU 淘汰:
 *   - MAX_ENTRIES = 256
 *   - 超限时淘汰最旧 index (环形覆写, gen++)
 */
export class LayoutCache {
public:
    LayoutCache();

    LayoutCache(const LayoutCache&) = delete;
    LayoutCache& operator=(const LayoutCache&) = delete;

    /**
     * @brief 查找或插入布局结果
     * @param key    布局 Key
     * @param result 输出: 布局结果 (查找命中则直接写入)
     * @return Token 组件持有, 下一帧可通过 token 快速定位
     *
     * 注意: 若布局结果被淘汰, getByToken 返回 nullptr,
     * 组件需重新调用 getOrCreate。
     */
    auto getOrCreate(const TextLayoutKey& key, const TextLayoutResult& result) -> TextLayoutToken;

    /**
     * @brief 通过 Token 获取布局结果
     * @return nullptr 表示已淘汰, 需重新排版
     */
    auto getByToken(TextLayoutToken token) -> TextLayoutResult*;

private:
    struct Entry {
        TextLayoutKey key;
        TextLayoutResult result;
        uint32_t gen = 0;
        bool alive = false;
    };

    static constexpr size_t kMaxEntries = 256;

    std::unordered_map<TextLayoutKey, uint32_t> keyToIndex_;
    std::vector<Entry> entries_;
    uint32_t nextIndex_ = 0;
};