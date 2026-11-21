/**
 * The Host's SpacetimeDB adapter (MultiplayerSplitPlan.md W3; rules W-8, W-10).
 *
 * Runs against the Host PC's local SpacetimeDB with the veil_world module
 * published (spacetime/start_spacetime.ps1, spacetime/publish_veil_world.ps1).
 * With no server answering on 127.0.0.1:3000 it says so and passes without
 * checking anything, so the suite never fails on a machine without it.
 *
 *   - An identity is minted once and reused from the token file after that.
 *   - A token file that exists but can't be read fails loudly and is never
 *     replaced by a newly minted identity.
 *   - One /sql read of the world row comes back in the expected shape.
 *
 * This file includes no raylib header: it must link beside cpp-httplib.
 */

#include <net/spacetime/SpacetimeHttp.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

namespace st = Net::Spacetime;

static const char* kDatabase = "veil-world";

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::printf("SpacetimeDB adapter tests\n\n");

	st::HttpClient client("127.0.0.1", 3000);
	const st::HttpResult ping = client.Sql(kDatabase, "SELECT * FROM world WHERE world_key = 'none:0000000000000000'", "");
	if (!ping.transportOk)
	{
		std::printf("  SKIPPED: no SpacetimeDB on 127.0.0.1:3000 (%s)\n", ping.transportError.c_str());
		std::printf("\n0 checks, 0 failures\n");
		return 0;
	}

	const std::filesystem::path dir = std::filesystem::temp_directory_path() / "veil-spacetime-tests";
	std::error_code ignored;
	std::filesystem::remove_all(dir, ignored);
	std::filesystem::create_directories(dir);
	const std::filesystem::path file = dir / "dev_identity.json";

	std::printf("an identity is minted once, then reused\n");
	st::Identity first;
	std::string error;
	Check(st::ObtainIdentity(client, file, first, error) == st::Obtained::Minted, "the first start mints an identity (" + error + ")");
	Check(std::filesystem::exists(file) && !std::filesystem::exists(dir / "dev_identity.json.tmp"), "and writes the file atomically, leaving no temp file");

	st::Identity second;
	Check(st::ObtainIdentity(client, file, second, error) == st::Obtained::Reused, "the next start reuses it");
	Check(second.identity == first.identity && second.token == first.token, "the same identity and token");

	std::printf("an unreadable token file fails loudly\n");
	const std::filesystem::path broken = dir / "broken.json";
	{ std::ofstream out(broken); out << "{ not json"; }
	st::Identity unused;
	error.clear();
	Check(st::ObtainIdentity(client, broken, unused, error) == st::Obtained::Failed, "a corrupt file is a failure");
	Check(!error.empty() && error.find(first.token) == std::string::npos, "with a reason that never contains a token");
	std::string contents;
	{
		// Closed again before the directory is removed: Windows can't delete an open file
		std::ifstream still(broken);
		contents.assign((std::istreambuf_iterator<char>(still)), std::istreambuf_iterator<char>());
	}
	Check(contents == "{ not json", "and the file is left exactly as it was: no silent new identity");

	std::printf("open_world, then one /sql read\n");
	const auto now = std::chrono::system_clock::now().time_since_epoch().count();
	char hex[17];
	std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(now));
	const std::string key = std::string("veil_http_test:") + hex;

	const st::HttpResult opened = client.CallReducer(kDatabase, "open_world", nlohmann::json::array({ key }), first.token);
	Check(opened.transportOk && opened.status == 200, "open_world answers 200 (got " + std::to_string(opened.status) + " " + opened.body + ")");

	const st::HttpResult read = client.Sql(kDatabase, "SELECT * FROM world WHERE world_key = '" + key + "'", first.token);
	std::vector<std::string> columns;
	std::vector<nlohmann::json> rows;
	Check(read.transportOk && read.status == 200 && st::ParseSqlRows(read.body, columns, rows), "the /sql read parses");
	Check(columns.size() == 2 && columns[0] == "world_key" && columns[1] == "owner", "the world table's columns are world_key, owner");
	Check(rows.size() == 1 && rows[0][0] == key, "and holds the world just opened");

	const st::HttpResult bad = client.CallReducer(kDatabase, "open_world", nlohmann::json::array({ "Not A Key" }), first.token);
	Check(bad.transportOk && bad.status == 530 && bad.body.find("invalid world key") != std::string::npos, "a reducer's Err comes back as 530 with its message");

	const st::HttpResult noToken = client.CallReducer(kDatabase, "open_world", nlohmann::json::array({ key }), "garbage");
	Check(noToken.transportOk && noToken.status == 401, "a bad token is 401");

	Check(!st::ParseSqlRows("not json", columns, rows) && !st::ParseSqlRows("[]", columns, rows) && !st::ParseSqlRows("[{\"rows\":[]}]", columns, rows), "malformed /sql bodies are refused, never read as empty");

	std::filesystem::remove_all(dir, ignored);

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
