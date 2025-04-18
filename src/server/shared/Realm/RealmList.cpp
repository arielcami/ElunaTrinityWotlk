/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
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

#include "RealmList.h"
#include "ClientBuildInfo.h"
#include "DatabaseEnv.h"
#include "DeadlineTimer.h"
#include "IoContext.h"
#include "Log.h"
#include "Resolver.h"
#include "Util.h"
#include <boost/asio/ip/tcp.hpp>

RealmList::RealmList() : _updateInterval(0)
{
}

RealmList::~RealmList() = default;

RealmList* RealmList::Instance()
{
    static RealmList instance;
    return &instance;
}

// Load the realm list from the database
void RealmList::Initialize(Trinity::Asio::IoContext& ioContext, uint32 updateInterval)
{
    _updateInterval = updateInterval;
    _updateTimer = std::make_unique<Trinity::Asio::DeadlineTimer>(ioContext);
    _resolver = std::make_unique<Trinity::Asio::Resolver>(ioContext);

    ClientBuild::LoadBuildInfo();
    // Get the content of the realmlist table in the database
    UpdateRealms(boost::system::error_code());
}

void RealmList::Close()
{
    _updateTimer->cancel();
}

void RealmList::UpdateRealm(RealmHandle const& id, uint32 build, std::string const& name,
    boost::asio::ip::address&& address, boost::asio::ip::address&& localAddr, boost::asio::ip::address&& localSubmask,
    uint16 port, uint8 icon, RealmFlags flag, uint8 timezone, AccountTypes allowedSecurityLevel, float population)
{
    // Create new if not exist or update existed
    Realm& realm = _realms[id];

    realm.Id = id;
    realm.Build = build;
    realm.Name = name;
    realm.Type = icon;
    realm.Flags = flag;
    realm.Timezone = timezone;
    realm.AllowedSecurityLevel = allowedSecurityLevel;
    realm.PopulationLevel = population;
    if (!realm.ExternalAddress || *realm.ExternalAddress != address)
        realm.ExternalAddress = std::make_unique<boost::asio::ip::address>(std::move(address));
    if (!realm.LocalAddress || *realm.LocalAddress != localAddr)
        realm.LocalAddress = std::make_unique<boost::asio::ip::address>(std::move(localAddr));
    if (!realm.LocalSubnetMask || *realm.LocalSubnetMask != localSubmask)
        realm.LocalSubnetMask = std::make_unique<boost::asio::ip::address>(std::move(localSubmask));
    realm.Port = port;
}

void RealmList::UpdateRealms(boost::system::error_code const& error)
{
    if (error)
        return;

    TC_LOG_DEBUG("server.authserver", "Updating Realm List...");

    LoginDatabasePreparedStatement *stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_REALMLIST);
    PreparedQueryResult result = LoginDatabase.Query(stmt);

    std::map<RealmHandle, std::string> existingRealms;
    for (auto const& p : _realms)
        existingRealms[p.first] = p.second.Name;

    _realms.clear();

    // Circle through results and add them to the realm map
    if (result)
    {
        do
        {
            try
            {
                Field* fields = result->Fetch();
                uint32 realmId = fields[0].GetUInt32();
                std::string name = fields[1].GetString();
                std::string externalAddressString = fields[2].GetString();
                std::string localAddressString = fields[3].GetString();
                std::string localSubmaskString = fields[4].GetString();

                Optional<boost::asio::ip::tcp::endpoint> externalAddress = _resolver->Resolve(boost::asio::ip::tcp::v4(), externalAddressString, "");
                if (!externalAddress)
                {
                    TC_LOG_ERROR("server.authserver", "Could not resolve address {} for realm \"{}\" id {}", externalAddressString, name, realmId);
                    continue;
                }

                Optional<boost::asio::ip::tcp::endpoint> localAddress = _resolver->Resolve(boost::asio::ip::tcp::v4(), localAddressString, "");
                if (!localAddress)
                {
                    TC_LOG_ERROR("server.authserver", "Could not resolve localAddress {} for realm \"{}\" id {}", localAddressString, name, realmId);
                    continue;
                }

                Optional<boost::asio::ip::tcp::endpoint> localSubmask = _resolver->Resolve(boost::asio::ip::tcp::v4(), localSubmaskString, "");
                if (!localSubmask)
                {
                    TC_LOG_ERROR("server.authserver", "Could not resolve localSubnetMask {} for realm \"{}\" id {}", localSubmaskString, name, realmId);
                    continue;
                }

                uint16 port = fields[5].GetUInt16();
                uint8 icon = fields[6].GetUInt8();
                if (icon == REALM_TYPE_FFA_PVP)
                    icon = REALM_TYPE_PVP;
                if (icon >= MAX_CLIENT_REALM_TYPE)
                    icon = REALM_TYPE_NORMAL;
                RealmFlags flag = RealmFlags(fields[7].GetUInt8());
                uint8 timezone = fields[8].GetUInt8();
                uint8 allowedSecurityLevel = fields[9].GetUInt8();
                float pop = fields[10].GetFloat();
                uint32 build = fields[11].GetUInt32();

                RealmHandle id{ realmId };

                UpdateRealm(id, build, name, externalAddress->address(), localAddress->address(), localSubmask->address(), port, icon, flag,
                    timezone, (allowedSecurityLevel <= SEC_ADMINISTRATOR ? AccountTypes(allowedSecurityLevel) : SEC_ADMINISTRATOR), pop);

                if (!existingRealms.count(id))
                    TC_LOG_INFO("server.authserver", "Added realm \"{}\" at {}:{}.", name, externalAddressString, port);
                else
                    TC_LOG_DEBUG("server.authserver", "Updating realm \"{}\" at {}:{}.", name, externalAddressString, port);

                existingRealms.erase(id);
            }
            catch (std::exception& ex)
            {
                TC_LOG_ERROR("server.authserver", "Realmlist::UpdateRealms has thrown an exception: {}", ex.what());
                ABORT();
            }
        }
        while (result->NextRow());
    }

    for (auto itr = existingRealms.begin(); itr != existingRealms.end(); ++itr)
        TC_LOG_INFO("server.authserver", "Removed realm \"{}\".", itr->second);

    if (_updateInterval)
    {
        _updateTimer->expires_after(std::chrono::seconds(_updateInterval));
        _updateTimer->async_wait(std::bind(&RealmList::UpdateRealms, this, std::placeholders::_1));
    }
}

Realm const* RealmList::GetRealm(RealmHandle const& id) const
{
    auto itr = _realms.find(id);
    if (itr != _realms.end())
        return &itr->second;

    return nullptr;
}
