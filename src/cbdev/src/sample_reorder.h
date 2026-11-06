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
/// whose head has not arrived.  Each sample group is tracked separately: every
/// channel picks its own group, so any of groups 1-6 can be active at once,
/// each at its own rate.  Only groups fast enough to fit a sample in under half
/// a millisecond (10 kHz and up) can mark a tail.  Non-sample packets (spikes,
/// config, monitor) never affect detection.
///
/// Release: the split puts out head, monitor, tail.  With the tail delivered
/// first, the head is at most two datagrams behind it (tail-head-monitor,
/// tail-monitor-head, or monitor-tail-head).  So a held tail waits for at most
/// that many datagrams: if the head arrives it is delivered first, then the
/// tail, then whatever queued behind the tail; otherwise everything is released
/// in arrival order (the head was really lost).  Only the late head ever moves.
/// The caller also releases on an idle socket, when nothing more is coming.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBDEV_SAMPLE_REORDER_H
#define CBDEV_SAMPLE_REORDER_H

#include <cbproto/cbproto.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace cbdev {

class SampleReorderBuffer {
public:
    /// A held datagram is only the tail of a split if it spans less than this.
    static constexpr PROCTIME kMaxTailSpanNs = 500'000;

    /// @param window   Datagrams a held tail waits for its late head (0 disables
    ///                 reordering entirely).
    /// @param padding  Extra zeroed bytes kept after each held datagram.
    ///                 Callbacks may read a full packet struct at any packet
    ///                 offset, so held copies get the same padding as the
    ///                 receive buffer.
    explicit SampleReorderBuffer(uint32_t window = 2, size_t padding = 0)
        : m_window(window), m_padding(padding) {}

    /// Feed one received datagram.  @p emit(const uint8_t* data, size_t len) is
    /// called for each datagram to deliver now, in delivery order.
    template <class Emit>
    void push(const uint8_t* data, size_t len, Emit&& emit) {
        ++m_stats.datagrams;
        const Scan scan = scanDatagram(data, len);

        if (!m_held.empty()) {
            ++m_waited;
            if (scan.count[m_held_group] > 0 && scan.first[m_held_group] < m_held_first_ts) {
                // The late head of the split: deliver it, then everything held.
                emit(data, len);
                learn(data, len);
                if (m_waited > m_stats.head_wait_max)
                    m_stats.head_wait_max = m_waited;
                releaseHeld(emit, m_stats.released_by_head);
                ++m_reordered;
                return;
            }
            if (m_waited < m_window) {
                hold(data, len);  // queue behind the tail to keep arrival order
                return;
            }
            // The head did not come: the gap was real.  This datagram is then
            // handled like any other, after what was held.
            releaseHeld(emit, m_stats.released_by_count);
        }

        if (m_window > 0) {
            for (uint16_t g = 1; g <= kMaxGroup; ++g) {
                if (scan.count[g] > 0 && looksLikeSplitTail(g, scan)) {
                    m_held_group = g;
                    m_held_first_ts = scan.first[g];
                    m_waited = 0;
                    ++m_stats.holds;
                    hold(data, len);
                    return;
                }
            }
        }

        emit(data, len);
        learn(data, len);
    }

    /// Release held datagrams unconditionally (idle socket, shutdown).
    template <class Emit>
    void releaseAll(Emit&& emit) {
        if (!m_held.empty())
            releaseHeld(emit, m_stats.released_by_idle);
    }

    /// True while a datagram is held.
    [[nodiscard]] bool holding() const { return !m_held.empty(); }

    /// Number of times a late datagram was put back in order.
    [[nodiscard]] uint64_t reorderedCount() const { return m_reordered; }

    /// Counters describing what the buffer has done.
    struct Stats {
        uint64_t datagrams = 0;          ///< datagrams pushed
        uint64_t holds = 0;              ///< times a datagram was held
        uint64_t released_by_head = 0;   ///< hold ended by the late head (a reorder fixed)
        uint64_t released_by_count = 0;  ///< hold ended after the window (the gap was real)
        uint64_t released_by_idle = 0;   ///< hold ended by an idle socket or shutdown
        uint32_t head_wait_max = 0;      ///< most datagrams a late head arrived after its tail
    };
    [[nodiscard]] const Stats& stats() const { return m_stats; }

private:
    static constexpr uint16_t kMaxGroup = 6;
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
    void releaseHeld(Emit&& emit, uint64_t& reason) {
        ++reason;
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

    uint32_t m_window;
    size_t m_padding;

    std::array<PROCTIME, kMaxGroup + 1> m_last_ts{};
    std::array<PROCTIME, kMaxGroup + 1> m_period{};

    std::deque<Held> m_held;   // the held tail, then anything queued behind it
    uint16_t m_held_group = 0;
    PROCTIME m_held_first_ts = 0;
    uint32_t m_waited = 0;     // datagrams pushed since the tail was held
    uint64_t m_reordered = 0;
    Stats m_stats;
};

} // namespace cbdev

#endif // CBDEV_SAMPLE_REORDER_H
