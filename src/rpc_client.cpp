// JSON-RPC client for Bitcoin Core, backed by libcurl.
//
// Implements the HTTP transport layer for communicating with bitcoind.

#include "rpc_client.h"

#include <curl/curl.h>

#include <stdexcept>
#include <string>
#include <utility>

// Constructor: set up a libcurl easy handle for HTTP requests.
BitcoinRPC::BitcoinRPC(RpcConfig cfg) : cfg_(std::move(cfg)) {
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("Failed to initialise libcurl easy handle");
    }
}

// Destructor: clean up the curl handle to avoid resource leaks.
BitcoinRPC::~BitcoinRPC() {
    if (curl_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_));
        curl_ = nullptr;
    }
}

// Move constructor
BitcoinRPC::BitcoinRPC(BitcoinRPC&& other) noexcept
    : cfg_(std::move(other.cfg_)), curl_(other.curl_) {
    other.curl_ = nullptr;
}

// Move assignment
BitcoinRPC& BitcoinRPC::operator=(BitcoinRPC&& other) noexcept {
    if (this != &other) {
        if (curl_) {
            curl_easy_cleanup(static_cast<CURL*>(curl_));
        }
        cfg_ = std::move(other.cfg_);
        curl_ = other.curl_;
        other.curl_ = nullptr;
    }
    return *this;
}

// Callback for libcurl
size_t BitcoinRPC::write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total_bytes = size * nmemb;
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, total_bytes);
    return total_bytes;
}

// POST a JSON-RPC request to the given URL and return the "result" field.
nlohmann::json BitcoinRPC::post(const std::string& url, const nlohmann::json& body) {
    CURL* handle = static_cast<CURL*>(curl_);
    std::string response_body;
    std::string request_data = body.dump();

    // Set the target URL
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());

    // Set HTTP POST method and the request body
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request_data.c_str());

    // Set the Content-Type header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    // Set HTTP Basic Auth credentials
    std::string userpwd = cfg_.user + ":" + cfg_.pass;
    curl_easy_setopt(handle, CURLOPT_USERPWD, userpwd.c_str());

    // Set the write callback
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);

    // Perform HTTP request
    CURLcode res = curl_easy_perform(handle);

    // Free the custom headers list
    curl_slist_free_all(headers);

    // Check for transport-level errors
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("curl request failed: ") + curl_easy_strerror(res));
    }

    // Parse the JSON response
    nlohmann::json json_response = nlohmann::json::parse(response_body);

    // Check for JSON-RPC errors
    if (!json_response["error"].is_null()) {
        std::string err_msg = "RPC error: " + json_response["error"].dump();
        throw std::runtime_error(err_msg);
    }

    // Return the "result" field, which contains the actual RPC response data
    return json_response["result"];
}

// Call a node-wide RPC method
nlohmann::json BitcoinRPC::call(const std::string& method, const nlohmann::json& params) {
    return post(build_base_url(cfg_), build_rpc_request(method, params));
}

// Call a wallet-scoped RPC method (uses the /wallet/<name> URL path).
nlohmann::json BitcoinRPC::wallet_call(const std::string& wallet,
                                       const std::string& method,
                                       const nlohmann::json& params) {
    return post(build_wallet_url(cfg_, wallet), build_rpc_request(method, params));
}
