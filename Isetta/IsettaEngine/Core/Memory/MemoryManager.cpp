/*
 * Copyright (c) 2018 Isetta
 */
#include "Core/Memory/MemoryManager.h"
#include "Core/Config/Config.h"
#include "Core/DataStructures/Array.h"
#include "Core/Math/Random.h"
#include "Core/Memory/ObjectHandle.h"
#include "Util.h"

namespace Isetta {

MemoryManager* MemoryManager::instance;

void* MemoryManager::AllocOnSingleFrame(const Size size, const U8 alignment) {
  return GetInstance()->singleFrameAllocator.Alloc(size, alignment);
}

void* MemoryManager::AllocOnDoubleBuffered(const Size size,
                                           const U8 alignment) {
  return GetInstance()->doubleBufferedAllocator.Alloc(size, alignment);
}

void* MemoryManager::AllocOnStack(const Size size, const U8 alignment) {
  return GetInstance()->lsrAndLevelAllocator.Alloc(size, alignment);
}

void* MemoryManager::AllocOnFreeList(const Size size, const U8 alignment) {
  return GetInstance()->freeListAllocator.Alloc(size, alignment);
}

void* MemoryManager::ReallocOnFreeList(void* memPtr, const Size size,
                                       const U8 alignment) {
  return GetInstance()->freeListAllocator.Realloc(memPtr, size, alignment);
}

void MemoryManager::FreeOnFreeList(void* memPtr) {
  GetInstance()->freeListAllocator.Free(memPtr);
}

MemoryManager::MemoryManager() {
  // TODO(YIDI): The two allocators are still initialized here by automatically
  // calling their default constructors, find a better way to handle this
  instance = this;
}

void MemoryManager::StartUp() {
  MemoryConfig& configs = Config::Instance().memoryConfig;
  // CONFIG_VAL(memoryConfig.lsrAndLevelAllocatorSize)
  lsrAndLevelAllocator =
      StackAllocator(configs.lsrAndLevelAllocatorSize.GetVal());
  singleFrameAllocator =
      StackAllocator(configs.singleFrameAllocatorSize.GetVal());
  doubleBufferedAllocator =
      DoubleBufferedAllocator(configs.doubleBufferedAllocatorSize.GetVal());
  dynamicArena = MemoryArena(configs.dynamicArenaSize.GetVal());
  freeListAllocator = FreeListAllocator(configs.freeListAllocatorSize.GetVal());
}

// Memory Manager's update needs to be called after everything that need memory
// allocation
void MemoryManager::Update() {
  singleFrameAllocator.Clear();
  doubleBufferedAllocator.SwapBuffer();
  doubleBufferedAllocator.ClearCurrentBuffer();
  dynamicArena.Defragment();
}

void MemoryManager::ShutDown() {
  singleFrameAllocator.Erase();
  doubleBufferedAllocator.Erase();
  dynamicArena.Erase();
  lsrAndLevelAllocator.Erase();
  freeListAllocator.Erase();
}

void MemoryManager::FinishEngineStartupListener() {
  lvlMemStartMarker = lsrAndLevelAllocator.GetMarker();
}

void MemoryManager::ClearLevelMemory() {
  lsrAndLevelAllocator.FreeToMarker(lvlMemStartMarker);
}

MemoryManager* MemoryManager::GetInstance() {
  if (instance == nullptr) {
    throw std::exception{Util::StrFormat(
        "MemoryManager::GetInstance => instance doesn't exist, make sure you "
        "access memory manager after its initialization")};
  }
  return instance;
}

void MemoryManager::DefragmentTest() {
  const U32 count = 1024;
  Array<ObjectHandle<U64>> arr;

  for (U32 i = 0; i < count; i++) {
    auto ref = NewDynamic<U64>();
    *ref = i;
    arr.PushBack(ref);
  }

  auto map = instance->dynamicArena.addressIndexMap;

  for (U32 i = 0; i < count / 2; i++) {
    int index = Math::Random::GetRandomGenerator(0, arr.Size() - 1).GetValue();
    DeleteDynamic(arr[index]);
    arr.Erase(arr.begin() + index);
  }
}
}  // namespace Isetta
