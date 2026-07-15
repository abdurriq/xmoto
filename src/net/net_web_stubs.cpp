/*
 * net_web_stubs.cpp — no-op singleton implementations for the web build.
 *
 * When ENABLE_NETWORK=0 (Emscripten/browser target), all net/ source files
 * are replaced by this single file.  Every method returns a safe default
 * value so the rest of the codebase can call them without branching.
 */

#include "NetClient.h"
#include "NetServer.h"
#include "NetActions.h"
#include "ActionReader.h"

/* -------------------------------------------------------------------------
   NetClient
   ------------------------------------------------------------------------ */
NetClient::NetClient()
    : m_isConnected(false), m_serverReceivesUdp(false),
      m_serverSendsUdp(false), m_universe(nullptr),
      m_mode(NETCLIENT_GHOST_MODE), m_points(0),
      m_udpSendPacket(nullptr), m_tcpReader(nullptr),
      m_lastOwnFPS(0), m_currentOwnFramesNb(0), m_currentOwnFramesTime(0),
      m_otherClientsLevelsList(nullptr) {
  m_lastPing.id = -1;
  m_lastPing.pingTime = -1;
  m_lastPing.pongTime = -1;
}

NetClient::~NetClient() {}

void NetClient::fastConnectDisconnect(const std::string &, int) {}
void NetClient::connect(const std::string &, int) {}
void NetClient::disconnect() {}
bool NetClient::isConnected() { return false; }
void NetClient::send(NetAction *, int, bool) {}
void NetClient::manageNetwork(int, xmDatabase *) {}
void NetClient::changeMode(NetClientMode) {}
int  NetClient::points() const { return 0; }
void NetClient::startPlay(Universe *) {}
bool NetClient::isPlayInitialized() { return false; }
void NetClient::endPlay() {}
void NetClient::getOtherClientsNameList(std::vector<std::string> &,
                                        const std::string &) {}
void NetClient::addChatTransformations(std::vector<std::string> &,
                                       const std::string &) {}
std::string NetClient::getDisplayMessage(const std::string &, const std::string &) { return ""; }
std::vector<NetOtherClient *> &NetClient::otherClients() {
  static std::vector<NetOtherClient *> v; return v;
}
void NetClient::fillPrivatePeople(const std::string &,
                                  const std::string &,
                                  std::vector<int> &,
                                  std::vector<std::string> &) {}
int  NetClient::getOwnFrameFPS() const { return 0; }
VirtualNetLevelsList *NetClient::getOtherClientLevelsList(xmDatabase *) { return nullptr; }
void NetClient::memoriesPP(const std::vector<int> &) {}
std::string NetClient::udpBindKey() const { return ""; }
UDPpacket  *NetClient::sendPacket()  { return nullptr; }
TCPsocket  *NetClient::tcpSocket()   { return nullptr; }
UDPsocket  *NetClient::udpSocket()   { return nullptr; }
unsigned int NetClient::getOtherClientNumberById(int) const { return 0; }
void NetClient::updateOtherClientsMode(std::vector<int>) {}
void NetClient::manageNetworkOneStep(int, xmDatabase *) {}
void NetClient::openListenConnectionGroup() {}
void NetClient::closeListenConnectionGroup() {}
void NetClient::manageAction(xmDatabase *, NetAction *) {}
void NetClient::cleanOtherClientsGhosts() {}

/* -------------------------------------------------------------------------
   NetServer
   ------------------------------------------------------------------------ */
NetServer::NetServer() : m_isStarted(false), m_serverThread(nullptr) {}
NetServer::~NetServer() {}
void NetServer::start(bool, int, const std::string &) {}
void NetServer::stop() {}
bool NetServer::isStarted()       { return false; }
void NetServer::wait()            {}
bool NetServer::acceptConnections() { return false; }
void NetServer::setStandAloneOptions() {}

/* -------------------------------------------------------------------------
   NetAction base class and static members
   ------------------------------------------------------------------------ */
unsigned int NetAction::m_biggestTCPPacketSent = 0;
unsigned int NetAction::m_biggestUDPPacketSent = 0;
unsigned int NetAction::m_nbTCPPacketsSent     = 0;
unsigned int NetAction::m_nbUDPPacketsSent     = 0;
unsigned int NetAction::m_TCPPacketsSizeSent   = 0;
unsigned int NetAction::m_UDPPacketsSizeSent   = 0;

NetAction::NetAction(bool i_forceTcp)
    : m_source(-2), m_subsource(-2), m_forceTCP(i_forceTcp) {}
NetAction::~NetAction() {}
void NetAction::send(TCPsocket *, UDPsocket *, UDPpacket *, IPaddress *) {}
void NetAction::logStats()   {}
void ActionReader::logStats() {}

/* Static member definitions required by ODR */
std::string NA_udpBind::ActionKey            = "udpbind";
std::string NA_udpBindQuery::ActionKey       = "udpbindingQuery";
std::string NA_udpBindValidation::ActionKey  = "udpbindingValidation";
std::string NA_clientInfos::ActionKey        = "clientInfos";
std::string NA_chatMessagePP::ActionKey      = "messagePP";
std::string NA_chatMessage::ActionKey        = "message";
std::string NA_serverError::ActionKey        = "serverError";
std::string NA_frame::ActionKey              = "f";
std::string NA_changeName::ActionKey         = "changeName";
std::string NA_clientsNumber::ActionKey      = "clientsNumber";
std::string NA_clientsNumberQuery::ActionKey = "clientsNumberQ";
std::string NA_playingLevel::ActionKey       = "playingLevel";
std::string NA_changeClients::ActionKey      = "changeClients";
std::string NA_slaveClientsPoints::ActionKey = "scpoints";
std::string NA_playerControl::ActionKey      = "c";
std::string NA_clientMode::ActionKey         = "clientMode";
std::string NA_prepareToPlay::ActionKey      = "prepareToPlay";
std::string NA_prepareToGo::ActionKey        = "prepareToGo";

NetActionType NA_udpBind::NAType            = TNA_udpBind;
NetActionType NA_udpBindQuery::NAType       = TNA_udpBindQuery;
NetActionType NA_udpBindValidation::NAType  = TNA_udpBindValidation;
NetActionType NA_clientInfos::NAType        = TNA_clientInfos;
NetActionType NA_chatMessagePP::NAType      = TNA_chatMessagePP;
NetActionType NA_chatMessage::NAType        = TNA_chatMessage;
NetActionType NA_serverError::NAType        = TNA_serverError;
NetActionType NA_frame::NAType              = TNA_frame;
NetActionType NA_changeName::NAType         = TNA_changeName;
NetActionType NA_clientsNumber::NAType      = TNA_clientsNumber;
NetActionType NA_clientsNumberQuery::NAType = TNA_clientsNumberQuery;
NetActionType NA_playingLevel::NAType       = TNA_playingLevel;
NetActionType NA_changeClients::NAType      = TNA_changeClients;
NetActionType NA_slaveClientsPoints::NAType = TNA_slaveClientsPoints;
NetActionType NA_playerControl::NAType      = TNA_playerControl;
NetActionType NA_clientMode::NAType         = TNA_clientMode;
NetActionType NA_prepareToPlay::NAType      = TNA_prepareToPlay;
NetActionType NA_prepareToGo::NAType        = TNA_prepareToGo;

/* Minimal constructor / destructor / send() stubs — enough for the linker */
#define NA_STUB_SEND(cls) \
  void cls::send(TCPsocket *, UDPsocket *, UDPpacket *, IPaddress *) {}

NA_udpBind::NA_udpBind(const std::string &k) : NetAction(false), m_key(k) {}
NA_udpBind::NA_udpBind(void *, unsigned int) : NetAction(false) {}
NA_udpBind::~NA_udpBind() {}
NA_STUB_SEND(NA_udpBind)

NA_udpBindQuery::NA_udpBindQuery() : NetAction(false) {}
NA_udpBindQuery::NA_udpBindQuery(void *, unsigned int) : NetAction(false) {}
NA_udpBindQuery::~NA_udpBindQuery() {}
NA_STUB_SEND(NA_udpBindQuery)

NA_udpBindValidation::NA_udpBindValidation() : NetAction(false) {}
NA_udpBindValidation::NA_udpBindValidation(void *, unsigned int) : NetAction(false) {}
NA_udpBindValidation::~NA_udpBindValidation() {}
NA_STUB_SEND(NA_udpBindValidation)

NA_clientInfos::NA_clientInfos(int v, const std::string &k)
    : NetAction(true), m_protocolVersion(v), m_udpBindKey(k) {}
NA_clientInfos::NA_clientInfos(void *, unsigned int)
    : NetAction(true), m_protocolVersion(0) {}
NA_clientInfos::~NA_clientInfos() {}
NA_STUB_SEND(NA_clientInfos)

NA_chatMessagePP::NA_chatMessagePP(const std::string &, const std::string &,
                                   const std::vector<int> &)
    : NetAction(true) {}
NA_chatMessagePP::NA_chatMessagePP(void *, unsigned int) : NetAction(true) {}
NA_chatMessagePP::~NA_chatMessagePP() {}
NA_STUB_SEND(NA_chatMessagePP)
std::string NA_chatMessagePP::getMessage() { return ""; }
const std::vector<int> &NA_chatMessagePP::privatePeople() const {
  static std::vector<int> v; return v;
}
void NA_chatMessagePP::ttransform(const std::string &) {}

NA_chatMessage::NA_chatMessage(const std::string &, const std::string &)
    : NetAction(true) {}
NA_chatMessage::NA_chatMessage(void *, unsigned int) : NetAction(true) {}
NA_chatMessage::~NA_chatMessage() {}
NA_STUB_SEND(NA_chatMessage)
void NA_chatMessage::ttransform(const std::string &) {}
std::string NA_chatMessage::getMessage() { return ""; }

NA_serverError::NA_serverError(const std::string &msg)
    : NetAction(true), m_msg(msg) {}
NA_serverError::NA_serverError(void *, unsigned int) : NetAction(true) {}
NA_serverError::~NA_serverError() {}
NA_STUB_SEND(NA_serverError)
std::string NA_serverError::getMessage() { return m_msg; }

NA_frame::NA_frame(SerializedBikeState *) : NetAction(false) {}
NA_frame::NA_frame(void *, unsigned int) : NetAction(false) {}
NA_frame::~NA_frame() {}
NA_STUB_SEND(NA_frame)
SerializedBikeState *NA_frame::getState() { return nullptr; }

NA_changeName::NA_changeName(const std::string &n)
    : NetAction(true), m_name(n) {}
NA_changeName::NA_changeName(void *, unsigned int) : NetAction(true) {}
NA_changeName::~NA_changeName() {}
NA_STUB_SEND(NA_changeName)
std::string NA_changeName::getName() { return m_name; }

NA_clientsNumber::NA_clientsNumber(int a, int b, int c, int d)
    : NetAction(false), m_ntcp(a), m_nudp(b), m_nghosts(c), m_nslaves(d) {}
NA_clientsNumber::NA_clientsNumber(void *, unsigned int)
    : NetAction(false), m_ntcp(0), m_nudp(0), m_nghosts(0), m_nslaves(0) {}
NA_clientsNumber::~NA_clientsNumber() {}
NA_STUB_SEND(NA_clientsNumber)

NA_clientsNumberQuery::NA_clientsNumberQuery() : NetAction(false) {}
NA_clientsNumberQuery::NA_clientsNumberQuery(void *, unsigned int)
    : NetAction(false) {}
NA_clientsNumberQuery::~NA_clientsNumberQuery() {}
NA_STUB_SEND(NA_clientsNumberQuery)

NA_playingLevel::NA_playingLevel(const std::string &lvl)
    : NetAction(false), m_levelId(lvl) {}
NA_playingLevel::NA_playingLevel(void *, unsigned int) : NetAction(false) {}
NA_playingLevel::~NA_playingLevel() {}
NA_STUB_SEND(NA_playingLevel)
std::string NA_playingLevel::getLevelId() { return m_levelId; }

NA_changeClients::NA_changeClients() : NetAction(true) {}
NA_changeClients::NA_changeClients(void *, unsigned int) : NetAction(true) {}
NA_changeClients::~NA_changeClients() {}
NA_STUB_SEND(NA_changeClients)

NA_slaveClientsPoints::NA_slaveClientsPoints() : NetAction(true) {}
NA_slaveClientsPoints::NA_slaveClientsPoints(void *, unsigned int)
    : NetAction(true) {}
NA_slaveClientsPoints::~NA_slaveClientsPoints() {}
NA_STUB_SEND(NA_slaveClientsPoints)

NA_playerControl::NA_playerControl(PlayerControl pc, float val)
    : NetAction(false), m_control(pc), m_value(val) {}
NA_playerControl::NA_playerControl(PlayerControl pc, bool val)
    : NetAction(false), m_control(pc), m_value(val ? 1.0f : 0.0f) {}
NA_playerControl::NA_playerControl(void *, unsigned int)
    : NetAction(false), m_control(PC_CHANGEDIR), m_value(0.0f) {}
NA_playerControl::~NA_playerControl() {}
NA_STUB_SEND(NA_playerControl)
PlayerControl NA_playerControl::getType()       { return m_control; }
float         NA_playerControl::getFloatValue() { return m_value; }

NA_clientMode::NA_clientMode(NetClientMode m)
    : NetAction(true), m_mode(m) {}
NA_clientMode::NA_clientMode(void *, unsigned int)
    : NetAction(true), m_mode(NETCLIENT_GHOST_MODE) {}
NA_clientMode::~NA_clientMode() {}
NA_STUB_SEND(NA_clientMode)

/* NA_prepareToPlay: default ctor is inline in header; only the non-default ones need stubs */
NA_prepareToPlay::NA_prepareToPlay(const std::string &lvl,
                                   std::vector<int> &players)
    : NetAction(false), m_id_level(lvl), m_players(players) {}
NA_prepareToPlay::NA_prepareToPlay(void *, unsigned int) : NetAction(false) {}
NA_prepareToPlay::~NA_prepareToPlay() {}
NA_STUB_SEND(NA_prepareToPlay)
std::string NA_prepareToPlay::idLevel() const { return m_id_level; }
const std::vector<int> &NA_prepareToPlay::players() { return m_players; }

NA_prepareToGo::NA_prepareToGo(int t) : NetAction(false), m_time(t) {}
NA_prepareToGo::NA_prepareToGo(void *, unsigned int) : NetAction(false), m_time(0) {}
NA_prepareToGo::~NA_prepareToGo() {}
NA_STUB_SEND(NA_prepareToGo)
int NA_prepareToGo::time() const { return m_time; }

/* -------------------------------------------------------------------------
   Additional NA_* classes needed by ActionReader's pre-allocated NA set
   ------------------------------------------------------------------------ */
std::string NA_killAlert::ActionKey  = "killAlert";
std::string NA_gameEvents::ActionKey = "e";
std::string NA_srvCmd::ActionKey     = "srvCmd";
std::string NA_srvCmdAsw::ActionKey  = "srvCmdAsw";
std::string NA_ping::ActionKey       = "ping";

NetActionType NA_killAlert::NAType  = TNA_killAlert;
NetActionType NA_gameEvents::NAType = TNA_gameEvents;
NetActionType NA_srvCmd::NAType     = TNA_srvCmd;
NetActionType NA_srvCmdAsw::NAType  = TNA_srvCmdAsw;
NetActionType NA_ping::NAType       = TNA_ping;

NA_killAlert::NA_killAlert(int t) : NetAction(true), m_time(t) {}
NA_killAlert::NA_killAlert(void *, unsigned int) : NetAction(true), m_time(0) {}
NA_killAlert::~NA_killAlert() {}
NA_STUB_SEND(NA_killAlert)
int NA_killAlert::time() const { return m_time; }

NA_gameEvents::NA_gameEvents(DBuffer *) : NetAction(false) {}
NA_gameEvents::NA_gameEvents(void *, unsigned int) : NetAction(false) {}
NA_gameEvents::~NA_gameEvents() {}
NA_STUB_SEND(NA_gameEvents)

NA_srvCmd::NA_srvCmd(const std::string &cmd) : NetAction(true), m_cmd(cmd) {}
NA_srvCmd::NA_srvCmd(void *, unsigned int) : NetAction(true) {}
NA_srvCmd::~NA_srvCmd() {}
NA_STUB_SEND(NA_srvCmd)

NA_srvCmdAsw::NA_srvCmdAsw(const std::string &asw) : NetAction(true), m_answer(asw) {}
NA_srvCmdAsw::NA_srvCmdAsw(void *, unsigned int) : NetAction(true) {}
NA_srvCmdAsw::~NA_srvCmdAsw() {}
NA_STUB_SEND(NA_srvCmdAsw)

NA_ping::NA_ping(NA_ping *) : NetAction(false) {}
NA_ping::NA_ping(void *, unsigned int) : NetAction(false) {}
NA_ping::~NA_ping() {}
NA_STUB_SEND(NA_ping)

/* -------------------------------------------------------------------------
   NetOtherClient stubs
   ------------------------------------------------------------------------ */
NetOtherClient::NetOtherClient(int id, const std::string &name)
    : m_id(id), m_name(name), m_netMode(NETCLIENT_GHOST_MODE), m_points(0) {
  for (int i = 0; i < NETACTION_MAX_SUBSRC; i++) m_ghosts[i] = nullptr;
}
NetOtherClient::~NetOtherClient() {}
int NetOtherClient::id() const { return m_id; }
std::string NetOtherClient::name() const { return m_name; }
NetClientMode NetOtherClient::mode() const { return m_netMode; }
void NetOtherClient::setName(const std::string &n) { m_name = n; }
void NetOtherClient::setMode(NetClientMode m) { m_netMode = m; }
std::string NetOtherClient::lastPlayingLevelId() { return ""; }
void NetOtherClient::setPlayingLevelId(xmDatabase *, const std::string &) {}
std::string NetOtherClient::playingLevelName() { return ""; }
NetGhost *NetOtherClient::netGhost(unsigned int) { return nullptr; }
void NetOtherClient::setNetGhost(unsigned int, NetGhost *) {}
int NetOtherClient::points() const { return m_points; }
void NetOtherClient::setPoints(int p) { m_points = p; }

/* Additional NetClient methods referenced from states */
NetClientMode NetClient::mode() const { return NETCLIENT_GHOST_MODE; }
std::string NetClient::getMemoriedPPAsString(const std::string &) { return ""; }
