/*
 *  Copyright (c) 2016, The OpenThread Authors.  All rights reserved.
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
 *   This file implements MUD functionality
 */

#include "mud.hpp"

#if CONFIG_OPENTHREAD_MUD
#include "common/string.hpp"
#include "common/message.hpp"
#include "openthread/server.h"
#include "thread/mud_tlvs.hpp"

namespace ot {

namespace Mud {

#if !OPENTHREAD_FTD
Mud::Mud(Instance &aInstance): InstanceLocator(aInstance) {}
#endif

#if OPENTHREAD_FTD
Mud::Mud(Instance &aInstance)
    : InstanceLocator(aInstance)
    , mMudSocket(aInstance, nullptr, this)
{
    if (mMudSocket.Open() != kErrorNone) {
        LogWarn("failed to open MUD socket on MLE router");
    }
}

Error Mud::ProcessMudUrl(String<kMaxMudUrlLength> aMUDUrl, const Child *newChild) {
    Error                   error           = kErrorNone;
    Child::AddressIterator  addressIterator = Child::kAddressIteratorInit;
    Message                *MUDmessage      = nullptr;
    Ip6::Address            childAddress;
    Ip6::Address            serverAddress;
    Ip6::MessageInfo        messageInfo;
    childAddress.Clear();

    if (!mMudSocket.IsOpen()) {
        ExitNow(error = kErrorInvalidState);
    }

    SuccessOrExit(error = FindMudForwarderIp(serverAddress));
    LogInfo("found mud forwarder ip: %s", serverAddress.ToString().AsCString());

    // Send UDP message with mudUrl and child external IP address to serveripaddress
    MUDmessage = mMudSocket.NewMessage();

    while (newChild->GetNextIp6Address(addressIterator, childAddress) == kErrorNone)
    {
        LogInfo("found child address: %s", childAddress.ToString().AsCString());
        if (MatchesOmrPrefix(childAddress)) {
            LogInfo("found child omr-address %s", childAddress.ToString().AsCString());
            SuccessOrExit(error = MUDmessage->AppendBytes(childAddress.ToString().AsCString(), childAddress.ToString().GetLength() + 1));
        }
    }

    SuccessOrExit(error = Tlv::Append<MudUrlForwarderTlv>(*MUDmessage, aMUDUrl.AsCString()));
    SuccessOrExit(error = Tlv::Append<ChildIpTlv>(*MUDmessage, childAddress.ToString().AsCString()));
    
    messageInfo.SetPeerPort(kMUDForwarderPort);
    messageInfo.SetPeerAddr(serverAddress);

    SuccessOrExit(error = mMudSocket.SendTo(*MUDmessage, messageInfo));
    LogInfo("MUD udp message is sent!");
    MUDmessage = nullptr;

exit:
    if (MUDmessage != nullptr)
    {
        MUDmessage->Free();
    }

    return error;
}

Error Mud::FindMudForwarderIp(Ip6::Address &serverAddress)
{
    Error                           error       = kErrorNone;
    String<kServiceNameMaxLength>   serviceName;
    NetworkData::Iterator           iterator    = NetworkData::kIteratorInit;
    NetworkData::ServiceConfig      service;
    NetworkData::ServiceData        serviceData;
    NetworkData::ServerData         serverData;
    uint8_t                         serverDataLength;

    // TODO change to identifier byte as other services
    serviceName.Append("MUD_Forwarder");

    // Find the correct service
    while (Get<NetworkData::Leader>().GetNextService(iterator, service) == kErrorNone)
    {
        service.GetServiceData(serviceData);
        if (service.mServiceDataLength == serviceName.GetLength() + 1 && serviceData.MatchesBytesIn(serviceName.AsCString()) ) {
            service.GetServerConfig().GetServerData(serverData);
            serverDataLength = service.GetServerConfig().mServerDataLength;

            // Get IPv6 address from serverdata
            if (serverDataLength > Ip6::Address::kInfoStringSize) {
                LogWarn("serverdatalenght %d > ip6 infostringsize %d ", serverDataLength, Ip6::Address::kInfoStringSize);
                ExitNow(error = kErrorParse);
            }
            
            // TODO: refactor this to a tlv with an ipaddress
            serverAddress.SetBytes(serverData.GetBytes());
        }
    }

exit:
    return error;
}

bool Mud::MatchesOmrPrefix(Ip6::Address aAddress)
{
    NetworkData::Iterator           iterator = NetworkData::kIteratorInit;
    NetworkData::OnMeshPrefixConfig prefixConfig;

    while (Get<NetworkData::Leader>().GetNextOnMeshPrefix(iterator, prefixConfig) == kErrorNone)
    {
        if (IsOmrPrefix(prefixConfig) && aAddress.MatchesPrefix(prefixConfig.GetPrefix())) {
            return true;
        }
    }

    return false;
}

bool Mud::IsOmrPrefix(const NetworkData::OnMeshPrefixConfig &aPrefixConfig)
{
    // By spec: OMR prefix is identifiable with stable, on mesh, preferred, and SLAAC all true
    // For some reason, BorderRouter::RoutingManager::IsValidOmrPrefix does not check if mPreferred is true.
    return isValidOmrPrefix(aPrefixConfig.GetPrefix()) && aPrefixConfig.mOnMesh && aPrefixConfig.mSlaac && aPrefixConfig.mStable && aPrefixConfig.mPreferred; 
}

bool Mud::isValidOmrPrefix(const Ip6::Prefix &aPrefix)
{
    return (aPrefix.GetLength() == kOmrPrefixLength) && !aPrefix.IsLinkLocal() && !aPrefix.IsMulticast();
}

#endif // OPENTHREAD_FTD

} // Namespace Mud

} // Namespace ot

#endif // CONFIG_OPENTHREAD_MUD
