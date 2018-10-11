/*
 * Copyright (c) 2018 Isetta
 */

#include "Networking/NetworkingModule.h"
#include <stdexcept>
#include <string>
#include "Audio/AudioSource.h"
#include "Core/Config/Config.h"
#include "Core/Debug/Logger.h"
#include "Core/IsettaAlias.h"
#include "Graphics/AnimationComponent.h"
#include "Networking/NetworkManager.h"

// F Windows
#ifdef SendMessage
#undef SendMessage
#endif

namespace Isetta {

// Defining static variables
CustomAdapter NetworkingModule::NetworkAdapter;

int NetworkRegistry::count;
std::unordered_map<
    int, std::pair<unsigned long long, Func<yojimbo::Message*, void*>>>
    NetworkRegistry::factories;
std::unordered_map<int, Action<yojimbo::Client*, yojimbo::Message*>>
    NetworkRegistry::clientFuncs;
std::unordered_map<int, Action<int, yojimbo::Server*, yojimbo::Message*>>
    NetworkRegistry::serverFuncs;
std::unordered_map<const char*, int> tags;

/**
 * @brief Code-generated struct to be used for sending integer values across the
 * network.
 *
 */
RPC_MESSAGE_DEFINE(HandleMessage)
HandleMessage() { handle = 0; }

// TODO(Caleb): choose a more reasonable range for the int serialization
template <typename Stream>
bool Serialize(Stream& stream) {
  serialize_int(stream, handle, 0, 64);

  return true;
}

public:
int handle;

RPC_CLIENT_FUNC {
  HandleMessage* handleMessage = static_cast<HandleMessage*>(message);
  LOG(Debug::Channel::Networking, "Server sends handle #%d",
      handleMessage->handle);
  if (handleMessage->handle == 0) {
    LOG(Debug::Channel::Networking,
        "Server says we should play the animation!");
  }
  if (handleMessage->handle == 1) {
    LOG(Debug::Channel::Networking,
        "Server says we should stop the animation!");
  }
  if (handleMessage->handle == 2) {
    AudioSource audio = AudioSource();
    audio.SetAudioClip("gunshot.aiff");
    audio.Play(false, 1.f);
  }
}

RPC_SERVER_FUNC {
  HandleMessage* handleMessage = reinterpret_cast<HandleMessage*>(message);
  LOG(Debug::Channel::Networking, "Client %d sends handle #%d", clientIdx,
      handleMessage->handle);
  for (int i = 0; i < server->GetMaxClients(); i++) {
    if (!server->IsClientConnected(i)) {
      continue;
    }
    NetworkManager::SendHandleMessageFromServer(i, handleMessage->handle);
  }
}
RPC_MESSAGE_FINISH(HandleMessage, "HNDL")

/**
 * @brief Code-generated struct to be used for sending string messages across
 * the network.
 *
 */
RPC_MESSAGE_DEFINE(StringMessage)

StringMessage() { string = ""; }

// TODO(Caleb): choose a more reasonable range for the int serialization
template <typename Stream>
bool Serialize(Stream& stream) {
  serialize_string(stream, const_cast<char*>(string.c_str()), 512);

  return true;
}

public:
std::string string;

RPC_CLIENT_FUNC {
  StringMessage* stringMessage = static_cast<StringMessage*>(message);
  LOG(Debug::Channel::Networking, "Server says: %s",
      stringMessage->string.c_str());
}

RPC_SERVER_FUNC {
  StringMessage* stringMessage = reinterpret_cast<StringMessage*>(message);
  LOG(Debug::Channel::Networking, "Client %d says: %s", clientIdx,
      stringMessage->string.c_str());
}

RPC_MESSAGE_FINISH(StringMessage, "STRN")

void NetworkingModule::StartUp() {
  NetworkManager::networkingModule = this;

  if (!InitializeYojimbo()) {
    throw std::exception(
        "NetworkingModule::StartUp => Could not initialize yojimbo.");
  }
  srand((unsigned int)time(NULL));

  // TODO(Caleb): Figure out some more robust channel settings
  networkConfig.numChannels = 1;
  networkConfig.channel[0].type = yojimbo::CHANNEL_TYPE_UNRELIABLE_UNORDERED;
  networkConfig.timeout = 20;

  privateKey = new (MemoryManager::AllocOnStack(
      sizeof(U8) * Config::Instance().networkConfig.keyBytes.GetVal()))
      U8[Config::Instance().networkConfig.keyBytes.GetVal()];
  // TODO(Caleb): Need to do something more insightful with the private key
  // than all 0s
  memset(privateKey, 0, Config::Instance().networkConfig.keyBytes.GetVal());

  clientId = 0;
  yojimbo::random_bytes(reinterpret_cast<U8*>(&clientId), 8);

  void* memPointer =
      MemoryManager::AllocOnStack(networkConfig.clientMemory + 1_MB);
  clientAllocator = new (MemoryManager::AllocOnStack(sizeof(NetworkAllocator)))
      NetworkAllocator(memPointer, (Size)networkConfig.clientMemory + 1_MB);

  if (Config::Instance().networkConfig.runServer.GetVal()) {
    Size serverMemorySize =
        (networkConfig.serverPerClientMemory +
         networkConfig.serverGlobalMemory) *
        (Config::Instance().networkConfig.maxClients.GetVal() + 1);

    memPointer = MemoryManager::AllocOnStack(serverMemorySize);
    serverAllocator =
        new (MemoryManager::AllocOnStack(sizeof(NetworkAllocator)))
            NetworkAllocator(memPointer, serverMemorySize);
  }

  client = new (MemoryManager::AllocOnStack(sizeof(yojimbo::Client)))
      yojimbo::Client(
          clientAllocator,
          yojimbo::Address(
              Config::Instance().networkConfig.defaultClientIP.GetVal().c_str(),
              Config::Instance().networkConfig.clientPort.GetVal()),
          networkConfig, NetworkAdapter, clock.GetElapsedTime());

  clientSendBuffer =
      MemoryManager::NewArrOnFreeList<RingBuffer<yojimbo::Message*>>(
          Config::Instance().networkConfig.clientQueueSize.GetVal());
}

void NetworkingModule::Update(float deltaTime) {
  clock.UpdateTime();

  // Check for new connections
  PumpClientServerUpdate(clock.GetElapsedTime());

  // Send out our messages
  SendClientToServerMessages();
  if (server) {
    for (int i = 0; i < Config::Instance().networkConfig.maxClients.GetVal();
         i++) {
      SendServerToClientMessages(i);
    }
  }

  // Receive and process the messages from the other side
  PumpClientServerUpdate(clock.GetElapsedTime());

  if (client->IsConnected()) {
    ProcessServerToClientMessages();
  }

  if (server) {
    for (int i = 0; i < Config::Instance().networkConfig.maxClients.GetVal();
         i++) {
      ProcessClientToServerMessages(i);
    }
  }
}

void NetworkingModule::ShutDown() {
  try {
    Disconnect();
  } catch (std::exception e) {
  }

  try {
    CloseServer();
  } catch (std::exception e) {
  }

  ShutdownYojimbo();
}

// NOTE: Deletes the oldest message in the queue if the queue is
// overflowing--it would be nice to return it, but then we wouldn't
// be able to guarantee the memory is reused
void NetworkingModule::AddClientToServerMessage(yojimbo::Message* message) {
  if (clientSendBuffer->IsFull()) {
    client->ReleaseMessage(
        clientSendBuffer->Get());  // This may be horribly wrong
  }
  clientSendBuffer->Put(message);
}

// NOTE: Deletes the oldest message in the queue if the queue is
// overflowing--it would be nice to return it, but then we wouldn't
// be able to guarantee the memory is reused
void NetworkingModule::AddServerToClientMessage(int clientIdx,
                                                yojimbo::Message* message) {
  if (serverSendBufferArray[clientIdx].IsFull()) {
    server->ReleaseMessage(
        clientIdx,
        serverSendBufferArray[clientIdx]
            .Get());  // TODO(Caleb): This may be horribly wrong; test that
                      // ReleaseMessage works from source
  }
  serverSendBufferArray[clientIdx].Put(message);
}

void NetworkingModule::PumpClientServerUpdate(double time) {
  client->SendPackets();
  if (server) {
    server->SendPackets();
  }

  client->ReceivePackets();
  if (server) {
    server->ReceivePackets();
  }

  client->AdvanceTime(time);
  if (server) {
    server->AdvanceTime(time);
  }
}

void NetworkingModule::SendClientToServerMessages() {
  const int channelIdx = 0;  // TODO(Caleb): Upgrade the channel indexing
  while (!clientSendBuffer->IsEmpty()) {
    if (!client->CanSendMessage(channelIdx)) {
      break;
    }

    yojimbo::Message* message = clientSendBuffer->Get();
    client->SendMessage(channelIdx, message);  // bugged out
  }
}

void NetworkingModule::SendServerToClientMessages(int clientIdx) {
  const int channelIdx = 0;  // TODO(Caleb): Upgrade the channel indexing
  while (!serverSendBufferArray[clientIdx].IsEmpty()) {
    if (!server->CanSendMessage(clientIdx, channelIdx)) {
      break;
    }

    yojimbo::Message* message = serverSendBufferArray[clientIdx].Get();
    server->SendMessage(clientIdx, channelIdx, message);
  }
}

void NetworkingModule::ProcessClientToServerMessages(int clientIdx) {
  const int channelIdx = 0;  // TODO(Caleb): Upgrade the channel indexing
  for (;;) {
    yojimbo::Message* message = server->ReceiveMessage(clientIdx, channelIdx);

    if (!message) {
      break;
    }

    NetworkRegistry::ServerFunc(message->GetType())(clientIdx, server, message);

    server->ReleaseMessage(clientIdx, message);
  }
}

void NetworkingModule::ProcessServerToClientMessages() {
  const int channelIdx = 0;  // TODO(Caleb): Upgrade the channel indexing
  for (;;) {
    yojimbo::Message* message = client->ReceiveMessage(channelIdx);

    if (!message) {
      break;
    }

    NetworkRegistry::ClientFunc(message->GetType())(client, message);

    client->ReleaseMessage(message);
  }
}

void NetworkingModule::Connect(const char* serverAddress, int serverPort,
                               Action<bool> callback) {
  yojimbo::Address address(serverAddress, serverPort);
  if (!address.IsValid()) {
    return;
  }

  client->InsecureConnect(privateKey, clientId, address, callback);
}

void NetworkingModule::Disconnect() {
  if (!client->IsConnecting()) {
    return;
  } else if (!client->IsConnected()) {
    throw std::exception(
        "NetworkingModule::Disconnect => Cannot disconnect the client if it is "
        "not already connected.");
  }
  client->Disconnect();
}

void NetworkingModule::CreateServer(const char* address, int port) {
  if (server && server->IsRunning()) {
    throw std::exception(
        "NetworkingModule::CreateServer => Cannot create a server while one is "
        "already running.");
  }
  serverSendBufferArray =
      MemoryManager::NewArrOnFreeList<RingBuffer<yojimbo::Message*>>(
          Config::Instance().networkConfig.maxClients.GetVal());
  for (int i = 0; i < Config::Instance().networkConfig.maxClients.GetVal();
       i++) {
    serverSendBufferArray[i] = RingBuffer<yojimbo::Message*>(
        Config::Instance().networkConfig.serverQueueSizePerClient.GetVal());
  }

  serverAddress = yojimbo::Address(address, port);
  server = MemoryManager::NewOnFreeList<yojimbo::Server>(
      serverAllocator, privateKey, serverAddress, networkConfig,
      &NetworkingModule::NetworkAdapter, clock.GetElapsedTime());
  server->Start(Config::Instance().networkConfig.maxClients.GetVal());

  if (!server->IsRunning()) {
    throw std::exception(
        "NetworkingModule::CreateServer => Unable to run server.");
  }
}

void NetworkingModule::CloseServer() {
  if (!server || !server->IsRunning()) {
    throw std::exception(
        "NetworkingModule::CloseServer() Cannot close the server if it is not "
        "running.");
  }
  server->Stop();
  MemoryManager::FreeOnFreeList(server);
  MemoryManager::FreeOnFreeList(serverSendBufferArray);
}
}  // namespace Isetta
