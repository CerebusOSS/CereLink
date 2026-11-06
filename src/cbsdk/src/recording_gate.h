///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   recording_gate.h
/// @brief  Which outbound packets a session refuses while the device is recording
///
/// Central greys out these settings in its own dialogs while a file is being
/// recorded (it never filters packets from other programs).  A CereLink session
/// applies the same rule to what it sends, so a script cannot change the
/// structure of a file mid-recording.  A full CHANSET is checked too, because
/// setChannelConfig() and loadCCF() can change a sample group with it.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSDK_RECORDING_GATE_H
#define CBSDK_RECORDING_GATE_H

#include <cbproto/cbproto.h>
#include <cstddef>

namespace cbsdk {

/// True while the device reports a recording in progress (last REPFILECFG).
inline bool isRecording(const cbPKT_FILECFG& fileinfo) {
    return fileinfo.options == cbFILECFG_OPT_REC;
}

/// What @p pkt would change that is locked while recording, or nullptr if it is allowed.
/// @param chaninfo  Current channel config, indexed by chan - 1
/// @param n_chans   Entries in @p chaninfo
/// @param status    Current spike-sorting status
inline const char* recordingBlockReason(const cbPKT_GENERIC& pkt, const cbPKT_CHANINFO* chaninfo,
                                        size_t n_chans, const cbPKT_SS_STATUS& status) {
    switch (pkt.cbpkt_header.type) {
        case cbPKTTYPE_SYSSETSPKLEN:         return "spike length";
        case cbPKTTYPE_SS_DETECTSET:
        case cbPKTTYPE_SS_NOISE_BOUNDARYSET: return "spike detection settings";
        case cbPKTTYPE_CHANSETNTRODEGROUP:
        case cbPKTTYPE_SETNTRODEINFO:        return "n-trode grouping";
        case cbPKTTYPE_SS_RESETSET:          return "spike sorting (restart)";
        case cbPKTTYPE_SS_STATUSSET: {
            // Freeze/lock (adapt NEVER) stays allowed; only switching continuous sorting on is locked.
            const auto& s = reinterpret_cast<const cbPKT_SS_STATUS&>(pkt);
            if (s.cntlNumUnits.nMode == ADAPT_ALWAYS && status.cntlNumUnits.nMode != ADAPT_ALWAYS)
                return "spike sorting (continuous)";
            return nullptr;
        }
        case cbPKTTYPE_CHANSET:
        case cbPKTTYPE_CHANSETSMP:
        case cbPKTTYPE_CHANSETAINP: {
            const auto& ci = reinterpret_cast<const cbPKT_CHANINFO&>(pkt);
            if (ci.chan < 1 || ci.chan > n_chans)
                return nullptr;
            const auto& cur = chaninfo[ci.chan - 1];
            const auto type = pkt.cbpkt_header.type;
            if (type != cbPKTTYPE_CHANSETAINP && ci.smpgroup != cur.smpgroup)
                return "sample group";
            if (type != cbPKTTYPE_CHANSETSMP && ((ci.ainpopts ^ cur.ainpopts) & cbAINP_RAWSTREAM_ENABLED))
                return "raw stream";
            return nullptr;
        }
        default:
            return nullptr;
    }
}

} // namespace cbsdk

#endif // CBSDK_RECORDING_GATE_H
