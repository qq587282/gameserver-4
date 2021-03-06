/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Simon Sandström
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef GAMEENGINE_EXPORT_PLAYER_CTRL_H_
#define GAMEENGINE_EXPORT_PLAYER_CTRL_H_

#include <string>
#include <vector>

#include "creature_ctrl.h"
#include "player.h"
#include "position.h"
#include "item.h"
#include "container.h"

class PlayerCtrl : public CreatureCtrl
{
 public:
  virtual ~PlayerCtrl() = default;

  // Called by GameEngine
  virtual CreatureId getPlayerId() const = 0;
  virtual void setPlayerId(CreatureId playerId) = 0;

  virtual void onEquipmentUpdated(const Player& player, int inventoryIndex) = 0;

  virtual void onOpenContainer(int clientContainerId, const Container& container, const Item& item) = 0;
  virtual void onCloseContainer(int clientContainerId) = 0;

  virtual void onContainerAddItem(int clientContainerId, const Item& item) = 0;
  virtual void onContainerUpdateItem(int clientContainerId, int containerSlot, const Item& item) = 0;
  virtual void onContainerRemoveItem(int clientContainerId, int containerSlot) = 0;

  virtual void sendTextMessage(int message_type, const std::string& message) = 0;

  virtual void sendCancel(const std::string& message) = 0;
  virtual void cancelMove() = 0;
};

#endif  // GAMEENGINE_EXPORT_PLAYER_CTRL_H_
