/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Simon Sandström
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

#ifndef WORLDSERVER_PROTOCOL71_H_
#define WORLDSERVER_PROTOCOL71_H_

#include "protocol.h"

#include <cstdint>
#include <array>
#include <string>

#include "gameengineproxy.h"
#include "worldinterface.h"
#include "outgoingpacket.h"
#include "player.h"
#include "creature.h"
#include "position.h"
#include "item.h"
#include "server.h"

class GameEngineProxy;
class WorldInterface;
class AccountReader;

class Protocol71 : public Protocol
{
 public:
  Protocol71(const std::function<void(void)>& closeProtocol,
             GameEngineProxy* gameEngineProxy,
             WorldInterface* worldInterface,
             ConnectionId connectionId,
             Server* server,
             AccountReader* accountReader);

  // Called by WorldServer (from Protocol)
  void disconnected();

  // Called by Server (from Protocol)
  void parsePacket(IncomingPacket* packet);

  // Called by World (from CreatureCtrl)
  void onCreatureSpawn(const Creature& creature, const Position& position);
  void onCreatureDespawn(const Creature& creature, const Position& position, uint8_t stackPos);
  void onCreatureMove(const Creature& creature,
                      const Position& oldPosition,
                      uint8_t oldStackPos,
                      const Position& newPosition,
                      uint8_t newStackPos);
  void onCreatureTurn(const Creature& creature, const Position& position, uint8_t stackPos);
  void onCreatureSay(const Creature& creature, const Position& position, const std::string& message);

  void onItemRemoved(const Position& position, uint8_t stackPos);
  void onItemAdded(const Item& item, const Position& position);

  void onTileUpdate(const Position& position);

  // Called by GameEngine (from Protocol)
  void onPlayerSpawn(const Player& player, const Position& position, const std::string& loginMessage);
  void onEquipmentUpdated(const Player& player, int inventoryIndex);
  void onUseItem(const Item& item);
  void sendTextMessage(const std::string& message);
  void sendCancel(const std::string& message);

 private:
  bool isLoggedIn() const { return playerId_ != Creature::INVALID_ID; }
  bool isConnected() const { return server_ != nullptr; }

  // Helper functions for creating OutgoingPackets
  bool canSee(const Position& position) const;
  void addPosition(const Position& position, OutgoingPacket* packet) const;
  void addMapData(const Position& position, int width, int height, OutgoingPacket* packet);
  void addCreature(const Creature& creature, OutgoingPacket* packet);
  void addItem(const Item& item, OutgoingPacket* packet) const;
  void addEquipment(const Player& player, int inventoryIndex, OutgoingPacket* packet) const;

  // Functions to parse IncomingPackets
  void parseLogin(IncomingPacket* packet);
  void parseMoveClick(IncomingPacket* packet);
  void parseMoveItem(IncomingPacket* packet);
  void parseUseItem(IncomingPacket* packet);
  void parseLookAt(IncomingPacket* packet);
  void parseSay(IncomingPacket* packet);
  void parseCancelMove(IncomingPacket* packet);

  // Helper functions when parsing IncomingPackets
  Position getPosition(IncomingPacket* packet) const;

  std::function<void(void)> closeProtocol_;
  CreatureId playerId_;
  GameEngineProxy* gameEngineProxy_;
  WorldInterface* worldInterface_;
  ConnectionId connectionId_;
  Server* server_;
  AccountReader* accountReader_;

  std::array<CreatureId, 64> knownCreatures_;
};

#endif  // WORLDSERVER_PROTOCOL71_H_