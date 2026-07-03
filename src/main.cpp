// Pure helper functions for the capstone (no network / no I/O).
//
// These are unit-tested by tests/main_test.cpp without a running Bitcoin node.
// Implement each TODO so that the GoogleTest suite passes.

#include "main.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Build the base RPC URL from the config, e.g. "http://127.0.0.1:18443".
std::string build_base_url(const RpcConfig& cfg) {
    return cfg.scheme + "://" + cfg.host + ":" + std::to_string(cfg.port);
}

// Append the wallet path to the base URL, e.g. "http://127.0.0.1:18443/wallet/Miner".
std::string build_wallet_url(const RpcConfig& cfg, const std::string& wallet) {
    return build_base_url(cfg) + "/wallet/" + wallet;
}

// Construct a JSON-RPC 1.0 request body that Bitcoin Core expects.
nlohmann::json build_rpc_request(const std::string& method,
                                 const nlohmann::json& params,
                                 const std::string& id) {
    nlohmann::json request;
    request["jsonrpc"] = "1.0";
    request["id"] = id;
    request["method"] = method;
    request["params"] = params;
    return request;
}

// Decide whether a wallet needs to be created, loaded, or is already usable
WalletAction decide_wallet_action(const std::vector<std::string>& on_disk,
                                  const std::vector<std::string>& loaded,
                                  const std::string& wallet) {
    // Check if the wallet is already loaded (ready to use).
    bool is_loaded = std::find(loaded.begin(), loaded.end(), wallet) != loaded.end();
    if (is_loaded) {
        return WalletAction::None;
    }

    // Check if the wallet exists on disk but isn't loaded yet
    bool is_on_disk = std::find(on_disk.begin(), on_disk.end(), wallet) != on_disk.end();
    if (is_on_disk) {
        return WalletAction::Load;
    }

    // Wallet doesn't exist at all, needs to be created
    return WalletAction::Create;
}

// Parse the "vout" array from a decoded transaction into VoutEntry structs.
std::vector<VoutEntry> parse_vouts(const nlohmann::json& decoded_tx) {
    std::vector<VoutEntry> result;

    for (const auto& vout : decoded_tx["vout"]) {
        VoutEntry entry;
        entry.value = vout["value"].get<double>();
        entry.n = vout["n"].get<long long>();

        // Try "address" first then fall back to "addresses[0]"
        const auto& spk = vout["scriptPubKey"];
        if (spk.contains("address")) {
            entry.address = spk["address"].get<std::string>();
        } else if (spk.contains("addresses") && !spk["addresses"].empty()) {
            entry.address = spk["addresses"][0].get<std::string>();
        }

        result.push_back(entry);
    }

    return result;
}

// Find the output that pays the trader
VoutEntry select_recipient_vout(const std::vector<VoutEntry>& vouts,
                                const std::string& trader_address) {
    for (const auto& v : vouts) {
        if (v.address == trader_address) {
            return v;
        }
    }
    throw std::runtime_error("Recipient output not found for address: " + trader_address);
}

// Find the change output
VoutEntry select_change_vout(const std::vector<VoutEntry>& vouts,
                             const std::string& trader_address) {
    for (const auto& v : vouts) {
        if (v.address != trader_address) {
            return v;
        }
    }
    throw std::runtime_error("Change output not found (all outputs match trader address)");
}

// Look up the previous transaction's output to find the input address and amount
VoutEntry resolve_input_prevout(const nlohmann::json& prev_decoded,
                                long long input_vout) {
    // Index into the previous transaction's vout array to find the spent output.
    const auto& vouts = prev_decoded["vout"];
    if (input_vout < 0 || input_vout >= static_cast<long long>(vouts.size())) {
        throw std::runtime_error("Input vout index out of range: " + std::to_string(input_vout));
    }

    VoutEntry entry;
    const auto& vout = vouts[static_cast<size_t>(input_vout)];
    entry.value = vout["value"].get<double>();
    entry.n = vout["n"].get<long long>();

    const auto& spk = vout["scriptPubKey"];
    if (spk.contains("address")) {
        entry.address = spk["address"].get<std::string>();
    } else if (spk.contains("addresses") && !spk["addresses"].empty()) {
        entry.address = spk["addresses"][0].get<std::string>();
    }

    return entry;
}

// Format a BTC amount
std::string format_btc(double amount) {
    char buf[64];

    // Try fixed-point notation with increasing decimal digits
    for (int decimals = 0; decimals <= 17; ++decimals) {
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, amount);

        // Check if this representation rounds up exactly
        char* end = nullptr;
        double parsed = std::strtod(buf, &end);
        if (parsed == amount) {
            // Strip trailing zeros after the decimal point
            std::string s(buf);
            if (s.find('.') != std::string::npos) {
                size_t last_nonzero = s.find_last_not_of('0');
                if (last_nonzero != std::string::npos) {
                    s.erase(last_nonzero + 1);
                }
                // Remove trailing decimal point
                if (!s.empty() && s.back() == '.') {
                    s.pop_back();
                }
            }

            // Check if scientific notation would be shorter
            char gbuf[64];
            for (int prec = 1; prec <= 17; ++prec) {
                std::snprintf(gbuf, sizeof(gbuf), "%.*g", prec, amount);
                double gparsed = std::strtod(gbuf, &end);
                if (gparsed == amount) {
                    std::string gs(gbuf);
                    if (gs.length() < s.length()) {
                        return gs;
                    }
                    break;
                }
            }

            return s;
        }
    }

    std::snprintf(buf, sizeof(buf), "%.17f", amount);
    return std::string(buf);
}

// Assemble the 10-line report for out.txt in the exact order as README
std::string format_report(const TxReport& r) {
    std::string report;
    report += r.txid + "\n";                           // 1. Transaction ID
    report += r.miner_input_address + "\n";            // 2. Miner's input address
    report += format_btc(r.miner_input_amount) + "\n"; // 3. Miner's input amount
    report += r.trader_output_address + "\n";           // 4. Trader's output address
    report += format_btc(r.trader_output_amount) + "\n";// 5. Trader's output amount
    report += r.miner_change_address + "\n";            // 6. Miner's change address
    report += format_btc(r.miner_change_amount) + "\n"; // 7. Miner's change amount
    report += format_btc(r.fee) + "\n";                 // 8. Transaction fee
    report += std::to_string(r.block_height) + "\n";    // 9. Block height
    report += r.block_hash + "\n";                      // 10. Block hash
    return report;
}
