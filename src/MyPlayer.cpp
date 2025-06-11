/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

enum MyPlayerAcoreString {
    HELLO_WORLD = 35410
};

class MyGlobal : GlobalScript {
private:
    static float getCorrectedChance(const Player *player, uint32 itemid, float inputChance) {

        int guid = player->GetGUID();
        std::string query =
                "SELECT `character_guid`,`slot_id`,`item_template_id` FROM `character_item_watchlist` WHERE `guid` = " +
                std::to_string(guid) + ";";
        QueryResult result = WorldDatabase.Query(query);

        float saved_chance{
                50.0f}; //TODO: replace with an actual stored value per player from its watchlist (e.g get from DB)
        if (saved_chance / 1.5 < inputChance) {
            saved_chance = saved_chance + inputChance;
        } else {
            saved_chance = saved_chance + saved_chance / 1.5f;
            //TODO: write this back to DB
        }
        return saved_chance;
    }

    static bool hasPlayerItemOnWatchlist(const Player *player, uint32 itemId) {

        if (itemId == 20) {
            return false;
        }

        return true;
    }

public:
    MyGlobal() : GlobalScript("MyGlobal") {}





    //TODO: to reset chance we could probably use sScriptMgr->OnBeforeDropAddItem(player, loot, rate, lootMode, item, store) to detect when we actually got it.

    bool OnItemRoll(const Player *player, LootStoreItem const *lootStoreItem, float &chance, Loot &loot,
                    const LootStore &lootStore) override {

        if (!player || !lootStoreItem)
            return true;

        uint32 itemId = lootStoreItem->itemid;
        float inputChance{chance};

        float saved_chance{0.0f};

        int guid = player->GetGUID();
        std::string query =
                "SELECT `slot_id`,`item_template_id`,'saved_chance' FROM `character_item_watchlist` WHERE `guid` = " +
                std::to_string(guid) + ";";
        QueryResult result = CharacterDatabase.Query(query);

        struct WatchlistEntry {
            uint32 itemTemplateId;
            float savedChance;
        };

        std::vector<std::optional<WatchlistEntry>> watchlist(5);

        if (result) {
            do {
                Field *fields = result->Fetch();
                uint32 slotId = fields[0].Get<uint32>();
                uint32 itemTemplateId = fields[1].Get<uint32>();
                float savedChance = fields[2].Get<float>();

                if (slotId >= watchlist.size())
                    watchlist.resize(slotId + 1);

                watchlist[slotId] = WatchlistEntry{itemTemplateId, savedChance};

            } while (result->NextRow());
        }

        auto it = std::find_if(watchlist.begin(), watchlist.end(),
                               [itemId](const std::optional<WatchlistEntry>& entry) {
                                   return entry && entry->itemTemplateId == itemId;
                               });

        if (it != watchlist.end()) {
            // Match found
            saved_chance = (*it)->savedChance;
        } else {
            return true; //No match found for this item on this players watchlist;
        }

        if (saved_chance / 1.5 < inputChance) {
            saved_chance = saved_chance + inputChance;
        } else {
            saved_chance = saved_chance + saved_chance / 1.5f;
            //TODO: write this back to DB
        }
        return saved_chance;

        float corrected_chance = getCorrectedChance(player, itemId, chance);
        chance = corrected_chance;

        std::ostringstream msg;
        std::string itemName = "Unknown";
        if (const ItemTemplate *itemTemplate = sObjectMgr->GetItemTemplate(itemId))
            itemName = itemTemplate->Name1;

        msg << "|cff00ff00[Roll Logger]|r "
            << "Player |cffffff00" << player->GetName() << "|r "
            << "is rolling for item |cffa335ee[" << itemName << "]|r "
            << "(ID: " << itemId << ", "
            << "chance: " << lootStoreItem->chance << ", "
            << "minCount: " << static_cast<int>(lootStoreItem->mincount) << ", "
            << "maxCount: " << static_cast<int>(lootStoreItem->maxcount) << ", "
            << "needs Quest: " << lootStoreItem->needs_quest << ", "
            << "has conditions: " << (lootStoreItem->conditions.empty() ? "no" : "yes") << ").";


        ChatHandler(player->GetSession()).SendSysMessage(msg.str().c_str());

        if (itemId == 17413) {
            chance = 100.0f;
            ChatHandler(player->GetSession()).SendSysMessage("detected potential codex loot, set drop chance to 100%");
        }

        return true;

    }
};

// Add player scripts
class MyPlayer : public PlayerScript {
public:
    MyPlayer() : PlayerScript("MyPlayer") {}

    void OnLogin(Player *player) override {
        ChatHandler(player->GetSession()).SendSysMessage("The This text should always be sent.");
        if (sConfigMgr->GetOption<bool>("MyModule.Enable", false))
            ChatHandler(player->GetSession()).PSendSysMessage(HELLO_WORLD);

    }

//    void OnItemRoll(Player *player, uint64 guid, uint32 itemId, RollVote vote) {
//        std::string itemName = "Unknown";
//        if (ItemTemplate const *itemTemplate = sObjectMgr->GetItemTemplate(itemId))
//            itemName = itemTemplate->Name1;
//
//        std::string voteStr;
//        switch (vote) {
//            case ROLL_NEED:
//                voteStr = "NEED";
//                break;
//            case ROLL_GREED:
//                voteStr = "GREED";
//                break;
//            case ROLL_DISENCHANT:
//                voteStr = "DISENCHANT";
//                break;
//            case ROLL_PASS:
//                voteStr = "PASS";
//                break;
//            default:
//                voteStr = "UNKNOWN";
//                break;
//        }
//
//        std::ostringstream msg;
//        msg << "|cff00ff00[Roll Logger]|r Player |cffffff00" << player->GetName()
//            << "|r voted |cffff0000" << voteStr
//            << "|r on item |cffa335ee[" << itemName
//            << "]|r (ID: " << itemId << ").";
//
//        ChatHandler(player->GetSession()).SendSysMessage(msg.str().c_str());
//    }
};

// Add all scripts in one
void AddMyPlayerScripts() {
    new MyPlayer();
    new MyGlobal();
}
