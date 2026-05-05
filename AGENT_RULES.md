# EnvyOS LoStar Architecture & Agent Rules

This document outlines critical architectural constraints and facts for AI agents working within the `lo-star` repository.

## 1. No SQLite! We use `LoDb` + Nanopb
**Never assume `lo-star` uses SQLite.** The ESP32 and nRF52 platforms lack the RAM and flash overhead to comfortably run full SQLite alongside complex mesh protocols.
*   **The Solution:** `lo-star` uses **`LoDb`**, a custom lightweight database built entirely on top of `lofs` (LittleFS) and Nanopb (Protocol Buffers).
*   **How it Works:** Each table (e.g., `known_wifi`, `users`, `sessions`) maps to a Nanopb struct (`.proto` file). `LoDb` handles persisting, updating, and querying these protobuf structs directly to/from the filesystem.

## 2. Pluggable Auth (`AuthProvider`)
The CLI authorization system (`louser`) does not strictly couple commands to the `LoDb` `users` table. 
*   **The Abstraction:** Guards (`require_admin`, `require_user`) rely on `lostar::AuthProvider`.
*   **Default Behavior:** `meshtastic` and standalone `lo-star` use the `DefaultAuthProvider`, which checks the local `LoDb` SQLite-style soft-sessions (requiring a `hi <user> <pass>` login).
*   **Mesh Core:** Platforms like `meshcore` inject a custom `MeshCoreAuthProvider` to natively authenticate admins using cryptographic packet signatures instead of requiring manual login over the mesh.

## 3. The Local/Mesh Divide (Transport Awareness)
*   **Local (Serial / BLE):** Connections coming over local serial or BLE *do not* inherently carry cryptographic identities. For these, users must establish soft-sessions using the CLI (`hi` / `bye`).
*   **Mesh:** Connections over LoRa mesh inherently carry identities (e.g., `pub_key`). The `AuthProvider` architecture allows the underlying mesh protocol to bless these packets directly without requiring the sender to type a password over the air.

## 4. WebDAV and HTTP Server
*   **There is no WebDAV or HTTP server running directly on the node.** 
*   The EnvyOS "WebDAV Gateway" is a *host-side* concept (e.g., running inside the Flutter Companion App or a Python gateway) that translates WebDAV/HTTP requests into RPC (`sys_fs_*`) or CLI commands, which are then transmitted to the node over Serial, BLE, or Mesh.
*   `lofi` provides Wi-Fi STA capabilities for outbound connections, HTTP POSTs, and auto-failover, but it does **not** serve incoming REST or WebDAV traffic.

## 5. Wi-Fi Auto-Failover (No Active Scanning)
When Wi-Fi (`lofi`) drops, it **does not** perform an active Wi-Fi scan to find the best network.
Instead, it uses a **Round-Robin Failover**: it iterates through the `known_wifi` cache (up to 8 most recently used networks) and blindly attempts to connect to the next one in the list until one succeeds. Active scanning is reserved strictly for manual user requests to save power and driver overhead.
