/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"


class ItemHunter : GlobalScript {

public:
    ItemHunter() : GlobalScript("ItemHunter") {}


    void OnBeforeDropAddItem(Player const *player, Loot &loot, bool canRate, uint16 lootMode,
                             LootStoreItem *lootStoreItem, LootStore const & /*store*/) override {
        if (!player || !lootStoreItem) {
            return;
        }

        int guid = player->GetGUID();
        uint32 itemId = lootStoreItem->itemid;

        std::string updateQuery = "UPDATE `character_item_watchlist` "
                                  "SET `saved_chance` = 0, `item_template_id` = 0 "
                                  " WHERE `character_guid` = " + std::to_string(guid) +
                                  " AND `item_template_id` = " + std::to_string(itemId) + ";";

        CharacterDatabase.AsyncQuery(
                updateQuery); //async is good enough here. (we value the lower performance impact more than making sure we don't drop an item multiple times by accident. (this would need two basically simultaneous kills/drops to happen)
    }


    bool OnItemRoll(const Player *player, LootStoreItem const *lootStoreItem, float &chance, Loot &loot,
                    const LootStore &lootStore) override {

        if (!player || !lootStoreItem)
            return true;

        uint32 itemId = lootStoreItem->itemid;
        float inputChance{chance};

        float saved_chance;

        if (const Group* group = player->GetGroup())
        {
            if (group->GetMembersCount() > 3)
                return true;
        }

        uint32 guid = player->GetGUID().GetCounter();
        std::string query = "SELECT `saved_chance` "
                            "FROM `character_item_watchlist` "
                            "WHERE `character_guid` = " + std::to_string(guid) +
                            " AND `item_template_id` = " + std::to_string(itemId) +
                            " LIMIT 1;";

        QueryResult result = CharacterDatabase.Query(query);

        if (result) {
            Field *fields = result->Fetch();
            saved_chance = fields[0].Get<float>();

        } else {
            return true; //we don't have item on this player-watchlist, so we stop hook execution.
        }
        float corrected_chance;
        if (saved_chance < 5.0f * inputChance) {
            corrected_chance = saved_chance + inputChance;
        } else {
            corrected_chance = saved_chance + (saved_chance * 0.8f + inputChance) / 2.0f;

            if (corrected_chance > 100.0f) {
                corrected_chance = 100.0f;
            }
        }

        chance = corrected_chance;

        std::ostringstream msg;
        std::string itemName = "Unknown";
        uint32 itemQuality{0};

        static const char* qualityColors[] =
                {
                        "|cff9d9d9d", // Poor       - 0
                        "|cffffffff", // Common     - 1
                        "|cff1eff00", // Uncommon  - 2
                        "|cff0070dd", // Rare      - 3
                        "|cffa335ee", // Epic      - 4
                        "|cffff8000", // Legendary - 5
                        "|cffe6cc80", // Artifact  - 6
                        "|cffa335ee", // Heirloom  - 7 (reuse epic color)
                };


        if (const ItemTemplate *itemTemplate = sObjectMgr->GetItemTemplate(itemId)){
            itemName = itemTemplate->Name1;
            itemQuality = itemTemplate->Quality;
        }

        std::string itemLink = std::string(qualityColors[itemQuality]) +
                               "|Hitem:" + std::to_string(itemId) + "::::::::::::|h[" + itemName + "]|h|r";


        msg << "|cffa335ee[Item hunter log]|r "
            << "Player " << player->GetName() << " "
            << "is rolling for item " << itemLink << " "
            << "(ID: " << itemId << ", "
            << "original chance: " << inputChance << ", "
            << "corrected chance: " << corrected_chance << ")";

        ChatHandler(player->GetSession()).SendSysMessage(msg.str().c_str());

        std::string updateQuery = "UPDATE `character_item_watchlist` "
                                  "SET `saved_chance` = " + std::to_string(corrected_chance) +
                                  " WHERE `character_guid` = " + std::to_string(guid) +
                                  " AND `item_template_id` = " + std::to_string(itemId) + ";";

        CharacterDatabase.AsyncQuery(updateQuery); //async query to lower performance impact if DB handler is busy

        return true;
    }
};

// Add all scripts in one
void AddItemHunterScripts() {
    new ItemHunter();
}
