/**
 * LoopbackTransport and protocol v1 (MultiplayerSplitPlan.md N1, N3; rules P-1
 * to P-4, P-9, S-2).
 *
 *   N1  The loopback carries reliable and unreliable messages; its dev
 *       fake-loss switch drops only unreliable ones.
 *   N3  Every message round-trips. At the protocol's maximum counts every
 *       message measures at most 2048 B and the Snapshot about 1100 B. Short,
 *       oversize, fuzzed, trailing and out-of-range input is refused, never read.
 *
 * Needs no window and no game object: only src/net/ and src/sim/.
 */

#include <net/LoopbackTransport.h>
#include <net/Protocol.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

using namespace Net;

static std::vector<std::byte> Bytes(std::span<const std::byte> span) { return { span.begin(), span.end() }; }

static std::span<const std::byte> AsBytes(const char* text)
{
	return { reinterpret_cast<const std::byte*>(text), std::strlen(text) };
}

// ---- N1 ------------------------------------------------------------------

static std::vector<NetEvent> Drain(INetTransport& transport)
{
	std::array<NetEvent, 64> events;
	const std::size_t n = transport.Poll(events);
	return { events.begin(), events.begin() + static_cast<std::ptrdiff_t>(n) };
}

static void TestLoopbackDelivers()
{
	std::printf("N1: loopback delivers reliable and unreliable messages\n");

	auto [server, client] = LoopbackTransport::CreatePair();
	Check(client->Connect("127.0.0.1:7777") == kInvalidConnection, "connecting before the server listens fails");

	server->Listen(kDefaultPort);
	const ConnectionId id = client->Connect("127.0.0.1:7777");
	Check(id != kInvalidConnection, "connecting once it listens succeeds");

	auto serverEvents = Drain(*server);
	auto clientEvents = Drain(*client);
	Check(serverEvents.size() == 1 && serverEvents[0].type == NetEventType::Connected, "the server sees Connected");
	Check(clientEvents.size() == 1 && clientEvents[0].type == NetEventType::Connected, "the client sees Connected");

	Check(client->Send(id, AsBytes("reliable"), Delivery::Reliable), "a reliable send");
	Check(client->Send(id, AsBytes("unreliable"), Delivery::Unreliable), "an unreliable send");
	serverEvents = Drain(*server);
	Check(serverEvents.size() == 2, "both arrive");
	Check(serverEvents.size() == 2 && std::string(reinterpret_cast<const char*>(serverEvents[0].payload.data()), serverEvents[0].payload.size()) == "reliable"
		&& std::string(reinterpret_cast<const char*>(serverEvents[1].payload.data()), serverEvents[1].payload.size()) == "unreliable", "in order, intact");

	server->Send(LoopbackTransport::kConnection, AsBytes("back"), Delivery::Reliable);
	clientEvents = Drain(*client);
	Check(clientEvents.size() == 1 && clientEvents[0].payload.size() == 4, "the server reaches the client too");

	// Fake loss drops unreliable messages only
	client->SetFakeLoss(0.5f, 7);
	for (int i = 0; i < 200; ++i)
	{
		client->Send(id, AsBytes("u"), Delivery::Unreliable);
		client->Send(id, AsBytes("r"), Delivery::Reliable);
	}
	serverEvents = Drain(*server);
	std::size_t reliable = 0, unreliable = 0;
	while (!serverEvents.empty())
	{
		for (const NetEvent& e : serverEvents)
		{
			if (e.payload.size() == 1 && static_cast<char>(e.payload[0]) == 'r') { ++reliable; }
			else if (e.payload.size() == 1) { ++unreliable; }
		}
		serverEvents = Drain(*server);
	}
	Check(reliable == 200, "every reliable message arrives under fake loss");
	Check(unreliable > 50 && unreliable < 150 && client->Dropped() == 200 - unreliable, "about half the unreliable ones are dropped (" + std::to_string(unreliable) + " arrived)");

	client->Disconnect(id);
	serverEvents = Drain(*server);
	Check(!serverEvents.empty() && serverEvents.back().type == NetEventType::Disconnected, "a disconnect reaches the server");
	Check(!client->Send(id, AsBytes("x"), Delivery::Reliable), "nothing is sent once disconnected");

	// The client end going away drops the connection for the server
	auto [server2, client2] = LoopbackTransport::CreatePair();
	server2->Listen(kDefaultPort);
	client2->Connect("x");
	Drain(*server2);
	client2.reset();
	serverEvents = Drain(*server2);
	Check(serverEvents.size() == 1 && serverEvents[0].type == NetEventType::Disconnected, "destroying one end disconnects the other");
}

// ---- N3 ------------------------------------------------------------------

static Limits StationLimits(int count = 10) { Limits l; l.stationCount = count; return l; }

static bool RoundTrip(const Message& in, Message& out, const Limits& limits = StationLimits())
{
	Encoder encoder;
	Decoder decoder;
	const auto bytes = Bytes(encoder.Encode(in, limits));
	if (bytes.empty()) { return false; }
	return decoder.Decode(bytes, limits, out) == DecodeError::None;
}

static void TestRoundTrips()
{
	std::printf("N3: every message round-trips\n");
	Message out;

	HelloMsg hello{ 0x0123456789abcdefULL, "Kuma" };
	Check(RoundTrip(hello, out) && std::get<HelloMsg>(out).worldHash == hello.worldHash && std::get<HelloMsg>(out).name == "Kuma", "Hello");

	WelcomeMsg welcome{ 3, 60, 4 };
	Check(RoundTrip(welcome, out) && std::get<WelcomeMsg>(out).slot == 3 && std::get<WelcomeMsg>(out).tickRate == 60 && std::get<WelcomeMsg>(out).maxPlayers == 4, "Welcome");

	Check(RoundTrip(RejectMsg{ RejectReason::WorldMismatch }, out) && std::get<RejectMsg>(out).reason == RejectReason::WorldMismatch, "Reject");

	CommandsMsg commands;
	commands.count = 3;
	for (int i = 0; i < 3; ++i)
	{
		auto& c = commands.commands[static_cast<std::size_t>(i)];
		c.sequence = 100u + static_cast<std::uint32_t>(i);
		c.clientTick = 7;
		c.moveX = -1; c.moveY = 1;
		c.buttons = Sim::BUTTON_JUMP | Sim::BUTTON_SPRINT;
		c.yawDelta = 0.25f; c.pitchDelta = -0.5f;
	}
	const bool commandsOk = RoundTrip(commands, out);
	Check(commandsOk, "Commands");
	if (commandsOk)
	{
		const auto& c = std::get<CommandsMsg>(out);
		Check(c.count == 3 && c.commands[2].sequence == 102 && c.commands[0].moveX == -1 && c.commands[0].moveY == 1
			&& c.commands[0].buttons == (Sim::BUTTON_JUMP | Sim::BUTTON_SPRINT) && c.commands[1].yawDelta == 0.25f && c.commands[1].pitchDelta == -0.5f,
			"and every field survives");
	}

	SnapshotMsg snapshot;
	snapshot.serverTick = 123456; snapshot.ackSeq = 99; snapshot.roundEpoch = 2;
	snapshot.playerCount = 2;
	snapshot.players[0] = { 0, 1.0f, 2.0f, 3.0f, 0.5f, -0.25f, true, false };
	snapshot.players[1] = { 3, -4.0f, 5.0f, 6.0f, 1.0f, 0.0f, false, true };
	snapshot.hasStalker = true;
	snapshot.stalker = { 7.0f, 0.5f, -7.0f, 3.1f, 2, StalkerGait::Sprint };
	snapshot.stationCount = 10;
	snapshot.stations[4] = { true, -1 };
	snapshot.stations[5] = { false, 3 };
	const bool snapOk = RoundTrip(snapshot, out);
	Check(snapOk, "Snapshot");
	if (snapOk)
	{
		const auto& s = std::get<SnapshotMsg>(out);
		Check(s.serverTick == 123456 && s.ackSeq == 99 && s.roundEpoch == 2 && s.playerCount == 2, "its header fields survive");
		Check(s.players[1].slot == 3 && s.players[1].x == -4.0f && s.players[1].crouching && s.players[0].flashlight, "its players survive");
		Check(s.hasStalker && s.stalker.state == 2 && s.stalker.gait == StalkerGait::Sprint && s.stalker.yaw == 3.1f, "its stalker survives");
		Check(s.stations[4].completed && s.stations[4].holder == -1 && s.stations[5].holder == 3 && !s.stations[0].completed, "its stations survive, in table order");
	}

	Check(RoundTrip(StationRequestMsg{ StationAction::Complete, 9 }, out) && std::get<StationRequestMsg>(out).action == StationAction::Complete && std::get<StationRequestMsg>(out).stationIndex == 9, "StationRequest");
	Check(RoundTrip(StationEventMsg{ Sim::StationEventKind::Denied, 0, 15 }, out) && std::get<StationEventMsg>(out).kind == Sim::StationEventKind::Denied && std::get<StationEventMsg>(out).slot == 15, "StationEvent");
	Check(RoundTrip(PlayerCaughtMsg{ 2 }, out) && std::get<PlayerCaughtMsg>(out).slot == 2, "PlayerCaught");
	Check(RoundTrip(RoundOverMsg{ Sim::RoundOutcome::Escaped, 41 }, out) && std::get<RoundOverMsg>(out).outcome == Sim::RoundOutcome::Escaped && std::get<RoundOverMsg>(out).roundEpoch == 41, "RoundOver");
}

static void TestSizes()
{
	std::printf("N3: sizes at the maximum counts (MeasureStream)\n");
	for (int t = kFirstMsgType; t <= kLastMsgType; ++t)
	{
		const std::size_t bytes = MeasureMaxBytes(static_cast<MsgType>(t));
		std::printf("  type %d: %zu B\n", t, bytes);
		Check(bytes >= kHeaderBytes && bytes <= kMaxMessage, "message type " + std::to_string(t) + " fits in 2048 B");
	}
	const std::size_t snapshot = MeasureMaxBytes(MsgType::Snapshot);
	Check(snapshot <= 1100, "the Snapshot at 16 players and 64 stations is at most ~1100 B (" + std::to_string(snapshot) + ")");
}

static void TestRefusals()
{
	std::printf("N3: short, oversize, trailing, fuzzed and out-of-range input is refused\n");
	Decoder decoder;
	Encoder encoder;
	Message out;
	const Limits limits = StationLimits();

	std::byte two[2] = {};
	Check(decoder.Decode(std::span<const std::byte>(two, 2), limits, out) == DecodeError::TooShort, "under 3 bytes");
	std::vector<std::byte> huge(kMaxMessage + 1);
	Check(decoder.Decode(huge, limits, out) == DecodeError::TooLong, "over 2048 bytes");

	std::vector<std::byte> unknown(8);
	unknown[0] = std::byte{ 0x7f };
	Check(decoder.Decode(unknown, limits, out) == DecodeError::UnknownType, "an unknown type");

	// A byte appended after a valid message: serialize_end refuses it
	auto valid = Bytes(encoder.Encode(PlayerCaughtMsg{ 1 }, limits));
	valid.push_back(std::byte{ 0 });
	Check(decoder.Decode(valid, limits, out) == DecodeError::Malformed, "a trailing byte");

	// A truncated Snapshot
	SnapshotMsg snapshot;
	snapshot.playerCount = 1;
	snapshot.stationCount = 10;
	auto snap = Bytes(encoder.Encode(snapshot, limits));
	snap.resize(snap.size() - 3);
	Check(decoder.Decode(snap, limits, out) != DecodeError::None, "a truncated Snapshot");

	// A Snapshot built for another StationTable
	auto other = Bytes(encoder.Encode(snapshot, limits));
	Check(decoder.Decode(other, StationLimits(9), out) != DecodeError::None, "a Snapshot with another station count");

	// A station index past the receiver's table
	auto request = Bytes(encoder.Encode(StationRequestMsg{ StationAction::Claim, 9 }, StationLimits(10)));
	Check(decoder.Decode(request, StationLimits(4), out) != DecodeError::None, "a station index outside the table");
	Check(decoder.Decode(request, StationLimits(0), out) != DecodeError::None, "any station index when there are no stations");

	// The encoder refuses what breaks the rules instead of writing it (P-3)
	Check(encoder.Encode(StationRequestMsg{ StationAction::Claim, 10 }, StationLimits(10)).empty(), "encoding an out-of-range station index is refused");
	Check(encoder.Encode(PlayerCaughtMsg{ Sim::kMaxPlayers }, limits).empty(), "encoding a slot past the ceiling is refused");
	Check(encoder.Encode(WelcomeMsg{ 4, 60, 4 }, limits).empty(), "a Welcome slot at or past maxPlayers is refused");
	Check(encoder.Encode(HelloMsg{ 1, std::string(25, 'a') }, limits).empty(), "a name over 24 bytes is refused");
	Check(encoder.Encode(HelloMsg{ 1, "bad\nname" }, limits).empty(), "a name with a control character is refused");
	Check(encoder.Encode(RoundOverMsg{ Sim::RoundOutcome::Ongoing, 0 }, limits).empty(), "RoundOver(Ongoing) is never sent");
	CommandsMsg none;
	Check(encoder.Encode(none, limits).empty(), "Commands with no command are refused");

	// Non-finite floats are replaced before encoding (P-4)
	SnapshotMsg nan;
	nan.playerCount = 1;
	nan.players[0].x = NAN;
	nan.players[0].yaw = INFINITY;
	nan.stationCount = 10;
	const auto nanBytes = Bytes(encoder.Encode(nan, limits));
	Check(!nanBytes.empty() && encoder.FloatsReplaced() == 2, "two non-finite floats are replaced");
	Check(decoder.Decode(nanBytes, limits, out) == DecodeError::None && std::get<SnapshotMsg>(out).players[0].x == 0.0f, "and arrive as 0");

	// Fuzz: random bytes never crash the decoder, and almost never decode
	std::mt19937 random(12345);
	int accepted = 0;
	for (int i = 0; i < 20000; ++i)
	{
		std::vector<std::byte> junk(1 + random() % 300);
		for (auto& b : junk) { b = static_cast<std::byte>(random() & 0xFF); }
		if (decoder.Decode(junk, limits, out) == DecodeError::None) { ++accepted; }
	}
	Check(accepted < 200, "20,000 fuzzed messages decode without a crash (" + std::to_string(accepted) + " happened to be well-formed)");

	// Flipping single bits in a real Snapshot is refused or decodes in range
	auto real = Bytes(encoder.Encode(snapshot, limits));
	bool allInRange = true;
	for (std::size_t bit = 0; bit < real.size() * 8; ++bit)
	{
		auto flipped = real;
		flipped[bit / 8] ^= static_cast<std::byte>(1u << (bit % 8));
		if (decoder.Decode(flipped, limits, out) == DecodeError::None)
		{
			if (const auto* s = std::get_if<SnapshotMsg>(&out))
			{
				for (int p = 0; p < s->playerCount; ++p) { if (!Sim::IsValidSlot(s->players[static_cast<std::size_t>(p)].slot)) { allInRange = false; } }
				for (int t = 0; t < s->stationCount; ++t) { if (s->stations[static_cast<std::size_t>(t)].holder < -1 || s->stations[static_cast<std::size_t>(t)].holder >= Sim::kMaxPlayers) { allInRange = false; } }
			}
		}
	}
	Check(allInRange, "a bit-flipped Snapshot never decodes an out-of-range slot or holder");
}

static void TestVersions()
{
	std::printf("P-1: the header is frozen; versions are checked first\n");
	Encoder encoder;
	Decoder decoder;
	Message out;
	const Limits limits = StationLimits();

	auto hello = Bytes(encoder.Encode(HelloMsg{ 42, "x" }, limits));
	Header header;
	Check(PeekHeader(hello, header) && header.type == MsgType::Hello && header.version == kProtocolVersion, "the header reads back as Hello, v1");

	// The same Hello from a version 2 client: bits 8..23 carry the version
	hello[1] = std::byte{ 2 };
	hello[2] = std::byte{ 0 };
	Check(PeekHeader(hello, header) && header.version == 2, "a v2 header is still read");
	Check(decoder.Decode(hello, limits, out, &header) == DecodeError::VersionMismatch && header.type == MsgType::Hello, "a v2 Hello is refused as VersionMismatch, and the server knows it was a Hello");

	// A Reject from another version is still read: its layout is frozen
	auto reject = Bytes(encoder.Encode(RejectMsg{ RejectReason::VersionMismatch }, limits));
	reject[1] = std::byte{ 9 };
	Check(decoder.Decode(reject, limits, out) == DecodeError::None && std::get<RejectMsg>(out).reason == RejectReason::VersionMismatch, "a Reject from any version is read");
}

int main()
{
	std::printf("Net protocol tests\n\n");

	TestLoopbackDelivers();
	TestRoundTrips();
	TestSizes();
	TestRefusals();
	TestVersions();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
