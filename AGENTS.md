# 🤖 AI Agent Context file (AGENTS.md)
This file contains the core contextual information about this project, designed specifically to help GeminiAI (and other AI agents) quickly understand the architecture, technology stack, and specific needs of the project. 

*Instructions for the Developer: Fill in the bracketed `[ ]` information. AI Agents should read this file upon starting a new task to contextualize their efforts.*

## 🎯 1. Project Overview & Needs
- **Project Name**: MediaStream
- **Primary Goal**: A private Netflix-like application that discovers, downloads, and streams torrents locally via HTTPS, with a focus on instantaneous streaming and cybersecurity.
- **Target Audience/Users**: Personal/Private use for media streaming across Windows 10, Android, and potentially web clients, served from a local Ubuntu server.
- **Key Features**:
  1. Content Discovery (Movies, TV Series, Anime via YTS, EZTV, Nyaa) and Metadata enrichment (TMDB).
  2. Sequential torrent downloading and management via `libtorrent`.
  3. Video streaming using HTTP Range requests for instant playback.
- **Current State/Phase**: MVP Development / In Progress. Core backend (REST API, Discovery, Torrent Engine) and Qt desktop client foundation built. Ongoing work on API endpoints and UI improvements.

## 🏗️ 2. Architecture & Design Patterns
*(Agent, please read this section to understand how code is structured before making any changes)*
- **Architecture Style**: Client-Server architecture with RESTful API.
- **Frontend Paradigm**: Desktop Client application (with planned Android/iOS support).
- **Backend Paradigm**: REST API serving JSON endpoints and HTTP Range streaming.
- **Database Schema approach**: Relational using SQLite via `oatpp-sqlite`.
- **Directory Structure Overview**:
  - `server/`: Backend C++ code (oatpp HTTP server, libtorrent integration, DB schema).
  - `client/`: Frontend C++ code (Qt5 Widgets, Qt5 Network).
  - `HttpMediaStream/`, `BrunoRequests/`: API testing/streaming utilities.

## 🛠️ 3. Technology Stack
*(Agent, strictly adhere to these technologies unless instructed otherwise)*
- **Frontend**: C++17, Qt5 (Widgets, Network, Multimedia).
- **Backend**: C++20, oatpp 1.3.0, libtorrent 2.0.10, libcurl 8.4.0.
- **Database**: SQLite (via oatpp-sqlite 1.3.0).
- **JSON Handling**: nlohmann_json 3.11.3.
- **Logging**: spdlog 1.12.0.
- **Testing**: Postman/Bruno for API requests (BrunoRequests directory).
- **DevOps/Deployment**: CMake builds (with Conan for Server, vcpkg for Client), running on Ubuntu Server VM (Bash scripts).

## 🔒 4. Rules & Best Practices
*(Agent, adhere to these project-specific rules when writing or modifying code)*
- **Coding Standards**: C++ best practices, proper memory management, separation of concerns between API routing, controllers, and services in oatpp.
- **Error Handling**: Robust error checking, specifically around network requests, database operations, and torrent engine states.
- **Security Protocols**: Cybersecurity prioritized. Forced RC4 encryption for torrents, restrictive permissions (non-root execution, 0077 umask), SSL/TLS (planned), input validation for magnet URIs.
- **Performance Guidelines**: Efficient streaming using HTTP Range requests (RFC 7233). Optimize database queries for watch history and content catalog.
- **Commit Guidelines**: Clear and descriptive commit messages indicating the component changed (e.g., Server API, Client UI).

## 🔧 5. Environment Setup & Scripts
*(Agent, use these commands to run, test, or build the application)*
- **Install dependencies (Server)**: `cd server && mkdir build && cd build && conan install .. --build=missing`
- **Build (Server)**: `cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && cmake --build .`
- **Run (Server)**: `./mediastream_server`
- **Build (Client)**: `cd client && mkdir build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build .`
- **Run (Client)**: `./client`
- **Quickstart VM**: `./quick-start-vm.sh`
