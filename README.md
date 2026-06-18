# Btrust Builders: C++ Language Club - Capstone Project: Setting up and interacting with a Bitcoin Node

## Overview

In this capstone project week, you'll learn how to use Bitcoin Core's RPC to interact with a running Bitcoin node from C++. The tasks involve connecting to a Bitcoin Core RPC daemon over JSON-RPC, creating wallets, generating addresses, creating transactions, and mining Bitcoin blocks. You'll need a Bitcoin node running in `regtest` mode on your local machine to test your solution.

A [docker-compose](./docker-compose.yaml) file is provided to help you launch a Bitcoin node in `regtest` mode locally.

> [!TIP]
> [OPTIONAL] You can also use the [bitcoin.conf](./bitcoin.conf) file to start a local regtest node with your locally built bitcoin binaries.
> Copy the `bitcoin.conf` file into the default bitcoin data-directory `~/.bitcoin/`. If you don't have the data-directory, just create one.

## Objective

Implement the tasks in [`src/app.cpp`](./src/app.cpp) (the program workflow) and the pure helper functions in [`src/main.cpp`](./src/main.cpp) and [`src/rpc_client.cpp`](./src/rpc_client.cpp). Look for the `// TODO:` comments.

Your program must:

- Create two wallets named `Miner` and `Trader`. The names are case-sensitive and should be exact.
- Generate one address from the `Miner` wallet with a label "Mining Reward".
- Mine new blocks to this address until you get a positive wallet balance (use `generatetoaddress`). Observe how many blocks it took to get a positive balance.
- Write a short comment describing why the wallet balance for block rewards behaves that way.
- Print the balance of the `Miner` wallet.
- Create a receiving address labeled "Received" from the `Trader` wallet.
- Send a transaction paying 20 BTC from the `Miner` wallet to the `Trader`'s wallet.
- Fetch the unconfirmed transaction from the node's mempool and print the result. (hint: `getrawmempool` / `getmempoolentry`).
- Confirm the transaction by mining 1 block.

### Output

- Fetch the following details of the transaction and output them to an `out.txt` file in the following format. Each attribute should be on a new line.
  - `Transaction ID (txid)`
  - `Miner's Input Address`
  - `Miner's Input Amount (in BTC)`
  - `Trader's Output Address`
  - `Trader's Output Amount (in BTC)`
  - `Miner's Change Address`
  - `Miner's Change Amount (in BTC)`
  - `Transaction Fees (in BTC)`
  - `Block height at which the transaction is confirmed`
  - `Block hash at which the transaction is confirmed`

- Sample output file:
  ```
  57ecbb84fd3246ebcc734455fd30f5536637878b40fb2742d1a4fced3c28862c
  bcrt1qv5plgft75j0hegtvf6zs5pajh7k0gxg2dhj224
  50
  bcrt1qak6gpu2p6zjpwrhvd4dvdnp4rt3ysm9rpst3wu
  20
  bcrt1qxw3msnuqps0kgn6dprs9ldlz79yfj63swqupd0
  29.9999859
  -1.41e-05
  102
  3b821acd7c32c2b3da143e2c6b0134e5aa8206aeae0a54bfa4963e73ac2857a0
  ```

## Project layout

```
src/
  main.h            # structs + pure helper declarations
  main.cpp          # pure helpers (URL/JSON building, vout selection, formatting) — implement the TODOs
  rpc_client.h      # BitcoinRPC class (libcurl-backed JSON-RPC client)
  rpc_client.cpp    # implement the TODOs
  app.cpp           # int main(): the capstone workflow that writes out.txt — implement the TODOs
tests/
  main_test.cpp     # GoogleTest unit tests for the pure helpers (no node required)
test/
  test.spec.ts      # language-agnostic autograder (reads out.txt + queries the node)
```

## Dependencies

- A C++17 compiler and CMake 3.14+.
- **libcurl** (used for HTTP). On Debian/Ubuntu: `sudo apt-get install libcurl4-openssl-dev`. On macOS it ships with the system / Xcode command line tools.
- GoogleTest and nlohmann/json are downloaded automatically by CMake via `FetchContent`.

## Build and run the unit tests

The unit tests cover the pure helper functions and do **not** require a running node:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

You can also run the test binary directly:

```bash
./build/test_main
```

## Run the program against a node

```bash
docker compose up -d        # start a regtest bitcoind on 127.0.0.1:18443
./run.sh                    # builds the `capstone` target and runs it -> out.txt
docker compose down -v      # stop the node
```

### Local Testing Steps

It's a good idea to run the whole test locally to ensure your code is working properly.

- Ensure that you have `npm` and `nvm` installed on your system. You will need `node v18` or greater to run the autograder.
- Ensure that there is no `bitcoind` process running on your system.
- Give execution permission to `test.sh`, by running `chmod +x ./test.sh`.
- Execute [`./test.sh`](./test.sh). It starts the node, builds and runs your program, then runs the autograder against the node.

If your code works, you will see the tests complete successfully.

## Submission

- Create a commit with your local changes.
- Push the commit to your forked repository (`git push origin main`).
- The autograder will run your script against the test script to verify the functionality.
- Check the status of the autograder on GitHub Actions to see if it passed or failed.
- You can submit multiple times before the deadline. The latest submission before the deadline will be considered your final submission.

Submit your final solution link to this form: [Google form](https://forms.gle/Hr7beNMa6h4FdTgh9).

### Common Issues

- Your submission should not stop the Bitcoin Core daemon at any point.
- Linux and macOS are the recommended operating systems for this challenge.
- The autograder runs on an Ubuntu environment. Make sure your code builds and runs there.
- If you are unable to run the test script locally, you can submit your solution and check the results on the GitHub Actions tab.

## Evaluation Criteria

Your submission will be evaluated based on:
- **Autograder**: Your code must pass the autograder [test script](./test/test.spec.ts) and the C++ unit tests.
- **Explainer Comments**: Include comments explaining each step of your code.
- **Code Quality**: Your code should be well-organized, commented, and adhere to best practices.

### Plagiarism Policy

Our plagiarism detection checker thoroughly identifies any instances of copying or cheating. Participants are required to publish their solutions in the designated repository, which is private and accessible only to the individual and the administrator. Solutions should not be shared publicly or with peers. In case of plagiarism, both parties involved will be directly disqualified to maintain fairness and integrity.

### AI Usage Disclaimer

You may use AI tools like ChatGPT to gather information and explore alternative approaches, but avoid relying solely on AI for complete solutions. Verify and validate any insights obtained and maintain a balance between AI assistance and independent problem-solving.

## Why These Restrictions?

These rules are designed to enhance your understanding of the technical aspects of Bitcoin. By completing this assignment, you gain practical experience with the technology that secures and maintains the trustlessness of Bitcoin. This challenge not only tests your ability to develop functional Bitcoin applications but also encourages deep engagement with the core elements of Bitcoin technology.

### Additional Resources

- [Bitcoin Core RPC Documentation](https://developer.bitcoin.org/reference/rpc/)
- [Bitcoin Core Documentation](https://developer.bitcoin.org/)
- [Learning Bitcoin from the Command Line](https://github.com/BlockchainCommons/Learning-Bitcoin-from-the-Command-Line)
- [libcurl](https://curl.se/libcurl/) · [nlohmann/json](https://github.com/nlohmann/json)
