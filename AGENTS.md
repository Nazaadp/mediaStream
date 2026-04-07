# 🤖 AI Agent Context file (AGENTS.md)

This file contains the core contextual information about this project, designed specifically to help GeminiAI (and other AI agents) quickly understand the architecture, technology stack, and specific needs of the project.

*Instructions for the Developer: Fill in the bracketed `[ ]` information. AI Agents should read this file upon starting a new task to contextualize their efforts.*

## 🎯 1. Project Overview & Needs

- **Project Name**: MediaStream
- **Primary Goal**: A private Netflix-like application that discovers, downloads, and streams torrents locally via HTTPS, with a focus on instantaneous streaming and enterprise-grade cybersecurity.
- **Target Audience/Users**: Personal/Private use for media streaming across Windows 10, Android, and potentially web clients, served from a local Ubuntu server.
- **Key Features**:
  1. Content Discovery (Movies, TV Series, Anime via YTS, EZTV, Nyaa) and Metadata enrichment (TMDB).
  2. Sequential torrent downloading and management via `libtorrent`.
  3. Video streaming using HTTP Range requests for instant playback.
- **Current State/Phase**: MVP Development / In Progress. Core backend (REST API, Discovery, Torrent Engine) and Tauri/SvelteKit desktop client foundation built. Ongoing work on API endpoints and UI improvements.
- **Security Posture**: Operating in a highly restrictive, Zero-Trust KVM virtualized environment with hypervisor-enforced network kill switches and reverse-proxied TLS.

## 🏗️ 2. Architecture & Design Patterns

(Agent, please read this section to understand how code is structured before making any changes)

- **Architecture Style**: Client-Server architecture with RESTful API.
- **Infrastructure Topology**: Physical Ubuntu Server -> KVM/libvirt VM (Ubuntu 24.04) -> Nginx Reverse Proxy (Port 443) -> systemd background daemon -> oatpp backend (127.0.0.1:8000).
- **Network Isolation**: The VM is blocked from accessing the Host LAN and Host Docker containers via an `iptables` DOCKER-USER/FORWARD/INPUT kill switch. Inbound traffic is DNAT routed from the Host's port 443 to the VM.
- **Frontend Paradigm**: Desktop Client application building using Tauri (Rust + Web Technologies).
- **Backend Paradigm**: REST API serving JSON endpoints and HTTP Range streaming.
- **Database Schema approach**: Relational using SQLite via `oatpp-sqlite`.
- **Directory Structure Overview**:
  - `server/`: Backend C++ code (oatpp HTTP server, libtorrent integration, DB schema).
  - `clientTauri/`: Frontend application (Tauri, Vite, HTML/CSS/JS).
  - `HttpMediaStream/`, `BrunoRequests/`: API testing/streaming utilities.

## 🛠️ 3. Technology Stack

(Agent, strictly adhere to these technologies unless instructed otherwise)

- **Frontend**: Tauri 2.0, Rust, HTML5, Vanilla CSS, JS/Vite.
- **Backend**: C++20, oatpp 1.3.0, libtorrent 2.0.10, libcurl 8.4.0.
- **Database**: SQLite (via oatpp-sqlite 1.3.0).
- **JSON Handling**: nlohmann_json 3.11.3.
- **Logging**: spdlog 1.12.0.
- **Testing**: Postman/Bruno for API requests (BrunoRequests directory).
- **Perimeter/Routing**: Nginx (Reverse Proxy), OpenSSL (4096-bit self-signed RSA), iptables/libvirt hooks.
- **DevOps/Deployment**:
  - CMake builds with Conan (installed via `pipx` for PEP-668 compliance) for Server.
  - Managed via `systemd` daemon (`mediastream.service`).
  - GitHub Deploy Keys (Read-Only) for secure source pulls.

## 🔒 4. Rules & Best Practices

(Agent, adhere to these project-specific rules when writing or modifying code)

- **Coding Standards**: C++ best practices, proper memory management, separation of concerns between API routing, controllers, and services in oatpp.
- **Compiler Hardening**: All C++ targets MUST compile with ASLR and stack protection: `-fPIE`, `-pie`, `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-Wl,-z,relro`, `-Wl,-z,now`.
- **Systemd Sandboxing**: The backend daemon runs as `appuser` (non-root) with `NoNewPrivileges=yes`, `ProtectSystem=full`, and dynamically injected `EnvironmentFile` variables.
- **Security Protocols**:
  - Never execute as root.
  - Downloads folder restricted to `chmod 750`.
  - Environment files restricted to `chmod 600`.
  - Never bypass the Nginx reverse proxy (backend must bind strictly to `127.0.0.1`).
- **Performance Guidelines**: Efficient streaming using HTTP Range requests (RFC 7233). Optimize database queries for watch history and content catalog.
- **Commit Guidelines**: Clear and descriptive commit messages indicating the component changed (e.g., Server API, Client UI).

## 🔧 5. Environment Setup & Scripts

(Agent, use these commands to run, test, or build the application)

- **Pull Code (VM)**: `cd ~/cpp-app && git pull` (Uses isolated Deploy Key).
- **Build Server (VM)**: `cd ~/cpp-app/server && ./quick-build.sh` (Script uses `pipx conan` and `cmake`).
- **Manage Service (VM)**:
  - `sudo systemctl restart mediastream` (Apply new build/env).
  - `sudo journalctl -u mediastream -f` (Tail live application logs).
- **Host VM Management**: `./start-cpp-vm.sh` (Spins up the KVM environment).
- **Build (Client)**: `cd clientTauri && npm install && npm run tauri dev`
