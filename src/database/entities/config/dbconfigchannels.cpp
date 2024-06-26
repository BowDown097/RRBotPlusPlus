#include "dbconfigchannels.h"
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>

DbConfigChannels::DbConfigChannels(bsoncxx::document::view doc)
{
    guildId = bsoncxx_get_or_default(doc["guildId"], int64);
    logsChannel = bsoncxx_get_or_default(doc["logsChannel"], int64);
    potChannel = bsoncxx_get_or_default(doc["potChannel"], int64);
    bsoncxx_elem_to_array(doc["noFilterChannels"], noFilterChannels, int64);
    bsoncxx_elem_to_array(doc["whitelistedChannels"], whitelistedChannels, int64);
}

bsoncxx::document::value DbConfigChannels::toDocument() const
{
    bsoncxx::builder::stream::array noFilterChannelsArr;
    for (int64_t noFilterChannel : noFilterChannels)
        noFilterChannelsArr << noFilterChannel;

    bsoncxx::builder::stream::array whitelistedChannelsArr;
    for (int64_t whitelistedChannel : whitelistedChannels)
        whitelistedChannelsArr << whitelistedChannel;

    return bsoncxx::builder::stream::document()
           << "guildId" << guildId
           << "logsChannel" << logsChannel
           << "noFilterChannels" << noFilterChannelsArr
           << "potChannel" << potChannel
           << "whitelistedChannels" << whitelistedChannelsArr
           << bsoncxx::builder::stream::finalize;
}
