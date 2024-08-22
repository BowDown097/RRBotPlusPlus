#pragma once
#include "dpp-command-handler/modules/module.h"
#include "dpp-interactive/interactiveresult.h"

namespace dpp { class guild_member; class message; }

class Crime : public dpp::module<Crime>
{
public:
    Crime();
private:
    dpp::task<dpp::command_result> bully(dpp::guild_member member, const dpp::remainder<std::string>& nickname);
    dpp::task<dpp::command_result> deal();
    dpp::task<dpp::command_result> hack(const dpp::guild_member& member, const std::string& crypto, long double amount);
    dpp::task<dpp::command_result> loot();
    dpp::task<dpp::command_result> rape(const dpp::guild_member& member);
    dpp::task<dpp::command_result> rob(const dpp::guild_member& member, long double amount);
    dpp::task<dpp::command_result> scavenge();
    dpp::task<dpp::command_result> slavery();
    dpp::task<dpp::command_result> whore();

    dpp::task<dpp::command_result> genericCrime(const std::span<const std::string_view>& successOutcomes,
                                                const std::span<const std::string_view>& failOutcomes,
                                                class DbUser& user, int64_t& cooldown, bool hasMehOutcome = false);
    dpp::task<void> handleScavenge(dpp::message& msg, const dpp::interactive_result<dpp::message>& result,
                                   DbUser& user, const dpp::guild_member& member,
                                   bool successCondition, std::string_view successResponse,
                                   std::string_view timeoutResponse, std::string_view failureResponse);
    void statUpdate(class DbUser& user, bool success, long double gain);
};
