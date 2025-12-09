# Changelog

All notable changes to LoBBS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.1] - 2025-12-09

### Patch
- Updated module registration from `#pragma MPM_MODULE` to `MPM_REGISTER_MESHTASTIC_MODULE` comment directive
- Renamed module variable from `lobbsPlugin` to `lobbsModule` for consistency

## [1.2.0] - 2025-12-09

### Minor
- Logo assets with logo.pxd and logo.webp files
- Admin user functionality - first registered user automatically becomes administrator

### Patch
- Walkthrough video link in README
- Updated README with improved documentation, license information, and links
- Replaced manual memory management with LoDb::freeRecords() in LoBBSModule
- Added extern declaration for lobbsPlugin and updated MPM module pragma

## [1.1.1] - 2025-12-05

### Patch
- Refactored header structure: removed LoBBS and meta headers, updated include paths, introduced new plugin header

## [1.1.0] - 2025-12-05

### Minor
- MPM plugin compatibility

### Patch
- Direct messages now properly filter out broadcasts

## [1.0.1] - 2025-12-05

### Patch
- Enhanced installation documentation with Mesh Forge method
- Updated installation instructions for MPM integration

## [1.0.0] - 2025-11-28

### Major
- Initial release of LoBBS (LoDB Bulletin Board System)
- User registration and authentication
- Bulletin board messaging system
- Direct messaging between users
- Session management
- User directory and search functionality

[Unreleased]: https://github.com/MeshEnvy/lobbs/compare/v1.2.1...HEAD
[1.2.1]: https://github.com/MeshEnvy/lobbs/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/MeshEnvy/lobbs/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/MeshEnvy/lobbs/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/MeshEnvy/lobbs/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/MeshEnvy/lobbs/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/MeshEnvy/lobbs/compare/c911244...v1.0.0
