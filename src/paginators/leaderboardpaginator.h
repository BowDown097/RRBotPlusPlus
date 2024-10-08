#pragma once
#include "dppinteract/pagination/paginator.h"

class LeaderboardPaginator final : public dppinteract::paginator
{
public:
    DEFAULT_PAGINATOR_OVERRIDES(LeaderboardPaginator)
    explicit LeaderboardPaginator(std::string_view currency, long double currencyValue, int64_t guildId)
        : currency(currency), currencyValue(currencyValue), guildId(guildId) {}
    dppinteract::interaction_page get_or_load_page(int pageIndex) override;
    int max_page_index() const override { return -1; }
private:
    std::string currency;
    long double currencyValue;
    int currentUserIndex{};
    int64_t guildId;
    int lastPageIndex{};
    int skippedUsers{};
};
