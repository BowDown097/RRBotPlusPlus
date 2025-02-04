#include "config.h"
#include "data/responses.h"
#include "database/entities/config/dbconfigchannels.h"
#include "database/entities/config/dbconfigmisc.h"
#include "database/entities/config/dbconfigranks.h"
#include "database/entities/config/dbconfigroles.h"
#include "database/mongomanager.h"
#include "dppcmd/services/moduleservice.h"
#include "dppcmd/utils/join.h"
#include "dppcmd/utils/strings.h"
#include "utils/ld.h"
#include "utils/strings.h"
#include <dpp/cache.h>
#include <dpp/colors.h>
#include <dpp/dispatcher.h>
#include <format>
#include <regex>

Config::Config() : dppcmd::module<Config>("Config", "This is where all the BORING administration stuff goes. Here, you can change how the bot does things in the server in a variety of ways. Huge generalization, but that's the best I can do.")
{
    register_command(&Config::addRank, std::in_place, "addrank", "Register a rank, its level, and the money required to get it.", "$addrank [level] [cost] [role]");
    register_command(&Config::clearConfig, std::in_place, "clearconfig", "Clear all configuration for this server.");
    register_command(&Config::currentConfig, std::in_place, "currentconfig", "List the current configuration for this server.");
    register_command(&Config::disableCommand, std::in_place, "disablecmd", "Disable a command for this server.", "$disablecmd [command]");
    register_command(&Config::disableFiltersInChannel, std::in_place, "disablefiltersinchannel", "Disable filters for a specific channel.", "$disablefiltersinchannel [channel]");
    register_command(&Config::disableModule, std::in_place, "disablemodule", "Disable a module for this server.", "$disablemodule [module]");
    register_command(&Config::enableCommand, std::in_place, "enablecmd", "Enable a previously disabled command.", "$enablecmd [command]");
    register_command(&Config::enableModule, std::in_place, "enablemodule", "Enable a previously disabled module.", "$enablemodule [module]");
    register_command(&Config::filterTerm, std::in_place, "filterterm", "Add a term to the filter system. Must be alphanumeric, including hyphens and spaces.", "$filterterm [term]");
    register_command(&Config::setAdminRole, std::in_place, "setadminrole", "Register a role that can use commands in the Administration and Config modules.", "$setadminrole [role]");
    register_command(&Config::setDjRole, std::in_place, "setdjrole", "Register a role as the DJ role, which is required for some of the music commands.", "$setdjrole [role]");
    register_command(&Config::setLogsChannel, std::in_place, "setlogschannel", "Register a channel for logs to be posted in.", "$setlogschannel [channel]");
    register_command(&Config::setModRole, std::in_place, "setmodrole", "Register a role that can use commands in the Moderation module.", "$setmodrole [role]");
    register_command(&Config::setPotChannel, std::in_place, "setpotchannel", "Register a channel for pot winnings to be announced in.", "$setpotchannel [channel]");
    register_command(&Config::toggleDrops, std::in_place, "toggledrops", "Toggles random drops, such as Bank Cheques.");
    register_command(&Config::toggleInviteFilter, std::in_place, "toggleinvitefilter", "Toggle the invite filter.");
    register_command(&Config::toggleNsfw, std::in_place, "togglensfw", "Enable age-restricted content to be played with the music feature.");
    register_command(&Config::toggleScamFilter, std::in_place, "togglescamfilter", "Toggle the scam filter.");
    register_command(&Config::unfilterTerm, std::in_place, "unfilterterm", "Remove a term from the filter system.", "$unfilterterm [term]");
    register_command(&Config::unwhitelistChannel, std::in_place, "unwhitelistchannel", "Remove a channel from the bot command whitelist.", "$unwhitelistchannel [channel]");
    register_command(&Config::whitelistChannel, std::in_place, "whitelistchannel", "Add a channel to a list of whitelisted channels for bot commands. All administration, moderation, and music commands will still work in every channel.", "$whitelistchannel [channel]");
}

dppcmd::command_result Config::addRank(int level, long double cost, dpp::role* role)
{
    DbConfigRanks ranks = MongoManager::fetchRankConfig(context->msg.guild_id);
    ranks.costs.emplace(level, cost);
    ranks.ids.emplace(level, role->id);

    MongoManager::updateRankConfig(ranks);
    return dppcmd::command_result::from_success(std::format(Responses::AddedRank, role->get_mention(), level, RR::utility::cash2str(cost)));
}

dppcmd::command_result Config::clearConfig()
{
    MongoManager::deleteChannelConfig(context->msg.guild_id);
    MongoManager::deleteMiscConfig(context->msg.guild_id);
    MongoManager::deleteRankConfig(context->msg.guild_id);
    MongoManager::deleteRoleConfig(context->msg.guild_id);
    return dppcmd::command_result::from_success(Responses::ClearedConfig);
}

dppcmd::command_result Config::currentConfig()
{
    DbConfigChannels channels = MongoManager::fetchChannelConfig(context->msg.guild_id);
    auto noFilterChannels = channels.noFilterChannels | std::views::transform(dpp::utility::channel_mention);
    auto whitelisted = channels.whitelistedChannels | std::views::transform(dpp::utility::channel_mention);

    std::string description = "***Channels***\n";
    description += std::format("Command Whitelisted Channels: {}\n", dppcmd::utility::join(whitelisted, ", "));
    description += std::format("Logs Channel: {}\n", dpp::channel::get_mention(channels.logsChannel));
    description += std::format("No Filter Channels: {}\n", dppcmd::utility::join(noFilterChannels, ", "));
    description += std::format("Pot Channel: {}\n", dpp::channel::get_mention(channels.potChannel));

    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    description += "***Miscellaneous***\n";
    description += std::format("Disabled Commands: {}\n", dppcmd::utility::join(misc.disabledCommands, ", "));
    description += std::format("Disabled Modules: {}\n", dppcmd::utility::join(misc.disabledModules, ", "));
    description += std::format("Filtered Terms: {}\n", dppcmd::utility::join(misc.filteredTerms, ", "));
    description += std::format("Invite Filter Enabled: {}\n", misc.inviteFilterEnabled);
    description += std::format("NSFW Enabled: {}\n", misc.nsfwEnabled);
    description += std::format("Scam Filter Enabled: {}\n", misc.scamFilterEnabled);

    DbConfigRanks ranks = MongoManager::fetchRankConfig(context->msg.guild_id);
    description += "***Ranks***\n";
    if (!ranks.costs.empty())
    {
        for (const auto& [level, cost] : std::ranges::to<std::map>(ranks.costs))
        {
            description += std::format("Level {}: {} - {}\n",
                level, dpp::role::get_mention(ranks.ids[level]), RR::utility::cash2str(cost));
        }
    }
    else
    {
        description += "None\n";
    }

    DbConfigRoles roles = MongoManager::fetchRoleConfig(context->msg.guild_id);
    description += "***Roles***\n";
    description += std::format("Admin Role: {}\n", dpp::role::get_mention(roles.staffLvl2Role));
    description += std::format("DJ Role: {}\n", dpp::role::get_mention(roles.djRole));
    description += std::format("Moderator Role: {}", dpp::role::get_mention(roles.staffLvl1Role));

    dpp::embed embed = dpp::embed()
        .set_color(dpp::colors::red)
        .set_title("Current Configuration")
        .set_description(description);

    context->reply(dpp::message(context->msg.channel_id, embed));
    return dppcmd::command_result::from_success();
}

dppcmd::command_result Config::disableCommand(const std::string& cmd)
{
    if (dppcmd::utility::iequals(cmd, "disablecmd") || dppcmd::utility::iequals(cmd, "enablecmd"))
        return dppcmd::command_result::from_error(Responses::BadIdea);

    std::vector<const dppcmd::command_info*> cmds = service->search_command(cmd);
    if (cmds.empty())
        return dppcmd::command_result::from_error(Responses::NonexistentCommand);

    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    misc.disabledCommands.insert(cmds.front()->name());

    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(Responses::SetCommandDisabled);
}

dppcmd::command_result Config::disableFiltersInChannel(dpp::channel* channel)
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    if (!misc.inviteFilterEnabled && !misc.scamFilterEnabled && misc.filteredTerms.empty())
        return dppcmd::command_result::from_error(Responses::NoFiltersToDisable);

    DbConfigChannels channels = MongoManager::fetchChannelConfig(context->msg.guild_id);
    channels.noFilterChannels.insert(channel->id);

    MongoManager::updateChannelConfig(channels);
    return dppcmd::command_result::from_success(std::format(Responses::DisabledFilters, channel->get_mention()));
}

dppcmd::command_result Config::disableModule(const std::string& module)
{
    if (dppcmd::utility::iequals(module, "Config"))
        return dppcmd::command_result::from_error(Responses::BadIdea);

    std::vector<const dppcmd::module_base*> modules = service->search_module(module);
    if (modules.empty())
        return dppcmd::command_result::from_error(Responses::NonexistentModule);

    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    misc.disabledModules.insert(modules.front()->name());

    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(Responses::SetModuleDisabled);
}

dppcmd::command_result Config::enableCommand(const std::string& cmd)
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    if (!std::erase_if(misc.disabledCommands, [&cmd](const std::string& c) { return dppcmd::utility::iequals(c, cmd); }))
        return dppcmd::command_result::from_error(Responses::NotDisabledCommand);

    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(Responses::SetCommandEnabled);
}

dppcmd::command_result Config::enableModule(const std::string& module)
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    if (!std::erase_if(misc.disabledModules, [&module](const std::string& m) { return dppcmd::utility::iequals(m, module); }))
        return dppcmd::command_result::from_error(Responses::NotDisabledModule);

    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(Responses::SetModuleEnabled);
}

dppcmd::command_result Config::filterTerm(const dppcmd::remainder<std::string>& term)
{
    std::string termLower = RR::utility::toLower(*term);
    if (!std::regex_match(termLower, std::regex("^[a-z0-9\x20\x2d]*$")))
        return dppcmd::command_result::from_error(Responses::InvalidFilteredTerm);

    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    if (auto res = misc.filteredTerms.insert(termLower); !res.second)
        return dppcmd::command_result::from_error(Responses::TermAlreadyFiltered);

    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(std::format(Responses::FilteredTerm, termLower));
}

dppcmd::command_result Config::setAdminRole(dpp::role* role)
{
    DbConfigRoles roles = MongoManager::fetchRoleConfig(context->msg.guild_id);
    roles.staffLvl2Role = role->id;
    MongoManager::updateRoleConfig(roles);
    return dppcmd::command_result::from_success(std::format(Responses::SetAdminRole, role->get_mention()));
}

dppcmd::command_result Config::setDjRole(dpp::role* role)
{
    DbConfigRoles roles = MongoManager::fetchRoleConfig(context->msg.guild_id);
    roles.djRole = role->id;
    MongoManager::updateRoleConfig(roles);
    return dppcmd::command_result::from_success(std::format(Responses::SetDjRole, role->get_mention()));
}

dppcmd::command_result Config::setLogsChannel(dpp::channel* channel)
{
    DbConfigChannels channels = MongoManager::fetchChannelConfig(context->msg.guild_id);
    channels.logsChannel = channel->id;
    MongoManager::updateChannelConfig(channels);
    return dppcmd::command_result::from_success(std::format(Responses::SetLogsChannel, channel->get_mention()));
}

dppcmd::command_result Config::setModRole(dpp::role* role)
{
    DbConfigRoles roles = MongoManager::fetchRoleConfig(context->msg.guild_id);
    roles.staffLvl1Role = role->id;
    MongoManager::updateRoleConfig(roles);
    return dppcmd::command_result::from_success(std::format(Responses::SetModRole, role->get_mention()));
}

dppcmd::command_result Config::setPotChannel(dpp::channel* channel)
{
    DbConfigChannels channels = MongoManager::fetchChannelConfig(context->msg.guild_id);
    channels.potChannel = channel->id;
    MongoManager::updateChannelConfig(channels);
    return dppcmd::command_result::from_success(std::format(Responses::SetPotChannel, channel->get_mention()));
}

dppcmd::command_result Config::toggleDrops()
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    misc.dropsDisabled = !misc.dropsDisabled;
    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(std::format(Responses::ToggledRandomDrops, misc.dropsDisabled ? "OFF" : "ON"));
}

dppcmd::command_result Config::toggleInviteFilter()
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    misc.inviteFilterEnabled = !misc.inviteFilterEnabled;
    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(std::format(Responses::ToggledInviteFilter, misc.inviteFilterEnabled ? "ON" : "OFF"));
}

dppcmd::command_result Config::toggleNsfw()
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    misc.nsfwEnabled = !misc.nsfwEnabled;
    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(std::format(Responses::ToggledNsfw, misc.nsfwEnabled ? "ON" : "OFF"));
}

dppcmd::command_result Config::toggleScamFilter()
{
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    misc.scamFilterEnabled = !misc.scamFilterEnabled;
    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(std::format(Responses::ToggledScamFilter, misc.scamFilterEnabled ? "ON" : "OFF"));
}

dppcmd::command_result Config::unfilterTerm(const dppcmd::remainder<std::string>& term)
{
    std::string termLower = RR::utility::toLower(*term);
    DbConfigMisc misc = MongoManager::fetchMiscConfig(context->msg.guild_id);
    if (!misc.filteredTerms.erase(termLower))
        return dppcmd::command_result::from_error(Responses::TermNotFiltered);

    MongoManager::updateMiscConfig(misc);
    return dppcmd::command_result::from_success(std::format(Responses::UnfilteredTerm, termLower));
}

dppcmd::command_result Config::unwhitelistChannel(dpp::channel* channel)
{
    DbConfigChannels channels = MongoManager::fetchChannelConfig(context->msg.guild_id);
    if (!channels.whitelistedChannels.erase(channel->id))
        return dppcmd::command_result::from_error(Responses::ChannelNotWhitelisted);

    MongoManager::updateChannelConfig(channels);
    return dppcmd::command_result::from_success(std::format(Responses::ChannelUnwhitelisted, channel->get_mention()));
}

dppcmd::command_result Config::whitelistChannel(dpp::channel* channel)
{
    DbConfigChannels channels = MongoManager::fetchChannelConfig(context->msg.guild_id);
    channels.whitelistedChannels.insert(channel->id);
    MongoManager::updateChannelConfig(channels);
    return dppcmd::command_result::from_success(std::format(Responses::ChannelWhitelisted, channel->get_mention()));
}
