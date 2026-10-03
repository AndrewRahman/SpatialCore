#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
#include <cmath>

namespace spatialcore
{

//==============================================================================
// Cartesian->Polar conversion per ITU-R BS.2127-0, moved verbatim from
// OpenSpatialDelayProcessor.cpp's file-local cartesianToPolar().
//==============================================================================
static inline void cartesianToPolar (float x, float y, float z,
                                     float& azDeg, float& elDeg, float& dist)
{
    static constexpr float kPi = 3.14159265358979323846f;
    azDeg = std::atan2 (-x, y) * (180.0f / kPi);
    float r = std::sqrt (x * x + y * y);
    elDeg = std::atan2 (z, r) * (180.0f / kPi);
    dist = juce::jlimit (0.0f, 1.0f, std::sqrt (x * x + y * y + z * z));
}

//==============================================================================
// OSC Receive -- message thread callback (MessageLoopCallback). Accepts
// ADM-OSC standard (/adm/obj/N/) and OSD custom (/osd/obj/N/, /osd/global/)
// address grammar, moved verbatim from
// OpenSpatialDelayProcessor::oscMessageReceived (Source/PluginProcessor.cpp) --
// message-parsing logic unchanged (Pitfall 4); only the dispatch target
// changes from direct member calls (handleOSCPosition/handleOSCParam) to the
// Listener callback interface, since this class no longer has access to
// APVTS/processor state directly.
//==============================================================================
void ADMOSCReceiver::oscMessageReceived (const juce::OSCMessage& message)
{
    const auto address = message.getAddressPattern().toString();

    // --- /adm/obj/N/... or /osd/obj/N/... — per-object messages ---
    if (address.startsWith ("/adm/obj/") || address.startsWith ("/osd/obj/"))
    {
        auto afterObj = address.substring (9);  // both prefixes are 9 chars
        auto slashIdx = afterObj.indexOf ("/");
        if (slashIdx < 0) return;

        int objNum = afterObj.substring (0, slashIdx).getIntValue();
        if (objNum < 1 || objNum > MAX_SOURCES) return;
        int objIdx = objNum - 1;

        auto property = afterObj.substring (slashIdx);

        // --- Position messages (accepted on both /adm/ and /osd/) ---
        // NOTE: the pre-move OSD handler read the OTHER two axes from cached
        // APVTS state so a single-axis message (e.g. /azim) could report a
        // full (az, el, dist) triple to handleOSCPosition. This class has no
        // APVTS access, so single-axis messages are forwarded with the other
        // two axes as NaN; the Listener sentinel-checks (std::isnan) and only
        // updates the axis that was actually received. Cartesian /x /y /z
        // partial-update state (oscCartesianX/Y/Z) also stays owned by the
        // consumer for the same reason -- forwarded as raw single-axis params.
        if (property == "/azim" && message.size() >= 1 && message[0].isFloat32())
        {
            listeners.call ([objIdx, v = message[0].getFloat32()] (Listener& l)
            { l.admPositionReceived (objIdx, v, NAN, NAN); });
        }
        else if (property == "/elev" && message.size() >= 1 && message[0].isFloat32())
        {
            listeners.call ([objIdx, v = message[0].getFloat32()] (Listener& l)
            { l.admPositionReceived (objIdx, NAN, v, NAN); });
        }
        else if (property == "/dist" && message.size() >= 1 && message[0].isFloat32())
        {
            listeners.call ([objIdx, v = message[0].getFloat32()] (Listener& l)
            { l.admPositionReceived (objIdx, NAN, NAN, v); });
        }
        else if (property == "/aed" && message.size() >= 3
                 && message[0].isFloat32() && message[1].isFloat32() && message[2].isFloat32())
        {
            float az = message[0].getFloat32(), el = message[1].getFloat32(), d = message[2].getFloat32();
            listeners.call ([objIdx, az, el, d] (Listener& l)
            { l.admPositionReceived (objIdx, az, el, d); });
        }
        else if (property == "/xyz" && message.size() >= 3
                 && message[0].isFloat32() && message[1].isFloat32() && message[2].isFloat32())
        {
            float azDeg, elDeg, dist;
            cartesianToPolar (message[0].getFloat32(), message[1].getFloat32(),
                              message[2].getFloat32(), azDeg, elDeg, dist);
            listeners.call ([objIdx, azDeg, elDeg, dist] (Listener& l)
            { l.admPositionReceived (objIdx, azDeg, elDeg, dist); });
        }
        else if ((property == "/x" || property == "/y" || property == "/z")
                 && message.size() >= 1 && message[0].isFloat32())
        {
            // Partial Cartesian axis update -- consumer owns the running
            // (x, y, z) state and recomputes polar position on receipt
            // (mirrors the pre-move oscCartesianX/Y/Z member fields).
            listeners.call ([objIdx, property, v = message[0].getFloat32()] (Listener& l)
            { l.admObjectParamReceived (objIdx, property.substring (1), v); });
        }
        // --- Per-object non-position params (/osd/obj/N/ only) ---
        else if (property == "/enabled" && message.size() >= 1)
        {
            float v = message[0].isFloat32() ? message[0].getFloat32()
                                             : static_cast<float> (message[0].getInt32());
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "enabled", v); });
        }
        else if (property == "/doppler" && message.size() >= 1 && message[0].isFloat32())
        {
            float v = message[0].getFloat32();
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "doppler", v); });
        }
        else if (property == "/pitch" && message.size() >= 1 && message[0].isFloat32())
        {
            float v = message[0].getFloat32();
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "pitch", v); });
        }
        else if (property == "/trajectory" && message.size() >= 1)
        {
            float v = message[0].isFloat32() ? message[0].getFloat32()
                                             : static_cast<float> (message[0].getInt32());
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "trajectory", v); });
        }
        else if (property == "/speed" && message.size() >= 1 && message[0].isFloat32())
        {
            float v = message[0].getFloat32();
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "speed", v); });
        }
        else if (property == "/direction" && message.size() >= 1)
        {
            float v = message[0].isFloat32() ? message[0].getFloat32()
                                             : static_cast<float> (message[0].getInt32());
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "direction", v); });
        }
        else if (property == "/input" && message.size() >= 1)
        {
            float v = message[0].isFloat32() ? message[0].getFloat32()
                                             : static_cast<float> (message[0].getInt32());
            listeners.call ([objIdx, v] (Listener& l) { l.admObjectParamReceived (objIdx, "input", v); });
        }
    }
    // --- /osd/global/... — global parameter messages ---
    else if (address.startsWith ("/osd/global/"))
    {
        auto property = address.substring (12);  // skip "/osd/global/" → "delaytime" etc.
        if (message.size() < 1) return;
        float val = message[0].isFloat32() ? message[0].getFloat32()
                                           : static_cast<float> (message[0].getInt32());

        listeners.call ([property, val] (Listener& l) { l.admGlobalParamReceived (property, val); });
    }
}

} // namespace spatialcore
