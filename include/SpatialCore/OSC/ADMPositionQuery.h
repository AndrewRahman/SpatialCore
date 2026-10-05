#pragma once

namespace spatialcore
{

// Which position property a query asked for. ADM-OSC rule: a message with no
// arguments sent to a position address is a query for the current value, and the
// receiver of the query answers with the same address carrying that value
// (Phase 4, D-08a). Reported through ADMOSCReceiver::Listener::admPositionQueried
// and answered with ADMOSCSender::queueReply.
//
// Kept in its own header so ADMOSCSender.h can name it without pulling in the whole
// receiver (and its juce::OSCReceiver bases) (IN-01). ADMOSCReceiver.h includes it,
// so existing code that includes the receiver header is unaffected.
enum class ADMPositionQuery { azim, elev, dist, aed, xyz };

} // namespace spatialcore
