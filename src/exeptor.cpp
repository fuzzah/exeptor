/*

file    :  src/exeptor.cpp
repo    :  https://github.com/fuzzah/exeptor
author  :  https://github.com/fuzzah
license :  MIT
check repository for more information


libexeptor
LD_PRELOAD this library to intercept calls to execl, execlp, execle, execv,
execvp, execve, execvpe, posix_spawn and posix_spawnp

not (yet) implemented: execveat, fexecve

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dlfcn.h>
#include <spawn.h>
#include <stdarg.h>
#include <unistd.h>

#include "config.hpp"

FILE *logfile = nullptr;

#if 0
#define logprintf(...)
#define logflush(...)
#else
#define logprintf(...)                                                         \
  if (logfile)                                                                 \
  fprintf(logfile, __VA_ARGS__)
#define logflush(...)                                                          \
  if (logfile)                                                                 \
  fflush(logfile)
#endif

#define FATAL(...)                                                             \
  do {                                                                         \
    logprintf("FATAL error in %s at %s:%u\nMessage: ", __func__, __FILE__,     \
              __LINE__);                                                       \
    logprintf(__VA_ARGS__);                                                    \
    logprintf("\n");                                                           \
    logflush();                                                                \
    _exit(7);                                                                  \
  } while (false)

#ifndef NDEBUG
#define DEBUG(...)                                                             \
  do {                                                                         \
    logprintf("DEBUG: %s at %s:%u. Message: ", __func__, __FILE__, __LINE__);  \
    logprintf(__VA_ARGS__);                                                    \
    logprintf("\n");                                                           \
    logflush();                                                                \
  } while (false)
#else
#define DEBUG(...)
#endif

typedef int (*execv_t)(const char *pathname, char *const argv[]);
execv_t real_execv = nullptr;

typedef int (*execvp_t)(const char *file, char *const argv[]);
execvp_t real_execvp = nullptr;

typedef int (*execve_t)(const char *pathname, char *const argv[],
                        char *const envp[]);
execve_t real_execve = nullptr;

typedef int (*execvpe_t)(const char *file, char *const argv[],
                         char *const envp[]);
execvpe_t real_execvpe = nullptr;

typedef int (*posix_spawn_t)(
    pid_t *__restrict pid, const char *__restrict path,
    const posix_spawn_file_actions_t *__restrict file_actions,
    const posix_spawnattr_t *__restrict attrp, char *const *__restrict argv,
    char *const *__restrict envp);

posix_spawn_t real_posix_spawn = nullptr;

typedef int (*posix_spawnp_t)(
    pid_t *__restrict pid, const char *__restrict file,
    const posix_spawn_file_actions_t *__restrict file_actions,
    const posix_spawnattr_t *__restrict attrp, char *const *__restrict argv,
    char *const *__restrict envp);

posix_spawnp_t real_posix_spawnp = nullptr;

ReplacementSettings g_settings;
bool g_intercept_allowed = true;
char cmdline[4096]; // cmdline of host application

void initlib() {
  static bool exeptor_initialized = false;
  if (exeptor_initialized)
    return;

  char *p = getenv("EXEPTOR_LOG");
  if (p) {
    logfile = fopen(p, "at");
    if (!logfile) {
      std::cerr << "libexeptor error: wasn't able to open file '" << p << "'"
                << std::endl;
      exit(2);
    }
  }

  char *config_path = getenv("EXEPTOR_CONFIG");
  char default_config_path[] = "/etc/libexeptor.yaml";
  if (!config_path) {
    config_path = default_config_path;
  }

  if (!g_settings.parse_from_file(config_path)) {
    std::cerr << "ERROR: failed to parse config file" << std::endl;
    exit(2);
  }

  real_execv = (execv_t)dlsym(RTLD_NEXT, "execv");
  real_execvp = (execvp_t)dlsym(RTLD_NEXT, "execvp");
  real_execve = (execve_t)dlsym(RTLD_NEXT, "execve");
  real_execvpe = (execvpe_t)dlsym(RTLD_NEXT, "execvpe");
  real_posix_spawn = (posix_spawn_t)dlsym(RTLD_NEXT, "posix_spawn");
  real_posix_spawnp = (posix_spawnp_t)dlsym(RTLD_NEXT, "posix_spawnp");

  if (!real_execv || !real_execvp || !real_execve || !real_execvpe ||
      !real_posix_spawn || !real_posix_spawnp) {
    std::cerr << "libexeptor error: wasn't able find original functions"
              << std::endl;
    exit(2);
  }

  FILE *f = fopen("/proc/self/cmdline", "rt");
  if (!f) {
    std::cerr << "libexeptor error: wasn't able to read /proc/self/cmdline"
              << std::endl;
    exit(3);
  }
  fread(cmdline, sizeof(cmdline), 1, f);
  fclose(f);

  logprintf("libexeptor: loaded to '%s'\n", cmdline);
  logflush();

  for (const auto &it : g_settings.programs) {
    if (it.second == cmdline) {
      g_intercept_allowed = false;
      break; // prevent recursive calls from tools that have already
             // been replaced
    }
  }

  exeptor_initialized = true;
}

// static void __attribute__((constructor)) libmain() { initlib(); }

#define num_exeptor_vars 4
char exeptor_envs[num_exeptor_vars][PATH_MAX + 20] = {
    "EXEPTOR_VERBOSE", "EXEPTOR_CONFIG", "EXEPTOR_LOG", "LD_PRELOAD"};

// for use with exec-calls not ending with 'e'
void prep_common_environ() {
  for (size_t i = 0; i < num_exeptor_vars; i++) {
    char *p = getenv(exeptor_envs[i]);

    if (!p) {
      continue;
    }

    setenv(exeptor_envs[i], p, 1);
  }
}

// for use with exec-calls ending with 'e'
void prep_common_envp(std::vector<const char *> &envs) {
  for (size_t i = 0; i < num_exeptor_vars; i++) {
    char *p = getenv(exeptor_envs[i]);

    if (!p) {
      continue;
    }

    // it's ok to change global vars because this code executes only once before
    // exec-call
    snprintf(exeptor_envs[i] + strlen(exeptor_envs[i]),
             sizeof(exeptor_envs[i]) - strlen(exeptor_envs[i]), "=%s", p);

    envs.push_back(exeptor_envs[i]);
    setenv(exeptor_envs[i], p, 1);
  }
  auto szorder = [](const char *left, const char *right) {
    return strcmp(left, right) < 0;
  };
  auto szequal = [](const char *left, const char *right) {
    return strcmp(left, right) == 0;
  };

  // deduplicate envs
  std::sort(envs.begin(), envs.end(), szorder);
  envs.erase(std::unique(envs.begin(), envs.end(), szequal), envs.end());
  envs.push_back(nullptr); // insert NULL in any case (for execXXe functions)
}

void prep_prog_argv(std::string &prog, std::vector<const char *> &args) {
  auto t = g_settings.programs.find(prog);
  if (t != g_settings.programs.end()) {
    auto &del_opts = g_settings.del_options[prog]; // [] may create vector here..
    auto &add_opts = g_settings.add_options[prog]; // ..and here, but it's ok

    if (args.size() > 0) {
      args[0] = t->second.c_str();
    }

    if (args.size() > 1 && !del_opts.empty()) {
      // don't replace argv[0]
      for (auto it = args.begin() + 1; it != args.end();) {
        if (del_opts.find(*it) != del_opts.end()) {
          it = args.erase(it);
        } else {
          it++;
        }
      }
    }

    for (auto &opt : add_opts) {
      args.push_back(opt.c_str());
    }

    prog = t->second.c_str();
  }

  args.push_back(nullptr); // insert NULL in any case (for execvXX functions)

  prep_common_environ();
}

void prep_prog_argv_env(std::string &prog, std::vector<const char *> &args,
                        std::vector<const char *> &envs) {
  prep_prog_argv(prog, args);

  // TODO: add-environ, del-environ
  /*
  auto t = g_settings.programs.find(prog);
  if (t != g_settings.programs.end()) {
  }
  */
  prep_common_envp(envs);
}

// return vector from NULL-terminated argv or envp list
std::vector<const char *> vec_from_argv_envp(const char *const *argv) {
  std::vector<const char *> ret;

  if (argv) {
    size_t i = 0;
    while (argv[i]) {
      ret.push_back(argv[i]);
      i++;
    }
  }

  return ret;
}

// return vector from va_list initialized with va_start.
// this function doesn't call va_end
std::vector<const char *> vec_from_va_list(va_list args, const char *arg) {
  std::vector<const char *> ret;

  const char *parg = arg;
  while (parg != nullptr) {
    ret.push_back(parg);
    parg = va_arg(args, const char *);
  }

  return ret;
}

// for posix_spawn & posix_spawnp
int _posix_spawn(pid_t *__restrict pid, const char *__restrict path,
                 const posix_spawn_file_actions_t *__restrict file_actions,
                 const posix_spawnattr_t *__restrict attrp,
                 char *const *__restrict argv, char *const *__restrict envp,
                 const char *funcname, posix_spawn_t posix_spawn_func) {
  logprintf("{intercept} app is calling '%s'('%s')\n", funcname, path);
  logflush();

  auto envs = vec_from_argv_envp(envp);

  if (g_intercept_allowed) {
    logprintf("{intercept} -> replacement for '%s' is not blocked\n", path);
    auto t = g_settings.programs.find(path);
    if (t != g_settings.programs.end()) {
      auto args = vec_from_argv_envp(argv);

      std::string prog = path;
      prep_prog_argv_env(prog, args, envs);

      logprintf("[INTERCEPT] %s( /* pid = */ %p, \"%s\", ... ); // replaced "
                "with '%s' \n",
                funcname, pid, path, prog.c_str());
      logflush();

      return posix_spawn_func(pid, prog.c_str(), file_actions, attrp,
                              const_cast<char *const *>(args.data()),
                              const_cast<char *const *>(envs.data()));
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", path);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", path);
  }
  prep_common_envp(envs);
  return posix_spawn_func(pid, path, file_actions, attrp, argv,
                          const_cast<char *const *>(envs.data()));
}

// for execv & execvp
int _execv(const char *pathname, char *const argv[], const char *funcname,
           execv_t execv_func) {
  logprintf("{intercept} app is calling %s('%s')\n", funcname, pathname);
  logflush();

  if (g_intercept_allowed) {
    auto t = g_settings.programs.find(pathname);
    if (t != g_settings.programs.end()) {
      auto args = vec_from_argv_envp(argv);

      std::string prog = pathname;
      prep_prog_argv(prog, args);

      logprintf("[INTERCEPT] %s(\"%s\", ...); // replaced with '%s' \n",
                funcname, pathname, prog.c_str());
      logflush();
      execv_func(prog.c_str(), const_cast<char *const *>(args.data()));
      FATAL("%s interception failed", funcname);
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", pathname);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", pathname);
  }
  prep_common_environ();
  execv_func(pathname, argv);
  FATAL("%s interception failed", funcname);
}

// for execve & execvpe
int _execve(const char *pathname, char *const argv[], char *const envp[],
            const char *funcname, execve_t execve_func) {
  logprintf("{intercept} app is calling %s('%s')\n", funcname, pathname);
  logflush();
  auto envs = vec_from_argv_envp(envp);

  if (g_intercept_allowed) {
    auto t = g_settings.programs.find(pathname);
    if (t != g_settings.programs.end()) {
      auto args = vec_from_argv_envp(envp);

      std::string prog = pathname;
      prep_prog_argv_env(prog, args, envs);

      logprintf("[INTERCEPT] %s(\"%s\", ...); // replaced with '%s' \n",
                funcname, pathname, prog.c_str());
      logflush();

      execve_func(prog.c_str(), const_cast<char *const *>(args.data()),
                  const_cast<char *const *>(envs.data()));
      FATAL("%s interception failed", funcname);
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", pathname);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", pathname);
  }
  prep_common_envp(envs);
  execve_func(pathname, argv, const_cast<char *const *>(envs.data()));
  FATAL("%s interception failed", funcname);
}

// for execl & execlp
int _execl(const char *pathname, std::vector<const char *> args,
           const char *origfuncname, execv_t execv_func) {
  logprintf("{intercept} app is calling %s('%s')\n", origfuncname, pathname);
  logflush();

  if (g_intercept_allowed) {
    auto t = g_settings.programs.find(pathname);
    if (t != g_settings.programs.end()) {
      std::string prog = pathname;
      prep_prog_argv(prog, args);

      logprintf("[INTERCEPT] execl(\"%s\", ...); // replaced with '%s' \n",
                pathname, prog.c_str());
      logflush();

      execv_func(prog.c_str(), const_cast<char *const *>(args.data()));
      FATAL("%s interception failed", origfuncname);
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", pathname);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", pathname);
  }
  args.push_back(nullptr);
  prep_common_environ();
  execv_func(pathname, const_cast<char *const *>(args.data()));
  FATAL("%s interception failed", origfuncname);
}

extern "C" {

int posix_spawn(pid_t *__restrict pid, const char *__restrict path,
                const posix_spawn_file_actions_t *__restrict file_actions,
                const posix_spawnattr_t *__restrict attrp,
                char *const *__restrict argv, char *const *__restrict envp) {
  initlib();
  return _posix_spawn(pid, path, file_actions, attrp, argv, envp, "posix_spawn",
                      real_posix_spawn);
}

int posix_spawnp(pid_t *__restrict pid, const char *__restrict file,
                 const posix_spawn_file_actions_t *__restrict file_actions,
                 const posix_spawnattr_t *__restrict attrp,
                 char *const *__restrict argv, char *const *__restrict envp) {
  initlib();
  return _posix_spawn(pid, file, file_actions, attrp, argv, envp,
                      "posix_spawnp", real_posix_spawnp);
}

int execv(const char *pathname, char *const argv[]) {
  initlib();
  return _execv(pathname, argv, "execv", real_execv);
}

int execvp(const char *pathname, char *const argv[]) {
  initlib();
  return _execv(pathname, argv, "execvp", real_execvp);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
  initlib();
  return _execve(file, argv, envp, "execvpe", real_execvpe);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
  initlib();
  return _execve(pathname, argv, envp, "execve", real_execve);
}

// call to execl gets converted to execv
int execl(const char *pathname, const char *arg, ...) {
  initlib();

  va_list vl;
  va_start(vl, arg);
  auto args = vec_from_va_list(vl, arg);
  va_end(vl);

  return _execl(pathname, args, "execl", real_execv);
}

// call to execlp gets converted to execvp
int execlp(const char *file, const char *arg, ...) {
  initlib();

  va_list vl;
  va_start(vl, arg);
  auto args = vec_from_va_list(vl, arg);
  va_end(vl);

  return _execl(file, args, "execlp", real_execvp);
}

// call to execle gets converted to execve
int execle(const char *pathname, const char *arg,
           ... /*, (char *) NULL, char *const envp[] */) {
  initlib();
  logprintf("{intercept} app is calling execle('%s')\n", pathname);
  logflush();

  std::vector<const char *> args;
  va_list vl;
  va_start(vl, arg);
  const char *parg = arg;
  while (parg != nullptr) {
    args.push_back(parg);
    parg = va_arg(vl, const char *);
  }
  char *const *envp = va_arg(vl, char *const *);
  va_end(vl);

  auto envs = vec_from_argv_envp(envp);

  if (g_intercept_allowed) {
    auto t = g_settings.programs.find(pathname);
    if (t != g_settings.programs.end()) {
      std::string prog = pathname;
      prep_prog_argv_env(prog, args, envs);

      logprintf("[INTERCEPT] execle(\"%s\", ...); // replaced with '%s' \n",
                pathname, prog.c_str());
      logflush();

      real_execve(prog.c_str(), const_cast<char *const *>(args.data()),
                  const_cast<char *const *>(envs.data()));
      FATAL("execle interception failed");
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", pathname);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", pathname);
  }
  args.push_back(nullptr);
  prep_common_envp(envs);
  real_execve(pathname, const_cast<char *const *>(args.data()),
              const_cast<char *const *>(envs.data()));
  FATAL("execle interception failed");
}

} // extern "C"
