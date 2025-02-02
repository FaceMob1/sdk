/**
 * @file usernotifications.cpp
 * @brief additional megaclient code for user notifications
 *
 * (c) 2013-2018 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include "mega.h"
#include "mega/megaclient.h"
#include <utility>

using std::to_string;
using std::make_pair;

namespace mega {

UserAlertRaw::UserAlertRaw()
    : t(0)
{
}

JSON UserAlertRaw::field(nameid nid) const
{
    map<nameid, string>::const_iterator i = fields.find(nid);
    JSON j;
    j.pos = i == fields.end() ? NULL : i->second.c_str();
    return j;
}

bool UserAlertRaw::has(nameid nid) const
{
    JSON j = field(nid);
    return j.pos != NULL;
}

int UserAlertRaw::getint(nameid nid, int default_value) const
{
    JSON j = field(nid);
    return j.pos && j.isnumeric() ? int(j.getint()) : default_value;
}

int64_t UserAlertRaw::getint64(nameid nid, int64_t default_value) const
{
    JSON j = field(nid);
    return j.pos && j.isnumeric() ? j.getint() : default_value;
}

handle UserAlertRaw::gethandle(nameid nid, int handlesize, handle default_value) const
{
    JSON j = field(nid);
    byte buf[9] = { 0 };
    return (j.pos && handlesize == Base64::atob(j.pos, buf, sizeof(buf))) ? MemAccess::get<handle>((const char*)buf) : default_value;
}

nameid UserAlertRaw::getnameid(nameid nid, nameid default_value) const
{
    JSON j = field(nid);
    nameid id = 0;
    while (*j.pos)
    {
        id = (id << 8) + static_cast<unsigned char>(*j.pos++);
    }

    return id ? id : default_value;
}

string UserAlertRaw::getstring(nameid nid, const char* default_value) const
{
    JSON j = field(nid);
    return j.pos ? j.pos : default_value;
}

bool UserAlertRaw::gethandletypearray(nameid nid, vector<handletype>& v) const
{
    JSON j = field(nid);
    if (j.pos && j.enterarray())
    {
        for (;;)
        {
            if (j.enterobject())
            {
                handletype ht;
                ht.h = UNDEF;
                ht.t = -1;
                bool fields = true;
                while (fields)
                {
                    switch (j.getnameid())
                    {
                    case 'h':
                        ht.h = j.gethandle(MegaClient::NODEHANDLE);
                        break;
                    case 't':
                        ht.t = int(j.getint());
                        break;
                    case EOO:
                        fields = false;
                        break;
                    default:
                        j.storeobject(NULL);
                    }
                }
                v.push_back(ht);
                j.leaveobject();
            }
            else
            {
                break;
            }
        }
        j.leavearray();
        return true;
    }
    return false;
}

bool UserAlertRaw::getstringarray(nameid nid, vector<string>& v) const
{
    JSON j = field(nid);
    if (j.pos && j.enterarray())
    {
        for (;;)
        {
            string s;
            if (j.storeobject(&s))
            {
                v.push_back(s);
            }
            else
            {
                break;
            }
        }
        j.leavearray();
    }
    return false;
}

UserAlertFlags::UserAlertFlags()
    : cloud_enabled(true)
    , contacts_enabled(true)
    , cloud_newfiles(true)
    , cloud_newshare(true)
    , cloud_delshare(true)
    , contacts_fcrin(true)
    , contacts_fcrdel(true)
    , contacts_fcracpt(true)
{
}

UserAlertPendingContact::UserAlertPendingContact()
    : u(0)
{
}



UserAlert::Base::Base(UserAlertRaw& un, unsigned int cid)
{
    id = cid;
    type = un.t;
    m_time_t timeDelta = un.getint64(MAKENAMEID2('t', 'd'), 0);
    timestamp = m_time() - timeDelta;
    userHandle = un.gethandle('u', MegaClient::USERHANDLE, UNDEF);
    userEmail = un.getstring('m', "");

    seen = false; // to be updated on EOO
    relevant = true;
    tag = -1;
}

UserAlert::Base::Base(nameid t, handle uh, const string& email, m_time_t ts, unsigned int cid)
{
    id = cid;
    type = t;
    userHandle = uh;
    userEmail = email;
    timestamp = ts;
    seen = false;
    relevant = true;
    tag = -1;
}

UserAlert::Base::~Base()
{
}

void UserAlert::Base::updateEmail(MegaClient* mc)
{
    if (User* u = mc->finduser(userHandle))
    {
        userEmail = u->email;
    }
}

bool UserAlert::Base::checkprovisional(handle , MegaClient*)
{
    return true;
}

void UserAlert::Base::text(string& header, string& title, MegaClient* mc)
{
    // should be overridden
    updateEmail(mc);
    ostringstream s;
    s << "notification: type " << type << " time " << timestamp << " user " << userHandle << " seen " << seen;
    title =  s.str();
    header = userEmail;
}

UserAlert::IncomingPendingContact::IncomingPendingContact(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    mPcrHandle = un.gethandle('p', MegaClient::PCRHANDLE, UNDEF);
    userHandle = mPcrHandle;    // for backwards compatibility, due to legacy bug

    m_time_t dts = un.getint64(MAKENAMEID3('d', 't', 's'), 0);
    m_time_t rts = un.getint64(MAKENAMEID3('r', 't', 's'), 0);
    initTs(dts, rts);
}

UserAlert::IncomingPendingContact::IncomingPendingContact(m_time_t dts, m_time_t rts, handle p, const string& email, m_time_t timestamp, unsigned int id)
    : Base(UserAlert::type_ipc, p, email, timestamp, id)
    // passing PCR's handle as the user's handle for backwards compatibility, due to legacy bug
{
    mPcrHandle = p;

    initTs(dts, rts);
}

void UserAlert::IncomingPendingContact::initTs(m_time_t dts, m_time_t rts)
{
    requestWasDeleted = dts != 0;
    requestWasReminded = rts != 0;

    if (requestWasDeleted)       timestamp = dts;
    else if (requestWasReminded) timestamp = rts;
}

void UserAlert::IncomingPendingContact::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    if (requestWasDeleted)
    {
        title = "Cancelled their contact request"; // 7151
    }
    else if (requestWasReminded)
    {
        title = "Reminder: You have a contact request"; // 7150
    }
    else
    {
        title = "Sent you a contact request"; // 5851
    }
    header = userEmail;
}

UserAlert::ContactChange::ContactChange(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    action = un.getint('c', -1);
    relevant = action >= 0 && action < 4;
    assert(action >= 0 && action < 4);
    otherUserHandle = un.gethandle(MAKENAMEID2('o', 'u'), MegaClient::USERHANDLE, UNDEF);
}

UserAlert::ContactChange::ContactChange(int c, handle uh, const string& email, m_time_t timestamp, unsigned int id)
    : Base(UserAlert::type_c, uh, email, timestamp, id)
{
    action = c;
    assert(action >= 0 && action < 4);
}

bool UserAlert::ContactChange::checkprovisional(handle ou, MegaClient* mc)
{
    return ou != mc->me;
}

void UserAlert::ContactChange::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);

    if (action == 0)
    {
        title = "Deleted you as a contact"; // 7146
    }
    else if (action == 1)
    {
        title = "Contact relationship established"; // 7145
    }
    else if (action == 2)
    {
        title = "Account has been deleted/deactivated"; // 7144
    }
    else if (action == 3)
    {
        title = "Blocked you as a contact"; //7143
    }
    header = userEmail;
}

UserAlert::UpdatedPendingContactIncoming::UpdatedPendingContactIncoming(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    action = un.getint('s', -1);
    relevant = action >= 1 && action < 4;
}

UserAlert::UpdatedPendingContactIncoming::UpdatedPendingContactIncoming(int s, handle uh, const string& email, m_time_t timestamp, unsigned int id)
    : Base(type_upci, uh, email, timestamp, id)
    , action(s)
{
}

void UserAlert::UpdatedPendingContactIncoming::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    if (action == 1)
    {
        title = "You ignored a contact request"; // 7149
    }
    else if (action == 2)
    {
        title = "You accepted a contact request"; // 7148
    }
    else if (action == 3)
    {
        title = "You denied a contact request"; // 7147
    }
    header = userEmail;
}

UserAlert::UpdatedPendingContactOutgoing::UpdatedPendingContactOutgoing(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    action = un.getint('s', -1);
    relevant = action == 2 || action == 3;
}

UserAlert::UpdatedPendingContactOutgoing::UpdatedPendingContactOutgoing(int s, handle uh, const string& email, m_time_t timestamp, unsigned int id)
    : Base(type_upco, uh, email, timestamp, id)
    , action(s)
{
}

void UserAlert::UpdatedPendingContactOutgoing::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    if (action == 2)
    {
        title = "Accepted your contact request"; // 5852
    }
    else if (action == 3)
    {
        title = "Denied your contact request"; // 5853
    }
    header = userEmail;
}

UserAlert::NewShare::NewShare(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    folderhandle = un.gethandle('n', MegaClient::NODEHANDLE, UNDEF);
}

UserAlert::NewShare::NewShare(handle h, handle uh, const string& email, m_time_t timestamp, unsigned int id)
    : Base(type_share, uh, email, timestamp, id)
{
    folderhandle = h;
}

void UserAlert::NewShare::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    if (!userEmail.empty())
    {
        title =  "New shared folder from " + userEmail; // 824
    }
    else
    {
        title = "New shared folder"; // 825
    }
    header = userEmail;
}

UserAlert::DeletedShare::DeletedShare(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    ownerHandle = un.gethandle('o', MegaClient::USERHANDLE, UNDEF);
    folderHandle = un.gethandle('n', MegaClient::NODEHANDLE, UNDEF);
}

UserAlert::DeletedShare::DeletedShare(handle uh, const string& email, handle ownerhandle, handle folderhandle, m_time_t ts, unsigned int id)
    : Base(type_dshare, uh, email, ts, id)
{
    ownerHandle = ownerhandle;
    folderHandle = folderhandle;
}

void UserAlert::DeletedShare::updateEmail(MegaClient* mc)
{
    Base::updateEmail(mc);

    if (Node* n = mc->nodebyhandle(folderHandle))
    {
        folderPath = n->displaypath();
        folderName = n->displayname();
    }
}

void UserAlert::DeletedShare::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    ostringstream s;

    if (userHandle == ownerHandle)
    {
        if (!userEmail.empty())
        {
            s << "Access to folders shared by " << userEmail << " was removed"; // 7879
        }
        else
        {
            s << "Access to folders was removed"; // 7880
        }
    }
    else
    {
       if (!userEmail.empty())
       {
           s << "User " << userEmail << " has left the shared folder " << folderName;  //19153
       }
       else
       {
           s << "A user has left the shared folder " << folderName;  //19154
       }
    }
    title = s.str();
    header = userEmail;
}

UserAlert::NewSharedNodes::NewSharedNodes(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{

    vector<UserAlertRaw::handletype> f;
    un.gethandletypearray('f', f);
    parentHandle = un.gethandle('n', MegaClient::NODEHANDLE, UNDEF);

    for (size_t n = f.size(); n--; )
    {
        if (f[n].t == FOLDERNODE)
        {
            folderNodeHandles.push_back(f[n].h);
        }
        else if (f[n].t == FILENODE)
        {
            fileNodeHandles.push_back(f[n].h);
        }
        // else should not be happening, we can add a sanity check
    }
}

UserAlert::NewSharedNodes::NewSharedNodes(handle uh, handle ph, m_time_t timestamp, unsigned int id,
                                          handle_alerttype_map_t alertTypePerFileNode,
                                          handle_alerttype_map_t alertTypePerFolderNode)
    : Base(UserAlert::type_put, uh, string(), timestamp, id)
    , parentHandle(ph)
{
    assert(!ISUNDEF(uh));
    for (auto& item: alertTypePerFileNode)
    {
        fileNodeHandles.push_back(item.first);
    }
    for (auto& item: alertTypePerFolderNode)
    {
        folderNodeHandles.push_back(item.first);
    }
}

void UserAlert::NewSharedNodes::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    ostringstream notificationText;

    // Get wording for the number of files and folders added
    const auto folderCount = folderNodeHandles.size();
    const auto fileCount = fileNodeHandles.size();
    if ((folderCount > 1) && (fileCount > 1)) {
        notificationText << folderCount << " folders and " << fileCount << " files";
    }
    else if ((folderCount > 1) && (fileCount == 1)) {
        notificationText << folderCount << " folders and 1 file";
    }
    else if ((folderCount == 1) && (fileCount > 1)) {
        notificationText << "1 folder and " << fileCount << " files";
    }
    else if ((folderCount == 1) && (fileCount == 1)) {
        notificationText << "1 folder and 1 file";
    }
    else if (folderCount > 1) {
        notificationText << folderCount << " folders";
    }
    else if (fileCount > 1) {
        notificationText << fileCount << " files";
    }
    else if (folderCount == 1) {
        notificationText << "1 folder";
    }
    else if (fileCount == 1) {
        notificationText << "1 file";
    }
    else {
        notificationText << "nothing";
    }

    // Set wording of the title
    if (!userEmail.empty())
    {
        title = userEmail + " added " + notificationText.str();
    }
    else if ((fileCount + folderCount) > 1)
    {
        title = notificationText.str() + " have been added";
    }
    else {
        title = notificationText.str() + " has been added";
    }
    header = userEmail;
}

UserAlert::RemovedSharedNode::RemovedSharedNode(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    vector<UserAlertRaw::handletype> handlesAndNodeTypes;
    un.gethandletypearray('f', handlesAndNodeTypes);

    for (const auto& handleAndType: handlesAndNodeTypes)
    {
        nodeHandles.push_back(handleAndType.h);
    }
}

UserAlert::RemovedSharedNode::RemovedSharedNode(handle uh, m_time_t timestamp, unsigned int id,
                                                handle_alerttype_map_t alertTypePerFileNode,
                                                handle_alerttype_map_t alertTypePerFolderNode)
    : Base(UserAlert::type_d, uh, string(), timestamp, id)
{
    for (auto& item: alertTypePerFileNode)
    {
        nodeHandles.push_back(item.first);
    }
    for (auto& item: alertTypePerFolderNode)
    {
        nodeHandles.push_back(item.first);
    }
}

void UserAlert::RemovedSharedNode::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    ostringstream s;
    const auto itemsNumber = nodeHandles.size();
    if (itemsNumber > 1)
    {
        s << "Removed " << itemsNumber << " items from a share"; // 8913
    }
    else
    {
        s << "Removed item from shared folder"; // 8910
    }
    title = s.str();
    header = userEmail;
}

UserAlert::UpdatedSharedNode::UpdatedSharedNode(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    vector<UserAlertRaw::handletype> handlesAndNodeTypes;
    un.gethandletypearray('f', handlesAndNodeTypes);

    for (const auto& handleAndType: handlesAndNodeTypes)
    {
        nodeHandles.push_back(handleAndType.h);
    }
}

UserAlert::UpdatedSharedNode::UpdatedSharedNode(handle uh, m_time_t timestamp, unsigned int id,
                                                handle_alerttype_map_t alertTypePerFileNode,
                                                handle_alerttype_map_t alertTypePerFolderNode)
    : Base(UserAlert::type_u, uh, string(), timestamp, id)
{
    for (auto& item: alertTypePerFileNode)
    {
        nodeHandles.push_back(item.first);
    }
    for (auto& item: alertTypePerFolderNode)
    {
        nodeHandles.push_back(item.first);
    }
}

void UserAlert::UpdatedSharedNode::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    header = userEmail;
    const auto itemsNumber = nodeHandles.size();
    const string& itemText = (itemsNumber == 1) ? "" : "s";
    title = "Updated " + to_string(itemsNumber) + " item" + itemText + " in shared folder";
}

string UserAlert::Payment::getProPlanName()
{
    switch (planNumber) {
    case 1:
        return "PRO I"; // 5819
    case 2:
        return "PRO II"; // 6125
    case 3:
        return "PRO III"; // 6126
    case 4:
        return "PRO LITE"; // 8413
    default:
        return "FREE"; // 435
    }
}

UserAlert::Payment::Payment(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    success = 's' == un.getnameid('r', 0);
    planNumber = un.getint('p', 0);
}

UserAlert::Payment::Payment(bool s, int plan, m_time_t timestamp, unsigned int id)
    : Base(type_psts, UNDEF, "", timestamp, id)
{
    success = s;
    planNumber = plan;
}

void UserAlert::Payment::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    ostringstream s;
    if (success)
    {
        s << "Your payment for the " << getProPlanName() << " plan was received. "; // 7142
    }
    else
    {
        s << "Your payment for the " << getProPlanName() << " plan was unsuccessful."; // 7141
    }
    title = s.str();
    header = "Payment info"; // 1230
}

UserAlert::PaymentReminder::PaymentReminder(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    expiryTime = un.getint64(MAKENAMEID2('t', 's'), timestamp);
    relevant = true;  // relevant until we see a subsequent payment
}

UserAlert::PaymentReminder::PaymentReminder(m_time_t expiryts, unsigned int id)
    : Base(type_pses, UNDEF, "", m_time(), id)
{
    expiryTime = expiryts;
    relevant = true; // relevant until we see a subsequent payment
}

void UserAlert::PaymentReminder::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    m_time_t now = m_time();
    int days = int((expiryTime - now) / 86400);

    ostringstream s;
    if (expiryTime < now)
    {
        s << "Your PRO membership plan expired " << -days << (days == -1 ? " day" : " days") << " ago";
    }
    else
    {
        s << "Your PRO membership plan will expire in " << days << (days == 1 ? " day." : " days.");   // 8596, 8597
    }
    title = s.str();
    header = "PRO membership plan expiring soon"; // 8598
}

UserAlert::Takedown::Takedown(UserAlertRaw& un, unsigned int id)
    : Base(un, id)
{
    int n = un.getint(MAKENAMEID4('d', 'o', 'w', 'n'), -1);
    isTakedown = n == 1;
    isReinstate = n == 0;
    nodeHandle = un.gethandle('h', MegaClient::NODEHANDLE, UNDEF);
    relevant = isTakedown || isReinstate;
}

UserAlert::Takedown::Takedown(bool down, bool reinstate, int /*t*/, handle nh, m_time_t timestamp, unsigned int id)
    : Base(type_ph, UNDEF, "", timestamp, id)
{
    isTakedown = down;
    isReinstate = reinstate;
    nodeHandle = nh;
    relevant = isTakedown || isReinstate;
}

void UserAlert::Takedown::text(string& header, string& title, MegaClient* mc)
{
    updateEmail(mc);
    const char* typestring = "node";
    string name;

    Node* node = mc->nodebyhandle(nodeHandle);
    if (node)
    {
        if (node->type == FOLDERNODE)
        {
            typestring = "folder";
        }
        else if (node->type == FILENODE)
        {
            typestring = "file";
        }

        name = node->displaypath();
    }

    if (name.empty())
    {
        char buffer[12];
        Base64::btoa((byte*)&(nodeHandle), MegaClient::NODEHANDLE, buffer);
        name = "handle ";
        name += buffer;
    }

    ostringstream s;
    if (isTakedown)
    {
        header = "Takedown notice";  //8521
        s << "Your publicly shared " << typestring << " (" << name << ") has been taken down."; //8522
    }
    else if (isReinstate)
    {
        header = "Takedown reinstated";  //8524
        s << "Your taken down " << typestring << " (" << name << ") has been reinstated."; // 8523
    }
    title = s.str();
}

UserAlerts::UserAlerts(MegaClient& cmc)
    : mc(cmc)
    , nextid(0)
    , begincatchup(false)
    , catchupdone(false)
    , catchup_last_timestamp(0)
    , lsn(UNDEF)
    , fsn(UNDEF)
    , lastTimeDelta(0)
    , provisionalmode(false)
    , notingSharedNodes(false)
    , ignoreNodesUnderShare(UNDEF)
{
}

unsigned int UserAlerts::nextId()
{
    return ++nextid;
}

bool UserAlerts::isUnwantedAlert(nameid type, int action)
{
    using namespace UserAlert;

    if (type == type_put || type == type_share || type == type_dshare)
    {
        if (!flags.cloud_enabled) {
            return true;
        }
    }
    else if (type == type_c || type == type_ipc || type == type_upci || type == type_upco)
    {
        if (!flags.contacts_enabled) {
            return true;
        }
    }

    if (type == type_put)
    {
        return !flags.cloud_newfiles;
    }
    else if (type == type_share)
    {
        return !flags.cloud_newshare;
    }
    else if (type == type_dshare)
    {
        return !flags.cloud_delshare;
    }
    else if (type == type_ipc)
    {
        return !flags.contacts_fcrin;
    }
    else if (type == type_c)
    {
        return (action == -1 || action == 0) && !flags.contacts_fcrdel;
    }
    else if (type == type_upco)
    {
        return (action == -1 || action == 2) && !flags.contacts_fcracpt;
    }

    return false;
}

void UserAlerts::add(UserAlertRaw& un)
{
    using namespace UserAlert;
    namespace u = UserAlert;
    Base* unb = NULL;

    switch (un.t) {
    case type_ipc:
        unb = new IncomingPendingContact(un, nextId());
        break;
    case type_c:
        unb = new ContactChange(un, nextId());
        break;
    case type_upci:
        unb = new UpdatedPendingContactIncoming(un, nextId());
        break;
    case type_upco:
        unb = new UpdatedPendingContactOutgoing(un, nextId());
        break;
    case type_share:
        unb = new u::NewShare(un, nextId());
        break;
    case type_dshare:
        unb = new DeletedShare(un, nextId());
        break;
    case type_put:
        unb = new NewSharedNodes(un, nextId());
        break;
    case type_d:
        unb = new RemovedSharedNode(un, nextId());
        break;
    case type_u:
        unb = new UpdatedSharedNode(un, nextId());
        break;
    case type_psts:
        unb = new Payment(un, nextId());
        break;
    case type_pses:
        unb = new PaymentReminder(un, nextId());
        break;
    case type_ph:
        unb = new Takedown(un, nextId());
        break;
    default:
        unb = NULL;   // If it's a notification type we do not recognise yet
    }

    if (unb)
    {
        add(unb);
    }
}

void UserAlerts::add(UserAlert::Base* unb)
{
    // unb is either directly from notification json, or constructed from actionpacket.
    // We take ownership.

    if (provisionalmode)
    {
        provisionals.push_back(unb);
        return;
    }

    if (!catchupdone && unb->timestamp > catchup_last_timestamp)
    {
        catchup_last_timestamp = unb->timestamp;
    }
    else if (catchupdone && unb->timestamp < catchup_last_timestamp)
    {
        // this is probably a duplicate from the initial set, generated from normal sc packets
        LOG_warn << "discarding duplicate user alert of type " << unb->type;
        delete unb;
        return;
    }

    if (!alerts.empty() && unb->type == UserAlert::type_put && alerts.back()->type == UserAlert::type_put)
    {
        // If it's file/folders added, and the prior one is for the same user and within 5 mins then we can combine instead
        UserAlert::NewSharedNodes* np = dynamic_cast<UserAlert::NewSharedNodes*>(unb);
        UserAlert::NewSharedNodes* op = dynamic_cast<UserAlert::NewSharedNodes*>(alerts.back());
        if (np && op)
        {
            if (np->userHandle == op->userHandle && np->timestamp - op->timestamp < 300 &&
                np->parentHandle == op->parentHandle && !ISUNDEF(np->parentHandle))
            {
                op->fileNodeHandles.insert(end(op->fileNodeHandles), begin(np->fileNodeHandles), end(np->fileNodeHandles));
                op->folderNodeHandles.insert(end(op->folderNodeHandles),
                                             begin(np->folderNodeHandles), end(np->folderNodeHandles));
                LOG_debug << "Merged user alert, type " << np->type << " ts " << np->timestamp;

                if (catchupdone && (useralertnotify.empty() || useralertnotify.back() != alerts.back()))
                {
                    alerts.back()->seen = false;
                    alerts.back()->tag = 0;
                    useralertnotify.push_back(alerts.back());
                    LOG_debug << "Updated user alert added to notify queue";
                }
                delete unb;
                return;
            }
        }
    }

    if (!alerts.empty() && unb->type == UserAlert::type_d && alerts.back()->type == UserAlert::type_d)
    {
        // If it's file/folders removed, and the prior one is for the same user and within 5 mins then we can combine instead
        UserAlert::RemovedSharedNode* nd = dynamic_cast<UserAlert::RemovedSharedNode*>(unb);
        UserAlert::RemovedSharedNode* od = dynamic_cast<UserAlert::RemovedSharedNode*>(alerts.back());
        if (nd && od)
        {
            if (nd->userHandle == od->userHandle && nd->timestamp - od->timestamp < 300)
            {
                od->nodeHandles.insert(end(od->nodeHandles), begin(nd->nodeHandles), end(nd->nodeHandles));
                LOG_debug << "Merged user alert, type " << nd->type << " ts " << nd->timestamp;

                if (catchupdone && (useralertnotify.empty() || useralertnotify.back() != alerts.back()))
                {
                    alerts.back()->seen = false;
                    alerts.back()->tag = 0;
                    useralertnotify.push_back(alerts.back());
                    LOG_debug << "Updated user alert added to notify queue";
                }
                delete unb;
                return;
            }
        }
    }

    if (!alerts.empty() && unb->type == UserAlert::type_u && alerts.back()->type == UserAlert::type_u)
    {
        // If it's file/folders updated, and the prior one is for the same user and within 5 mins then we can combine instead
        UserAlert::UpdatedSharedNode* nd = dynamic_cast<UserAlert::UpdatedSharedNode*>(unb);
        UserAlert::UpdatedSharedNode* od = dynamic_cast<UserAlert::UpdatedSharedNode*>(alerts.back());
        if (nd && od)
        {
            if (nd->userHandle == od->userHandle && nd->timestamp - od->timestamp < 300)
            {
                od->nodeHandles.insert(end(od->nodeHandles), begin(nd->nodeHandles), end(nd->nodeHandles));
                LOG_debug << "Merged user alert, type " << nd->type << " ts " << nd->timestamp;

                if (catchupdone && (useralertnotify.empty() || useralertnotify.back() != alerts.back()))
                {
                    alerts.back()->seen = false;
                    alerts.back()->tag = 0;
                    useralertnotify.push_back(alerts.back());
                    LOG_debug << "Updated user alert added to notify queue";
                }
                delete unb;
                return;
            }
        }
    }

    if (!alerts.empty() && unb->type == UserAlert::type_psts && static_cast<UserAlert::Payment*>(unb)->success)
    {
        // if a successful payment is made then hide/remove any reminders received
        for (Alerts::iterator i = alerts.begin(); i != alerts.end(); ++i)
        {
            if ((*i)->type == UserAlert::type_pses && (*i)->relevant)
            {
                (*i)->relevant = false;
                if (catchupdone)
                {
                    useralertnotify.push_back(*i);
                }
            }
        }
    }

    unb->updateEmail(&mc);
    alerts.push_back(unb);
    LOG_debug << "Added user alert, type " << alerts.back()->type << " ts " << alerts.back()->timestamp;

    if (catchupdone)
    {
        unb->tag = 0;
        useralertnotify.push_back(unb);
        LOG_debug << "New user alert added to notify queue";
    }
}

void UserAlerts::startprovisional()
{
    provisionalmode = true;
}

void UserAlerts::evalprovisional(handle originatinguser)
{
    provisionalmode = false;
    for (unsigned i = 0; i < provisionals.size(); ++i)
    {
        if (provisionals[i]->checkprovisional(originatinguser, &mc))
        {
            add(provisionals[i]);
        }
        else
        {
            delete provisionals[i];
        }
    }
    provisionals.clear();
}


void UserAlerts::beginNotingSharedNodes()
{
    notingSharedNodes = true;
    notedSharedNodes.clear();
}

void UserAlerts::noteSharedNode(handle user, int type, m_time_t ts, Node* n, nameid alertType)
{
    if (catchupdone && notingSharedNodes && (type == FILENODE || type == FOLDERNODE))
    {
        assert(!ISUNDEF(user));

        if (!ISUNDEF(ignoreNodesUnderShare) && (alertType != UserAlert::type_d))
        {
            // don't make alerts on files/folders already in the new share
            for (Node* p = n; p != NULL; p = p->parent)
            {
                if (p->nodehandle == ignoreNodesUnderShare)
                    return;
            }
        }

        ff& f = notedSharedNodes[make_pair(user, n ? n->parenthandle : UNDEF)];
        if (n && type == FOLDERNODE)
        {
            f.alertTypePerFolderNode[n->nodehandle] = alertType;
        }
        else if (n && type == FILENODE)
        {
            f.alertTypePerFileNode[n->nodehandle] = alertType;
        }
        // there shouldn't be any other types

        if (!f.timestamp || (ts && ts < f.timestamp))
        {
            f.timestamp = ts;
        }
    }
}

bool UserAlerts::isConvertReadyToAdd(handle originatingUser) const
{
    return catchupdone && notingSharedNodes && (originatingUser != mc.me);
}

void UserAlerts::convertNotedSharedNodes(bool added)
{
    using namespace UserAlert;
    for (notedShNodesMap::iterator i = notedSharedNodes.begin(); i != notedSharedNodes.end(); ++i)
    {
        add(added ? (Base *) new NewSharedNodes(i->first.first, i->first.second,
                                                i->second.timestamp, nextId(),
                                                i->second.alertTypePerFileNode,
                                                i->second.alertTypePerFolderNode)
            : (Base *) new RemovedSharedNode(i->first.first, m_time(), nextId(),
                                             i->second.alertTypePerFileNode,
                                             i->second.alertTypePerFolderNode));
    }
}

void UserAlerts::clearNotedSharedMembers()
{
    notedSharedNodes.clear();
    notingSharedNodes = false;
    ignoreNodesUnderShare = UNDEF;
}

// make a notification out of the shared nodes noted
void UserAlerts::convertNotedSharedNodes(bool added, handle originatingUser)
{
    if (isConvertReadyToAdd(originatingUser))
    {
        convertNotedSharedNodes(added);
    }
    clearNotedSharedMembers();
}

void UserAlerts::ignoreNextSharedNodesUnder(handle h)
{
    ignoreNodesUnderShare = h;
}

pair<bool, UserAlert::handle_alerttype_map_t::difference_type>
UserAlerts::findNotedSharedNodeIn(handle nodeHandle, const notedShNodesMap& notedSharedNodesMap) const
{
    auto it = find_if(begin(notedSharedNodesMap), end(notedSharedNodesMap),
                        [nodeHandle](const pair<pair<handle, handle>, ff>& element)
                        {
                            const auto& fileAlertTypes = element.second.alertTypePerFileNode;
                            const auto& folderAlertTypes = element.second.alertTypePerFolderNode;
                            return (std::distance(fileAlertTypes.find(nodeHandle), end(fileAlertTypes))
                                || std::distance(folderAlertTypes.find(nodeHandle), end(folderAlertTypes)));
                        });

    return make_pair(it != end(notedSharedNodesMap), std::distance(begin(notedSharedNodesMap), it));
}

bool UserAlerts::containsRemovedNodeAlert(handle nh, const UserAlert::Base* a) const
{
    const UserAlert::RemovedSharedNode* delNodeAlert = dynamic_cast<const UserAlert::RemovedSharedNode*>(a);
    if (!delNodeAlert) return false;

    return (find(begin(delNodeAlert->nodeHandles), end(delNodeAlert->nodeHandles), nh)
            != end(delNodeAlert->nodeHandles));
}

UserAlert::NewSharedNodes* UserAlerts::eraseNewNodeAlert(handle nodeHandleToRemove, UserAlert::Base* alertToCheck)
{
    UserAlert::NewSharedNodes* ptrNewNodeAlert = dynamic_cast<UserAlert::NewSharedNodes*>(alertToCheck);

    bool found = false;
    if (ptrNewNodeAlert)
    {
        auto it = find(begin(ptrNewNodeAlert->fileNodeHandles), end(ptrNewNodeAlert->fileNodeHandles),
                            nodeHandleToRemove);
        if (it != end(ptrNewNodeAlert->fileNodeHandles))
        {
            ptrNewNodeAlert->fileNodeHandles.erase(it);
            found = true;
        }
        else
        {
            it = find(begin(ptrNewNodeAlert->folderNodeHandles), end(ptrNewNodeAlert->folderNodeHandles),
                           nodeHandleToRemove);
            if (it != end(ptrNewNodeAlert->folderNodeHandles))
            {
                ptrNewNodeAlert->folderNodeHandles.erase(it);
                found = true;
            }
        }
    }

    return found ? ptrNewNodeAlert : nullptr;
}

UserAlert::RemovedSharedNode* UserAlerts::eraseRemovedNodeAlert(handle nh, UserAlert::Base* a)
{
    UserAlert::RemovedSharedNode* ret = dynamic_cast<UserAlert::RemovedSharedNode*>(a);

    if (ret)
    {
        auto it = find(begin(ret->nodeHandles), end(ret->nodeHandles), nh);
        if (it != end(ret->nodeHandles))
        {
            ret->nodeHandles.erase(it);
        }
    }

    return ret;
}

bool UserAlerts::isSharedNodeNotedAsRemoved(handle nodeHandleToFind) const
{
    // check first in the stash
    return isSharedNodeNotedAsRemovedFrom(nodeHandleToFind, deletedSharedNodesStash)
        || isSharedNodeNotedAsRemovedFrom(nodeHandleToFind, notedSharedNodes);
}

bool UserAlerts::isSharedNodeNotedAsRemovedFrom(handle nodeHandleToFind,
                                                const notedShNodesMap& notedSharedNodesMap) const
{
    using handletoalert_t = UserAlert::handle_alerttype_map_t;
    if (catchupdone && notingSharedNodes)
    {
        auto itToNotedSharedNodes = find_if(begin(notedSharedNodesMap),
                                                 end(notedSharedNodesMap),
        [nodeHandleToFind](const pair<pair<handle, handle>, ff>& element)
        {
            const handletoalert_t& fileAlertTypes = element.second.alertTypePerFileNode;
            auto itToFileNodeHandleAndAlertType = fileAlertTypes.find(nodeHandleToFind);
            const handletoalert_t& folderAlertTypes = element.second.alertTypePerFolderNode;
            auto itToFolderNodeHandleAndAlertType = folderAlertTypes.find(nodeHandleToFind);

            bool isInFileNodes = ((itToFileNodeHandleAndAlertType != end(fileAlertTypes))
                                  && (itToFileNodeHandleAndAlertType->second == UserAlert::type_d));

            // shortcircuit in case it was already found
            bool isInFolderNodes = isInFileNodes ||
                ((itToFolderNodeHandleAndAlertType != end(folderAlertTypes))
                  && (itToFolderNodeHandleAndAlertType->second == UserAlert::type_d));

            return (isInFileNodes || isInFolderNodes);
        });

        return itToNotedSharedNodes != end(notedSharedNodesMap);
    }
    return false;
}

bool UserAlerts::removeNotedSharedNodeFrom(notedShNodesMap::iterator itToStashedNodeToRemove, Node* nodeToRemove, notedShNodesMap& notedSharedNodesMap)
{
    if (itToStashedNodeToRemove != end(notedSharedNodesMap))
    {
        ff& f = itToStashedNodeToRemove->second;
        if (nodeToRemove->type == FOLDERNODE)
        {
            f.alertTypePerFolderNode.erase(nodeToRemove->nodehandle);
        }
        else if (nodeToRemove->type == FILENODE)
        {
            f.alertTypePerFileNode.erase(nodeToRemove->nodehandle);
        }
        // there shouldn't be any other type

        if (f.alertTypePerFolderNode.empty() && f.alertTypePerFileNode.empty())
        {
            notedSharedNodesMap.erase(itToStashedNodeToRemove);
        }
        return true;
    }
    return false;
}

bool UserAlerts::removeNotedSharedNodeFrom(Node* n, notedShNodesMap& notedSharedNodesMap)
{
    if (catchupdone && notingSharedNodes)
    {
        auto found = findNotedSharedNodeIn(n->nodehandle, notedSharedNodesMap);
        if (found.first)
        {
            auto it = notedSharedNodesMap.begin();
            std::advance(it, found.second);
            return removeNotedSharedNodeFrom(it, n, notedSharedNodesMap);
        }
    }
    return false;
}

bool UserAlerts::setNotedSharedNodeToUpdate(Node* nodeToChange)
{
    using handletoalert_t = UserAlert::handle_alerttype_map_t;
    // noted nodes stash contains only deleted noted nodes, thus, we only check noted nodes map
    if (catchupdone && notingSharedNodes && !notedSharedNodes.empty())
    {
        auto found = findNotedSharedNodeIn(nodeToChange->nodehandle, notedSharedNodes);
        if (!found.first) return false;
        auto itToNotedSharedNodes = notedSharedNodes.begin();
        std::advance(itToNotedSharedNodes, found.second);
        if (itToNotedSharedNodes == end(notedSharedNodes)) return false;

        add(new UserAlert::UpdatedSharedNode(itToNotedSharedNodes->first.first,
                                             itToNotedSharedNodes->second.timestamp,
                                             nextId(),
                                             handletoalert_t{{nodeToChange->nodehandle,
                                                              UserAlert::type_u}},
                                             handletoalert_t{}));
        if (removeNotedSharedNodeFrom(itToNotedSharedNodes, nodeToChange, notedSharedNodes))
        {
            LOG_debug << "Node with node handle |" << nodeToChange->nodehandle << "| removed from annotated node add-alerts and update-alert created in its place";
        }
        return true;
    }
    return false;
}

bool UserAlerts::isHandleInAlertsAsRemoved(handle nodeHandleToFind) const
{
    std::function<bool (UserAlert::Base*)> isAlertWithTypeRemoved =
        [nodeHandleToFind, this](UserAlert::Base* alertToCheck)
            {
                return containsRemovedNodeAlert(nodeHandleToFind, alertToCheck);
            };

    std::string debug_msg = "Found removal-alert with nodehandle |" + std::to_string(nodeHandleToFind) + "| in ";
    // check in existing alerts
    if (find_if(begin(alerts), end(alerts), isAlertWithTypeRemoved) != end(alerts))
    {
        LOG_debug << debug_msg << "alerts";
        return true;
    }

    // check in existing notifications meant to become alerts
    if (find_if(begin(useralertnotify), end(useralertnotify), isAlertWithTypeRemoved)
        != end(useralertnotify))
    {
        LOG_debug << debug_msg << "useralertnotify";
        return true;
    }

    // check in annotated changes pending to become notifications to become alerts
    if (isSharedNodeNotedAsRemoved(nodeHandleToFind))
    {
        LOG_debug << debug_msg << "stash or noted nodes";
        return true;
    }

    return false;
}

void UserAlerts::removeNodeAlerts(Node* nodeToRemoveAlert)
{
    if (!nodeToRemoveAlert)
    {
        LOG_err << "Unable to remove alerts for node. Empty Node* passed.";
        return;
    }

    handle nodeHandleToRemove = nodeToRemoveAlert->nodehandle;
    std::string debug_msg = "Suppressed alert for node with handle |" + std::to_string(nodeHandleToRemove) + "| found as a ";
    std::function<bool (UserAlert::Base*)> isAlertToRemove =
        // context captured as a reference to avoid copying debug_msg string
        [&](UserAlert::Base* alertToCheck)
            {
                bool ret = false; // whether the whole user alert must be deleted or not
                UserAlert::RemovedSharedNode* ptrDelNodeAlert;
                UserAlert::NewSharedNodes* ptrNewNodeAlert = eraseNewNodeAlert(nodeHandleToRemove, alertToCheck);
                if (ptrNewNodeAlert)
                {
                    ret = ptrNewNodeAlert->fileNodeHandles.empty()
                          && ptrNewNodeAlert->folderNodeHandles.empty();
                    LOG_debug << debug_msg << "new-alert type";
                }
                else if ((ptrDelNodeAlert = eraseRemovedNodeAlert(nodeHandleToRemove, alertToCheck)))
                {

                    ret = ptrDelNodeAlert->nodeHandles.empty();
                    LOG_debug << debug_msg << "removal-alert type";
                }
                return ret;
            };

    // remove from possible existing alerts
    alerts.erase(remove_if(begin(alerts), end(alerts), isAlertToRemove), end(alerts));

    // remove from possible notifications meant to become alerts
    useralertnotify.erase(remove_if(begin(useralertnotify), end(useralertnotify), isAlertToRemove)
                          , end(useralertnotify));

    // remove from annotated changes pending to become notifications to become alerts
    if (removeNotedSharedNodeFrom(nodeToRemoveAlert, deletedSharedNodesStash))
    {
        LOG_debug << debug_msg << "removal-alert in the stash";
    }
    if (removeNotedSharedNodeFrom(nodeToRemoveAlert, notedSharedNodes))
    {
        LOG_debug << debug_msg << "new-alert in noted nodes";
    }
}

void UserAlerts::setNewNodeAlertToUpdateNodeAlert(Node* nodeToUpdate)
{
    if (!nodeToUpdate)
    {
        LOG_err << "Unable to set alert new-alert node to update-alert. Empty node* passed";
        return;
    }

    handle nodeHandleToUpdate = nodeToUpdate->nodehandle;
    std::string debug_msg = "New-alert replaced by update-alert for nodehandle |"
        + std::to_string(nodeHandleToUpdate) + "|";

    vector<UserAlert::NewSharedNodes*> nodesToUpdate;
    auto isAlertToBeRemoved =
        // context captured as reference to avoid copying debug_msg string
        [&nodeHandleToUpdate, &debug_msg, &nodesToUpdate, this](UserAlert::Base* alertToCheck)
            {
                bool ret = false;
                UserAlert::NewSharedNodes* ptrNewNodeAlert = eraseNewNodeAlert(nodeHandleToUpdate, alertToCheck);
                if (ptrNewNodeAlert)
                {
                    nodesToUpdate.push_back(ptrNewNodeAlert);
                    // if there are no files or folders for this entry in the alerts, we can remove the whole alert
                    ret = ptrNewNodeAlert->fileNodeHandles.empty()
                          && ptrNewNodeAlert->folderNodeHandles.empty();
                    LOG_debug << debug_msg << " there are " << (ret ? "no " : "") << " remaining alters for this folder";
                }
                return ret;
            };
    auto createUpdateAlerts = [&nodeHandleToUpdate, &nodesToUpdate, this]()
           {
               using handletoalert_t = UserAlert::handle_alerttype_map_t;

               for(auto n : nodesToUpdate)
               {
                   // for an update alert, it doesn't matter if the node is a file or a folder
                   add(new UserAlert::UpdatedSharedNode(
                           n->userHandle, n->timestamp, nextId(),
                           handletoalert_t {{nodeHandleToUpdate, UserAlert::type_u}},
                           handletoalert_t {}));
               }
               nodesToUpdate.clear();
           };
    bool done = false;
    // remove from existing alerts
    {
        auto newEnd = remove_if(begin(alerts), end(alerts), isAlertToBeRemoved);
        if (end(alerts) != newEnd)
        {
            alerts.erase(newEnd, end(alerts));
            done = true;
        }
        createUpdateAlerts();
        if (done) return;
    }

    // remove from notifications meant to be alerts
    {
        auto newEnd = remove_if(begin(useralertnotify), end(useralertnotify), isAlertToBeRemoved);
        if (end(useralertnotify) != newEnd)
        {
            useralertnotify.erase(newEnd, end(useralertnotify));
            done = true;
        }
        createUpdateAlerts();
        if (done) return;
    }

    // remove from noted nodes pending to be notifications meant to be alerts
    if (setNotedSharedNodeToUpdate(nodeToUpdate))
    {
        LOG_debug << debug_msg << " new-alert found in noted nodes";
    }
}

void UserAlerts::convertStashedDeletedSharedNodes()
{
    notedSharedNodes = deletedSharedNodesStash;
    deletedSharedNodesStash.clear();

    convertNotedSharedNodes(false);

    clearNotedSharedMembers();
    LOG_debug << "Removal-alert noted-nodes stashed alert notifications converted to notifications";
}

bool UserAlerts::isDeletedSharedNodesStashEmpty() const
{
    return deletedSharedNodesStash.empty();
}

void UserAlerts::stashDeletedNotedSharedNodes(handle originatingUser)
{
    if (isConvertReadyToAdd(originatingUser))
    {
        deletedSharedNodesStash = notedSharedNodes;
    }

    clearNotedSharedMembers();
    LOG_debug << "Removal-alert noted-nodes alert notifications stashed";
}

// process server-client notifications
bool UserAlerts::procsc_useralert(JSON& jsonsc)
{
    for (;;)
    {
        switch (jsonsc.getnameid())
        {
        case 'u':
            if (jsonsc.enterarray())
            {
                for (;;)
                {
                    UserAlertPendingContact ul;
                    if (jsonsc.enterobject())
                    {
                        bool inobject = true;
                        while (inobject)
                        {
                            switch (jsonsc.getnameid())
                            {
                            case 'u':
                                ul.u = jsonsc.gethandle(MegaClient::USERHANDLE);
                                break;
                            case 'm':
                                jsonsc.storeobject(&ul.m);
                                break;
                            case MAKENAMEID2('m', '2'):
                                if (jsonsc.enterarray())
                                {
                                    for (;;)
                                    {
                                        string s;
                                        if (jsonsc.storeobject(&s))
                                        {
                                            ul.m2.push_back(s);
                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                    jsonsc.leavearray();
                                }
                                break;
                            case 'n':
                                jsonsc.storeobject(&ul.n);
                                break;
                            case EOO:
                                inobject = false;
                            }
                        }
                        jsonsc.leaveobject();
                        if (ul.u)
                        {
                            pendingContactUsers[ul.u] = ul;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                jsonsc.leavearray();
            }
            break;

        case MAKENAMEID3('l', 's', 'n'):
            lsn = jsonsc.gethandle(8);
            break;

        case MAKENAMEID3('f', 's', 'n'):
            fsn = jsonsc.gethandle(8);
            break;

        case MAKENAMEID3('l', 't', 'd'):   // last notifcation seen time delta (or 0)
            lastTimeDelta = jsonsc.getint();
            break;

        case EOO:
            for (Alerts::iterator i = alerts.begin(); i != alerts.end(); ++i)
            {
                UserAlert::Base* b = *i;
                b->seen = b->timestamp + lastTimeDelta < m_time();

                if (b->userEmail.empty() && b->userHandle != UNDEF)
                {
                    map<handle, UserAlertPendingContact>::iterator i = pendingContactUsers.find(b->userHandle);
                    if (i != pendingContactUsers.end())
                    {
                        b->userEmail = i->second.m;
                        if (b->userEmail.empty() && !i->second.m2.empty())
                        {
                            b->userEmail = i->second.m2[0];
                        }
                    }
                }
            }
            begincatchup = false;
            catchupdone = true;
            return true;

        case 'c':  // notifications
            if (jsonsc.enterarray())
            {
                for (;;)
                {
                    if (jsonsc.enterobject())
                    {
                        UserAlertRaw un;
                        bool inobject = true;
                        while (inobject)
                        {
                            // 't' designates type - but it appears late in the packet
                            nameid nid = jsonsc.getnameid();
                            switch (nid)
                            {

                            case 't':
                                un.t = jsonsc.getnameid();
                                break;

                            case EOO:
                                inobject = false;
                                break;

                            default:
                                // gather up the fields to interpret later as we don't know what type some are
                                // until we get the 't' field which is late in the packet
                                jsonsc.storeobject(&un.fields[nid]);
                            }
                        }

                        if (!isUnwantedAlert(un.t, un.getint('c', -1)))
                        {
                            add(un);
                        }
                        jsonsc.leaveobject();
                    }
                    else
                    {
                        break;
                    }
                }
                jsonsc.leavearray();
                break;
            }

            // fall through
        default:
            assert(false);
            if (!jsonsc.storeobject())
            {
                LOG_err << "Error parsing sc user alerts";
                begincatchup = false;
                catchupdone = true;  // if we fail to get useralerts, continue anyway
                return true;
            }
        }
    }
}

void UserAlerts::acknowledgeAll()
{
    for (Alerts::iterator i = alerts.begin(); i != alerts.end(); ++i)
    {
        if (!(*i)->seen)
        {
            (*i)->seen = true;
            if ((*i)->tag != 0)
            {
                (*i)->tag = mc.reqtag;
            }
            useralertnotify.push_back(*i);
        }
    }

    // notify the API.  Eg. on when user closes the useralerts list
    mc.reqs.add(new CommandSetLastAcknowledged(&mc));
}

void UserAlerts::onAcknowledgeReceived()
{
    if (catchupdone)
    {
        for (Alerts::iterator i = alerts.begin(); i != alerts.end(); ++i)
        {
            if (!(*i)->seen)
            {
                (*i)->seen = true;
                (*i)->tag = 0;
                useralertnotify.push_back(*i);
            }
        }
    }
}

void UserAlerts::clear()
{
    for (Alerts::iterator i = alerts.begin(); i != alerts.end(); ++i)
    {
        delete *i;
    }
    alerts.clear();
    useralertnotify.clear();
    begincatchup = false;
    catchupdone = false;
    catchup_last_timestamp = 0;
    lsn = UNDEF;
    fsn = UNDEF;
    lastTimeDelta = 0;
    nextid = 0;
}

UserAlerts::~UserAlerts()
{
    clear();
}


} // namespace
