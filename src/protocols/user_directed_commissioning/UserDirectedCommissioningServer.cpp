/*
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *      This file implements an object for a CHIP Echo unsolicitied
 *      responder (server).
 *
 */

#include "UserDirectedCommissioning.h"

namespace chip {
namespace Protocols {
namespace UserDirectedCommissioning {

void UserDirectedCommissioningServer::OnMessageReceived(const Transport::PeerAddress & source, System::PacketBufferHandle && msgBuf)
{
    msgBuf->DebugDump("NEW UserDirectedCommissioningServer::OnMessageReceive");
    ChipLogProgress(AppServer, "NEW UserDirectedCommissioningServer::OnMessageReceived");

    PacketHeader packetHeader;

    ReturnOnFailure(packetHeader.DecodeAndConsume(msgBuf));

    if (packetHeader.GetFlags().Has(Header::FlagValues::kEncryptedMessage))
    {
        ChipLogError(AppServer, "UDC encryption flag set - ignoring");
        return;
    }

    // TODO: do we need these std::move calls?
    System::PacketBufferHandle && msg = std::move(msgBuf);

    PayloadHeader payloadHeader;
    ReturnOnFailure(payloadHeader.DecodeAndConsume(msg));

    System::PacketBufferHandle && payload = std::move(msg);

    char instanceName[chip::Mdns::kMaxInstanceNameSize + 1];
    int instanceNameLength =
        (payload->DataLength() > (chip::Mdns::kMaxInstanceNameSize)) ? chip::Mdns::kMaxInstanceNameSize : payload->DataLength();
    payload->Read((uint8_t *) instanceName, instanceNameLength);

    instanceName[instanceNameLength] = '\0';

    ChipLogProgress(AppServer, "UDC instance=%s", instanceName);

    UDCClientState * client = mUdcClients.FindUDCClientState(instanceName, nullptr);
    if (client == nullptr)
    {
        ChipLogProgress(AppServer, "UDC new instance state received");

        CHIP_ERROR err;
        err = mUdcClients.CreateNewUDCClientState(instanceName, &client);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(AppServer, "UDC error creating new connection state");
            return;
        }
        if (client == nullptr)
        {
            ChipLogError(AppServer, "UDC no memory");
            return;
        }

        // Call the registered InstanceNameResolver, if any.
        if (mInstanceNameResolver != nullptr)
        {
            mInstanceNameResolver->FindCommissionableNode(instanceName);
        }
        else
        {
            ChipLogError(AppServer, "UserDirectedCommissioningServer::OnMessageReceived no mInstanceNameResolver registered");
        }
    }

    mUdcClients.MarkUDCClientActive(client);
}

void UserDirectedCommissioningServer::SetUDCClientProcessingState(char * instanceName, UDCClientProcessingState state)
{
    UDCClientState * client = mUdcClients.FindUDCClientState(instanceName, nullptr);
    if (client == nullptr)
    {
        // printf("SetUDCClientProcessingState new instance state received\n");
        CHIP_ERROR err;
        err = mUdcClients.CreateNewUDCClientState(instanceName, &client);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(AppServer,
                         "UserDirectedCommissioningServer::SetUDCClientProcessingState error creating new connection state");
            return;
        }
        if (client == nullptr)
        {
            ChipLogError(AppServer, "UserDirectedCommissioningServer::SetUDCClientProcessingState no memory");
            return;
        }
    }

    ChipLogDetail(AppServer, "SetUDCClientProcessingState instance=%s new state=%d", instanceName, (int) state);

    client->SetUDCClientProcessingState(state);

    mUdcClients.MarkUDCClientActive(client);

    return;
}

void UserDirectedCommissioningServer::OnCommissionableNodeFound(const Mdns::DiscoveredNodeData & nodeData)
{
    UDCClientState * client = mUdcClients.FindUDCClientState(nodeData.instanceName, nullptr);
    if (client != nullptr && client->GetUDCClientProcessingState() == UDCClientProcessingState::kDiscoveringNode)
    {
        ChipLogDetail(AppServer, "OnCommissionableNodeFound instance: name=%s, expiration=%lu old_state=%d new_state=%d",
                      client->GetInstanceName(), client->GetExpirationTimeMs(), (int) client->GetUDCClientProcessingState(),
                      (int) UDCClientProcessingState::kPromptingUser);
        client->SetUDCClientProcessingState(UDCClientProcessingState::kPromptingUser);

        // Call the registered mUserConfirmationProvider, if any.
        if (mUserConfirmationProvider != nullptr)
        {
            mUserConfirmationProvider->OnUserDirectedCommissioningRequest(nodeData);
        }
    }
}

} // namespace UserDirectedCommissioning
} // namespace Protocols
} // namespace chip
