// Capstone program entry point.
//
// Drive the Bitcoin Core regtest node through the full workflow and write the
// 10-line out.txt at the repo root.
//
// Workflow to implement (see README.md for details):
//   1. Create/load the "Miner" and "Trader" wallets (idempotent).
//   2. Generate a Miner address (label "Mining Reward") and mine 101 blocks to
//      it so the first coinbase reward matures (100-confirmation rule).
//   3. Print the Miner balance.
//   4. Generate a Trader receiving address (label "Received").
//   5. Send 20 BTC from Miner to Trader.
//   6. Confirm the transaction is in the mempool.
//   7. Mine 1 block to confirm it.
//   8. Fetch transaction details (gettransaction / getrawtransaction /
//      decoderawtransaction) and write the report to out.txt.

#include "main.h"
#include "rpc_client.h"

#include <curl/curl.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Helper: set up a wallet (create or load) so it's ready to use.
// Uses listwalletdir and listwallets RPC calls to decide the right action.
static void ensure_wallet(BitcoinRPC& client, const std::string& wallet_name) {
    // Get the list of wallets that exist on disk.
    nlohmann::json dir_result = client.call("listwalletdir");
    std::vector<std::string> on_disk;
    for (const auto& entry : dir_result["wallets"]) {
        on_disk.push_back(entry["name"].get<std::string>());
    }

    // Get the list of wallets currently loaded into the node.
    nlohmann::json loaded_result = client.call("listwallets");
    std::vector<std::string> loaded;
    for (const auto& name : loaded_result) {
        loaded.push_back(name.get<std::string>());
    }

    // Decide what to do based on current wallet state
    WalletAction action = decide_wallet_action(on_disk, loaded, wallet_name);

    if (action == WalletAction::Create) {
        std::cout << "Creating wallet: " << wallet_name << std::endl;
        client.call("createwallet", {wallet_name});
    } else if (action == WalletAction::Load) {
        std::cout << "Loading wallet: " << wallet_name << std::endl;
        client.call("loadwallet", {wallet_name});
    } else {
        std::cout << "Wallet already loaded: " << wallet_name << std::endl;
    }
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int exit_code = 0;
    try {
        RpcConfig cfg;  // defaults: alice:password @ 127.0.0.1:18443
        BitcoinRPC client(cfg);

        // Create/load Miner and Trader wallets
        ensure_wallet(client, "Miner");
        ensure_wallet(client, "Trader");

        // Generate an address from the Miner wallet with label "Mining Reward"
        std::string miner_address = client.wallet_call(
            "Miner", "getnewaddress", {"Mining Reward"}
        ).get<std::string>();
        std::cout << "Miner address: " << miner_address << std::endl;

        // Mine 101 blocks to the Miner address using generatetoaddress
        client.call("generatetoaddress", {101, miner_address});
        std::cout << "Mined 101 blocks to Miner address." << std::endl;

        // Print the Miner wallet balance
        double miner_balance = client.wallet_call("Miner", "getbalance").get<double>();
        std::cout << "Miner balance: " << miner_balance << " BTC" << std::endl;

        // Generate a Trader receiving address labeled "Received"
        std::string trader_address = client.wallet_call(
            "Trader", "getnewaddress", {"Received"}
        ).get<std::string>();
        std::cout << "Trader address: " << trader_address << std::endl;

        // Send 20 BTC from Miner to Trader
        std::string txid = client.wallet_call(
            "Miner", "sendtoaddress", {trader_address, 20}
        ).get<std::string>();
        std::cout << "Sent 20 BTC, txid: " << txid << std::endl;

        // Confirm the transaction is in the mempool
        nlohmann::json mempool = client.call("getrawmempool");
        std::cout << "Mempool contents: " << mempool.dump() << std::endl;

        nlohmann::json mempool_entry = client.call("getmempoolentry", {txid});
        std::cout << "Mempool entry for txid: " << mempool_entry.dump() << std::endl;

        // Mine 1 block to confirm transaction
        client.call("generatetoaddress", {1, miner_address});
        std::cout << "Mined 1 block to confirm transaction." << std::endl;

        // Fetch transaction details and write out.txt
        nlohmann::json tx_details = client.wallet_call(
            "Miner", "gettransaction", {txid, false, true}
        );

        // Build the TxReport struct from transaction data
        TxReport report;
        report.txid = txid;

        nlohmann::json decoded = tx_details["decoded"];

        // Parse the outputs to find the trader's and change outputs.
        std::vector<VoutEntry> vouts = parse_vouts(decoded);
        VoutEntry recipient = select_recipient_vout(vouts, trader_address);
        VoutEntry change = select_change_vout(vouts, trader_address);

        report.trader_output_address = recipient.address;
        report.trader_output_amount = recipient.value;
        report.miner_change_address = change.address;
        report.miner_change_amount = change.value;

        // Look up previous transaction to find Miner's input address and amount
        std::string prev_txid = decoded["vin"][0]["txid"].get<std::string>();
        long long prev_vout = decoded["vin"][0]["vout"].get<long long>();

        // Get the raw hex of the previous transaction, then decode it
        std::string prev_raw_hex = client.call(
            "getrawtransaction", {prev_txid}
        ).get<std::string>();
        nlohmann::json prev_decoded = client.call(
            "decoderawtransaction", {prev_raw_hex}
        );

        VoutEntry input_entry = resolve_input_prevout(prev_decoded, prev_vout);
        report.miner_input_address = input_entry.address;
        report.miner_input_amount = input_entry.value;

        // Get the fee from the gettransaction result
        report.fee = tx_details["fee"].get<double>();

        // Get the block height and hash where the transaction was confirmed
        report.block_height = tx_details["blockheight"].get<long long>();
        report.block_hash = tx_details["blockhash"].get<std::string>();

        // Write the report to out.txt.
        std::string report_text = format_report(report);
        std::ofstream out_file("out.txt");
        if (!out_file) {
            throw std::runtime_error("Failed to open out.txt for writing");
        }
        out_file << report_text;
        out_file.close();

        std::cout << "Report written to out.txt:" << std::endl;
        std::cout << report_text << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error occurred: " << e.what() << "\n";
        exit_code = 1;
    }

    curl_global_cleanup();
    return exit_code;
}
