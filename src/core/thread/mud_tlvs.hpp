/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for generating and processing MUD Forward message TLVs.
 */


#ifndef MUD_TLVS_HPP_
#define MUD_TLVS_HPP_

#if OPENTHREAD_FTD

#include "openthread/mud.h"
#include "common/tlvs.hpp"

namespace ot
{
namespace Mud 
{

OT_TOOL_PACKED_BEGIN
class MudTlv : public ot::Tlv
{
public:
    enum Type : uint8_t
    {
        kMudUrl     = OT_MUD_FORWARD_TLV_MUD_URL,
        kMucChildIP = OT_MUD_FORWARD_TLV_DEVICE_IP,
    };

    static constexpr u_int8_t kMaxMudUrlLength = 40; ///< Max length of a MUD URL Tlv

    /**
     * Returns the Type value.
     *
     * @returns The Type value.
     *
     */
    Type GetType(void) const { return static_cast<Type>(ot::Tlv::GetType()); }

    /**
     * Sets the Type value.
     *
     * @param[in]  aType  The Type value.
     *
     */
    void SetType(Type aType) { ot::Tlv::SetType(static_cast<uint8_t>(aType)); }
} OT_TOOL_PACKED_END;

typedef StringTlvInfo<MudTlv::kMudUrl, MudTlv::kMaxMudUrlLength> MudUrlForwarderTlv;

typedef StringTlvInfo<MudTlv::kMucChildIP, MudTlv::kMaxMudUrlLength> ChildIpTlv;

} // namespace Mud

} // namespace ot

#endif // OPENTHREAD_FTD

#endif // MUD_TLVS_HPP_
