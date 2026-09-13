cmake_minimum_required(VERSION 3.16)
project(qrx C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# -----------------------------
# macOS architecture
# -----------------------------
# On Apple Silicon, CMake may still produce x86_64 binaries if the shell,
# compiler, Homebrew/OpenSSL, or an old CMake cache is running under Rosetta.
# Default to the native host architecture unless the user explicitly set
# CMAKE_OSX_ARCHITECTURES.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture for macOS" FORCE)
    elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architecture for macOS" FORCE)
    endif()
endif()


option(QRX_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(QRX_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(QRX_ENABLE_HARDENING "Enable basic hardening flags" ON)
option(QRX_REQUIRE_PQC "Require OpenSSL with ML-DSA / post-quantum support" ON)
option(QRX_BUILD_TESTS "Build QRX internal test executables" OFF)

# Prefer caller-supplied OpenSSL root/library paths.
# Recommended static build:
#   -DOPENSSL_USE_STATIC_LIBS=TRUE
#   -DOPENSSL_ROOT_DIR=/opt/qrx-openssl-static
#   -DOPENSSL_INCLUDE_DIR=/opt/qrx-openssl-static/include
#   -DOPENSSL_CRYPTO_LIBRARY=/opt/qrx-openssl-static/lib64/libcrypto.a
if(DEFINED OPENSSL_ROOT_DIR AND OPENSSL_ROOT_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${OPENSSL_ROOT_DIR}")
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Different CMake/OpenSSL versions expose the version string under
# different variable names. Normalize it first.
if(DEFINED OPENSSL_VERSION)
    set(QRX_OPENSSL_VERSION "${OPENSSL_VERSION}")
elseif(DEFINED OPENSSL_VERSION_STRING)
    set(QRX_OPENSSL_VERSION "${OPENSSL_VERSION_STRING}")
elseif(DEFINED OpenSSL_VERSION)
    set(QRX_OPENSSL_VERSION "${OpenSSL_VERSION}")
else()
    set(QRX_OPENSSL_VERSION "0.0.0")
endif()

message(STATUS "Detected OpenSSL version: ${QRX_OPENSSL_VERSION}")
message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL crypto lib : ${OPENSSL_CRYPTO_LIBRARY}")

if(QRX_REQUIRE_PQC AND QRX_OPENSSL_VERSION VERSION_LESS "3.5.0")
    message(FATAL_ERROR
        "QRX hybrid consensus requires OpenSSL >= 3.5.0 with ML-DSA support. "
        "Detected: ${QRX_OPENSSL_VERSION}. "
        "Please build OpenSSL 3.6.x and configure with "
        "-DOPENSSL_ROOT_DIR=/opt/qrx-openssl-static, "
        "or use -DQRX_REQUIRE_PQC=OFF for developer-only builds."
    )
endif()

# -----------------------------
# Core Library
# -----------------------------
set(QRX_CORE_SOURCES
    src/qrx.c
    src/core_frontend.c
    src/chain_params.c
    src/qrxdb.c
    src/mempool/qrx_velocity_mempool.c
    src/mempool/qrx_velocity_scheduler.c
    src/mempool/qrx_velocity_mvcc.c
    src/bitcoin/qrx_btc_spv.c
    src/consensus/qrx_finality.c
    src/consensus/qrx_finality_state.c
    src/consensus/qrx_slashing.c
    src/economics/qrx_economics.c
    src/treasury/qrx_dev_addresses.c
    src/resource/qrx_resource.c
    src/resource/qrx_storage_capacity.c
    src/resource/qrx_capacity_proof.c
    src/resource/qrx_storage_placement.c
    src/resource/qrx_storage_market.c
    src/resource/qrx_storage_repair.c
    src/resource/qrx_storage_atlas.c
    src/resource/qrx_resource_dashboard.c
    src/resource/qrx_resource_live.c
    src/resource/qrx_drive_live.c
    src/resource/qrx_storage_consensus.c
    src/storage/qrx_storage_fs.c
    src/storage/qrx_merkle.c
    src/storage/qrx_drive_crypto.c
    src/storage/qrx_drive_manifest.c
    src/storage/qrx_drive_object.c
    src/storage/qrx_erasure.c
    src/storage/qrx_postor.c
    src/storage/qrx_storage_transport.c
    src/storage/qrx_storage_network.c
    src/storage/qrx_transfer_journal.c
    src/storage/qrx_drive_runtime.c
    src/storage/qrx_drive_private_pq.c
    src/storage/qrx_drive_prepare.c
    src/storage/qrx_drive_contract.c
    src/storage/qrx_storage_p2p.c
    src/storage/qrx_storage_activation.c
    src/storage/qrx_storage_postor_runtime.c
    src/storage/qrx_storage_repair_runtime.c
    src/storage/qrx_storage_discovery.c
    src/net/qrx_net_name.c
    src/net/qrx_net_consensus.c
    src/net/qrx_net_resolver.c
    src/net/qrx_net_registry.c
    src/net/qrx_net_site.c
    src/net/qrx_net_publisher.c
    src/net/qrx_net_browser.c
    src/net/qrx_net_sandbox.c
    src/net/qrx_net_permissions.c
    src/net/qrx_net_dapp_bridge.c
    src/net/qrx_net_ads.c
    src/net/qrx_net_youth.c
    src/genesis/qrx_bootstrap_validators.c
    src/genesis/qrx_genesis_governance.c
)

add_library(qrxcore STATIC ${QRX_CORE_SOURCES})

target_include_directories(qrxcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
)

# OpenSSL is a required part of the QRX ABI.  Always link through CMake's
# imported OpenSSL::Crypto target so include paths, architecture selection and
# transitive link requirements stay attached to every consumer of qrxcore.
# The release builder pins OPENSSL_ROOT_DIR on macOS to prevent CMake from
# accidentally selecting an Intel/Rosetta Homebrew installation for arm64.
if(NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR "CMake OpenSSL package did not provide OpenSSL::Crypto")
endif()

add_library(qrx_crypto_deps INTERFACE)
target_link_libraries(qrx_crypto_deps INTERFACE OpenSSL::Crypto Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(qrx_crypto_deps INTERFACE ${CMAKE_DL_LIBS})
endif()

set(QRX_OPENSSL_LINK_LIBS qrx_crypto_deps)
target_link_libraries(qrxcore PUBLIC qrx_crypto_deps)

if(WIN32)
    target_link_libraries(qrxcore PUBLIC ws2_32)
    target_compile_definitions(qrxcore PUBLIC
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# -----------------------------
# Compiler warnings
# -----------------------------
if(MSVC)
    target_compile_options(qrxcore PRIVATE /W4)
else()
    target_compile_options(qrxcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------
# Hardening compile flags
# -----------------------------
if(QRX_ENABLE_HARDENING)

    if(MSVC)
        target_compile_options(qrxcore PRIVATE /GS)

    elseif(APPLE)
        target_compile_definitions(qrxcore PRIVATE _FORTIFY_SOURCE=2)
        target_compile_options(qrxcore PRIVATE -fstack-protector)

    elseif(UNIX)
        target_compile_definitions(qrxcore PRIVATE _FORTIFY_SOURCE=2)
        target_compile_options(qrxcore PRIVATE -fstack-protector-strong -fPIE)
    endif()

endif()

# -----------------------------
# Sanitizers
# MSVC support is intentionally not enabled here.
# Use Clang/GCC for sanitizer builds.
# -----------------------------
if(NOT MSVC)
    if(QRX_ENABLE_ASAN)
        target_compile_options(qrxcore PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(qrxcore PRIVATE -fsanitize=address)
    endif()

    if(QRX_ENABLE_UBSAN)
        target_compile_options(qrxcore PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
        target_link_options(qrxcore PRIVATE -fsanitize=undefined)
    endif()
endif()

# -----------------------------
# Executables
# -----------------------------
set(QRX_OPENSSL_APPLINK_SRC "")
if(WIN32)
    set(QRX_OPENSSL_APPLINK_SRC src/openssl_applink.c)
endif()

add_executable(qrx src/qrx_main.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrx PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})

add_executable(qrxd src/qrxd.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrxd PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})

add_executable(qrx-cli src/qrx_cli.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrx-cli PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})

# Keep the extended command-line wallet in the build output beside qrx-cli.
# It is the shared non-shell frontend for Phase 4F.2 arbitrage, secure Kraken
# startup and the State-Root-verified complete ledger exporter.
add_custom_target(qrx-wallet-cli ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/qrx-wallet-cli.py
        ${CMAKE_CURRENT_BINARY_DIR}/qrx-wallet-cli.py
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_CURRENT_BINARY_DIR}/qrx-wallet-resources
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/qrx-complete-ledger-export.py
        ${CMAKE_CURRENT_BINARY_DIR}/qrx-wallet-resources/qrx-complete-ledger-export.py
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/gateways/qrx-arbitrage-engine.py
        ${CMAKE_CURRENT_BINARY_DIR}/qrx-wallet-resources/qrx-arbitrage-engine.py
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/gateways/qrx-gateway-kraken.py
        ${CMAKE_CURRENT_BINARY_DIR}/qrx-wallet-resources/qrx-gateway-kraken.py
)

add_executable(qrxdb_verify src/qrxdb_verify_tool.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrxdb_verify PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})

add_executable(qrxdb_salvage src/qrxdb_salvage_tool.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrxdb_salvage PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})

add_executable(qrxdb_compact src/qrxdb_compact_tool.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrxdb_compact PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})

add_executable(qrxdb_snapshot src/qrxdb_snapshot_tool.c ${QRX_OPENSSL_APPLINK_SRC})
target_link_libraries(qrxdb_snapshot PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})

if(WIN32)
    target_link_libraries(qrx PRIVATE ws2_32)
    target_link_libraries(qrxd PRIVATE ws2_32)
    target_link_libraries(qrx-cli PRIVATE ws2_32)
    target_link_libraries(qrxdb_verify PRIVATE ws2_32)
    target_link_libraries(qrxdb_salvage PRIVATE ws2_32)
    target_link_libraries(qrxdb_compact PRIVATE ws2_32)
    target_link_libraries(qrxdb_snapshot PRIVATE ws2_32)
endif()

# -----------------------------
# Linker options
# -----------------------------
foreach(tgt qrx qrxd qrx-cli qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot)

    if(QRX_ENABLE_HARDENING)

        if(MSVC)
            target_link_options(${tgt} PRIVATE
                /DYNAMICBASE
                /NXCOMPAT
            )

        elseif(APPLE)
            target_link_options(${tgt} PRIVATE
                -Wl,-dead_strip
            )

        elseif(UNIX)
            target_link_options(${tgt} PRIVATE
                -pie
                -Wl,-z,relro
                -Wl,-z,now
            )
        endif()

    endif()

    if(NOT MSVC)
        if(QRX_ENABLE_ASAN)
            target_link_options(${tgt} PRIVATE -fsanitize=address)
        endif()

        if(QRX_ENABLE_UBSAN)
            target_link_options(${tgt} PRIVATE -fsanitize=undefined)
        endif()
    endif()

endforeach()
# QRX economics/bootstrap sources
# qrx-core/src/economics/qrx_economics.c
# qrx-core/src/treasury/qrx_dev_addresses.c
# qrx-core/src/genesis/qrx_bootstrap_validators.c

# QRX P2P seed/maxconnection/peer persistence sources:
# src/network/qrx_p2p_config.c
# src/network/qrx_peer_store.c
# src/network/qrx_peer_relay.c

# src/consensus/qrx_deterministic_rotation.c

# Finality voting layer sources
# src/consensus/qrx_finality.c
# src/consensus/qrx_finality_state.c

# Slashing/signature enforcement
# src/consensus/qrx_slashing.c

# -----------------------------
# Internal tests (off for normal release builds)
# -----------------------------
if(QRX_BUILD_TESTS)
    enable_testing()
    add_executable(qrx_velocity_engine_test tests/velocity_phase4_engine.c)
    target_include_directories(qrx_velocity_engine_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_velocity_engine_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME velocity_phase4_engine COMMAND qrx_velocity_engine_test)

    add_executable(qrx_velocity_mvcc_test tests/velocity_phase4b_mvcc.c)
    target_include_directories(qrx_velocity_mvcc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_velocity_mvcc_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME velocity_phase4b_mvcc COMMAND qrx_velocity_mvcc_test)

    add_executable(qrx_velocity_stateful_mvcc_test tests/velocity_phase4c_stateful.c)
    target_include_directories(qrx_velocity_stateful_mvcc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_velocity_stateful_mvcc_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME velocity_phase4c_stateful_mvcc COMMAND qrx_velocity_stateful_mvcc_test)

    add_executable(qrx_velocity_dynamic_mvcc_test tests/velocity_phase4d_dynamic.c)
    target_include_directories(qrx_velocity_dynamic_mvcc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_velocity_dynamic_mvcc_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME velocity_phase4d_dynamic_mvcc COMMAND qrx_velocity_dynamic_mvcc_test)

    add_executable(qrx_velocity_speculative_mvcc_test tests/velocity_phase4e_speculative.c)
    target_include_directories(qrx_velocity_speculative_mvcc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_velocity_speculative_mvcc_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME velocity_phase4e_speculative_mvcc COMMAND qrx_velocity_speculative_mvcc_test)

    add_executable(qrx_velocity_block_scheduler_test tests/velocity_phase4f_scheduler.c)
    target_include_directories(qrx_velocity_block_scheduler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_velocity_block_scheduler_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME velocity_phase4f_block_scheduler COMMAND qrx_velocity_block_scheduler_test)

    add_executable(qrx_btc_spv_hardening_test tests/qrx_btc_spv_hardening.c)
    target_include_directories(qrx_btc_spv_hardening_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_btc_spv_hardening_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME bitcoin_spv_phase71_hardening COMMAND qrx_btc_spv_hardening_test)

    add_executable(qrx_resource_foundation_test tests/resource_phase080_foundation.c)
    target_include_directories(qrx_resource_foundation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_resource_foundation_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME resource_phase080_foundation COMMAND qrx_resource_foundation_test)

    add_executable(qrx_storage_filesystem_test tests/storage_phase081_filesystem.c)
    target_include_directories(qrx_storage_filesystem_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_filesystem_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase081_filesystem COMMAND qrx_storage_filesystem_test)

    add_executable(qrx_storage_capacity_test tests/storage_phase082_capacity.c)
    target_include_directories(qrx_storage_capacity_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_capacity_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase082_capacity COMMAND qrx_storage_capacity_test)

    add_executable(qrx_capacity_proof_test tests/storage_phase084_capacity_proof.c)
    target_include_directories(qrx_capacity_proof_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_capacity_proof_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase084_capacity_proof COMMAND qrx_capacity_proof_test)

    add_executable(qrx_drive_pq_crypto_test tests/storage_phase085_pq_crypto.c)
    target_include_directories(qrx_drive_pq_crypto_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_drive_pq_crypto_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase085_pq_crypto COMMAND qrx_drive_pq_crypto_test)

    add_executable(qrx_drive_manifest_test tests/storage_phase086_manifest.c)
    target_include_directories(qrx_drive_manifest_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_drive_manifest_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase086_manifest COMMAND qrx_drive_manifest_test)

    add_executable(qrx_erasure_test tests/storage_phase087_erasure.c)
    target_include_directories(qrx_erasure_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_erasure_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase087_erasure COMMAND qrx_erasure_test)

    add_executable(qrx_storage_placement_test tests/storage_phase088_placement.c)
    target_include_directories(qrx_storage_placement_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_placement_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase088_placement COMMAND qrx_storage_placement_test)

    add_executable(qrx_postor_test tests/storage_phase089_postor.c)
    target_include_directories(qrx_postor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_postor_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase089_postor COMMAND qrx_postor_test)

    add_executable(qrx_storage_market_test tests/storage_phase090_market.c)
    target_include_directories(qrx_storage_market_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_market_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase090_market COMMAND qrx_storage_market_test)

    add_executable(qrx_storage_repair_test tests/storage_phase091_repair.c)
    target_include_directories(qrx_storage_repair_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_repair_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase091_repair COMMAND qrx_storage_repair_test)

    add_executable(qrx_storage_atlas_test tests/storage_phase092_atlas.c)
    target_include_directories(qrx_storage_atlas_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_atlas_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase092_atlas COMMAND qrx_storage_atlas_test)

    add_executable(qrx_storage_transport_test tests/storage_phase093_transport.c)
    target_include_directories(qrx_storage_transport_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_transport_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase093_transport COMMAND qrx_storage_transport_test)

    add_executable(qrx_net_name_test tests/net_phase844_name.c)
    target_include_directories(qrx_net_name_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_net_name_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME net_phase844_name COMMAND qrx_net_name_test)

    add_executable(qrx_net_registry_test tests/net_phase844_registry.c)
    target_include_directories(qrx_net_registry_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_net_registry_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME net_phase844_registry COMMAND qrx_net_registry_test)

    add_executable(qrx_net_resolver_test tests/net_phase846_resolver.c)
    target_include_directories(qrx_net_resolver_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_net_resolver_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME net_phase846_resolver COMMAND qrx_net_resolver_test)

    add_executable(qrx_net_site_test tests/net_phase847_site.c)
    target_include_directories(qrx_net_site_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_net_site_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME net_phase847_site COMMAND qrx_net_site_test)

    add_executable(qrx_net_permissions_test tests/net_phase854_permissions.c)
    target_include_directories(qrx_net_permissions_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_net_permissions_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME net_phase854_permissions COMMAND qrx_net_permissions_test)

    add_executable(qrx_storage_consensus_test tests/storage_phase094_consensus.c)
    target_include_directories(qrx_storage_consensus_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_consensus_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase094_consensus COMMAND qrx_storage_consensus_test)

    add_executable(qrx_net_consensus_test tests/net_phase855_consensus.c)
    target_include_directories(qrx_net_consensus_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_net_consensus_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME net_phase855_consensus COMMAND qrx_net_consensus_test)

    add_executable(qrx_storage_attestation_test tests/storage_phase095_attestation.c)
    target_include_directories(qrx_storage_attestation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_attestation_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase095_attestation COMMAND qrx_storage_attestation_test)

    add_executable(qrx_storage_p2p_test tests/storage_phase096_p2p.c)
    target_include_directories(qrx_storage_p2p_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_p2p_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase096_p2p COMMAND qrx_storage_p2p_test)

    add_executable(qrx_storage_large_object_test tests/storage_phase097_large_object.c)
    target_link_libraries(qrx_storage_large_object_test PRIVATE qrxcore ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase097_large_object COMMAND qrx_storage_large_object_test)


    add_executable(qrx_storage_discovery_test tests/storage_phase099_discovery.c)
    target_include_directories(qrx_storage_discovery_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_discovery_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase099_discovery COMMAND qrx_storage_discovery_test)

    add_executable(qrx_resource_dashboard_test tests/storage_phase100_dashboard.c)
    target_include_directories(qrx_resource_dashboard_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_resource_dashboard_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase100_resource_dashboard COMMAND qrx_resource_dashboard_test)
    add_executable(qrx_resource_live_test tests/storage_phase101_live_dashboard.c)
    target_link_libraries(qrx_resource_live_test PRIVATE qrxcore)
    add_test(NAME storage_phase101_live_dashboard COMMAND qrx_resource_live_test)
    add_executable(qrx_drive_live_test tests/storage_phase102_drive_live.c)
    target_link_libraries(qrx_drive_live_test PRIVATE qrxcore)
    add_test(NAME storage_phase102_drive_live COMMAND qrx_drive_live_test)
    add_executable(qrx_storage_network_transfer_test tests/storage_phase103_network_transfer.c)
    target_link_libraries(qrx_storage_network_transfer_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase103_network_transfer COMMAND qrx_storage_network_transfer_test)

    add_executable(qrx_drive_runtime_test tests/storage_phase104_drive_runtime.c)
    target_link_libraries(qrx_drive_runtime_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase104_drive_runtime COMMAND qrx_drive_runtime_test)

    add_executable(qrx_drive_recovery_test tests/storage_phase105_drive_recovery.c)
    target_link_libraries(qrx_drive_recovery_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase105_drive_recovery COMMAND qrx_drive_recovery_test)

    add_executable(qrx_storage_gossip_streaming_test tests/storage_phase106_gossip_streaming.c)
    target_link_libraries(qrx_storage_gossip_streaming_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase106_gossip_streaming COMMAND qrx_storage_gossip_streaming_test)

    add_executable(qrx_storage_identity_gossip_test tests/storage_phase107_identity_gossip.c)
    target_link_libraries(qrx_storage_identity_gossip_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase107_identity_gossip COMMAND qrx_storage_identity_gossip_test)

    add_executable(qrx_drive_private_pq_test tests/storage_phase108_private_pq.c)
    target_link_libraries(qrx_drive_private_pq_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase108_private_pq COMMAND qrx_drive_private_pq_test)

    add_executable(qrx_drive_prepare_test tests/storage_phase109_prepare.c)
    target_link_libraries(qrx_drive_prepare_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase109_prepare COMMAND qrx_drive_prepare_test)
    add_executable(qrx_drive_prepared_runtime_test tests/storage_phase110_prepared_runtime.c)
    target_link_libraries(qrx_drive_prepared_runtime_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase110_prepared_runtime COMMAND qrx_drive_prepared_runtime_test)
    add_executable(qrx_drive_contract_orchestration_test tests/storage_phase111_contract_orchestration.c)
    target_link_libraries(qrx_drive_contract_orchestration_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase111_contract_orchestration COMMAND qrx_drive_contract_orchestration_test)

    add_executable(qrx_storage_multifetch_test tests/storage_phase098_multifetch.c)
    target_include_directories(qrx_storage_multifetch_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(qrx_storage_multifetch_test PRIVATE qrxcore Threads::Threads ${QRX_OPENSSL_LINK_LIBS})
    add_test(NAME storage_phase098_multifetch COMMAND qrx_storage_multifetch_test)

    if(NOT MSVC)
        foreach(test_tgt qrx_velocity_engine_test qrx_velocity_mvcc_test qrx_velocity_stateful_mvcc_test qrx_velocity_dynamic_mvcc_test qrx_velocity_speculative_mvcc_test qrx_velocity_block_scheduler_test qrx_btc_spv_hardening_test qrx_resource_foundation_test qrx_storage_filesystem_test qrx_storage_capacity_test qrx_capacity_proof_test qrx_drive_pq_crypto_test qrx_drive_manifest_test qrx_erasure_test qrx_storage_placement_test qrx_postor_test qrx_storage_market_test qrx_storage_repair_test qrx_storage_atlas_test qrx_storage_transport_test qrx_net_name_test qrx_net_registry_test qrx_net_resolver_test qrx_net_site_test qrx_net_permissions_test qrx_storage_consensus_test qrx_net_consensus_test qrx_storage_attestation_test qrx_storage_p2p_test qrx_storage_large_object_test qrx_storage_multifetch_test qrx_storage_discovery_test)
            target_compile_options(${test_tgt} PRIVATE -UNDEBUG)
            if(QRX_ENABLE_ASAN)
                target_link_options(${test_tgt} PRIVATE -fsanitize=address)
            endif()
            if(QRX_ENABLE_UBSAN)
                target_link_options(${test_tgt} PRIVATE -fsanitize=undefined)
            endif()
        endforeach()
    endif()

  add_executable(storage_phase112_provider_activation tests/storage_phase112_provider_activation.c)
  target_link_libraries(storage_phase112_provider_activation PRIVATE qrxcore)
  add_test(NAME storage_phase112_provider_activation COMMAND storage_phase112_provider_activation)
  add_executable(storage_phase113_postor_runtime tests/storage_phase113_postor_runtime.c)
  target_link_libraries(storage_phase113_postor_runtime PRIVATE qrxcore)
  add_test(NAME storage_phase113_postor_runtime COMMAND storage_phase113_postor_runtime)
  add_executable(storage_phase114_repair_runtime tests/storage_phase114_repair_runtime.c)
  target_link_libraries(storage_phase114_repair_runtime PRIVATE qrxcore)
  add_test(NAME storage_phase114_repair_runtime COMMAND storage_phase114_repair_runtime)
  add_executable(net_phase115_domain_management tests/net_phase115_domain_management.c)
  target_link_libraries(net_phase115_domain_management PRIVATE qrxcore)
  add_test(NAME net_phase115_domain_management COMMAND net_phase115_domain_management)
  add_executable(net_phase116_site_package tests/net_phase116_site_package.c)
  target_link_libraries(net_phase116_site_package PRIVATE qrxcore)
  add_test(NAME net_phase116_site_package COMMAND net_phase116_site_package)
  add_executable(net_phase117_directory_publisher tests/net_phase117_directory_publisher.c)
  target_link_libraries(net_phase117_directory_publisher PRIVATE qrxcore)
  add_test(NAME net_phase117_directory_publisher COMMAND net_phase117_directory_publisher)
  add_executable(net_phase118_hosting_activation tests/net_phase118_hosting_activation.c)
  target_link_libraries(net_phase118_hosting_activation PRIVATE qrxcore)
  add_test(NAME net_phase118_hosting_activation COMMAND net_phase118_hosting_activation)
  add_executable(net_phase119_browser_fetch tests/net_phase119_browser_fetch.c)
  target_link_libraries(net_phase119_browser_fetch PRIVATE qrxcore)
  add_test(NAME net_phase119_browser_fetch COMMAND net_phase119_browser_fetch)
  add_executable(net_phase120_browser_sandbox tests/net_phase120_browser_sandbox.c)
  target_link_libraries(net_phase120_browser_sandbox PRIVATE qrxcore)
  add_test(NAME net_phase120_browser_sandbox COMMAND net_phase120_browser_sandbox)
  add_executable(net_phase121_dapp_bridge tests/net_phase121_dapp_bridge.c)
  target_link_libraries(net_phase121_dapp_bridge PRIVATE qrxcore)
  add_test(NAME net_phase121_dapp_bridge COMMAND net_phase121_dapp_bridge)
  add_executable(net_phase122_ad_economy tests/net_phase122_ad_economy.c)
  target_link_libraries(net_phase122_ad_economy PRIVATE qrxcore)
  add_test(NAME net_phase122_ad_economy COMMAND net_phase122_ad_economy)
  add_executable(net_phase123_youth_safety tests/net_phase123_youth_safety.c)
  target_link_libraries(net_phase123_youth_safety PRIVATE qrxcore)
  add_test(NAME net_phase123_youth_safety COMMAND net_phase123_youth_safety)
  add_executable(net_phase124_hosting_missions tests/net_phase124_hosting_missions.c)
  target_link_libraries(net_phase124_hosting_missions PRIVATE qrxcore)
  add_test(NAME net_phase124_hosting_missions COMMAND net_phase124_hosting_missions)

  add_executable(net_phase125_final_readiness tests/net_phase125_final_readiness.c)
  target_link_libraries(net_phase125_final_readiness PRIVATE qrxcore)
  add_test(NAME net_phase125_final_readiness COMMAND net_phase125_final_readiness)

  if(NOT MSVC)
    foreach(test_tgt qrx_resource_live_test qrx_drive_live_test qrx_storage_network_transfer_test qrx_drive_runtime_test qrx_drive_recovery_test qrx_storage_gossip_streaming_test qrx_storage_identity_gossip_test qrx_drive_private_pq_test qrx_drive_prepare_test qrx_drive_prepared_runtime_test qrx_drive_contract_orchestration_test storage_phase112_provider_activation storage_phase113_postor_runtime storage_phase114_repair_runtime net_phase115_domain_management net_phase116_site_package net_phase117_directory_publisher net_phase118_hosting_activation net_phase119_browser_fetch net_phase120_browser_sandbox net_phase121_dapp_bridge net_phase122_ad_economy net_phase123_youth_safety net_phase124_hosting_missions net_phase125_final_readiness)
      target_compile_options(${test_tgt} PRIVATE -UNDEBUG)
    endforeach()
  endif()
endif()
