///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   device_factory.cpp
/// @author CereLink Development Team
/// @date   2025-01-15
///
/// @brief  Factory implementation for creating protocol-specific device sessions
///
///////////////////////////////////////////////////////////////////////////////////////////////////

// Platform headers MUST be included first (before cbproto)
#include "platform_first.h"

#include <cbdev/device_factory.h>
#include "device_session_impl.h"
#include "device_session_311.h"
#include "device_session_400.h"
#include "device_session_410.h"
#include "protocol_detector.h"

namespace cbdev {

Result<std::unique_ptr<IDeviceSession>> createDeviceSession(
    const ConnectionParams& config,
    ProtocolVersion version) {

    // Resolve client address before protocol detection so the probe socket
    // binds to the correct interface (e.g. loopback for NPLAY).
    std::string client_addr = config.client_address;
    if (client_addr.empty()) {
        client_addr = detectClientAddress(config.type);
    }

    // Auto-detect protocol if unknown
    if (version == ProtocolVersion::UNKNOWN) {
        auto detect_result = detectProtocol(
            config.device_address.c_str(), config.send_port,
            client_addr.c_str(), config.recv_port,
            2000  // 2 second timeout
        );

        if (detect_result.isError()) {
            return Result<std::unique_ptr<IDeviceSession>>::error(
                "Failed to detect protocol: " + detect_result.error()
            );
        }

        version = detect_result.value();
    }

    // Create appropriate implementation based on protocol version
    switch (version) {
        case ProtocolVersion::PROTOCOL_CURRENT: {
            // Create modern protocol implementation
            auto result = DeviceSession::create(config);
            if (result.isError()) {
                return Result<std::unique_ptr<IDeviceSession>>::error(result.error());
            }

            // Move into unique_ptr<IDeviceSession>
            auto device = std::make_unique<DeviceSession>(std::move(result.value()));
            return Result<std::unique_ptr<IDeviceSession>>::ok(std::move(device));
        }

        case ProtocolVersion::PROTOCOL_410: {
            // Create modern protocol implementation
            auto result = DeviceSession_410::create(config);
            if (result.isError()) {
                return Result<std::unique_ptr<IDeviceSession>>::error(result.error());
            }

            // Move into unique_ptr<IDeviceSession>
            auto device = std::make_unique<DeviceSession_410>(std::move(result.value()));
            return Result<std::unique_ptr<IDeviceSession>>::ok(std::move(device));
        }

        case ProtocolVersion::PROTOCOL_400: {
            // Create protocol 4.0 wrapper
            auto result = DeviceSession_400::create(config);
            if (result.isError()) {
                return Result<std::unique_ptr<IDeviceSession>>::error(result.error());
            }

            // Move into unique_ptr<IDeviceSession>
            auto device = std::make_unique<DeviceSession_400>(std::move(result.value()));
            return Result<std::unique_ptr<IDeviceSession>>::ok(std::move(device));
        }

        case ProtocolVersion::PROTOCOL_311: {
            // Create protocol 3.11 wrapper
            auto result = DeviceSession_311::create(config);
            if (result.isError()) {
                return Result<std::unique_ptr<IDeviceSession>>::error(result.error());
            }

            // Move into unique_ptr<IDeviceSession>
            auto device = std::make_unique<DeviceSession_311>(std::move(result.value()));
            return Result<std::unique_ptr<IDeviceSession>>::ok(std::move(device));
        }

        case ProtocolVersion::UNKNOWN:
        default:
            return Result<std::unique_ptr<IDeviceSession>>::error(
                "Cannot create device session: protocol version unknown or invalid"
            );
    }
}

} // namespace cbdev
