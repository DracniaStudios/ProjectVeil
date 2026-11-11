#pragma once
#ifndef NET_SPACETIME_HTTP_H
#define NET_SPACETIME_HTTP_H

// The Host's only line to SpacetimeDB: its HTTP API, over plain HTTP to
// 127.0.0.1 (MultiplayerSplitPlan.md Q54, S3). Called only from the World
// store's worker thread, never from the tick (rule R8).
//
// Rule R8: no SpacetimeDB or HTTP-library type outside src/net/spacetime/. This
// header holds none either (the client is a pimpl), so including it never
// drags <windows.h> in with cpp-httplib.

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Net::Spacetime {

	// One HTTP exchange. A transport failure (refused, timed out, stopped) has
	// transportOk false and no status.
	struct HttpResult
	{
		bool transportOk = false;
		int status = 0;
		std::string body;
		std::string transportError;
	};

	// The Host's SpacetimeDB identity and its bearer token. The token is a secret:
	// it is never logged (W-10).
	struct Identity
	{
		std::string identity;
		std::string token;
	};

	class HttpClient
	{
	public:
		HttpClient(std::string host, int port);
		~HttpClient();
		HttpClient(const HttpClient&) = delete;
		HttpClient& operator=(const HttpClient&) = delete;

		// POST /v1/identity: mints a new identity and its token
		HttpResult MintIdentity();

		// POST /v1/database/<database>/call/<reducer>?confirmed=true, with the
		// arguments as a JSON array built by nlohmann::json, never by
		// concatenation (W-8)
		HttpResult CallReducer(std::string_view database, std::string_view reducer, const nlohmann::json& arguments, std::string_view token);

		// POST /v1/database/<database>/sql
		HttpResult Sql(std::string_view database, std::string_view query, std::string_view token);

		// Stops the request in flight, from another thread (W-7). Requests after
		// this fail as transport errors.
		void Stop();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};

	// Parses the body of POST /v1/identity
	bool ParseIdentity(std::string_view body, Identity& out);

	/** The token file (W-10) **/
	enum class IdentityFile { Loaded, Missing, Unreadable };

	// Reads the identity file. A file that exists but can't be read or parsed is
	// Unreadable: the caller fails loudly rather than minting a new identity,
	// which would orphan every world the old one owns.
	IdentityFile LoadIdentity(const std::filesystem::path& file, Identity& out);

	// Writes the identity atomically: a temp file next to it, then a rename
	bool SaveIdentity(const std::filesystem::path& file, const Identity& identity);

	enum class Obtained { Reused, Minted, Failed };

	// Reads the identity file, or mints an identity and writes the file when
	// there is none. `error` says why on Failed, and never contains the token.
	Obtained ObtainIdentity(HttpClient& client, const std::filesystem::path& file, Identity& out, std::string& error);

	/** /sql results **/
	// One statement's result: its column names and its rows, each a JSON array
	// in column order
	struct SqlResult
	{
		std::vector<std::string> columns;
		std::vector<nlohmann::json> rows;
	};

	// Every statement's result, in order. False for anything that isn't that
	// shape, never an empty success.
	bool ParseSqlResults(std::string_view body, std::vector<SqlResult>& out);

	// The first statement's result only
	bool ParseSqlRows(std::string_view body, std::vector<std::string>& columns, std::vector<nlohmann::json>& rows);
}

#endif
