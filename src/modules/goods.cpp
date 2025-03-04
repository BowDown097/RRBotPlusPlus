#include "goods.h"
#include "data/constants.h"
#include "data/responses.h"
#include "database/entities/dbuser.h"
#include "database/mongomanager.h"
#include "dppcmd/extensions/cache.h"
#include "dppinteract/interactiveservice.h"
#include "dppinteract/pagination/staticpaginator.h"
#include "entities/goods/ammo.h"
#include "entities/goods/crate.h"
#include "entities/goods/item.h"
#include "entities/goods/perk.h"
#include "entities/goods/tool.h"
#include "entities/goods/weapon.h"
#include "systems/itemsystem.h"
#include "utils/ld.h"
#include "utils/random.h"
#include "utils/timestamp.h"
#include <dpp/colors.h>
#include <dpp/dispatcher.h>

Goods::Goods() : dppcmd::module<Goods>("Goods", "Items, crates, and everything about 'em.")
{
    register_command(&Goods::buy, std::in_place, "buy", "Buy an item from the shop.", "$buy [item]");
    register_command(&Goods::daily, std::in_place, "daily", "Get a daily reward.");
    register_command(&Goods::discard, std::in_place, { "discard", "sell" }, "Toss an item you don't want anymore for some cash.");
    register_command(&Goods::itemInfo, std::in_place, "item", "View information on an item.", "$item [item]");
    register_command(&Goods::items, std::in_place, { "items", "inv", "inventory" }, "View your own or someone else's items.", "$items <user>");
    register_command(&Goods::open, std::in_place, { "open", "oc" }, "Open a crate.", "$open [crate]");
    register_command(&Goods::shop, std::in_place, "shop", "Check out what's available for purchase in the shop.");
    register_command(&Goods::use, std::in_place, "use", "Use a consumable.", "$use [consumable]");
}

dpp::task<dppcmd::command_result> Goods::buy(const dppcmd::remainder<std::string>& itemIn)
{
    if (const Item* item = ItemSystem::getItem(*itemIn))
    {
        if (item->name() == "Daily Crate")
            co_return dppcmd::command_result::from_error(Responses::CantBePurchased);

        auto member = dppcmd::find_guild_member_opt(context->msg.guild_id, context->msg.author.id);
        if (!member)
            co_return dppcmd::command_result::from_error(Responses::GetUserFailed);

        DbUser user = MongoManager::fetchUser(context->msg.author.id, context->msg.guild_id);

        dppcmd::command_result result;
        if (dynamic_cast<const Ammo*>(item) || dynamic_cast<const Consumable*>(item) || dynamic_cast<const Weapon*>(item))
            result = dppcmd::command_result::from_error(Responses::InCratesOnly);
        else if (dynamic_cast<const Collectible*>(item))
            result = dppcmd::command_result::from_error(Responses::CantBePurchased);
        else if (const Crate* crate = dynamic_cast<const Crate*>(item))
            result = co_await ItemSystem::buyCrate(*crate, member.value(), user, cluster);
        else if (const Perk* perk = dynamic_cast<const Perk*>(item))
            result = co_await ItemSystem::buyPerk(*perk, member.value(), user, cluster);
        else if (const Tool* tool = dynamic_cast<const Tool*>(item))
            result = co_await ItemSystem::buyTool(*tool, member.value(), user, cluster);

        MongoManager::updateUser(user);
        co_return result;
    }

    co_return dppcmd::command_result::from_error(Responses::NotAnItem);
}

dpp::task<dppcmd::command_result> Goods::daily()
{
    auto member = dppcmd::find_guild_member_opt(context->msg.guild_id, context->msg.author.id);
    if (!member)
        co_return dppcmd::command_result::from_error(Responses::GetUserFailed);

    auto dailyCrate = std::ranges::find_if(Constants::Crates, [](const Crate& c) { return c.name() == "Daily Crate"; });
    if (!dailyCrate)
        co_return dppcmd::command_result::from_error();

    DbUser user = MongoManager::fetchUser(context->msg.author.id, context->msg.guild_id);
    dppcmd::command_result result = co_await ItemSystem::buyCrate(*dailyCrate, member.value(), user, cluster, false);
    if (!result.success())
        co_return result;

    user.modCooldown(user.dailyCooldown = Constants::DailyCooldown, member.value());
    MongoManager::updateUser(user);
    co_return dppcmd::command_result::from_success(Responses::GotDailyCrate);
}

dpp::task<dppcmd::command_result> Goods::discard(const dppcmd::remainder<std::string>& itemIn)
{
    if (const Item* item = ItemSystem::getItem(*itemIn))
    {
        auto member = dppcmd::find_guild_member_opt(context->msg.guild_id, context->msg.author.id);
        if (!member)
            co_return dppcmd::command_result::from_error(Responses::GetUserFailed);

        DbUser user = MongoManager::fetchUser(context->msg.author.id, context->msg.guild_id);
        if (dynamic_cast<const Ammo*>(item) || dynamic_cast<const Consumable*>(item) || dynamic_cast<const Crate*>(item))
        {
            co_return dppcmd::command_result::from_error(Responses::CantBeDiscarded);
        }
        else if (const Collectible* collectible = dynamic_cast<const Collectible*>(item))
        {
            std::string itemName(item->name());
            if (user.collectibles[itemName] <= 0)
                co_return dppcmd::command_result::from_error(std::format(Responses::DontHaveAThing, item->name()));
            if (!collectible->discardable())
                co_return dppcmd::command_result::from_error(Responses::CantBeDiscarded);

            long double worth = item->worth() > 0 ? item->worth() : RR::utility::random(100.0L, 1500.0L);
            co_await user.setCashWithoutAdjustment(member.value(), user.cash + worth, cluster);

            user.collectibles[itemName]--;
            MongoManager::updateUser(user);

            co_return dppcmd::command_result::from_success(std::format(Responses::ItemDiscarded,
                item->name(), RR::utility::cash2str(worth)));
        }
        else if (dynamic_cast<const Perk*>(item))
        {
            if (item->name() != "Pacifist")
                co_return dppcmd::command_result::from_error(Responses::CanOnlyDiscardPacifist);
            if (auto it = user.perks.erase(user.perks.find("Pacifist")); it == user.perks.end())
                co_return dppcmd::command_result::from_error(std::format(Responses::DontHaveThing, "the Pacifist perk"));

            user.modCooldown(user.pacifistCooldown = Constants::PacifistCooldown, member.value());
            MongoManager::updateUser(user);
            co_return dppcmd::command_result::from_success(Responses::DiscardedPacifist);
        }
        else if (dynamic_cast<const Tool*>(item))
        {
            if (auto it = user.tools.erase(user.tools.find(item->name())); it == user.tools.end())
                co_return dppcmd::command_result::from_error(std::format(Responses::DontHaveAThing, item->name()));
            co_await user.setCashWithoutAdjustment(member.value(), user.cash + (item->worth() * 0.9L), cluster);
            MongoManager::updateUser(user);
            co_return dppcmd::command_result::from_success(std::format(Responses::ItemDiscarded,
                item->name(), RR::utility::cash2str(item->worth() * 0.9L)));
        }
        else if (dynamic_cast<const Weapon*>(item))
        {
            if (auto it = user.weapons.erase(user.weapons.find(item->name())); it == user.weapons.end())
                co_return dppcmd::command_result::from_error(std::format(Responses::DontHaveAThing, item->name()));
            co_await user.setCashWithoutAdjustment(member.value(), user.cash + 5000, cluster);
            MongoManager::updateUser(user);
            co_return dppcmd::command_result::from_success(std::format(Responses::ItemDiscarded,
                item->name(), RR::utility::cash2str(5000)));
        }
    }

    co_return dppcmd::command_result::from_error(Responses::NotAnItem);
}

dppcmd::command_result Goods::itemInfo(const dppcmd::remainder<std::string>& itemIn)
{
    if (const Item* item = ItemSystem::getItem(*itemIn))
    {
        dpp::embed embed = dpp::embed().set_color(dpp::colors::red);
        if (const Ammo* ammo = dynamic_cast<const Ammo*>(item))
        {
            embed.set_title(std::string(ammo->name()));
            std::vector<std::string_view> acceptedWeapons = Constants::Weapons
                | std::views::filter([ammo](const Weapon& w) { return w.ammo() == ammo->name(); })
                | std::views::transform([](const Weapon& w) { return w.name(); })
                | std::ranges::to<std::vector>();
            std::ranges::sort(acceptedWeapons);
            embed.add_field("Accepted By", dppcmd::utility::join(acceptedWeapons, ", "));
        }
        else if (const Collectible* collectible = dynamic_cast<const Collectible*>(item))
        {
            std::string worthDesc = collectible->worth() > 0
                ? RR::utility::cash2str(collectible->worth()) : "Some amount of money";
            embed.set_thumbnail(std::string(collectible->image()));
            embed.set_title(std::string(collectible->name()));
            embed.add_field("Description", std::string(collectible->description()), true);
            embed.add_field("Worth", worthDesc, true);
        }
        else if (const Consumable* con = dynamic_cast<const Consumable*>(item))
        {
            embed.set_title(std::string(con->name()));

            std::string description = std::format("ℹ️ {}\n⏱️ {}\n➕ {}\n➖ {}",
                con->information(), RR::utility::formatSeconds(con->duration()), con->posEffect(), con->negEffect());
            if (con->max() > 0)
                description += std::format("\n⚠️ {} max", con->max());

            embed.set_description(description);
        }
        else if (const Crate* crate = dynamic_cast<const Crate*>(item))
        {
            embed.set_title(std::string(crate->name()));
            if (crate->worth() > 0)
                embed.add_field("Worth", RR::utility::cash2str(crate->worth()), true);
            if (crate->cash() > 0)
                embed.add_field("Cash", RR::utility::cash2str(crate->cash()), true);
            if (crate->consumableCount() > 0)
                embed.add_field("Consumables", dppcmd::utility::lexical_cast<std::string>(crate->consumableCount()), true);
            if (crate->toolCount() > 0)
                embed.add_field("Tools", dppcmd::utility::lexical_cast<std::string>(crate->toolCount()), true);
        }
        else if (const Perk* perk = dynamic_cast<const Perk*>(item))
        {
            embed.set_title(std::string(perk->name()));
            embed.set_description(std::string(perk->description()));
            embed.add_field("Type", "Perk", true);
            embed.add_field("Worth", RR::utility::cash2str(perk->worth()), true);
            if (perk->duration() > 0)
                embed.add_field("Duration", RR::utility::formatSeconds(perk->duration()), true);
        }
        else if (const Tool* tool = dynamic_cast<const Tool*>(item))
        {
            embed.set_title(std::string(tool->name()));
            embed.add_field("Type", "Tool", true);
            embed.add_field("Worth", RR::utility::cash2str(tool->worth()), true);
            embed.add_field("Cash Range", tool->name().ends_with("Pickaxe")
                ? RR::utility::cash2str(128 * tool->mult()) + " - " + RR::utility::cash2str(256 * tool->mult())
                : RR::utility::cash2str(tool->genericMin()) + " - " + RR::utility::cash2str(tool->genericMax()),
                true);

            if (tool->tier() >= Tool::Tier::Netherite)
                embed.add_field("Additional Info", "Only obtainable from Diamond crates", true);
        }
        else if (const Weapon* weapon = dynamic_cast<const Weapon*>(item))
        {
            embed.set_title(std::string(weapon->name()));
            embed.set_description(std::string(weapon->information()));
            embed.add_field("Type", std::string(weapon->typeAsString()), true);
            embed.add_field("Accuracy", dppcmd::utility::lexical_cast<std::string>(weapon->accuracy()) + '%', true);
            embed.add_field("Ammo", std::string(weapon->ammo()), true);
            embed.add_field("Damage Range", std::format("{} - {}", weapon->damageMin(), weapon->damageMax()), true);
            embed.add_field("Drop Chance", dppcmd::utility::lexical_cast<std::string>(weapon->dropChance()) + '%', true);
            embed.add_field("Available In Crates", dppcmd::utility::join(weapon->insideCrates(), ", "), true);
        }

        context->reply(dpp::message(context->msg.channel_id, embed));
        return dppcmd::command_result::from_success();
    }

    return dppcmd::command_result::from_error(Responses::NotAnItem);
}

inline void createItemsPage(std::vector<dppinteract::interaction_page>& pages, std::string_view title,
                            std::ranges::range auto&& range)
{
    if (std::ranges::empty(range))
        return;
    pages.push_back(dppinteract::interaction_page()
        .set_color(dpp::colors::red)
        .set_title(title)
        .set_description(dppcmd::utility::join(range, '\n')));
}

dppcmd::command_result Goods::items(const std::optional<dpp::guild_member>& memberOpt)
{
    DbUser dbUser = MongoManager::fetchUser(memberOpt ? memberOpt->user_id : context->msg.author.id, context->msg.guild_id);

    auto applicableCollectibles = dbUser.collectibles
        | std::views::filter([](const auto& col) { return col.second > 0; })
        | std::views::transform([](const auto& col) { return std::format("{} ({}x)", col.first, col.second); });

    auto applicableConsumables = dbUser.consumables
        | std::views::filter([](const auto& con) { return con.second > 0; })
        | std::views::transform([](const auto& con) { return std::format("{} ({}x)", con.first, con.second); });

    auto applicableCrates = dbUser.crates
        | std::views::filter([](const auto& crate) { return crate.second > 0; })
        | std::views::transform([](const auto& crate) { return std::format("{} ({}x)", crate.first, crate.second); });

    auto applicablePerks = dbUser.perks
        | std::views::filter([](const auto& perk) { return perk.second > RR::utility::unixTimestamp(); })
        | std::views::transform([](const auto& perk) { return perk.first; });

    std::vector<dppinteract::interaction_page> pages;
    createItemsPage(pages, "Tools", dbUser.tools);
    createItemsPage(pages, "Weapons", dbUser.weapons);
    createItemsPage(pages, "Perks", applicablePerks);
    createItemsPage(pages, "Collectibles", applicableCollectibles);
    createItemsPage(pages, "Consumables", applicableConsumables);
    createItemsPage(pages, "Crates", applicableCrates);

    if (pages.empty())
    {
        return dppcmd::command_result::from_error(memberOpt
            ? std::format(Responses::UserHasNothing, memberOpt->get_mention())
            : Responses::YouHaveNothing);
    }

    auto paginator = std::make_unique<dppinteract::static_paginator>();
    paginator->with_default_buttons().add_user(context->msg.author.id).set_pages(pages);

    extra_data<dppinteract::interactive_service*>()->send_paginator(std::move(paginator), *context);
    return dppcmd::command_result::from_success();
}

dpp::task<dppcmd::command_result> Goods::open(const dppcmd::remainder<std::string>& crateIn)
{
    if (const Item* item = ItemSystem::getItem(*crateIn))
    {
        if (const Crate* crate = dynamic_cast<const Crate*>(item))
        {
            std::string crateName(crate->name());
            DbUser user = MongoManager::fetchUser(context->msg.author.id, context->msg.guild_id);
            if (user.crates[crateName] <= 0)
                co_return dppcmd::command_result::from_error(std::format(Responses::DontHaveAThing, crate->name()));

            user.crates[crateName]--;

            std::string description = "You got:";
            long double totalCash{};
            if (crate->cash() > 0)
            {
                description += std::format("\n**Cash** ({})", RR::utility::cash2str(crate->cash()));
                totalCash += crate->cash();
            }

            std::unordered_map<std::string, int> countableItemsMap;
            CrateDrop drop = crate->open(user);
            for (const Item* dropItem : drop.items)
            {
                if (const Ammo* ammo = dynamic_cast<const Ammo*>(dropItem))
                {
                    std::string ammoName(ammo->name());
                    user.ammo[ammoName]++;
                    countableItemsMap[ammoName]++;
                }
                else if (const Consumable* con = dynamic_cast<const Consumable*>(dropItem))
                {
                    std::string conName(con->name());
                    user.consumables[conName]++;
                    countableItemsMap[conName]++;
                }
                else if (const Tool* tool = dynamic_cast<const Tool*>(dropItem))
                {
                    user.tools.emplace(tool->name());
                    description += std::format("\n**{}**", tool->name());
                }
                else if (const Weapon* weapon = dynamic_cast<const Weapon*>(dropItem))
                {
                    user.weapons.emplace(weapon->name());
                    description += std::format("\n**{}**", weapon->name());
                }
            }

            for (const auto& [name, count] : countableItemsMap)
                description += std::format("\n**{}** ({}x)", name, count);

            if (drop.refund > 0)
            {
                description += std::format("\n*+{} refund from duplicate tools or weapons*", RR::utility::cash2str(drop.refund));
                totalCash += drop.refund;
            }

            if (totalCash > 0)
            {
                auto member = dppcmd::find_guild_member_opt(context->msg.guild_id, context->msg.author.id);
                if (!member)
                    co_return dppcmd::command_result::from_error(Responses::GetUserFailed);
                co_await user.setCashWithoutAdjustment(member.value(), user.cash + totalCash, cluster);
            }

            dpp::embed embed = dpp::embed()
                .set_color(dpp::colors::red)
                .set_title(crateName)
                .set_description(description);

            MongoManager::updateUser(user);
            context->reply(dpp::message(context->msg.channel_id, embed));
            co_return dppcmd::command_result::from_success();
        }

        co_return dppcmd::command_result::from_error(Responses::NotACrate);
    }

    co_return dppcmd::command_result::from_error(Responses::NotAnItem);
}

dppcmd::command_result Goods::shop()
{
    auto transformPerk = [](const Perk& p) {
        return std::format("**{}**: {}\nDuration: {}\nWorth: {}",
                           p.name(), p.description(),
                           RR::utility::formatSeconds(p.duration()),
                           RR::utility::cash2str(p.worth()));
    };

    auto crates = Constants::Crates
        | std::views::filter([](const Crate& c) { return c.name() != "Daily Crate"; })
        | std::views::transform([](const Crate& c) { return std::format("**{}**: {}", c.name(), RR::utility::cash2str(c.worth())); });
    auto perks = Constants::Perks | std::views::transform(transformPerk);
    auto tools = Constants::Tools
        | std::views::filter([](const Tool& t) { return t.tier() < Tool::Tier::Netherite; })
        | std::views::transform([](const Tool& t) { return std::format("**{}**: {}", t.name(), RR::utility::cash2str(t.worth())); });
    auto weapons = Constants::Weapons
        | std::views::transform([](const Weapon& w) { return std::format("**{}**: {}", w.name(), w.information()); });

    std::vector<dppinteract::interaction_page> pages;
    createItemsPage(pages, "Tools", tools);
    createItemsPage(pages, "Weapons", weapons);
    createItemsPage(pages, "Perks", perks);
    createItemsPage(pages, "Crates", crates);

    auto paginator = std::make_unique<dppinteract::static_paginator>();
    paginator->with_default_buttons().add_user(context->msg.author.id).set_pages(pages);

    extra_data<dppinteract::interactive_service*>()->send_paginator(std::move(paginator), *context);
    return dppcmd::command_result::from_success();
}

dpp::task<dppcmd::command_result> Goods::use(const dppcmd::remainder<std::string>& consumableIn)
{
    if (const Item* item = ItemSystem::getItem(*consumableIn))
    {
        if (const Consumable* con = dynamic_cast<const Consumable*>(item))
        {
            auto member = dppcmd::find_guild_member_opt(context->msg.guild_id, context->msg.author.id);
            if (!member)
                co_return dppcmd::command_result::from_error(Responses::GetUserFailed);

            std::string conName(con->name());
            DbUser user = MongoManager::fetchUser(context->msg.author.id, context->msg.guild_id);
            if (user.consumables[conName] <= 0)
                co_return dppcmd::command_result::from_error(std::format(Responses::DontHaveAnyThing, con->name()));
            if (con->max() > 0 && user.usedConsumables[conName] >= con->max())
                co_return dppcmd::command_result::from_error(std::format(Responses::UsedMaxConsumable, con->max(), con->name()));

            user.consumables[conName]--;
            user.usedConsumables[conName]++;

            std::string outcome;
            if (con->name() == "Black Hat")
            {
                outcome = co_await genericUse("Black Hat", user, member.value(), Responses::BlackHatSuccess,
                    Responses::BlackHatFail, user.blackHatEndTime = Constants::BlackHatDuration, 1.5L, 3.0L);
            }
            else if (con->name() == "Cocaine")
            {
                if (RR::utility::random(0.0, 1.0) < (1 - std::pow(0.95, user.usedConsumables["Cocaine"] + 1)))
                {
                    user.cocaineEndTime = 0;
                    user.consumables["Cocaine"] = 0;
                    user.cocaineRecoveryTime = RR::utility::unixTimestamp(3600 * user.usedConsumables["Cocaine"]);
                    outcome = std::format(Responses::CocaineFail, user.usedConsumables["Cocaine"]);
                    user.usedConsumables["Cocaine"] = 0;
                }
                else
                {
                    user.modCooldown(user.cocaineEndTime = Constants::CocaineDuration, member.value(), true, true, false);
                    for (const auto& [_, value] : user.constructCooldownMap())
                        if (int64_t cooldownSecs = value - RR::utility::unixTimestamp(); cooldownSecs > 0)
                            value = RR::utility::unixTimestamp(cooldownSecs * 0.9);

                    outcome = Responses::CocaineSuccess;
                }
            }
            else if (con->name() == "Ski Mask")
            {
                outcome = co_await genericUse("Ski Mask", user, member.value(), Responses::SkiMaskSuccess,
                    Responses::SkiMaskFail, user.skiMaskEndTime = Constants::SkiMaskDuration);
            }
            else if (con->name() == "Viagra")
            {
                outcome = co_await genericUse("Viagra", user, member.value(), Responses::ViagraSuccess,
                    Responses::ViagraFail, user.viagraEndTime = Constants::ViagraDuration);
            }

            MongoManager::updateUser(user);
            co_return dppcmd::command_result::from_success(outcome);
        }

        co_return dppcmd::command_result::from_error(Responses::NotAConsumable);
    }

    co_return dppcmd::command_result::from_error(Responses::NotAnItem);
}

dpp::task<std::string> Goods::genericUse(const std::string& con, DbUser& user, const dpp::guild_member& gm,
                                         std::string_view successMsg, std::string_view loseMsg, int64_t& cooldown,
                                         long double divMin, long double divMax)
{
    if (RR::utility::random(5) == 1)
    {
        user.consumables[con] = 0;
        user.usedConsumables[con] = 0;
        long double lostCash = user.cash / RR::utility::random(divMin, divMax);
        std::string lostCashStr = RR::utility::cash2str(lostCash);

        co_await user.setCashWithoutAdjustment(gm, user.cash - lostCash, cluster);
        co_return std::vformat(loseMsg, std::make_format_args(lostCashStr));
    }

    user.modCooldown(cooldown, gm);
    co_return std::string(successMsg);
}
