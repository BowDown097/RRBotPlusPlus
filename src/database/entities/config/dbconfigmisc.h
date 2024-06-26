#pragma once
#include "database/entities/dbobject.h"
#include <vector>

struct DbConfigMisc : DbObject
{
    std::vector<std::string> disabledCommands;
    std::vector<std::string> disabledModules;
    bool dropsDisabled{};
    std::vector<std::string> filteredTerms;
    int64_t guildId{};
    bool inviteFilterEnabled{};
    bool nsfwEnabled{};
    bool scamFilterEnabled{};

    DbConfigMisc() = default;
    explicit DbConfigMisc(bsoncxx::document::view doc);
    bsoncxx::document::value toDocument() const override;
};
