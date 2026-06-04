#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace ca::sim {

// SimpleCache 是一个教学用 cache 命中/缺失模型。
//
// 它只回答一个问题：
//   某个 address 对应的 cache line 当前在不在 cache 里？
//
// 当前版本故意不实现容量、组相联、替换策略、写回策略和一致性协议。
// 这些都属于更完整的 cache simulator，而不是本 demo 要解释的第一层概念。
class SimpleCache {
public:
    // lineSize 表示一个 cache line 覆盖多少字节。
    // 例如 lineSize=64 时，0x1000 和 0x1008 属于同一个 cache line。
    SimpleCache(std::string name, std::uint64_t lineSize)
        : name_(std::move(name)), lineSize_(lineSize) {
        if (lineSize_ == 0) {
            throw std::invalid_argument("cache line size must be non-zero");
        }
    }

    const std::string& name() const {
        return name_;
    }

    std::uint64_t lineSize() const {
        return lineSize_;
    }

    std::uint64_t accesses() const {
        return accesses_;
    }

    std::uint64_t hits() const {
        return hits_;
    }

    std::uint64_t misses() const {
        return misses_;
    }

    std::uint64_t lines() const {
        return static_cast<std::uint64_t>(lines_.size());
    }

    // 把字节地址转换成 cache line 地址。
    // cache 判断命中时看的是 line，而不是原始 byte address。
    std::uint64_t lineAddress(std::uint64_t address) const {
        return address / lineSize_;
    }

    // 只检查是否命中，不更新 accesses/hits/misses 统计。
    bool contains(std::uint64_t address) const {
        return lines_.count(lineAddress(address)) != 0;
    }

    // 记录一次 cache 访问，并返回 hit/miss。
    bool access(std::uint64_t address) {
        ++accesses_;
        if (contains(address)) {
            ++hits_;
            return true;
        }

        ++misses_;
        return false;
    }

    // 回填 cache line。通常在下游 L2/DRAM 返回数据后调用。
    void fill(std::uint64_t address) {
        lines_.insert(lineAddress(address));
    }

private:
    std::string name_;
    std::uint64_t lineSize_;
    std::unordered_set<std::uint64_t> lines_;
    std::uint64_t accesses_ = 0;
    std::uint64_t hits_ = 0;
    std::uint64_t misses_ = 0;
};

} // namespace ca::sim
