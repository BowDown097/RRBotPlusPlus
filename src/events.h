#pragma once

namespace dpp { class cluster; }
namespace dppcmd { class module_service; }

namespace Events
{
    void connectEvents(dpp::cluster* cluster, dppcmd::module_service* modules);
};