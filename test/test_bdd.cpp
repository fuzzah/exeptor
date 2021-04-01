#include "catch.hpp"

#include "../src/exeptor.cpp"

SCENARIO("vector-generating functions work on argv/envp", "[generic]") {
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

SCENARIO("argument replacement works", "[argv]") {
  GIVEN("Settings loaded with 2 distinct programs and arguments") {
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

      THEN("resulting argv should contain every added option") {
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

      THEN("resulting argv should match check exactly") {
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
