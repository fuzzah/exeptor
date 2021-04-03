#include "catch.hpp"

#include "../src/exeptor.cpp"

#include <map>

SCENARIO("vector-generating functions should work on argv/envp", "[generic]") {
  GIVEN("list with some elements in argv") {
    const size_t num_argv = 4;
    const char *argv[num_argv] = {"prog", "--arg1", "--a2", nullptr};

    REQUIRE(argv[num_argv - 1] == nullptr);

    WHEN("converting argv to vector") {
      auto args = vec_from_argv_envp(argv);

      // + 1 because args don't include nullptr
      THEN("vector size + 1 equals argv size") {
        REQUIRE(args.size() + 1 == num_argv);
      }

      THEN("vector elements point to same argv elements") {
        for (size_t i = 0; i < num_argv - 1; i++) {
          REQUIRE(args[i] == argv[i]);
        }
      }
    }
  }
}

SCENARIO("argument replacement should work correctly", "[argv]") {
  GIVEN("Settings loaded with two distinct programs and arguments") {
    g_settings.programs.clear();
    g_settings.add_options.clear();
    g_settings.del_options.clear();

    std::string orig1_name = "orig_prog";
    std::string repl1_name = "rep_prog";
    g_settings.programs[orig1_name] = repl1_name;
    g_settings.add_options[orig1_name] = {"--add1", "--add2"};
    g_settings.del_options[orig1_name] = {"--rem1", "--rem2", "--rem3"};

    std::string orig2_name = "orig_prog2";
    std::string repl2_name = "rep_prog2";
    g_settings.programs[orig2_name] = repl2_name;
    g_settings.add_options[orig2_name] = {"--added1", "--added2", "--added3"};

    REQUIRE(g_settings.programs.size() == 2);
    REQUIRE(g_settings.add_options.size() == 2);
    REQUIRE(g_settings.del_options.size() ==
            1); // second program has only "add-options"

    WHEN("input program doesn't match") {
      std::string prog = "something";
      auto argv_vec =
          std::vector<const char *>{prog.c_str(),  "--add1", "--added2", "-a",
                                    "--something", "--rem3", nullptr};
      // argv as seen in execv-like calls
      auto argv = const_cast<char *const *>(argv_vec.data());

      auto args = vec_from_argv_envp(argv); // argv as used in prep_argv_envp

      // + 1 because args don't include nullptr
      REQUIRE(args.size() + 1 == argv_vec.size());

      prep_prog_argv(prog, args);

      THEN("resulting program should stay unchanged") {
        REQUIRE(prog == "something");
      }

      THEN("resulting argv should stay unchanged") {
        auto check =
            std::vector<const char *>{"something",   "--add1", "--added2", "-a",
                                      "--something", "--rem3", nullptr};

        REQUIRE(args.size() == check.size());

        for (size_t i = 0; i < args.size() - 1; i++) {
          REQUIRE(std::strcmp(args[i], check[i]) == 0);
        }
        REQUIRE(args[args.size() - 1] == nullptr);
      }
    }

    WHEN("input program matches program with add-options") {
      std::string prog = orig2_name;
      auto argv_vec =
          std::vector<const char *>{prog.c_str(),  "--add1", "--added2", "-a",
                                    "--something", "--rem3", nullptr};
      // argv as seen in execv-like calls
      auto argv = const_cast<char *const *>(argv_vec.data());

      auto args = vec_from_argv_envp(argv); // argv as used in prep_argv_envp

      // + 1 because args don't include nullptr
      REQUIRE(args.size() + 1 == argv_vec.size());

      prep_prog_argv(prog, args);

      THEN("resulting program should change to correct replacement") {
        REQUIRE(prog == repl2_name);
      }

      THEN("argv[0] should change to correct replacement") {
        REQUIRE(repl2_name == args[0]);
      }

      THEN("resulting argv should pass check") {
        auto check = std::vector<const char *>{
            repl2_name.c_str(), "--add1", "--added2", "-a",
            "--something",      "--rem3", "--added1", "--added2",
            "--added3",         nullptr};

        REQUIRE(args.size() == check.size());

        for (size_t i = 0; i < args.size() - 1; i++) {
          REQUIRE(std::strcmp(args[i], check[i]) == 0);
        }

        REQUIRE(args[args.size() - 1] == nullptr);
      }
    }

    WHEN("input program matches program with both add-options & del-options") {
      std::string prog = orig1_name;
      auto argv_vec =
          std::vector<const char *>{prog.c_str(),  "--add1", "--added2", "-a",
                                    "--something", "--rem3", nullptr};
      auto argv = const_cast<char *const *>(
          argv_vec.data()); // argv as seen in execv-like calls

      auto args = vec_from_argv_envp(argv); // argv as used in prep_argv_envp

      // + 1 because args don't include nullptr
      REQUIRE(args.size() + 1 == argv_vec.size());

      prep_prog_argv(prog, args);

      THEN("resulting program should change to correct replacement") {
        REQUIRE(prog == repl1_name);
      }

      THEN("argv[0] should change to correct replacement") {
        REQUIRE(repl1_name == args[0]);
      }

      THEN("resulting argv should pass check") {
        auto check = std::vector<const char *>{
            repl1_name.c_str(), "--add1", "--added2", "-a",
            "--something",      "--add1", "--add2",   nullptr};

        REQUIRE(args.size() == check.size());

        for (size_t i = 0; i < args.size() - 1; i++) {
          REQUIRE(std::strcmp(args[i], check[i]) == 0);
        }

        REQUIRE(args[args.size() - 1] == nullptr);
      }
    }
  }
}

SCENARIO("exeptor env variables should propagate correctly", "[env]") {
  GIVEN("The host app passes envp with some variables") {
    std::vector<const char *> envp = {"KEY=value", "K=\"123 124 125\"",
                                      "SOMEPATH=/var/logs:/home/user/logs",
                                      nullptr};

    auto envs = vec_from_argv_envp(envp.data());

    clearenv();

    WHEN("exeptor variables present") {
      for (size_t i = 0; i < num_exeptor_vars; i++) {
        setenv(exeptor_envs[i], exeptor_envs[i], 1);
      }

      prep_common_envp(envs);

      THEN("env vars passed by application should be kept") {
        REQUIRE(envs.size() >= envp.size());

        for (const auto &e : envp) {
          REQUIRE(std::find(envs.cbegin(), envs.cend(), e) != envs.cend());
        }
      }

      THEN("exeptor env vars should be added with correct values") {
        REQUIRE(envs.size() >= envp.size() + 1);

        char tmp[sizeof(exeptor_envs[0]) * 2 + 2];

        for (size_t i = 0; i < num_exeptor_vars; i++) {
          snprintf(tmp, sizeof(tmp), "%s=%s", exeptor_envs[i], exeptor_envs[i]);
          auto variable_present =
              std::find_if(envs.cbegin(), envs.cend(), [&tmp](const char *s) {
                return s != nullptr && strcmp(s, tmp) == 0;
              }) != envs.cend();

          REQUIRE(variable_present);
        }
      }

      THEN("resulting envp should terminate with NULL") {
        REQUIRE(envs[envs.size() - 1] == nullptr);
      }

      THEN("no surplus env vars should be added") {
        REQUIRE(envs.size() == envp.size() + num_exeptor_vars);
      }
    }

    WHEN("NO exeptor variables present") {
      prep_common_envp(envs);

      THEN("env vars passed by application should be kept") {
        for (const auto &e : envp) {
          REQUIRE(std::find(envs.cbegin(), envs.cend(), e) != envs.cend());
        }
      }

      THEN("resulting envp should terminate with NULL") {
        REQUIRE(envs[envs.size() - 1] == nullptr);
      }

      THEN("no surplus env vars should be added") {
        REQUIRE(envs.size() == envp.size());
      }
    }
  }
}

SCENARIO("changes to argv and envp should not affect each other", "[generic]") {
  GIVEN("Settings loaded with one program") {
    g_settings.programs.clear();
    g_settings.add_options.clear();
    g_settings.del_options.clear();

    std::string orig1_name = "orig_prog";
    std::string repl1_name = "rep_prog";
    g_settings.programs[orig1_name] = repl1_name;
    g_settings.add_options[orig1_name] = {"--add1", "--add2"};
    g_settings.del_options[orig1_name] = {"--rem1", "--rem2", "--rem3"};

    clearenv();

    AND_GIVEN("exeptor env variables present") {
      for (size_t i = 0; i < num_exeptor_vars; i++) {
        setenv(exeptor_envs[i], exeptor_envs[i], 1);
      }

      WHEN("host app tries to run matching app with some argv and envp") {
        std::vector<const char *> argv = {"argv0", "--rem1", "--good",
                                          "1",     "--rem2", nullptr};

        std::vector<const char *> envp = {"KEY=value", "K=\"123 124 125\"",
                                          "SOMEPATH=/var/logs:/home/user/logs",
                                          nullptr};

        auto args = vec_from_argv_envp(argv.data());
        auto envs = vec_from_argv_envp(envp.data());

        std::string prog = "orig_prog";

        prep_prog_argv_env(prog, args, envs);

        THEN("intercepted argv options should pass check") {
          auto check = std::vector<const char *>{
              repl1_name.c_str(), "--good", "1", "--add1", "--add2", nullptr};

          REQUIRE(args.size() == check.size());

          for (size_t i = 0; i < args.size() - 1; i++) {
            REQUIRE(std::strcmp(args[i], check[i]) == 0);
          }

          REQUIRE(args[args.size() - 1] == nullptr);
        }

        THEN("env vars passed by application should be kept") {
          REQUIRE(envs.size() >= envp.size());

          for (const auto &e : envp) {
            REQUIRE(std::find(envs.cbegin(), envs.cend(), e) != envs.cend());
          }
        }

        THEN("exeptor env vars should be added with correct values") {
          REQUIRE(envs.size() >= envp.size() + 1);

          char tmp[sizeof(exeptor_envs[0]) * 2 + 2];

          for (size_t i = 0; i < num_exeptor_vars; i++) {
            snprintf(tmp, sizeof(tmp), "%s=%s", exeptor_envs[i],
                     exeptor_envs[i]);
            auto variable_present =
                std::find_if(envs.cbegin(), envs.cend(), [&tmp](const char *s) {
                  return s != nullptr && strcmp(s, tmp) == 0;
                }) != envs.cend();

            REQUIRE(variable_present);
          }
        }

        THEN("resulting envp should terminate with NULL") {
          REQUIRE(envs[envs.size() - 1] == nullptr);
        }

        THEN("no surplus env vars should be added") {
          REQUIRE(envs.size() == envp.size() + num_exeptor_vars);
        }
      }

      WHEN("host app tries to run matching app with some argv but without "
           "envp") {
        std::vector<const char *> argv = {"argv0", "--rem1", "--good",
                                          "1",     "--rem2", nullptr};

        auto args = vec_from_argv_envp(argv.data());

        std::string prog = "orig_prog";

        prep_prog_argv(prog, args);

        THEN("intercepted argv options should pass check") {
          auto check = std::vector<const char *>{
              repl1_name.c_str(), "--good", "1", "--add1", "--add2", nullptr};

          REQUIRE(args.size() == check.size());

          for (size_t i = 0; i < args.size() - 1; i++) {
            REQUIRE(std::strcmp(args[i], check[i]) == 0);
          }

          REQUIRE(args[args.size() - 1] == nullptr);
        }

        THEN("exeptor env vars should be added with correct values") {
          auto envs = vec_from_argv_envp(environ);

          REQUIRE(envs[envs.size() - 1] != nullptr);

          // no NULL in envs, hence exact size match required
          REQUIRE(envs.size() == num_exeptor_vars);

          char tmp[sizeof(exeptor_envs[0]) * 2 + 2];

          for (size_t i = 0; i < num_exeptor_vars; i++) {
            snprintf(tmp, sizeof(tmp), "%s=%s", exeptor_envs[i],
                     exeptor_envs[i]);
            auto variable_present =
                std::find_if(envs.cbegin(), envs.cend(), [&tmp](const char *s) {
                  return s != nullptr && strcmp(s, tmp) == 0;
                }) != envs.cend();

            REQUIRE(variable_present);
          }
        }
      }
    }
  }
}
