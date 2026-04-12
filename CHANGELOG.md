# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.6] - 2026-4-12

### About
- This is the initial release of whichbot-r and should be equivalent to [whichbot 1.05](https://sourceforge.net/projects/whichbot/files/whichbot/)

### Removed

- Removed STL_port. Using the c++ standard library instead.

### Fixed

-  Crash when parsing hive messages under [ENSL version 3.3](https://github.com/ENSL/NS)

### Changes

- Upgraded GetPot library from 1.0 to 1.1.18

- Changes needed to compile under modern c++. For example, switching from std::auto_ptr to std::unique_ptr, using <unordered_map> instead of <hash_map> etc.

### Added

- add ability to use Cmake for the build system