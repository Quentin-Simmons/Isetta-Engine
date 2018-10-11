/*
 * Copyright (c) 2018 Isetta
 */
#pragma once

#include <GLFW/glfw3.h>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>
#include "Core/IsettaAlias.h"
#include "Input/InputEnum.h"

namespace Isetta::Math {
class Vector2;
}

namespace Isetta {
class InputModule {
  using CBMap = std::unordered_map<int, std::list<std::pair<U64, Action<>>>>;

 public:
  /**
   * \brief Register a callback function to window close event
   * \param callback The callback function
   */
  void RegisterWindowCloseCallback(const Action<>& callback);
  U64 RegisterWindowSizeCallback(const Action<int, int>& callback);
  void UnegisterWindowSizeCallback(U64 handle);
  /**
   * \brief Check if the key is pressed
   * \param key The keycode to detect
   */
  bool IsKeyPressed(KeyCode key) const;
  /**
   * \brief Register a callback function to the key press event and return its
   * handle
   * \param key The keycode to detect
   * \param callback The callback function
   */
  U64 RegisterKeyPressCallback(KeyCode key, const Action<>& callback);
  /**
   * \brief Unregister a callback by the key and handle
   * \param key The key to detect
   * \param handle The handle to unregister
   */
  void UnregisterKeyPressCallback(KeyCode key, U64 handle);
  /**
   * \brief Register a callback function to the key release event and return its
   * handle
   * \param key The keycode to detect
   * \param callback The callback function
   */
  U64 RegisterKeyReleaseCallback(KeyCode key, const Action<>& callback);
  /**
   * \brief Unregister a callback by the key and handle
   * \param key The key to detect
   * \param handle The handle to unregister
   */
  void UnregisterKeyReleaseCallback(KeyCode key, U64 handle);
  /**
   * \brief Get the position of the mouse
   */
  Math::Vector2 GetMousePosition() const;
  /**
   * \brief Check if the mouse button is pressed
   * \param mouseButton The mouse button to detect
   */
  bool IsMouseButtonPressed(MouseButtonCode mouseButton) const;
  /**
   * \brief Register a callback function to the mouse press event and return its
   * handle
   * \param mouseButton The mouse button to detect
   * \param callback The callback function
   */
  U64 RegisterMousePressCallback(MouseButtonCode mouseButton,
                                 const Action<>& callback);
  /**
   * \brief Unregister a callback by the mouse button and handle
   * \param mouseButton The mouse button to detect
   * \param handle The handle to unregister
   */
  void UnregisterMousePressCallback(MouseButtonCode mouseButton, U64 handle);
  /**
   * \brief Register a callback function to the mouse release event and return
   * its handle
   * \param mouseButton The mouse button to detect
   * \param callback The callback function
   */
  U64 RegisterMouseReleaseCallback(MouseButtonCode mouseButton,
                                   const Action<>& callback);
  /**
   * \brief Unregister a callback by the mouse button and handle
   * \param mouseButton The mouse button to detect
   * \param handle The handle to unregister
   */
  void UnregisterMouseReleaseCallback(MouseButtonCode mouseButton, U64 handle);

  // TODO(Chaojie + Jacob): Talk about these, should unregister return bool?
  U64 RegisterMouseButtonCallback(
      const Action<GLFWwindow*, int, int, int>& callback);
  void UnregisterMouseButtonCallback(U64 handle);
  U64 RegisterKeyCallback(
      const Action<GLFWwindow*, int, int, int, int>& callback);
  void UnegisterKeyCallback(U64 handle);
  U64 RegisterScrollCallback(
      const Action<GLFWwindow*, double, double>& callback);
  void UnegisterScrollCallback(U64 handle);
  U64 RegisterCharCallback(const Action<GLFWwindow*, unsigned int>& callback);
  void UnegisterCharCallback(U64 handle);

 private:
  static GLFWwindow* winHandle;

  InputModule() = default;
  ~InputModule() = default;

  void StartUp(GLFWwindow* win);
  void Update(float deltaTime);
  void ShutDown();

  U64 RegisterCallback(int key, const Action<>& callback, CBMap* callbackMap);
  void UnregisterCallback(int key, U64 handle, CBMap* callbackMap);
  int KeyCodeToGlfwKey(KeyCode key) const;
  int MouseButtonToGlfwKey(MouseButtonCode mouseButton) const;

  static std::list<Action<>> windowCloseCallbacks;
  static void WindowCloseListener(GLFWwindow* win);

  static CBMap keyPressCallbacks;
  static CBMap keyReleaseCallbacks;
  static CBMap mousePressCallbacks;
  static CBMap mouseReleaseCallbacks;
  static void KeyEventListener(GLFWwindow* win, int key, int scancode,
                               int action, int mods);
  static void MouseEventListener(GLFWwindow* win, int button, int action,
                                 int mods);
  static void CharEventListener(GLFWwindow*, unsigned int c);
  static void ScrollEventListener(GLFWwindow*, double xoffset, double yoffset);
  static void WinSizeListener(GLFWwindow* win, int width, int height);

  static std::unordered_map<U64, Action<GLFWwindow*, int, int, int>>
      mouseButtonCallbacks;
  static std::unordered_map<U64, Action<GLFWwindow*, int, int, int, int>>
      keyCallbacks;
  static std::unordered_map<U64, Action<GLFWwindow*, double, double>>
      scrollCallbacks;
  static std::unordered_map<U64, Action<GLFWwindow*, unsigned int>>
      charCallbacks;
  static std::unordered_map<U64, Action<int, int>> winSizeCallbacks;

  // TODO(Chaojie) Discuss if one handle for all callbacks is enough
  // TODO(Jacob) U64 might be overkill? maybe just U16?
  static U64 totalHandle;

  friend class EngineLoop;
};
}  // namespace Isetta