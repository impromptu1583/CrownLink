#pragma once
#define JSON_DIAGNOSTICS 1

#include <string>
#include <concepts>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <format>
#include <initializer_list>
#include <fstream>
#include <chrono>
#include <mutex>

#include <filesystem>
namespace fs = std::filesystem;

using namespace std::literals;

#define EnumStringCase(X) case X: return #X