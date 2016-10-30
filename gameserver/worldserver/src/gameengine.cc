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

#include "gameengine.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <utility>

#include "player.h"
#include "playerctrl.h"
#include "creature.h"
#include "creaturectrl.h"
#include "item.h"
#include "npcctrl.h"
#include "position.h"
#include "world.h"
#include "worldfactory.h"
#include "logger.h"

GameEngine::GameEngine(boost::asio::io_service* io_service,
                       const std::string& loginMessage,
                       const std::string& dataFilename,
                       const std::string& itemsFilename,
                       const std::string& worldFilename)
  : taskQueue_(io_service, std::bind(&GameEngine::onTask, this, std::placeholders::_1)),
    state_(INITIALIZED),
    loginMessage_(loginMessage),
    world_(WorldFactory::createWorld(dataFilename, itemsFilename, worldFilename))
{
}

GameEngine::~GameEngine()
{
  if (state_ == RUNNING)
  {
    stop();
  }
}

bool GameEngine::start()
{
  if (state_ == RUNNING)
  {
    LOG_ERROR("%s: GameEngine is already running", __func__);
    return false;
  }

  // Check so that world was created successfully
  if (!world_)
  {
    LOG_DEBUG("%s: World could not be loaded", __func__);
    return false;
  }

  state_ = RUNNING;
  return true;
}

bool GameEngine::stop()
{
  if (state_ == RUNNING)
  {
    state_ = CLOSING;
    return true;
  }
  else
  {
    LOG_ERROR("%s: GameEngine is already stopped", __func__);
    return false;
  }
}

CreatureId GameEngine::createPlayer(const std::string& name, const std::function<void(OutgoingPacket&&)>& sendPacket)
{
  // Create Player and PlayerCtrl here
  Player player{name};
  PlayerCtrl playerCtrl{world_.get(), player.getCreatureId(), sendPacket};

  auto creatureId = player.getCreatureId();

  players_.insert({creatureId, std::move(player)});
  playerCtrls_.insert({creatureId, std::move(playerCtrl)});

  return creatureId;
}

void GameEngine::playerSpawn(CreatureId creatureId)
{
  auto& player = getPlayer(creatureId);
  auto& playerCtrl = getPlayerCtrl(creatureId);

  LOG_DEBUG("%s: Spawn player: %s", __func__, player.getName().c_str());

  // adjustedPosition is the position where the creature actually spawned
  // i.e., if there is a creature already at the given position
  Position position(222, 222, 7);
  auto adjustedPosition = world_->addCreature(&player, &playerCtrl, position);
  if (adjustedPosition == Position::INVALID)
  {
    LOG_DEBUG("%s: Could not spawn player", __func__);
    // TODO(gurka): playerCtrl.disconnectPlayer();
    return;
  }
  playerCtrl.onPlayerSpawn(player, adjustedPosition, loginMessage_);
}

void GameEngine::playerDespawn(CreatureId creatureId)
{
  LOG_DEBUG("%s: Despawn player, creature id: %d", __func__, creatureId);
  world_->removeCreature(creatureId);

  // Remove Player and PlayerCtrl
  players_.erase(creatureId);
  playerCtrls_.erase(creatureId);
}

void GameEngine::playerMove(CreatureId creatureId, Direction direction)
{
  auto& playerCtrl = getPlayerCtrl(creatureId);
  auto nextWalkTime = playerCtrl.getNextWalkTime();
  auto now = boost::posix_time::ptime(boost::posix_time::microsec_clock::local_time());

  if (nextWalkTime <= now)
  {
    LOG_DEBUG("%s: Player move now, creature id: %d", __func__, creatureId);
    auto rc = world_->creatureMove(creatureId, direction);
    if (rc == World::ReturnCode::THERE_IS_NO_ROOM)
    {
      getPlayerCtrl(creatureId).sendCancel("There is no room.");
    }
  }
  else
  {
    LOG_DEBUG("%s: Player move delayed, creature id: %d", __func__, creatureId);
    auto creatureMoveFunc = [this, creatureId, direction]
    {
      auto rc = world_->creatureMove(creatureId, direction);
      if (rc == World::ReturnCode::THERE_IS_NO_ROOM)
      {
        getPlayerCtrl(creatureId).sendCancel("There is no room.");
      }
    };
    taskQueue_.addTask(creatureMoveFunc, nextWalkTime);
  }
}

void GameEngine::playerMovePath(CreatureId creatureId, const std::deque<Direction>& path)
{
  auto& playerCtrl = getPlayerCtrl(creatureId);
  playerCtrl.queueMoves(path);

  taskQueue_.addTask(std::bind(&GameEngine::playerMovePathStep, this, creatureId), playerCtrl.getNextWalkTime());
}

void GameEngine::playerMovePathStep(CreatureId creatureId)
{
  if (players_.count(creatureId) == 0)
  {
    // TODO(gurka): We might want to be able to tag all tasks, so that we can
    //              throw away all tasks that belongs to a player when the player disconnects
    LOG_DEBUG("%s: Player not found (disconnected while walking?)", __func__);
    return;
  }

  auto& playerCtrl = getPlayerCtrl(creatureId);

  // Make sure that the queued moves hasn't been canceled
  if (playerCtrl.hasQueuedMove())
  {
    LOG_DEBUG("%s: Player move, creature id: %d", __func__, creatureId);
    world_->creatureMove(creatureId, playerCtrl.getNextQueuedMove());
  }

  // Add a new task if there are more queued moves
  if (playerCtrl.hasQueuedMove())
  {
    taskQueue_.addTask(std::bind(&GameEngine::playerMovePathStep, this, creatureId), playerCtrl.getNextWalkTime());
  }
}

void GameEngine::playerCancelMove(CreatureId creatureId)
{
  LOG_DEBUG("%s: creature id: %d", __func__, creatureId);

  auto& playerCtrl = getPlayerCtrl(creatureId);
  if (playerCtrl.hasQueuedMove())
  {
    playerCtrl.cancelMove();
  }
}

void GameEngine::playerTurn(CreatureId creatureId, Direction direction)
{
  LOG_DEBUG("%s: Player turn, creature id: %d", __func__, creatureId);
  world_->creatureTurn(creatureId, direction);
}

void GameEngine::playerSay(CreatureId creatureId,
                           uint8_t type,
                           const std::string& message,
                           const std::string& receiver,
                           uint16_t channelId)
{
  LOG_DEBUG("%s: creatureId: %d, message: %s", __func__, creatureId, message.c_str());

  // Check if message is a command
  if (message.size() > 0 && message[0] == '/')
  {
    // Extract everything after '/'
    auto fullCommand = message.substr(1, std::string::npos);

    // Check if there is arguments
    auto space = fullCommand.find_first_of(' ');
    std::string command;
    std::string option;
    if (space == std::string::npos)
    {
      command = fullCommand;
      option = "";
    }
    else
    {
      command = fullCommand.substr(0, space);
      option = fullCommand.substr(space + 1);
    }

    // Check commands
    if (command == "debug" || command == "debugf")
    {
      // Different position for debug / debugf
      Position position;

      // Show debug info for a tile
      if (command == "debug")
      {
        // Show debug information on player tile
        position = world_->getCreaturePosition(creatureId);
      }
      else if (command == "debugf")
      {
        // Show debug information on tile in front of player
        const auto& player = getPlayer(creatureId);
        position = world_->getCreaturePosition(creatureId).addDirection(player.getDirection());
      }

      const auto& tile = world_->getTile(position);

      std::ostringstream oss;
      oss << "Position: " << position.toString() << "\n";

      for (const auto& item : tile.getItems())
      {
        oss << "Item: " << item.getItemId() << " (" << item.getName() << ")\n";
      }

      for (const auto& creatureId : tile.getCreatureIds())
      {
        oss << "Creature: " << creatureId << "\n";
      }

      getPlayerCtrl(creatureId).sendTextMessage(oss.str());
    }
    else if (command == "put")
    {
      std::istringstream iss(option);
      int itemId = 0;
      iss >> itemId;

      if (itemId < 100 || itemId > 2381)
      {
        getPlayerCtrl(creatureId).sendTextMessage("Invalid itemId");
      }
      else
      {
        const auto& player = getPlayer(creatureId);
        auto position = world_->getCreaturePosition(creatureId).addDirection(player.getDirection());
        world_->addItem(itemId, position);
      }
    }
    else
    {
      getPlayerCtrl(creatureId).sendTextMessage("Invalid command");
    }
  }
  else
  {
    world_->creatureSay(creatureId, message);
  }
}

void GameEngine::playerMoveItemFromPosToPos(CreatureId creatureId,
                                            const Position& fromPosition,
                                            int fromStackPos,
                                            int itemId,
                                            int count,
                                            const Position& toPosition)
{
  LOG_DEBUG("%s: Move Item from Tile to Tile, creature id: %d, from: %s, stackPos: %d, itemId: %d, count: %d, to: %s",
            __func__,
            creatureId,
            fromPosition.toString().c_str(),
            fromStackPos,
            itemId,
            count,
            toPosition.toString().c_str());

  World::ReturnCode rc = world_->moveItem(creatureId, fromPosition, fromStackPos, itemId, count, toPosition);

  switch (rc)
  {
    case World::ReturnCode::OK:
    {
      break;
    }

    case World::ReturnCode::CANNOT_MOVE_THAT_OBJECT:
    {
      getPlayerCtrl(creatureId).sendCancel("You cannot move that object.");
      break;
    }

    case World::ReturnCode::CANNOT_REACH_THAT_OBJECT:
    {
      getPlayerCtrl(creatureId).sendCancel("You are too far away.");
      break;
    }

    case World::ReturnCode::THERE_IS_NO_ROOM:
    {
      getPlayerCtrl(creatureId).sendCancel("There is no room.");
      break;
    }

    default:
    {
      // TODO(gurka): Disconnect player?
      break;
    }
  }
}

void GameEngine::playerMoveItemFromPosToInv(CreatureId creatureId,
                                            const Position& fromPosition,
                                            int fromStackPos,
                                            int itemId,
                                            int count,
                                            int toInventoryId)
{
  LOG_DEBUG("%s: Move Item from Tile to Inventory, creature id: %d, from: %s, stackPos: %d, itemId: %d, count: %d, toInventoryId: %d",
            __func__,
            creatureId,
            fromPosition.toString().c_str(),
            fromStackPos,
            itemId,
            count,
            toInventoryId);

  auto& player = getPlayer(creatureId);
  auto& playerCtrl = getPlayerCtrl(creatureId);

  // Check if the player can reach the fromPosition
  if (!world_->creatureCanReach(creatureId, fromPosition))
  {
    playerCtrl.sendCancel("You are too far away.");
    return;
  }

  // Get the Item from the position
  auto item = world_->getTile(fromPosition).getItem(fromStackPos);
  if (!item.isValid() || item.getItemId() != itemId)
  {
    LOG_ERROR("%s: Could not find Item with given itemId at fromPosition", __func__);
    return;
  }

  // Check if we can add the Item to that inventory slot
  auto& equipment = player.getEquipment();
  if (!equipment.canAddItem(item, toInventoryId))
  {
    playerCtrl.sendCancel("You cannot equip that object.");
    return;
  }

  // Remove the Item from the fromTile
  World::ReturnCode rc = world_->removeItem(itemId, count, fromPosition, fromStackPos);
  if (rc != World::ReturnCode::OK)
  {
    LOG_ERROR("%s: Could not remove item %d (count %d) from %s (stackpos: %d)",
              __func__,
              itemId,
              count,
              fromPosition.toString().c_str(),
              fromStackPos);
    // TODO(gurka): Disconnect player?
    return;
  }

  // Add the Item to the inventory
  equipment.addItem(item, toInventoryId);
  playerCtrl.onEquipmentUpdated(player, toInventoryId);
}

void GameEngine::playerMoveItemFromInvToPos(CreatureId creatureId,
                                            int fromInventoryId,
                                            int itemId,
                                            int count,
                                            const Position& toPosition)
{
  LOG_DEBUG("%s: Move Item from Inventory to Tile, creature id: %d, from: %d, itemId: %d, count: %d, to: %s",
            __func__,
            creatureId,
            fromInventoryId,
            itemId,
            count,
            toPosition.toString().c_str());

  auto& player = getPlayer(creatureId);
  auto& playerCtrl = getPlayerCtrl(creatureId);
  auto& equipment = player.getEquipment();

  // Check if there is an Item with correct itemId at the fromInventoryId
  auto item = equipment.getItem(fromInventoryId);
  if (!item.isValid() || item.getItemId() != itemId)
  {
    LOG_ERROR("%s: Could not find Item with given itemId at fromInventoryId", __func__);
    return;
  }

  // Check if the player can throw the Item to the toPosition
  if (!world_->creatureCanThrowTo(creatureId, toPosition))
  {
    playerCtrl.sendCancel("There is no room.");
    return;
  }

  // Remove the Item from the inventory slot
  if (!equipment.removeItem(item, fromInventoryId))
  {
    LOG_ERROR("%s: Could not remove item %d from inventory slot %d", __func__, itemId, fromInventoryId);
    return;
  }

  playerCtrl.onEquipmentUpdated(player, fromInventoryId);

  // Add the Item to the toPosition
  world_->addItem(item, toPosition);
}

void GameEngine::playerMoveItemFromInvToInv(CreatureId creatureId,
                                            int fromInventoryId,
                                            int itemId,
                                            int count,
                                            int toInventoryId)
{
  LOG_DEBUG("%s: Move Item from Inventory to Inventory, creature id: %d, from: %d, itemId: %d, count: %d, to: %d",
            __func__,
            creatureId,
            fromInventoryId,
            itemId,
            count,
            toInventoryId);

  auto& player = getPlayer(creatureId);
  auto& playerCtrl = getPlayerCtrl(creatureId);
  auto& equipment = player.getEquipment();

  // TODO(gurka): Count

  // Check if there is an Item with correct itemId at the fromInventoryId
  auto item = equipment.getItem(fromInventoryId);
  if (!item.isValid() || item.getItemId() != itemId)
  {
    LOG_ERROR("%s: Could not find Item with given itemId at fromInventoryId", __func__);
    return;
  }

  // Check if we can add the Item to the toInventoryId
  if (!equipment.canAddItem(item, toInventoryId))
  {
    playerCtrl.sendCancel("You cannot equip that object.");
    return;
  }

  // Remove the Item from the fromInventoryId
  if (!equipment.removeItem(item, fromInventoryId))
  {
    LOG_ERROR("%s: Could not remove item %d from inventory slot %d",
              __func__,
              itemId,
              fromInventoryId);
    return;
  }

  // Add the Item to the to-inventory slot
  equipment.addItem(item, toInventoryId);

  playerCtrl.onEquipmentUpdated(player, fromInventoryId);
  playerCtrl.onEquipmentUpdated(player, toInventoryId);
}

void GameEngine::playerUseInvItem(CreatureId creatureId,
                                  int itemId,
                                  int inventoryIndex)
{
  LOG_DEBUG("%s: Use Item in inventory, creature id: %d, itemId: %d, inventoryIndex: %d",
            __func__,
            creatureId,
            itemId,
            inventoryIndex);

  // TODO(gurka): Fix
  //  world_->useItem(creatureId, itemId, inventoryIndex);
}

void GameEngine::playerUsePosItem(CreatureId creatureId,
                                  int itemId,
                                  const Position& position,
                                  int stackPos)
{
  LOG_DEBUG("%s: Use Item at position, creature id: %d, itemId: %d, position: %s, stackPos: %d",
            __func__,
            creatureId,
            itemId,
            position.toString().c_str(),
            stackPos);

  // TODO(gurka): Fix
  //  world_->useItem(creatureId, itemId, position, stackPos);
}

void GameEngine::playerLookAtInvItem(CreatureId creatureId, int inventoryIndex, ItemId itemId)
{
  auto& player = getPlayer(creatureId);
  const auto& playerEquipment = player.getEquipment();

  if (!playerEquipment.hasItem(inventoryIndex))
  {
    LOG_DEBUG("%s: There is no item in inventoryIndex %u",
              __func__,
              inventoryIndex);
    return;
  }

  const auto& item = playerEquipment.getItem(inventoryIndex);

  if (item.getItemId() != itemId)
  {
    LOG_DEBUG("%s: Item at given inventoryIndex does not match given itemId, given itemId: %u inventory itemId: %u",
              __func__,
              itemId,
              item.getItemId());
    return;
  }

  if (!item.isValid())
  {
    LOG_DEBUG("%s: Item at given inventoryIndex is not valid", __func__);
    return;
  }

  std::ostringstream ss;

  if (!item.getName().empty())
  {
    if (item.isStackable() && item.getCount() > 1)
    {
      ss << "You see " << item.getCount() << " " << item.getName() << "s.";
    }
    else
    {
      ss << "You see a " << item.getName() << ".";
    }
  }
  else
  {
    ss << "You see an item with id " << itemId << ".";
  }

  if (item.hasAttribute("weight"))
  {
    ss << "\nIt weights " << item.getAttribute<float>("weight")<< " oz.";
  }

  if (item.hasAttribute("description"))
  {
    ss << "\n" << item.getAttribute<std::string>("description");
  }

  getPlayerCtrl(creatureId).sendTextMessage(ss.str());
}

void GameEngine::playerLookAtPosItem(CreatureId creatureId, const Position& position, ItemId itemId, int stackPos)
{
  std::ostringstream ss;

  const auto& tile = world_->getTile(position);

  if (itemId == 99)
  {
    const auto& creatureIds = tile.getCreatureIds();
    if (!creatureIds.empty())
    {
      const auto& creatureId = creatureIds.front();
      const auto& creature = world_->getCreature(creatureId);
      ss << "You see " << creature.getName() << ".";
    }
    else
    {
      LOG_DEBUG("%s: No Creatures at given position: %s", __func__, position.toString().c_str());
      return;
    }
  }
  else
  {
    Item item;

    for (const auto& tileItem : tile.getItems())
    {
      if (tileItem.getItemId() == itemId)
      {
        item = tileItem;
        break;
      }
    }

    if (!item.isValid())
    {
      LOG_DEBUG("%s: No Item with itemId %d at given position: %s", __func__, itemId, position.toString().c_str());
      return;
    }

    if (!item.getName().empty())
    {
      if (item.isStackable() && item.getCount() > 1)
      {
        ss << "You see " << item.getCount() << " " << item.getName() << "s.";
      }
      else
      {
        ss << "You see a " << item.getName() << ".";
      }
    }
    else
    {
      ss << "You see an item with id " << itemId << ".";
    }

    // TODO(gurka): Can only see weight if standing next to the item
    if (item.hasAttribute("weight"))
    {
      ss << "\nIt weights " << item.getAttribute<float>("weight")<< " oz.";
    }

    if (item.hasAttribute("description"))
    {
      ss << "\n" << item.getAttribute<std::string>("description");
    }
  }

  getPlayerCtrl(creatureId).sendTextMessage(ss.str());
}

void GameEngine::onTask(const TaskFunction& task)
{
  switch (state_)
  {
    case RUNNING:
    {
      task();
      break;
    }

    case CLOSING:
    {
      LOG_DEBUG("%s: State is CLOSING, not executing task.", __func__);
      // TODO(gurka): taskQueue_.stop();
      state_ = CLOSED;
      break;
    }

    case CLOSED:
    {
      LOG_DEBUG("%s: State is CLOSED, not executing task.", __func__);
      break;
    }

    default:
    {
      LOG_ERROR("%s: Unknown state: %d", state_, __func__);
      break;
    }
  }
}
