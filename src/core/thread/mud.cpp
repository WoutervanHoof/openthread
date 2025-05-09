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

#include "common/linked_list.hpp"
#include "common/locator_getters.hpp"
#include "common/log.hpp"
#include "common/message.hpp"
#include "common/string.hpp"
#include "instance/instance.hpp"
#include "net/netif.hpp"
#include "thread/thread_netif.hpp"
#include "thread/network_data_leader.hpp"

namespace ot {
namespace Mud {

RegisterLogModule("Mud");

MudProcessor::MudProcessor(Instance &aInstance)
    : InstanceLocator(aInstance)
#if OPENTHREAD_FTD
    , mMudSocket(aInstance, nullptr, this)
#endif
{
#if OPENTHREAD_FTD
    if (mMudSocket.Open() != kErrorNone) {
        LogWarn("failed to open MUD socket on MLE router");
    }
#endif
    mMudUrl.Clear();
    mMudUrl.Append(CONFIG_OPENTHREAD_MUD_URL);
    LogInfo("Mud Processor initialized");
}

#if OPENTHREAD_FTD
Error MudProcessor::ProcessMudUrl(String<kMaxMudUrlLength> aMudUrl, Ip6::Address &aChildAddress) {
    Error            error          = kErrorNone;
    Message         *mudMessage     = nullptr;
    Ip6::Address     serverAddress;
    Ip6::MessageInfo messageInfo;

    if (!mMudSocket.IsOpen()) {
        ExitNow(error = kErrorInvalidState);
    }

    SuccessOrExit(error = FindMudForwarderIp(serverAddress));
    LogInfo("found mud forwarder ip: %s", serverAddress.ToString().AsCString());

    // Send UDP message with mudUrl and child external IP address to serveripaddress
    mudMessage = mMudSocket.NewMessage();

    LogInfo("Adding MUD URL: %s", aMudUrl.AsCString());
    LogInfo("Adding Child IP: %s", aChildAddress.ToString().AsCString());
    SuccessOrExit(error = Tlv::Append<MudUrlForwarderTlv>(*mudMessage, aMudUrl.AsCString()));
    SuccessOrExit(error = Tlv::Append<ChildIpTlv>(*mudMessage, aChildAddress.ToString().AsCString()));
    
    messageInfo.SetPeerPort(kMUDForwarderPort);
    messageInfo.SetPeerAddr(serverAddress);

    // Possible improvement: check for appropriate/default route in netdata
    SuccessOrExit(error = mMudSocket.SendTo(*mudMessage, messageInfo));
    LogInfo("MUD udp message is sent!");
    mudMessage = nullptr;

exit:
    if (mudMessage != nullptr)
    {
        mudMessage->Free();
    }

    return error;
}

Error MudProcessor::FindMudForwarderIp(Ip6::Address &serverAddress)
{
    Error                         error       = kErrorNone;
    String<kServiceNameMaxLength> serviceName;
    NetworkData::Iterator         iterator    = NetworkData::kIteratorInit;
    NetworkData::ServiceConfig    service;
    NetworkData::ServiceData      serviceData;
    NetworkData::ServerData       serverData;
    uint8_t                       serverDataLength;

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

bool MudProcessor::MatchesOmrPrefix(Ip6::Address aAddress)
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

bool MudProcessor::IsOmrPrefix(const NetworkData::OnMeshPrefixConfig &aPrefixConfig)
{
    // By spec: OMR prefix is identifiable with stable, on mesh, preferred, and SLAAC all true
    // For some reason, BorderRouter::RoutingManager::IsValidOmrPrefix does not check if mPreferred is true.
    return IsValidOmrPrefix(aPrefixConfig.GetPrefix()) && aPrefixConfig.mOnMesh && aPrefixConfig.mSlaac && aPrefixConfig.mStable && aPrefixConfig.mPreferred; 
}

bool MudProcessor::IsValidOmrPrefix(const Ip6::Prefix &aPrefix)
{
    return (aPrefix.GetLength() == kOmrPrefixLength) && !aPrefix.IsLinkLocal() && !aPrefix.IsMulticast();
}

void MudProcessor::HandleNotifierEvents(Events aEvents)
{
    if (aEvents.Contains(kEventIp6AddressAdded)) {
        LogInfo("notified of new IP6 Address");
        HandleNewIp6Address();
    }
}

void MudProcessor::HandleNewIp6Address() 
{
    LinkedList<Ip6::Netif::UnicastAddress> addresses;
    Error error = kErrorNone;

    addresses = Get<ThreadNetif>().GetUnicastAddresses();
    for (Ip6::Netif::UnicastAddress &address : addresses)
    {
        // TODO: track already notified IP addresses to save some bandwidth
        if (MatchesOmrPrefix(address.GetAddress())) {
            LogInfo("processing newly added address %s", address.GetAddress().ToString().AsCString());
            error = ProcessMudUrl(mMudUrl, address.GetAddress());
            LogWarnOnError(error, "failed to process MUD URL");
            return;
        }
    }
}

#endif // OPENTHREAD_FTD

} // Namespace Mud

} // Namespace ot

#endif // CONFIG_OPENTHREAD_MUD
