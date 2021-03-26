#pragma once

#include <iostream>
#include <map>
#include <set>

#include "yaml-cpp/yaml.h"

class ReplacementSettings {
public:
  using options_t = std::set<std::string>;

  std::map<std::string, std::string> programs;
  std::map<std::string, options_t> add_options;
  std::map<std::string, options_t> del_options;

  ReplacementSettings() {}
  ~ReplacementSettings() {}

  bool parse_from_file(std::string path) {
    bool verbose = getenv("EXEPTOR_VERBOSE") != nullptr;
    if (verbose) {
      std::cout << "Reading config file '" << path << "'" << std::endl;
    }
    YAML::Node config = YAML::LoadFile(path);

    if (!config["target_groups"]) {
      std::cerr << "Error: config file '" << path
                << "' does not contain 'target_groups'" << std::endl;
      return false;
    }

    auto groups = config["target_groups"];

    if (!groups.IsMap()) {
      std::cerr << "Error: 'target_groups' is empty" << std::endl;
      return false;
    }

    enum class OType : uint8_t { ADD, DELETE } optType;

    for (auto group = groups.begin(); group != groups.end(); group++) {
      auto group_name = group->first.as<std::string>();
      auto group_items = group->second;

      if (!group_items.IsMap()) {
        std::cerr << "Error: target_group '" << group_name
                  << "' is not a key-value list" << std::endl;
        return false;
      }
      if (verbose) {
        std::cout << "Found group '" << group_name << "'" << std::endl;
      }
      if (!group_items["replacements"]) {
        std::cerr << "Error: target_group '" << group_name
                  << "' doesn't contain 'replacements' setting" << std::endl;
        return false;
      }

      auto replacements = group_items["replacements"];
      if (!replacements.IsMap()) {
        std::cerr << "Error: setting 'replacements' is not a key-value list in "
                     "group '"
                  << group_name << "'" << std::endl;
        return false;
      }
      for (auto repl = replacements.begin(); repl != replacements.end();
           repl++) {
        auto binary = repl->first.as<std::string>();
        auto binary_replacement = repl->second;
        if (!binary_replacement.IsScalar()) {
          std::cerr << "Error: binary replacement for '" << binary
                    << "' in group '" << group_name << "' is not a simple value"
                    << std::endl;
          return false;
        }
        if (verbose) {
          std::cout << "Replacement for '" << binary << "' is '"
                    << binary_replacement.as<std::string>() << "'" << std::endl;
        }
        programs[binary] = binary_replacement.as<std::string>();
      }

      for (auto key = group_items.begin(); key != group_items.end(); key++) {
        auto settingName = key->first.as<std::string>();
        auto setting = key->second;

        if (settingName == "add-options") {
          optType = OType::ADD;
        } else if (settingName == "del-options") {
          optType = OType::DELETE;
        } else if (settingName == "replacements") {
          continue;
        } else {
          std::cerr << "Error: unknown setting '" << settingName
                    << "' in group '" << group_name << "'" << std::endl;
          return false;
        }

        if (!setting.IsSequence()) {
          std::cerr << "Error: setting '" << settingName
                    << "' is not a list of values in group '"
                    << group->first.as<std::string>() << "'" << std::endl;
          return false;
        }

        for (auto k = setting.begin(); k != setting.end(); k++) {
          if (!k->IsScalar()) {
            std::cerr << "Error: setting '" << settingName
                      << "' is not a simple value in group '"
                      << group->first.as<std::string>() << "'" << std::endl;
            return false;
          }
          bool quiet = !verbose;
          if (quiet) {
            for (auto &prog : programs) {
              if (optType == OType::ADD) {
                add_options[prog.first].insert(k->as<std::string>());
              } else {
                del_options[prog.first].insert(k->as<std::string>());
              }
            }
          } else {
            for (auto &prog : programs) {
              std::cout << "Program '" << prog.first << "': ";

              if (optType == OType::ADD) {
                std::cout << "add ";
                add_options[prog.first].insert(k->as<std::string>());
              } else {
                std::cout << "delete ";
                del_options[prog.first].insert(k->as<std::string>());
              }

              std::cout << "setting '" << k->as<std::string>() << "'"
                        << std::endl;
            }
          }
        }
      }
    }

    return true;
  }
};