#pragma once
#include "mqdb.hpp"
#include "opt_maps.hpp"
#include "rgba_buffer.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2res {

// LRU cache with O(1) get/put. Front of list = MRU, back = LRU.
template <typename K, typename V> class LruCache {
public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    // Returns pointer to value (promotes to MRU), or nullptr on miss.
    V* get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end())
            return nullptr;
        order_.splice(order_.begin(), order_, it->second.first);
        return &it->second.second;
    }

    // Insert or update. Evicts LRU entry when at capacity.
    template <typename Vv> void put(const K& key, Vv&& value) {
        if (capacity_ == 0) {
            return;
        }
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second.second = std::forward<Vv>(value);
            order_.splice(order_.begin(), order_, it->second.first);
            return;
        }
        if (map_.size() >= capacity_) {
            K lru = order_.back();
            map_.erase(lru);
            order_.pop_back();
        }
        order_.push_front(key);
        map_.emplace(key, std::make_pair(order_.begin(), std::forward<Vv>(value)));
    }

    [[nodiscard]] std::size_t size() const noexcept { return map_.size(); }

private:
    std::size_t                                                          capacity_;
    std::list<K>                                                         order_;
    std::unordered_map<K, std::pair<typename std::list<K>::iterator, V>> map_;
};

struct DecodedImage {
    std::string          logical_name;
    uint32_t             width = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> rgba; // RGBA8, row-major
    nlohmann::json       metadata;
};

class ImageResourceDecoder {
public:
    ImageResourceDecoder(const MqdbContainer& container, const OptMaps& maps);

    // Returns logical names of all image entries (id != -1) in IndexMap order.
    std::vector<std::string> list_images() const;

    // Decode one logical image by name. Name is matched case-insensitively.
    // Throws ParseError if the image cannot be decoded.
    DecodedImage decode_image(std::string_view logical_name) const;
    DecodedImage decode_image_with_research_metadata(std::string_view logical_name) const;

    static constexpr std::size_t kPngCacheCapacity = 64;

private:
    const MqdbContainer& container_;
    const OptMaps&       maps_;

    mutable std::mutex                                               cache_mutex_;
    mutable LruCache<std::size_t, std::shared_ptr<const RgbaBuffer>> base_cache_{kPngCacheCapacity};

    std::shared_ptr<const RgbaBuffer> get_base_png(const IndexEntry& entry) const;
    DecodedImage decode_image_impl(std::string_view logical_name, bool research_metadata) const;
};

} // namespace d2res
