#pragma once

#include "KBarRpcService.h"
#include "NetCore/Singleton.h"


#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <algorithm>

using namespace NetCore;

class KBarManager : public Singleton<KBarManager> {
    friend class Singleton<KBarManager>;

public:
    static constexpr int PERIOD_M1  = 1;
    static constexpr int PERIOD_M5  = 5;
    static constexpr int PERIOD_M15 = 15;
    static constexpr int PERIOD_M30 = 30;
    static constexpr int PERIOD_H1  = 60;
    static constexpr int PERIOD_H4  = 240;
    static constexpr int PERIOD_D1  = 1440;
    static constexpr int PERIOD_W1  = 10080;
    static constexpr int PERIOD_MN1 = 43200;

    void add_kbar(const KBar& kbar) {
        std::unique_lock lock(mutex_);
        auto& bars = get_bars_unlocked(kbar.symbol, kbar.timeFrame);
        bars[kbar.time] = kbar; // map keyed by time, auto-sorted, insert or update
    }

    void add_kbars(const std::string& symbol, int timeFrame, const std::vector<KBar>& bars) {
        if (bars.empty()) return;

        std::unique_lock lock(mutex_);
        auto& target = get_bars_unlocked(symbol, timeFrame);

        for (const auto& kbar : bars) {
            target[kbar.time] = kbar;
        }
    }

    std::vector<KBar> get_kbars(const std::string& symbol, int timeFrame) const {
        std::shared_lock lock(mutex_);
        auto key = make_key(symbol, timeFrame);
        auto it = bars_map_.find(key);
        if (it != bars_map_.end()) {
            std::vector<KBar> result;
            result.reserve(it->second.size());
            for (const auto& [_, kbar] : it->second) {
                result.push_back(kbar);
            }
            return result;
        }
        return {};
    }

    size_t kbar_count(const std::string& symbol, int timeFrame) const {
        std::shared_lock lock(mutex_);
        auto key = make_key(symbol, timeFrame);
        auto it = bars_map_.find(key);
        return it != bars_map_.end() ? it->second.size() : 0;
    }

    KBar latest_kbar(const std::string& symbol, int timeFrame) const {
        std::shared_lock lock(mutex_);
        auto key = make_key(symbol, timeFrame);
        auto it = bars_map_.find(key);
        if (it != bars_map_.end() && !it->second.empty()) {
            return it->second.rbegin()->second;
        }
        return KBar{};
    }

    std::vector<KBar> get_kbars_range(const std::string& symbol, int timeFrame,
                                       uint64_t startTime, uint64_t endTime) const {
        std::vector<KBar> result;
        std::shared_lock lock(mutex_);
        auto key = make_key(symbol, timeFrame);
        auto it = bars_map_.find(key);
        if (it == bars_map_.end()) return result;

        const auto& bars = it->second;
        // lower_bound: first element with time >= startTime
        auto lo = bars.lower_bound(startTime);
        // upper_bound: first element with time > endTime
        auto hi = bars.upper_bound(endTime);

        for (auto iter = lo; iter != hi; ++iter) {
            result.push_back(iter->second);
        }
        return result;
    }

    std::vector<KBar> latest_kbars(const std::string& symbol, int timeFrame, size_t count) const {
        std::vector<KBar> result;
        std::shared_lock lock(mutex_);
        auto key = make_key(symbol, timeFrame);
        auto it = bars_map_.find(key);
        if (it == bars_map_.end() || it->second.empty()) return result;

        const auto& bars = it->second;
        size_t total = bars.size();
        size_t start = (total > count) ? total - count : 0;

        auto iter = bars.begin();
        std::advance(iter, start);
        for (; iter != bars.end(); ++iter) {
            result.push_back(iter->second);
        }
        return result;
    }

    void clear(const std::string& symbol, int timeFrame) {
        std::unique_lock lock(mutex_);
        auto key = make_key(symbol, timeFrame);
        bars_map_.erase(key);
    }

    void clear_all() {
        std::unique_lock lock(mutex_);
        bars_map_.clear();
    }

    std::vector<std::string> all_symbols() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> symbols;
        for (const auto& [key, _] : bars_map_) {
            auto sym = extract_symbol(key);
            if (std::find(symbols.begin(), symbols.end(), sym) == symbols.end()) {
                symbols.push_back(sym);
            }
        }
        return symbols;
    }

    std::vector<int> all_timeframes(const std::string& symbol) const {
        std::shared_lock lock(mutex_);
        std::vector<int> tfs;
        for (const auto& [key, _] : bars_map_) {
            auto [sym, tf] = extract_key(key);
            if (sym == symbol) {
                tfs.push_back(tf);
            }
        }
        return tfs;
    }

private:
    KBarManager() = default;
    ~KBarManager() = default;

    static std::string make_key(const std::string& symbol, int timeFrame) {
        return symbol + "|" + std::to_string(timeFrame);
    }

    static std::string extract_symbol(const std::string& key) {
        auto pos = key.find('|');
        return (pos != std::string::npos) ? key.substr(0, pos) : key;
    }

    static std::pair<std::string, int> extract_key(const std::string& key) {
        auto pos = key.find('|');
        if (pos != std::string::npos) {
            return {key.substr(0, pos), std::stoi(key.substr(pos + 1))};
        }
        return {key, 0};
    }

    std::map<uint64_t, KBar>& get_bars_unlocked(const std::string& symbol, int timeFrame) {
        auto key = make_key(symbol, timeFrame);
        return bars_map_[key];
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::map<uint64_t, KBar>> bars_map_;
};
