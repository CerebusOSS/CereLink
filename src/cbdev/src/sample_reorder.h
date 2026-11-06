///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   sample_reorder.h
/// @author CereLink Development Team
///
/// @brief  Restore datagram order when the host delivers sample datagrams out of order
///
/// The device sends sample groups in time order, roughly one datagram per
/// millisecond.  Now and then its protocol-monitor packet, which travels in a
/// datagram of its own, splits a millisecond across two datagrams a few
/// microseconds apart, e.g. 27 samples, the monitor, then the remaining 3.  On
/// Windows the host occasionally hands the small trailing datagram to the
/// socket before the large one, although they were on the wire in order (#213:
/// ~1 per minute, seen identically by Central and CereLink).  Nothing is lost,
/// but sample timestamps step backwards.
///
/// Detection: a datagram is held when, for some sample group it carries, its
/// first sample comes *after a gap* in that group AND it spans less than half
/// a millisecond of that group -- i.e. it looks like the small tail of a split
/// whose head has not arrived.  The head, when it arrives, is delivered first,
/// then the held tail, then anything that queued up behind the tail.  Only the
/// late head is ever moved; everything else keeps arrival order.  A gap left
/// by genuine loss is usually followed by a full-millisecond datagram, which is
/// not held, so loss costs no extra latency; a held datagram is released
/// anyway when a sample datagram that is not the head arrives, or after the
/// hold time.
///
/// Each sample group is tracked separately (group 6 at 30 kHz alongside one of
/// groups 1-4 at a lower rate, which the device publishes slightly later), and
/// non-sample packets (spikes, config, monitor) never affect detection.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_SAMPLE_REORDER_H
#define CBDEV_SAMPLE_REORDER_H

#include <cbproto/cbproto.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <vector>

namespace cbdev {

class SampleReorderBuffer {
public:
    using Clock = std::chrono::steady_clock;

    /// A held datagram is only the tail of a split if it spans less than this.
    static constexpr PROCTIME kMaxTailSpanNs = 500'000;

    /// @param hold     Longest a datagram may be held waiting for an earlier one
    ///                 (zero disables reordering entirely).
    /// @param padding  Extra zeroed bytes kept after each held datagram.
    ///                 Callbacks may read a full packet struct at any packet
    ///                 offset, so held copies get the same padding as the
    ///                 receive buffer.
    explicit SampleReorderBuffer(std::chrono::microseconds hold, size_t padding = 0)
        : m_hold(hold), m_padding(padding) {}

    /// Feed one received datagram.  @p emit(const uint8_t* data, size_t len) is
    /// called for each datagram to deliver now, in delivery order.
    template <class Emit>
    void push(const uint8_t* data, size_t len, Clock::time_point now, Emit&& emit) {
        ++m_stats.datagrams;
        const Scan scan = scanDatagram(data, len);

        if (!m_held.empty()) {
            if (scan.count[m_held_group] > 0 && scan.first[m_held_group] < m_held_first_ts) {
                // The late head of the split: deliver it, then everything held.
                emit(data, len);
                learn(data, len);
                releaseHeld(emit, now, m_stats.released_by_head);
                ++m_reordered;
                return;
            }
            if (!scan.any && m_held.size() < kMaxQueued) {
                // No samples (monitor, config, spikes): it cannot be the head,
                // so queue it behind the held tail to keep arrival order.
                hold(data, len);
                return;
            }
            // Samples that are not the head: the gap was real.
            releaseHeld(emit, now, m_stats.released_by_other);
        }

        if (m_hold.count() > 0) {
            for (uint16_t g = 1; g <= kMaxGroup; ++g) {
                if (scan.count[g] > 0 && looksLikeSplitTail(g, scan)) {
                    m_held_group = g;
                    m_held_first_ts = scan.first[g];
                    m_deadline = now + m_hold;
                    m_held_since = now;
                    ++m_stats.holds;
                    hold(data, len);
                    return;
                }
            }
        }

        emit(data, len);
        learn(data, len);
    }

    /// Release held datagrams if the hold time has expired.
    template <class Emit>
    void releaseIfDue(Clock::time_point now, Emit&& emit) {
        if (!m_held.empty() && now >= m_deadline)
            releaseHeld(emit, now, m_stats.released_by_timeout);
    }

    /// Release held datagrams unconditionally (e.g. on shutdown).
    template <class Emit>
    void releaseAll(Emit&& emit) {
        if (!m_held.empty())
            releaseHeld(emit, Clock::now(), m_stats.released_by_timeout);
    }

    /// When held datagrams must be released by, if any are held.
    [[nodiscard]] std::optional<Clock::time_point> deadline() const {
        return m_held.empty() ? std::nullopt : std::optional<Clock::time_point>(m_deadline);
    }

    /// Number of times a late datagram was put back in order.
    [[nodiscard]] uint64_t reorderedCount() const { return m_reordered; }

    /// Counters describing what the buffer has done.
    struct Stats {
        uint64_t datagrams = 0;           ///< datagrams pushed
        uint64_t holds = 0;               ///< times a datagram was held
        uint64_t released_by_head = 0;    ///< hold ended by the late head (a reorder fixed)
        uint64_t released_by_other = 0;   ///< hold ended by other samples (the gap was real)
        uint64_t released_by_timeout = 0; ///< hold ended by the hold time (or shutdown)
        std::chrono::nanoseconds held_total{0};  ///< summed hold durations
        std::chrono::nanoseconds held_max{0};    ///< longest hold
    };
    [[nodiscard]] const Stats& stats() const { return m_stats; }

private:
    static constexpr uint16_t kMaxGroup = 6;
    static constexpr size_t kMaxQueued = 16;

    struct Scan {
        std::array<PROCTIME, kMaxGroup + 1> first{};
        std::array<PROCTIME, kMaxGroup + 1> last{};
        std::array<uint32_t, kMaxGroup + 1> count{};
        bool any = false;
    };

    static bool isSamplePacket(const cbPKT_HEADER& h) {
        return h.chid == 0 && h.type >= 1 && h.type <= kMaxGroup;
    }

    template <class Fn>
    static void forEachPacket(const uint8_t* data, size_t len, Fn&& fn) {
        size_t off = 0;
        while (off + cbPKT_HEADER_SIZE <= len) {
            cbPKT_HEADER h;
            std::memcpy(&h, data + off, sizeof(h));
            const size_t size = cbPKT_HEADER_SIZE + size_t(h.dlen) * 4;
            if (off + size > len)
                break;
            fn(h);
            off += size;
        }
    }

    static Scan scanDatagram(const uint8_t* data, size_t len) {
        Scan s;
        forEachPacket(data, len, [&](const cbPKT_HEADER& h) {
            if (!isSamplePacket(h))
                return;
            if (s.count[h.type]++ == 0)
                s.first[h.type] = h.time;
            s.last[h.type] = h.time;
            s.any = true;
        });
        return s;
    }

    /// Starts after a gap in group @p g and spans less than half a millisecond.
    bool looksLikeSplitTail(uint16_t g, const Scan& s) const {
        const PROCTIME last = m_last_ts[g];
        const PROCTIME period = m_period[g];
        if (last == 0 || period == 0 || s.first[g] <= last)
            return false;  // no learned period yet, or not ahead of what we have
        const bool gap = (s.first[g] - last) * 2 > period * 3;
        const PROCTIME span = (s.last[g] - s.first[g]) + period;  // n samples cover n periods
        return gap && span < kMaxTailSpanNs;
    }

    /// Track each group's last delivered timestamp and its sample period (the
    /// smallest positive step seen; real gaps are always larger).
    void learn(const uint8_t* data, size_t len) {
        forEachPacket(data, len, [&](const cbPKT_HEADER& h) {
            if (!isSamplePacket(h))
                return;
            PROCTIME& last = m_last_ts[h.type];
            if (last != 0 && h.time > last) {
                const PROCTIME step = h.time - last;
                PROCTIME& period = m_period[h.type];
                if (period == 0 || step < period)
                    period = step;
            }
            // Also follows a timestamp reset (device restart), where time goes back.
            last = h.time;
        });
    }

    void hold(const uint8_t* data, size_t len) {
        std::vector<uint8_t> copy(len + m_padding, 0);
        std::memcpy(copy.data(), data, len);
        m_held.push_back(Held{std::move(copy), len});
    }

    template <class Emit>
    void releaseHeld(Emit&& emit, Clock::time_point now, uint64_t& reason) {
        ++reason;
        const auto held_for = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_held_since);
        m_stats.held_total += held_for;
        if (held_for > m_stats.held_max)
            m_stats.held_max = held_for;
        std::deque<Held> out;
        out.swap(m_held);
        for (const Held& d : out) {
            emit(d.data.data(), d.len);
            learn(d.data.data(), d.len);
        }
    }

    struct Held {
        std::vector<uint8_t> data;
        size_t len;
    };

    std::chrono::microseconds m_hold;
    size_t m_padding;

    std::array<PROCTIME, kMaxGroup + 1> m_last_ts{};
    std::array<PROCTIME, kMaxGroup + 1> m_period{};

    std::deque<Held> m_held;   // the held tail, then anything queued behind it
    uint16_t m_held_group = 0;
    PROCTIME m_held_first_ts = 0;
    Clock::time_point m_deadline{};
    uint64_t m_reordered = 0;
    Clock::time_point m_held_since{};
    Stats m_stats;
};

} // namespace cbdev

#endif // CBDEV_SAMPLE_REORDER_H
