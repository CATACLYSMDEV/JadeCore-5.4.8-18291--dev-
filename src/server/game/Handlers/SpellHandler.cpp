/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "DBCStores.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "GuildMgr.h"
#include "SpellMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "GameObjectAI.h"
#include "SpellAuraEffects.h"

void WorldSession::HandleClientCastFlags(WorldPacket& recvPacket, uint8 castFlags, SpellCastTargets& targets)
{
}

void WorldSession::HandleUseItemOpcode(WorldPacket& recvPacket)
{
    /// @todo add targets.read() check
    Player* pUser = _player;
    Unit* mover = _player->m_mover;

    // ignore for remote control state
    if (mover != pUser)
        return;

    uint8 bagIndex, slot;
    uint8 castCount = 0;
    uint8 castFlags = 0;
    uint32 spellId = 0;
    uint32 glyphIndex = 0;
    uint32 targetMask = 0;
    uint32 targetStringLength = 0;
    float elevation = 0.0f;
    float missileSpeed = 0.0f;
    ObjectGuid itemGuid = 0;
    ObjectGuid targetGuid = 0;
    ObjectGuid itemTargetGuid = 0;
    ObjectGuid destTransportGuid = 0;
    ObjectGuid srcTransportGuid = 0;
    Position srcPos;
    Position destPos;
    std::string targetString;

    // Movement data
    MovementInfo movementInfo;
    ObjectGuid movementTransportGuid = 0;
    ObjectGuid movementGuid = 0;
    bool hasTransport = false;
    bool hasTransportTime2 = false;
    bool hasTransportTime3 = false;
    bool hasFallData = false;
    bool hasFallDirection = false;
    bool hasTimestamp = false;
    bool hasSplineElevation = false;
    bool hasPitch = false;
    bool hasOrientation = false;
    bool hasUnkMovementField = false;
    uint32 unkMovementLoopCounter = 0;
    Unit* caster = mover;

    recvPacket >> slot >> bagIndex;

    bool hasElevation = !recvPacket.ReadBit();
    itemGuid[6] = recvPacket.ReadBit();
    bool hasTargetString = !recvPacket.ReadBit();
    itemGuid[1] = recvPacket.ReadBit();
    bool hasCastFlags = !recvPacket.ReadBit();
    bool hasDestLocation = recvPacket.ReadBit();
    itemGuid[2] = recvPacket.ReadBit();
    itemGuid[7] = recvPacket.ReadBit();
    itemGuid[0] = recvPacket.ReadBit();
    bool hasTargetMask = !recvPacket.ReadBit();
    bool hasMissileSpeed = !recvPacket.ReadBit();
    bool hasMovement = recvPacket.ReadBit();
    bool hasCastCount = !recvPacket.ReadBit();
    bool hasSpellId = !recvPacket.ReadBit();
    recvPacket.ReadBit();
    bool hasGlyphIndex = !recvPacket.ReadBit();
    recvPacket.ReadBit();
    itemGuid[4] = recvPacket.ReadBit();
    bool hasSrcLocation = recvPacket.ReadBit();
    itemGuid[3] = recvPacket.ReadBit();
    itemGuid[5] = recvPacket.ReadBit();
    uint8 researchDataCount = recvPacket.ReadBits(2);

    for (uint8 i = 0; i < researchDataCount; ++i)
        recvPacket.ReadBits(2);

    if (hasMovement)
    {
        hasPitch = !recvPacket.ReadBit();
        hasTransport = recvPacket.ReadBit();
        recvPacket.ReadBit();

        if (hasTransport)
        {
            movementTransportGuid[7] = recvPacket.ReadBit();
            movementTransportGuid[2] = recvPacket.ReadBit();
            movementTransportGuid[4] = recvPacket.ReadBit();
            movementTransportGuid[5] = recvPacket.ReadBit();
            movementTransportGuid[6] = recvPacket.ReadBit();
            movementTransportGuid[0] = recvPacket.ReadBit();
            movementTransportGuid[1] = recvPacket.ReadBit();
            hasTransportTime3 = recvPacket.ReadBit();
            movementTransportGuid[4] = recvPacket.ReadBit();
            hasTransportTime2 = recvPacket.ReadBit();
        }

        movementGuid[6] = recvPacket.ReadBit();
        movementGuid[2] = recvPacket.ReadBit();
        movementGuid[1] = recvPacket.ReadBit();
        unkMovementLoopCounter = recvPacket.ReadBits(22);
        recvPacket.ReadBit();
        bool hasMovementFlags2 = !recvPacket.ReadBit();
        hasFallData = recvPacket.ReadBit();
        movementGuid[5] = recvPacket.ReadBit();
        hasSplineElevation = !recvPacket.ReadBit();
        recvPacket.ReadBit();
        movementGuid[7] = recvPacket.ReadBit();
        movementGuid[0] = recvPacket.ReadBit();

        if (hasFallData)
            hasFallDirection = recvPacket.ReadBit();

        hasOrientation = !recvPacket.ReadBit();
        movementGuid[4] = recvPacket.ReadBit();
        movementGuid[3] = recvPacket.ReadBit();
        hasTimestamp = !recvPacket.ReadBit();
        hasUnkMovementField = !recvPacket.ReadBit();
        bool hasMovementFlags = !recvPacket.ReadBit();

        if (hasMovementFlags2)
            movementInfo.flags2 = recvPacket.ReadBits(13);

        if (hasMovement)
            movementInfo.flags = recvPacket.ReadBits(30);
    }

    if (hasSrcLocation)
    {
        srcTransportGuid[3] = recvPacket.ReadBit();
        srcTransportGuid[1] = recvPacket.ReadBit();
        srcTransportGuid[7] = recvPacket.ReadBit();
        srcTransportGuid[4] = recvPacket.ReadBit();
        srcTransportGuid[2] = recvPacket.ReadBit();
        srcTransportGuid[0] = recvPacket.ReadBit();
        srcTransportGuid[6] = recvPacket.ReadBit();
        srcTransportGuid[5] = recvPacket.ReadBit();
    }

    if (hasDestLocation)
    {
        destTransportGuid[2] = recvPacket.ReadBit();
        destTransportGuid[4] = recvPacket.ReadBit();
        destTransportGuid[1] = recvPacket.ReadBit();
        destTransportGuid[7] = recvPacket.ReadBit();
        destTransportGuid[6] = recvPacket.ReadBit();
        destTransportGuid[0] = recvPacket.ReadBit();
        destTransportGuid[3] = recvPacket.ReadBit();
        destTransportGuid[5] = recvPacket.ReadBit();
    }

    if (hasTargetString)
        targetStringLength = recvPacket.ReadBits(7);

    itemTargetGuid[1] = recvPacket.ReadBit();
    itemTargetGuid[0] = recvPacket.ReadBit();
    itemTargetGuid[5] = recvPacket.ReadBit();
    itemTargetGuid[3] = recvPacket.ReadBit();
    itemTargetGuid[6] = recvPacket.ReadBit();
    itemTargetGuid[4] = recvPacket.ReadBit();
    itemTargetGuid[7] = recvPacket.ReadBit();
    itemTargetGuid[2] = recvPacket.ReadBit();

    targetGuid[4] = recvPacket.ReadBit();
    targetGuid[5] = recvPacket.ReadBit();
    targetGuid[0] = recvPacket.ReadBit();
    targetGuid[1] = recvPacket.ReadBit();
    targetGuid[3] = recvPacket.ReadBit();
    targetGuid[7] = recvPacket.ReadBit();
    targetGuid[6] = recvPacket.ReadBit();
    targetGuid[2] = recvPacket.ReadBit();

    if (hasCastFlags)
        castFlags = recvPacket.ReadBits(5);

    if (hasTargetMask)
        targetMask = recvPacket.ReadBits(20);

    recvPacket.ReadByteSeq(itemGuid[0]);
    recvPacket.ReadByteSeq(itemGuid[5]);
    recvPacket.ReadByteSeq(itemGuid[6]);
    recvPacket.ReadByteSeq(itemGuid[3]);
    recvPacket.ReadByteSeq(itemGuid[4]);
    recvPacket.ReadByteSeq(itemGuid[2]);
    recvPacket.ReadByteSeq(itemGuid[1]);

    for (uint8 i = 0; i < researchDataCount; ++i)
    {
        recvPacket.read_skip<uint32>();
        recvPacket.read_skip<uint32>();
    }

    recvPacket.ReadByteSeq(itemGuid[7]);

    if (hasMovement)
    {
        for (uint8 i = 0; i != unkMovementLoopCounter; i++)
            recvPacket.read_skip<uint32>();

        if (hasTransport)
        {
            recvPacket >> movementInfo.t_pos.m_positionY;
            recvPacket >> movementInfo.t_pos.m_positionZ;
            recvPacket.ReadByteSeq(movementTransportGuid[1]);

            if (hasTransportTime3)
                recvPacket >> movementInfo.t_time3;

            recvPacket.ReadByteSeq(movementTransportGuid[7]);
            recvPacket.ReadByteSeq(movementTransportGuid[5]);
            recvPacket.ReadByteSeq(movementTransportGuid[2]);
            recvPacket.ReadByteSeq(movementTransportGuid[4]);
            recvPacket >> movementInfo.t_pos.m_positionX;
            movementInfo.t_pos.SetOrientation(recvPacket.read<float>());
            recvPacket.ReadByteSeq(movementTransportGuid[0]);
            recvPacket >> movementInfo.t_seat;
            recvPacket >> movementInfo.t_time;
            recvPacket.ReadByteSeq(movementTransportGuid[6]);
            recvPacket.ReadByteSeq(movementTransportGuid[3]);

            if (hasTransportTime2)
                recvPacket >> movementInfo.t_time2;
        }

        if (hasFallData)
        {
            recvPacket >> movementInfo.j_zspeed;

            if (hasFallDirection)
            {
                recvPacket >> movementInfo.j_sinAngle;
                recvPacket >> movementInfo.j_cosAngle;
                recvPacket >> movementInfo.j_xyspeed;
            }

            recvPacket >> movementInfo.fallTime;
        }

        recvPacket.ReadByteSeq(movementTransportGuid[3]);
        recvPacket.ReadByteSeq(movementTransportGuid[7]);
        recvPacket.ReadByteSeq(movementTransportGuid[6]);
        recvPacket.ReadByteSeq(movementTransportGuid[1]);
        recvPacket >> movementInfo.pos.m_positionY;

        if (hasSplineElevation)
            recvPacket >> movementInfo.splineElevation;

        if (hasUnkMovementField)
            recvPacket.read_skip<uint32>();

        if (hasOrientation)
            movementInfo.pos.SetOrientation(recvPacket.read<float>());

        recvPacket.ReadByteSeq(movementTransportGuid[2]);
        recvPacket >> movementInfo.pos.m_positionZ;

        if (hasTimestamp)
            recvPacket >> movementInfo.time;

        recvPacket >> movementInfo.pos.m_positionX;
        recvPacket.ReadByteSeq(movementTransportGuid[5]);
        recvPacket.ReadByteSeq(movementTransportGuid[0]);

        if (hasPitch)
            movementInfo.pitch = G3D::wrap(recvPacket.read<float>(), float(-M_PI), float(M_PI));

        recvPacket.ReadByteSeq(movementTransportGuid[4]);
    }

    if (hasDestLocation)
    {
        float x, y, z;

        recvPacket.ReadByteSeq(destTransportGuid[7]);
        recvPacket >> x;
        recvPacket.ReadByteSeq(destTransportGuid[0]);
        recvPacket.ReadByteSeq(destTransportGuid[6]);
        recvPacket.ReadByteSeq(destTransportGuid[1]);
        recvPacket.ReadByteSeq(destTransportGuid[3]);
        recvPacket >> y;
        recvPacket.ReadByteSeq(destTransportGuid[5]);
        recvPacket >> z;
        recvPacket.ReadByteSeq(destTransportGuid[4]);
        recvPacket.ReadByteSeq(destTransportGuid[2]);

        destPos.Relocate(x, y, z);
    }
    else
    {
        destTransportGuid = caster->GetTransGUID();

        if (destTransportGuid)
            destPos.Relocate(caster->GetTransOffsetX(), caster->GetTransOffsetY(), caster->GetTransOffsetZ(), caster->GetTransOffsetO());
        else
            destPos.Relocate(caster);
    }

    recvPacket.ReadByteSeq(targetGuid[6]);
    recvPacket.ReadByteSeq(targetGuid[7]);
    recvPacket.ReadByteSeq(targetGuid[2]);
    recvPacket.ReadByteSeq(targetGuid[0]);
    recvPacket.ReadByteSeq(targetGuid[3]);
    recvPacket.ReadByteSeq(targetGuid[4]);
    recvPacket.ReadByteSeq(targetGuid[1]);
    recvPacket.ReadByteSeq(targetGuid[5]);

    if (hasSrcLocation)
    {
        float x, y, z;

        recvPacket.ReadByteSeq(srcTransportGuid[7]);
        recvPacket >> x;
        recvPacket.ReadByteSeq(srcTransportGuid[1]);
        recvPacket.ReadByteSeq(srcTransportGuid[5]);
        recvPacket.ReadByteSeq(srcTransportGuid[4]);
        recvPacket >> z;
        recvPacket.ReadByteSeq(srcTransportGuid[6]);
        recvPacket.ReadByteSeq(srcTransportGuid[0]);
        recvPacket.ReadByteSeq(srcTransportGuid[3]);
        recvPacket >> y;
        recvPacket.ReadByteSeq(srcTransportGuid[2]);

        srcPos.Relocate(x, y, z);
    }
    else
    {
        srcTransportGuid = caster->GetTransGUID();
        if (srcTransportGuid)
            srcPos.Relocate(caster->GetTransOffsetX(), caster->GetTransOffsetY(), caster->GetTransOffsetZ(), caster->GetTransOffsetO());
        else
            srcPos.Relocate(caster);
    }

    if (hasSpellId)
        recvPacket >> spellId;

    recvPacket.ReadByteSeq(itemTargetGuid[1]);
    recvPacket.ReadByteSeq(itemTargetGuid[4]);
    recvPacket.ReadByteSeq(itemTargetGuid[3]);
    recvPacket.ReadByteSeq(itemTargetGuid[6]);
    recvPacket.ReadByteSeq(itemTargetGuid[2]);
    recvPacket.ReadByteSeq(itemTargetGuid[0]);
    recvPacket.ReadByteSeq(itemTargetGuid[7]);
    recvPacket.ReadByteSeq(itemTargetGuid[5]);

    if (hasTargetString)
        targetString = recvPacket.ReadString(targetStringLength);

    if (hasElevation)
        recvPacket >> elevation;

    if (hasGlyphIndex)
        recvPacket >> glyphIndex;

    if (hasMissileSpeed)
        recvPacket >> missileSpeed;

    if (hasCastCount)
        recvPacket >> castCount;

    if (glyphIndex >= MAX_GLYPH_SLOT_INDEX)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }

    Item* pItem = pUser->GetUseableItemByPos(bagIndex, slot);
    if (!pItem)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }

    if (pItem->GetGUID() != itemGuid)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }

    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_USE_ITEM packet, bagIndex: %u, slot: %u, castCount: %u, spellId: %u, Item: %u, glyphIndex: %u, data length = %i", bagIndex, slot, castCount, spellId, pItem->GetEntry(), glyphIndex, (uint32)recvPacket.size());

    ItemTemplate const* proto = pItem->GetTemplate();
    if (!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, NULL);
        return;
    }

    // some item classes can be used only in equipped state
    if (proto->InventoryType != INVTYPE_NON_EQUIP && !pItem->IsEquipped())
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItem, NULL);
        return;
    }

    InventoryResult msg = pUser->CanUseItem(pItem);
    if (msg != EQUIP_ERR_OK)
    {
        pUser->SendEquipError(msg, pItem, NULL);
        return;
    }

    // only allow conjured consumable, bandage, poisons (all should have the 2^21 item flag set in DB)
    if (proto->Class == ITEM_CLASS_CONSUMABLE && !(proto->Flags & ITEM_PROTO_FLAG_USEABLE_IN_ARENA) && pUser->InArena())
    {
        pUser->SendEquipError(EQUIP_ERR_NOT_DURING_ARENA_MATCH, pItem, NULL);
        return;
    }

    // don't allow items banned in arena
    if (proto->Flags & ITEM_PROTO_FLAG_NOT_USEABLE_IN_ARENA && pUser->InArena())
    {
        pUser->SendEquipError(EQUIP_ERR_NOT_DURING_ARENA_MATCH, pItem, NULL);
        return;
    }

    if (pUser->isInCombat())
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[i].SpellId))
            {
                if (!spellInfo->CanBeUsedInCombat())
                {
                    pUser->SendEquipError(EQUIP_ERR_NOT_IN_COMBAT, pItem, NULL);
                    return;
                }
            }
        }
    }

    // check also  BIND_WHEN_PICKED_UP and BIND_QUEST_ITEM for .additem or .additemset case by GM (not binded at adding to inventory)
    if (pItem->GetTemplate()->Bonding == BIND_WHEN_USE || pItem->GetTemplate()->Bonding == BIND_WHEN_PICKED_UP || pItem->GetTemplate()->Bonding == BIND_QUEST_ITEM)
    {
        if (!pItem->IsSoulBound())
        {
            pItem->SetState(ITEM_CHANGED, pUser);
            pItem->SetBinding(true);
        }
    }

    SpellCastTargets targets;
    HandleClientCastFlags(recvPacket, castFlags, targets);

    // Note: If script stop casting it must send appropriate data to client to prevent stuck item in gray state.
    if (!sScriptMgr->OnItemUse(pUser, pItem, targets))
    {
        // no script or script not process request by self
        pUser->CastItemUseSpell(pItem, targets, castCount, glyphIndex);
    }
}

void WorldSession::HandleOpenItemOpcode(WorldPacket& recvPacket)
{
    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_OPEN_ITEM packet, data length = %i", (uint32)recvPacket.size());

    Player* pUser = _player;

    // ignore for remote control state
    if (pUser->m_mover != pUser)
        return;

    uint8 bagIndex, slot;

    recvPacket >> bagIndex >> slot;

    sLog->outInfo(LOG_FILTER_NETWORKIO, "bagIndex: %u, slot: %u", bagIndex, slot);

    Item* item = pUser->GetItemByPos(bagIndex, slot);
    if (!item)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL);
        return;
    }

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        pUser->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, item, NULL);
        return;
    }

    // Verify that the bag is an actual bag or wrapped item that can be used "normally"
    if (!(proto->Flags & ITEM_PROTO_FLAG_OPENABLE) && !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_WRAPPED))
    {
        pUser->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, NULL);
        sLog->outError(LOG_FILTER_NETWORKIO, "Possible hacking attempt: Player %s [guid: %u] tried to open item [guid: %u, entry: %u] which is not openable!",
                pUser->GetName(), pUser->GetGUIDLow(), item->GetGUIDLow(), proto->ItemId);
        return;
    }

    // locked item
    uint32 lockId = proto->LockID;
    if (lockId)
    {
        LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

        if (!lockInfo)
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, item, NULL);
            sLog->outError(LOG_FILTER_NETWORKIO, "WORLD::OpenItem: item [guid = %u] has an unknown lockId: %u!", item->GetGUIDLow(), lockId);
            return;
        }

        // was not unlocked yet
        if (item->IsLocked())
        {
            pUser->SendEquipError(EQUIP_ERR_ITEM_LOCKED, item, NULL);
            return;
        }
    }

    if (item->HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAG_WRAPPED))// wrapped?
    {
        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GIFT_BY_ITEM);

        stmt->setUInt32(0, item->GetGUIDLow());

        PreparedQueryResult result = CharacterDatabase.Query(stmt);

        if (result)
        {
            Field* fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();
            uint32 flags = fields[1].GetUInt32();

            item->SetUInt64Value(ITEM_FIELD_GIFTCREATOR, 0);
            item->SetEntry(entry);
            item->SetUInt32Value(ITEM_FIELD_FLAGS, flags);
            item->SetState(ITEM_CHANGED, pUser);
        }
        else
        {
            sLog->outError(LOG_FILTER_NETWORKIO, "Wrapped item %u don't have record in character_gifts table and will deleted", item->GetGUIDLow());
            pUser->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            return;
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GIFT);

        stmt->setUInt32(0, item->GetGUIDLow());

        CharacterDatabase.Execute(stmt);
    }
    else
        pUser->SendLoot(item->GetGUID(), LOOT_CORPSE);
}

void WorldSession::HandleGameObjectUseOpcode(WorldPacket& recvData)
{
    ObjectGuid guid;

    guid[6] = recvData.ReadBit();
    guid[1] = recvData.ReadBit();
    guid[3] = recvData.ReadBit();
    guid[4] = recvData.ReadBit();
    guid[0] = recvData.ReadBit();
    guid[5] = recvData.ReadBit();
    guid[7] = recvData.ReadBit();
    guid[2] = recvData.ReadBit();

    recvData.ReadByteSeq(guid[0]);
    recvData.ReadByteSeq(guid[1]);
    recvData.ReadByteSeq(guid[6]);
    recvData.ReadByteSeq(guid[2]);
    recvData.ReadByteSeq(guid[3]);
    recvData.ReadByteSeq(guid[4]);
    recvData.ReadByteSeq(guid[5]);
    recvData.ReadByteSeq(guid[7]);

    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Recvd CMSG_GAMEOBJECT_USE Message [guid=%u]", GUID_LOPART(guid));

    // ignore for remote control state
    if (_player->m_mover != _player)
        return;

    if (GameObject* obj = GetPlayer()->GetMap()->GetGameObject(guid))
        obj->Use(_player);
}

void WorldSession::HandleGameobjectReportUse(WorldPacket& recvData)
{
    ObjectGuid guid;

	guid[5] = recvData.ReadBit();
	guid[3] = recvData.ReadBit();
	guid[1] = recvData.ReadBit();
	guid[4] = recvData.ReadBit();
	guid[6] = recvData.ReadBit();
	guid[7] = recvData.ReadBit();
	guid[2] = recvData.ReadBit();
	guid[0] = recvData.ReadBit();

	recvData.ReadByteSeq(guid[6]);
	recvData.ReadByteSeq(guid[1]);
	recvData.ReadByteSeq(guid[5]);
	recvData.ReadByteSeq(guid[3]);
	recvData.ReadByteSeq(guid[4]);
	recvData.ReadByteSeq(guid[0]);
	recvData.ReadByteSeq(guid[2]);
	recvData.ReadByteSeq(guid[7]);

    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Recvd CMSG_GAMEOBJECT_REPORT_USE Message [in game guid: %u]", GUID_LOPART(guid));

    // ignore for remote control state
    if (_player->m_mover != _player)
        return;

    GameObject* go = GetPlayer()->GetMap()->GetGameObject(guid);
    if (!go)
        return;

    if (!go->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
        return;

    if (go->AI()->GossipHello(_player))
        return;

    _player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_USE_GAMEOBJECT, go->GetEntry());
}

void WorldSession::HandleCastSpellOpcode(WorldPacket& recvPacket)
{
    // ignore for remote control state (for player case)
    Unit* mover = _player->m_mover;
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
    {
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    uint8 castCount = 0;
    uint8 castFlags = 0;
    uint32 spellId = 0;
    uint32 glyphIndex = 0;
    uint32 targetMask = 0;
    uint32 targetStringLength = 0;
    float elevation = 0.0f;
    float missileSpeed = 0.0f;
    ObjectGuid targetGuid = 0;
    ObjectGuid itemTargetGuid = 0;
    ObjectGuid destTransportGuid = 0;
    ObjectGuid srcTransportGuid = 0;
    Position srcPos;
    Position destPos;
    std::string targetString;

    // Movement data
    MovementInfo movementInfo;
    ObjectGuid movementTransportGuid = 0;
    ObjectGuid movementGuid = 0;
    bool hasTransport = false;
    bool hasTransportTime2 = false;
    bool hasTransportTime3 = false;
    bool hasFallData = false;
    bool hasFallDirection = false;
    bool hasTimestamp = false;
    bool hasSplineElevation = false;
    bool hasPitch = false;
    bool hasOrientation = false;
    bool hasUnkMovementField = false;
    uint32 unkMovementLoopCounter = 0;
    Unit* caster = mover;

    recvPacket.ReadBit(); // Fake bit
    bool hasTargetString = !recvPacket.ReadBit();
    recvPacket.ReadBit(); // Fake Bit
    bool hasCastCount = !recvPacket.ReadBit();
    bool hasSrcLocation = recvPacket.ReadBit();
    bool hasDestLocation = recvPacket.ReadBit();
    bool hasSpellId = !recvPacket.ReadBit();
    uint8 researchDataCount = recvPacket.ReadBits(2);
    bool hasTargetMask = !recvPacket.ReadBit();
    bool hasMissileSpeed = !recvPacket.ReadBit();

    for (uint8 i = 0; i < researchDataCount; ++i)
        recvPacket.ReadBits(2);

    bool hasGlyphIndex = !recvPacket.ReadBit();
    bool hasMovement = recvPacket.ReadBit();
    bool hasElevation = !recvPacket.ReadBit();
    bool hasCastFlags = !recvPacket.ReadBit();

    targetGuid[5] = recvPacket.ReadBit();
    targetGuid[4] = recvPacket.ReadBit();
    targetGuid[2] = recvPacket.ReadBit();
    targetGuid[7] = recvPacket.ReadBit();
    targetGuid[1] = recvPacket.ReadBit();
    targetGuid[6] = recvPacket.ReadBit();
    targetGuid[3] = recvPacket.ReadBit();
    targetGuid[0] = recvPacket.ReadBit();

    if (hasDestLocation)
    {
        destTransportGuid[1] = recvPacket.ReadBit();
        destTransportGuid[3] = recvPacket.ReadBit();
        destTransportGuid[5] = recvPacket.ReadBit();
        destTransportGuid[0] = recvPacket.ReadBit();
        destTransportGuid[2] = recvPacket.ReadBit();
        destTransportGuid[6] = recvPacket.ReadBit();
        destTransportGuid[7] = recvPacket.ReadBit();
        destTransportGuid[4] = recvPacket.ReadBit();
    }


    if (hasMovement)
    {
        unkMovementLoopCounter = recvPacket.ReadBits(22);
        recvPacket.ReadBit();
        movementGuid[4] = recvPacket.ReadBit();
        hasTransport = recvPacket.ReadBit();

        if (hasTransport)
        {
            hasTransportTime2 = recvPacket.ReadBit();
            movementTransportGuid[7] = recvPacket.ReadBit();
            movementTransportGuid[4] = recvPacket.ReadBit();
            movementTransportGuid[1] = recvPacket.ReadBit();
            movementTransportGuid[0] = recvPacket.ReadBit();
            movementTransportGuid[6] = recvPacket.ReadBit();
            movementTransportGuid[3] = recvPacket.ReadBit();
            movementTransportGuid[5] = recvPacket.ReadBit();
            hasTransportTime3 = recvPacket.ReadBit();
            movementTransportGuid[2] = recvPacket.ReadBit();
        }

        recvPacket.ReadBit();
        movementGuid[7] = recvPacket.ReadBit();
        hasOrientation = !recvPacket.ReadBit();
        movementGuid[6] = recvPacket.ReadBit();
        hasSplineElevation = !recvPacket.ReadBit();
        hasPitch = !recvPacket.ReadBit();
        movementGuid[0] = recvPacket.ReadBit();
        recvPacket.ReadBit();
        bool hasMovementFlags = !recvPacket.ReadBit();
        hasTimestamp = !recvPacket.ReadBit();
        hasUnkMovementField = !recvPacket.ReadBit();

        if (hasMovementFlags)
            movementInfo.flags = recvPacket.ReadBits(30);

        movementGuid[1] = recvPacket.ReadBit();
        movementGuid[3] = recvPacket.ReadBit();
        movementGuid[2] = recvPacket.ReadBit();
        movementGuid[5] = recvPacket.ReadBit();
        hasFallData = recvPacket.ReadBit();

        if (hasFallData)
            hasFallDirection = recvPacket.ReadBit();

        bool hasMovementFlags2 = !recvPacket.ReadBit();

        if (hasMovementFlags2)
            movementInfo.flags2 = recvPacket.ReadBits(13);
    }

    itemTargetGuid[1] = recvPacket.ReadBit();
    itemTargetGuid[0] = recvPacket.ReadBit();
    itemTargetGuid[7] = recvPacket.ReadBit();
    itemTargetGuid[4] = recvPacket.ReadBit();
    itemTargetGuid[6] = recvPacket.ReadBit();
    itemTargetGuid[5] = recvPacket.ReadBit();
    itemTargetGuid[3] = recvPacket.ReadBit();
    itemTargetGuid[2] = recvPacket.ReadBit();

    if (hasSrcLocation)
    {
        srcTransportGuid[4] = recvPacket.ReadBit();
        srcTransportGuid[5] = recvPacket.ReadBit();
        srcTransportGuid[3] = recvPacket.ReadBit();
        srcTransportGuid[0] = recvPacket.ReadBit();
        srcTransportGuid[7] = recvPacket.ReadBit();
        srcTransportGuid[1] = recvPacket.ReadBit();
        srcTransportGuid[6] = recvPacket.ReadBit();
        srcTransportGuid[2] = recvPacket.ReadBit();
    }

    if (hasTargetMask)
        targetMask = recvPacket.ReadBits(20);

    if (hasCastFlags)
        castFlags = recvPacket.ReadBits(5);

    if (hasTargetString)
        targetStringLength = recvPacket.ReadBits(7);

    for (uint8 i = 0; i < researchDataCount; ++i)
    {
        recvPacket.read_skip<uint32>();
        recvPacket.read_skip<uint32>();
    }

    if (hasMovement)
    {
        recvPacket >> movementInfo.pos.m_positionX;
        recvPacket.ReadByteSeq(movementGuid[0]);

        if (hasTransport)
        {
            recvPacket.ReadByteSeq(movementTransportGuid[2]);
            recvPacket >> movementInfo.t_seat;
            recvPacket.ReadByteSeq(movementTransportGuid[3]);
            recvPacket.ReadByteSeq(movementTransportGuid[7]);
            recvPacket >> movementInfo.t_pos.m_positionX;
            recvPacket.ReadByteSeq(movementTransportGuid[5]);

            if (hasTransportTime3)
                recvPacket >> movementInfo.t_time3;

            recvPacket >> movementInfo.t_pos.m_positionZ;
            recvPacket >> movementInfo.t_pos.m_positionY;

            recvPacket.ReadByteSeq(movementTransportGuid[6]);
            recvPacket.ReadByteSeq(movementTransportGuid[1]);
            movementInfo.t_pos.SetOrientation(recvPacket.read<float>());

            recvPacket.ReadByteSeq(movementTransportGuid[4]);

            if (hasTransportTime2)
                recvPacket >> movementInfo.t_time2;

            recvPacket.ReadByteSeq(movementTransportGuid[0]);
            recvPacket >> movementInfo.t_time;

        }

        recvPacket.ReadByteSeq(movementGuid[5]);

        if (hasFallData)
        {
            recvPacket >> movementInfo.fallTime;
            recvPacket >> movementInfo.j_zspeed;

            if (hasFallDirection)
            {
                recvPacket >> movementInfo.j_sinAngle;
                recvPacket >> movementInfo.j_xyspeed;
                recvPacket >> movementInfo.j_cosAngle;
            }
        }

        if (hasSplineElevation)
            recvPacket >> movementInfo.splineElevation;

        recvPacket.ReadByteSeq(movementGuid[6]);

        if (hasUnkMovementField)
            recvPacket.read_skip<uint32>();

        recvPacket.ReadByteSeq(movementGuid[4]);

        if (hasOrientation)
            movementInfo.pos.SetOrientation(recvPacket.read<float>());

        if (hasTimestamp)
            recvPacket >> movementInfo.time;

        recvPacket.ReadByteSeq(movementGuid[1]);

        if (hasPitch)
            movementInfo.pitch = G3D::wrap(recvPacket.read<float>(), float(-M_PI), float(M_PI));

        recvPacket.ReadByteSeq(movementGuid[3]);

        for (uint8 i = 0; i != unkMovementLoopCounter; i++)
            recvPacket.read_skip<uint32>();

        recvPacket >> movementInfo.pos.m_positionY;
        recvPacket.ReadByteSeq(movementGuid[7]);
        recvPacket >> movementInfo.pos.m_positionZ;
        recvPacket.ReadByteSeq(movementGuid[2]);
    }

    recvPacket.ReadByteSeq(itemTargetGuid[4]);
    recvPacket.ReadByteSeq(itemTargetGuid[2]);
    recvPacket.ReadByteSeq(itemTargetGuid[1]);
    recvPacket.ReadByteSeq(itemTargetGuid[5]);
    recvPacket.ReadByteSeq(itemTargetGuid[7]);
    recvPacket.ReadByteSeq(itemTargetGuid[3]);
    recvPacket.ReadByteSeq(itemTargetGuid[6]);
    recvPacket.ReadByteSeq(itemTargetGuid[0]);

    if (hasDestLocation)
    {
        float x, y, z;
        recvPacket.ReadByteSeq(destTransportGuid[2]);
        recvPacket >> x;
        recvPacket.ReadByteSeq(destTransportGuid[4]);
        recvPacket.ReadByteSeq(destTransportGuid[1]);
        recvPacket.ReadByteSeq(destTransportGuid[0]);
        recvPacket.ReadByteSeq(destTransportGuid[3]);
        recvPacket >> y;
        recvPacket.ReadByteSeq(destTransportGuid[7]);
        recvPacket >> z;
        recvPacket.ReadByteSeq(destTransportGuid[5]);
        recvPacket.ReadByteSeq(destTransportGuid[6]);
        destPos.Relocate(x, y, z);
    }
    else
    {
        destTransportGuid = caster->GetTransGUID();
        if (destTransportGuid)
            destPos.Relocate(caster->GetTransOffsetX(), caster->GetTransOffsetY(), caster->GetTransOffsetZ(), caster->GetTransOffsetO());
        else
            destPos.Relocate(caster);
    }

    recvPacket.ReadByteSeq(targetGuid[3]);
    recvPacket.ReadByteSeq(targetGuid[4]);
    recvPacket.ReadByteSeq(targetGuid[7]);
    recvPacket.ReadByteSeq(targetGuid[6]);
    recvPacket.ReadByteSeq(targetGuid[2]);
    recvPacket.ReadByteSeq(targetGuid[0]);
    recvPacket.ReadByteSeq(targetGuid[1]);
    recvPacket.ReadByteSeq(targetGuid[5]);

    if (hasSrcLocation)
    {
        float x, y, z;
        recvPacket >> y;
        recvPacket.ReadByteSeq(srcTransportGuid[5]);
        recvPacket.ReadByteSeq(srcTransportGuid[1]);
        recvPacket.ReadByteSeq(srcTransportGuid[7]);
        recvPacket.ReadByteSeq(srcTransportGuid[6]);
        recvPacket >> x;
        recvPacket.ReadByteSeq(srcTransportGuid[3]);
        recvPacket.ReadByteSeq(srcTransportGuid[2]);
        recvPacket.ReadByteSeq(srcTransportGuid[0]);
        recvPacket.ReadByteSeq(srcTransportGuid[4]);
        recvPacket >> z;
        srcPos.Relocate(x, y, z);
    }
    else
    {
        srcTransportGuid = caster->GetTransGUID();
        if (srcTransportGuid)
            srcPos.Relocate(caster->GetTransOffsetX(), caster->GetTransOffsetY(), caster->GetTransOffsetZ(), caster->GetTransOffsetO());
        else
            srcPos.Relocate(caster);
    }

    if (hasTargetString)
        targetString = recvPacket.ReadString(targetStringLength);

    if (hasMissileSpeed)
        recvPacket >> missileSpeed;

    if (hasElevation)
        recvPacket >> elevation;

    if (hasCastCount)
        recvPacket >> castCount;

    if (hasSpellId)
        recvPacket >> spellId;

    if (hasGlyphIndex)
        recvPacket >> glyphIndex;

    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: got cast spell packet, castCount: %u, spellId: %u, targetFlags: %u, data length = %u", castCount, spellId, castFlags, (uint32)recvPacket.size());
	
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        sLog->outError(LOG_FILTER_NETWORKIO, "WORLD: unknown spell id %u", spellId);
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }

    // Override spell Id, client send base spell and not the overrided id
    if (!spellInfo->OverrideSpellList.empty())
    {
        for (auto itr : spellInfo->OverrideSpellList)
        {
            if (_player->HasSpell(itr))
            {
                SpellInfo const* overrideSpellInfo = sSpellMgr->GetSpellInfo(itr);
                if (overrideSpellInfo)
                {
                    spellInfo = overrideSpellInfo;
                    spellId = itr;
                }
                break;
            }
        }
    }

    if (mover->GetTypeId() == TYPEID_PLAYER)
    {
        // not have spell in spellbook or spell passive and not casted by client
        if (!mover->ToPlayer()->HasActiveSpell(spellId) || spellInfo->IsPassive())
        {
            //cheater? kick? ban?
            recvPacket.rfinish(); // prevent spam at ignore packet
            return;
        }
    }
    else
    {
        // not have spell in spellbook or spell passive and not casted by client
        if ((mover->GetTypeId() == TYPEID_UNIT && !mover->ToCreature()->HasSpell(spellId)) || spellInfo->IsPassive())
        {
            //cheater? kick? ban?
            recvPacket.rfinish(); // prevent spam at ignore packet
            return;
        }
    }

    Unit::AuraEffectList swaps = mover->GetAuraEffectsByType(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS);
    Unit::AuraEffectList const& swaps2 = mover->GetAuraEffectsByType(SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2);
    if (!swaps2.empty())
        swaps.insert(swaps.end(), swaps2.begin(), swaps2.end());

    if (!swaps.empty())
    {
        for (Unit::AuraEffectList::const_iterator itr = swaps.begin(); itr != swaps.end(); ++itr)
        {
            if ((*itr)->IsAffectingSpell(spellInfo))
            {
                if (SpellInfo const* newInfo = sSpellMgr->GetSpellInfo((*itr)->GetAmount()))
                {
                    spellInfo = newInfo;
                    spellId = newInfo->Id;
                }
                break;
            }
        }
    }

    // Client is resending autoshot cast opcode when other spell is casted during shoot rotation
    // Skip it to prevent "interrupt" message
    if (spellInfo->IsAutoRepeatRangedSpell() && _player->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)
        && _player->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)->m_spellInfo == spellInfo)
    {
        recvPacket.rfinish();
        return;
    }

    // can't use our own spells when we're in possession of another unit,
    if (_player->isPossessing())
    {
        recvPacket.rfinish(); // prevent spam at ignore packet
        return;
    }
	
    // client provided targets
    SpellCastTargets targets;
	/*
    targets.Initialize(targetMask, targetGuid, itemTargetGuid);
    targets.SetElevation(elevation);
    targets.SetSpeed(speed);
    targets.Update(mover);
	*/
    // auto-selection buff level base at target level (in spellInfo)
    if (targets.GetUnitTarget())
    {
        SpellInfo const* actualSpellInfo = spellInfo->GetAuraRankForLevel(targets.GetUnitTarget()->getLevel());

        // if rank not found then function return NULL but in explicit cast case original spell can be casted and later failed with appropriate error message
        if (actualSpellInfo)
            spellInfo = actualSpellInfo;
    }

    // Custom MoP Script
    // Power Word : Solace - 129250 and Power Word : Insanity - 129249
    if (spellInfo->Id == 129250 && _player->GetShapeshiftForm() == FORM_SHADOW)
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(129249);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Aimed Shot - 19434 and Aimed Shot (for Master Marksman) - 82928
    else if (spellInfo->Id == 19434 && _player->HasAura(82926))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(82928);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Alter Time - 108978 and Alter Time (overrided) - 127140
    else if (spellInfo->Id == 108978 && _player->HasAura(110909))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(127140);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Fix Dark Soul for Destruction warlocks
    else if (spellInfo->Id == 113860 && _player->GetSpecializationId(_player->GetActiveSpec()) == SPEC_WARLOCK_DESTRUCTION)
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(113858);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Halo - 120517 and Halo - 120644 (shadow form)
    else if (spellInfo->Id == 120517 && _player->HasAura(15473))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(120644);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Consecration - 116467 and Consecration - 26573
    else if (spellInfo->Id == 116467)
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(26573);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Cascade (shadow) - 127632 and Cascade - 121135
    else if (spellInfo->Id == 121135 && _player->HasAura(15473))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(127632);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Zen Pilgrimage - 126892 and Zen Pilgrimage : Return - 126895
    else if (spellInfo->Id == 126892 && _player->HasAura(126896))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(126895);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Soul Swap - 86121 and Soul Swap : Exhale - 86213
    else if (spellInfo->Id == 86121 && _player->HasAura(86211))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(86213);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
            _player->RemoveAura(86211);
        }
    }
    // Mage Bomb - 125430 and  Living Bomb - 44457
    else if (spellInfo->Id == 125430 && _player->HasSpell(44457))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(44457);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Mage Bomb - 125430 and Frost Bomb - 112948
    else if (spellInfo->Id == 125430 && _player->HasSpell(112948))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(112948);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Mage Bomb - 125430 and  Nether Tempest - 114923
    else if (spellInfo->Id == 125430 && _player->HasSpell(114923))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(114923);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Evocation - 12051 and  Rune of Power - 116011
    else if (spellInfo->Id == 12051 && _player->HasSpell(116011))
    {
        SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(116011);
        if (newSpellInfo)
        {
            spellInfo = newSpellInfo;
            spellId = newSpellInfo->Id;
        }
    }
    // Frostbolt - 116 and Frostbolt - 126201 (heal for water elemental)
    else if (spellInfo->Id == 116 && targets.GetUnitTarget())
    {
        if (targets.GetUnitTarget()->GetOwner() && targets.GetUnitTarget()->GetOwner()->GetTypeId() == TYPEID_PLAYER && targets.GetUnitTarget()->GetOwner()->GetGUID() == _player->GetGUID())
        {
            SpellInfo const* newSpellInfo = sSpellMgr->GetSpellInfo(126201);
            if (newSpellInfo)
            {
                spellInfo = newSpellInfo;
                spellId = newSpellInfo->Id;
            }
        }
    }
    // Surging Mist - 116694 and Surging Mist - 116995
    // Surging Mist is instantly casted if player is channeling Soothing Mist
    else if (spellInfo->Id == 116694 && _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL) && _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->GetSpellInfo()->Id == 115175)
    {
        recvPacket.rfinish();
        _player->CastSpell(targets.GetUnitTarget(), 116995, true);
        _player->EnergizeBySpell(_player, 116995, 1, POWER_CHI);
        int32 powerCost = spellInfo->CalcPowerCost(_player, spellInfo->GetSchoolMask(), _player->GetSpellPowerEntryBySpell(spellInfo));
        _player->ModifyPower(POWER_MANA, -powerCost);
        return;
    }
    // Enveloping Mist - 124682 and Enveloping Mist - 132120
    // Enveloping Mist is instantly casted if player is channeling Soothing Mist
    else if (spellInfo->Id == 124682 && _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL) && _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->GetSpellInfo()->Id == 115175)
    {
        recvPacket.rfinish();
        _player->CastSpell(targets.GetUnitTarget(), 132120, true);
        int32 powerCost = spellInfo->CalcPowerCost(_player, spellInfo->GetSchoolMask(), _player->GetSpellPowerEntryBySpell(spellInfo));
        _player->ModifyPower(POWER_CHI, -powerCost);
        return;
    }

    Spell* spell = new Spell(mover, spellInfo, TRIGGERED_NONE, 0, false);
    spell->m_cast_count = castCount;                       // set count of casts
    spell->m_glyphIndex = glyphIndex;
    spell->prepare(&targets);
}

void WorldSession::HandleCancelCastOpcode(WorldPacket& recvPacket)
{
    uint32 spellId = 0;
    uint8 counter = 0;

    bool hasCounter = !recvPacket.ReadBit();
    bool hasSpellId = !recvPacket.ReadBit();

    recvPacket.FlushBits();
    
    if (hasSpellId)
        recvPacket >> spellId;

    if (hasCounter)
        recvPacket >> counter;

    if (_player->IsNonMeleeSpellCasted(false))
        _player->InterruptNonMeleeSpells(false, spellId, false);
}

void WorldSession::HandleCancelAuraOpcode(WorldPacket& recvPacket)
{
    uint32 spellId;
    recvPacket >> spellId;

    ObjectGuid guid;

    recvPacket.ReadBit(); // Fake Bit
    guid[6] = recvPacket.ReadBit();
    guid[5] = recvPacket.ReadBit();
    guid[1] = recvPacket.ReadBit();
    guid[0] = recvPacket.ReadBit();
    guid[4] = recvPacket.ReadBit();
    guid[3] = recvPacket.ReadBit();
    guid[2] = recvPacket.ReadBit();
    guid[7] = recvPacket.ReadBit();

    recvPacket.FlushBits();

    recvPacket.ReadByteSeq(guid[3]);
    recvPacket.ReadByteSeq(guid[2]);
    recvPacket.ReadByteSeq(guid[1]);
    recvPacket.ReadByteSeq(guid[0]);
    recvPacket.ReadByteSeq(guid[4]);
    recvPacket.ReadByteSeq(guid[7]);
    recvPacket.ReadByteSeq(guid[5]);
    recvPacket.ReadByteSeq(guid[6]);

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    // not allow remove spells with attr SPELL_ATTR0_CANT_CANCEL
    if (spellInfo->Attributes & SPELL_ATTR0_CANT_CANCEL)
        return;

    // channeled spell case (it currently casted then)
    if (spellInfo->IsChanneled())
    {
        if (Spell* curSpell = _player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (curSpell->m_spellInfo->Id == spellId)
                _player->InterruptSpell(CURRENT_CHANNELED_SPELL);
        return;
    }

    // non channeled case:
    // don't allow remove non positive spells
    // don't allow cancelling passive auras (some of them are visible)
    if (!spellInfo->IsPositive() || spellInfo->IsPassive())
        return;

    _player->RemoveOwnedAura(spellId, 0, 0, AURA_REMOVE_BY_CANCEL);
}

void WorldSession::HandlePetCancelAuraOpcode(WorldPacket& recvPacket)
{
    uint64 guid;
    uint32 spellId;

    recvPacket >> guid;
    recvPacket >> spellId;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        sLog->outError(LOG_FILTER_NETWORKIO, "WORLD: unknown PET spell id %u", spellId);
        return;
    }

    Creature* pet=ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!pet)
    {
        sLog->outError(LOG_FILTER_NETWORKIO, "HandlePetCancelAura: Attempt to cancel an aura for non-existant pet %u by player '%s'", uint32(GUID_LOPART(guid)), GetPlayer()->GetName());
        return;
    }

    if (pet != GetPlayer()->GetGuardianPet() && pet != GetPlayer()->GetCharm())
    {
        sLog->outError(LOG_FILTER_NETWORKIO, "HandlePetCancelAura: Pet %u is not a pet of player '%s'", uint32(GUID_LOPART(guid)), GetPlayer()->GetName());
        return;
    }

    if (!pet->isAlive())
    {
        pet->SendPetActionFeedback(FEEDBACK_PET_DEAD);
        return;
    }

    pet->RemoveOwnedAura(spellId, 0, 0, AURA_REMOVE_BY_CANCEL);

    pet->AddCreatureSpellCooldown(spellId);
}

void WorldSession::HandleCancelGrowthAuraOpcode(WorldPacket& /*recvPacket*/)
{
}

void WorldSession::HandleCancelAutoRepeatSpellOpcode(WorldPacket& /*recvPacket*/)
{
    // may be better send SMSG_CANCEL_AUTO_REPEAT?
    // cancel and prepare for deleting
    _player->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
}

void WorldSession::HandleCancelChanneling(WorldPacket& recvData)
{
    recvData.read_skip<uint32>();                          // spellid, not used

    // ignore for remote control state (for player case)
    Unit* mover = _player->m_mover;
    if (mover != _player && mover->GetTypeId() == TYPEID_PLAYER)
        return;

    mover->InterruptSpell(CURRENT_CHANNELED_SPELL);
}

void WorldSession::HandleTotemDestroyed(WorldPacket& recvPacket)
{
    // ignore for remote control state
    if (_player->m_mover != _player)
        return;

    uint8 slotId;
    uint64 unkSkip;

    recvPacket >> slotId;
    recvPacket >> unkSkip;

    ++slotId;
    if (slotId >= MAX_TOTEM_SLOT)
        return;

    if (!_player->m_SummonSlot[slotId])
        return;

    Creature* totem = GetPlayer()->GetMap()->GetCreature(_player->m_SummonSlot[slotId]);
    if (totem && totem->isTotem())
        totem->ToTotem()->UnSummon();
}

void WorldSession::HandleSelfResOpcode(WorldPacket& /*recvData*/)
{
    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_SELF_RES");                  // empty opcode

    if (_player->HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
        return; // silent return, client should display error by itself and not send this opcode

    if (_player->GetUInt32Value(PLAYER_SELF_RES_SPELL))
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(_player->GetUInt32Value(PLAYER_SELF_RES_SPELL));
        if (spellInfo)
            _player->CastSpell(_player, spellInfo, false, 0);

        _player->SetUInt32Value(PLAYER_SELF_RES_SPELL, 0);
    }
}

void WorldSession::HandleSpellClick(WorldPacket& recvData)
{
    ObjectGuid guid;
    
    guid[1] = recvData.ReadBit();
    guid[7] = recvData.ReadBit();
    guid[4] = recvData.ReadBit();
    guid[5] = recvData.ReadBit();
    guid[3] = recvData.ReadBit();
    guid[6] = recvData.ReadBit();
    guid[2] = recvData.ReadBit();
    recvData.ReadBit();
    guid[0] = recvData.ReadBit();

    recvData.ReadByteSeq(guid[0]);
    recvData.ReadByteSeq(guid[2]);
    recvData.ReadByteSeq(guid[3]);
    recvData.ReadByteSeq(guid[1]);
    recvData.ReadByteSeq(guid[4]);
    recvData.ReadByteSeq(guid[6]);
    recvData.ReadByteSeq(guid[5]);
    recvData.ReadByteSeq(guid[7]);

    // this will get something not in world. crash
    Creature* unit = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid);

    if (!unit)
        return;

    // TODO: Unit::SetCharmedBy: 28782 is not in world but 0 is trying to charm it! -> crash
    if (!unit->IsInWorld())
        return;

    unit->HandleSpellClick(_player);
}

void WorldSession::HandleMirrorImageDataRequest(WorldPacket& recvData)
{
    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_GET_MIRRORIMAGE_DATA");
    uint64 guid;
    uint32 displayId;
    recvData >> guid >> displayId;

    // Get unit for which data is needed by client
    Unit* unit = ObjectAccessor::GetObjectInWorld(guid, (Unit*)NULL);
    if (!unit)
        return;

    if (!unit->HasAuraType(SPELL_AURA_CLONE_CASTER))
        return;

    // Get creator of the unit (SPELL_AURA_CLONE_CASTER does not stack)
    Unit* creator = unit->GetAuraEffectsByType(SPELL_AURA_CLONE_CASTER).front()->GetCaster();
    if (!creator)
        return;

    if (creator->GetDarkSimulacrum())
        creator = creator->GetDarkSimulacrum();

    WorldPacket data(SMSG_MIRROR_IMAGE_DATA, 68);
    data << uint64(guid);
    data << uint32(creator->GetDisplayId());
    data << uint8(creator->getRace());
    data << uint8(creator->getGender());
    data << uint8(creator->getClass());

    if (creator->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = creator->ToPlayer();
        Guild* guild = NULL;

        if (uint32 guildId = player->GetGuildId())
            guild = sGuildMgr->GetGuildById(guildId);

        data << uint8(player->GetByteValue(PLAYER_FIELD_BYTES, 0));   // skin
        data << uint8(player->GetByteValue(PLAYER_FIELD_BYTES, 1));   // face
        data << uint8(player->GetByteValue(PLAYER_FIELD_BYTES, 2));   // hair
        data << uint8(player->GetByteValue(PLAYER_FIELD_BYTES, 3));   // haircolor
        data << uint8(player->GetByteValue(PLAYER_BYTES_2, 0)); // facialhair
        data << uint64(guild ? guild->GetGUID() : 0);

        static EquipmentSlots const itemSlots[] =
        {
            EQUIPMENT_SLOT_HEAD,
            EQUIPMENT_SLOT_SHOULDERS,
            EQUIPMENT_SLOT_BODY,
            EQUIPMENT_SLOT_CHEST,
            EQUIPMENT_SLOT_WAIST,
            EQUIPMENT_SLOT_LEGS,
            EQUIPMENT_SLOT_FEET,
            EQUIPMENT_SLOT_WRISTS,
            EQUIPMENT_SLOT_HANDS,
            EQUIPMENT_SLOT_BACK,
            EQUIPMENT_SLOT_TABARD,
            EQUIPMENT_SLOT_END
        };

        // Display items in visible slots
        for (EquipmentSlots const* itr = &itemSlots[0]; *itr != EQUIPMENT_SLOT_END; ++itr)
        {
            if (*itr == EQUIPMENT_SLOT_HEAD && player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM))
                data << uint32(0);
            else if (*itr == EQUIPMENT_SLOT_BACK && player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK))
                data << uint32(0);
            else if (Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, *itr))
                data << uint32(item->GetTemplate()->DisplayInfoID);
            else
                data << uint32(0);
        }
    }
    else
    {
        // Skip player data for creatures
        data << uint8(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
    }

    SendPacket(&data);
}

void WorldSession::HandleUpdateProjectilePosition(WorldPacket& recvPacket)
{
    sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_UPDATE_PROJECTILE_POSITION");

    uint64 casterGuid;
    uint32 spellId;
    uint8 castCount;
    float x, y, z;    // Position of missile hit

    recvPacket >> casterGuid;
    recvPacket >> spellId;
    recvPacket >> castCount;
    recvPacket >> x;
    recvPacket >> y;
    recvPacket >> z;

    Unit* caster = ObjectAccessor::GetUnit(*_player, casterGuid);
    if (!caster)
        return;

    Spell* spell = caster->FindCurrentSpellBySpellId(spellId);
    if (!spell || !spell->m_targets.HasDst())
        return;

    Position pos = *spell->m_targets.GetDstPos();
    pos.Relocate(x, y, z);
    spell->m_targets.ModDst(pos);

    WorldPacket data(SMSG_SET_PROJECTILE_POSITION, 21);
    data << uint64(casterGuid);
    data << uint8(castCount);
    data << float(x);
    data << float(y);
    data << float(z);
    caster->SendMessageToSet(&data, true);
}