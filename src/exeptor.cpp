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

ReplacementSettings settings;

bool intercept_allowed = true;
bool exeptor_initialized = false;
char cmdline[4096]; // cmdline of host application

void initlib() {
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

  if (!settings.parse_from_file(config_path)) {
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

  for (const auto &it : settings.programs) {
    if (it.second == cmdline) {
      intercept_allowed = false;
      break; // prevent recursive calls from tools that have already
             // been replaced
    }
  }

  exeptor_initialized = true;
}

// static void __attribute__((constructor)) libmain() { initlib(); }

void prep_prog_argv(std::string &prog, std::vector<const char *> &args) {
  auto t = settings.programs.find(prog);
  if (t != settings.programs.end()) {
    auto &del_opts = settings.del_options[prog];
    auto &add_opts = settings.add_options[prog];
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
}

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

void prep_prog_argv_env(std::string &prog, std::vector<const char *> &args,
                        std::vector<const char *> &envs) {
  prep_prog_argv(prog, args);

  // TODO: add-environ, del-environ
  /*
  auto t = settings.programs.find(prog);
  if (t != settings.programs.end()) {
  }
  */
  prep_common_envp(envs);
}

extern "C" {

int execl(const char *pathname, const char *arg, ...) {
  // execl -> execv
  initlib();
  logprintf("{intercept} app is calling execl('%s')\n", pathname);
  logflush();

  std::vector<const char *> args;
  va_list vl;
  va_start(vl, arg);
  const char *parg = arg;
  while (parg != nullptr) {
    args.push_back(parg);
    parg = va_arg(vl, const char *);
  }
  va_end(vl);

  if (intercept_allowed) {
    auto t = settings.programs.find(pathname);
    if (t != settings.programs.end()) {
      logprintf("[INTERCEPT] execl(\"%s\"", pathname);

      for (const auto &parg : args) {
        logprintf(", \"%s\"", parg);
      }

      std::string prog = pathname;
      prep_prog_argv(prog, args);
      prep_common_environ();

      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      real_execv(prog.c_str(), const_cast<char *const *>(args.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    }
  }
  args.push_back(nullptr);
  prep_common_environ();
  real_execv(pathname, const_cast<char *const *>(args.data()));
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int execlp(const char *file, const char *arg, ...) {
  // execlp -> execvp
  initlib();
  logprintf("{intercept} app is calling execlp('%s')\n", file);
  logflush();

  std::vector<const char *> args;
  va_list vl;
  va_start(vl, arg);
  const char *parg = arg;
  while (parg != nullptr) {
    args.push_back(parg);
    parg = va_arg(vl, const char *);
  }
  va_end(vl);

  if (intercept_allowed) {
    auto t = settings.programs.find(file);
    if (t != settings.programs.end()) {
      logprintf("[INTERCEPT] execlp(\"%s\"", file);

      for (const auto &parg : args) {
        logprintf(", \"%s\"", parg);
      }

      std::string prog = file;
      prep_prog_argv(prog, args);
      prep_common_environ();

      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      real_execvp(prog.c_str(), const_cast<char *const *>(args.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    }
  }
  args.push_back(nullptr);
  prep_common_environ();
  real_execvp(file, const_cast<char *const *>(args.data()));
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int execle(const char *pathname, const char *arg,
           ... /*, (char *) NULL, char *const envp[] */) {
  // execle -> execve
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

  std::vector<const char *> envs;
  if (envp) {
    size_t i = 0;
    while (envp[i]) {
      envs.push_back(envp[i]);
      i++;
    }
  }

  if (intercept_allowed) {
    auto t = settings.programs.find(pathname);
    if (t != settings.programs.end()) {
      logprintf("[INTERCEPT] execle(\"%s\"", pathname);

      for (const auto &parg : args) {
        logprintf(", \"%s\"", parg);
      }

      if (envp) {
        logprintf(", NULL, /* envp: {");
        size_t i = 0;
        while (envp[i]) {
          logprintf("'%s' ", envp[i]);
          i++;
        }
        logprintf("} */ %p);", envp);
      } else {
        logprintf(", NULL, /* envp = */ NULL);");
      }

      std::string prog = pathname;
      prep_prog_argv_env(prog, args, envs);
      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      real_execve(prog.c_str(), const_cast<char *const *>(args.data()),
                  const_cast<char *const *>(envs.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    }
  }
  args.push_back(nullptr);
  prep_common_envp(envs);
  real_execve(pathname, const_cast<char *const *>(args.data()),
              const_cast<char *const *>(envs.data()));
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int execv(const char *pathname, char *const argv[]) {
  initlib();
  logprintf("{intercept} app is calling execv('%s')\n", pathname);
  logflush();

  if (intercept_allowed) {
    auto t = settings.programs.find(pathname);
    if (t != settings.programs.end()) {
      std::vector<const char *> args;

      logprintf("[INTERCEPT] execv(\"%s\", /* argv: {", pathname);

      size_t i = 0;
      while (argv[i]) {
        logprintf("'%s' ", argv[i]);
        args.push_back(argv[i]);
        i++;
      }

      std::string prog = pathname;
      prep_prog_argv(prog, args);
      prep_common_environ();

      logprintf("} */ %p); // replaced with '%s' \n", argv, prog.c_str());
      logflush();

      real_execv(prog.c_str(), const_cast<char *const *>(args.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    }
  }
  prep_common_environ();
  real_execv(pathname, argv);
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int execvp(const char *pathname, char *const argv[]) {
  initlib();
  logprintf("{intercept} app is calling execvp('%s')\n", pathname);
  logflush();

  if (intercept_allowed) {
    auto t = settings.programs.find(pathname);
    if (t != settings.programs.end()) {
      std::vector<const char *> args;

      logprintf("[INTERCEPT] execvp(\"%s\", /* argv: {", pathname);

      size_t i = 0;
      while (argv[i]) {
        logprintf("'%s' ", argv[i]);
        args.push_back(argv[i]);
        i++;
      }

      std::string prog = pathname;
      prep_prog_argv(prog, args);
      prep_common_environ();

      logprintf("} */ %p); // replaced with '%s' \n", argv, prog.c_str());
      logflush();

      real_execvp(prog.c_str(), const_cast<char *const *>(args.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    }
  }
  prep_common_environ();
  real_execvp(pathname, argv);
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
  initlib();
  logprintf("{intercept} app is calling execvpe('%s')\n", file);
  logflush();

  std::vector<const char *> envs;
  if (envp) {
    size_t i = 0;
    while (envp[i]) {
      envs.push_back(envp[i]);
      i++;
    }
  }

  if (intercept_allowed) {
    auto t = settings.programs.find(file);
    if (t != settings.programs.end()) {
      std::vector<const char *> args;

      logprintf("[INTERCEPT] execvpe(\"%s\", /* argv: {", file);

      size_t i = 0;
      while (argv[i]) {
        logprintf("'%s' ", argv[i]);
        args.push_back(argv[i]);
        i++;
      }

      if (envp) {
        logprintf("} */ %p,  /* envp: {", argv);
        i = 0;
        while (envp[i]) {
          logprintf("'%s' ", envp[i]);
          i++;
        }
        logprintf("} */ %p);", envp);
      } else {
        logprintf("} */ NULL);");
      }

      std::string prog = file;
      prep_prog_argv_env(prog, args, envs);
      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      real_execvpe(prog.c_str(), const_cast<char *const *>(args.data()),
                   const_cast<char *const *>(envs.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    }
  }
  prep_common_envp(envs);
  real_execvpe(file, argv, const_cast<char *const *>(envs.data()));
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
  initlib();
  logprintf("{intercept} app is calling execve('%s')\n", pathname);
  logflush();

  std::vector<const char *> envs;
  if (envp) {
    size_t i = 0;
    while (envp[i]) {
      envs.push_back(envp[i]);
      i++;
    }
  }

  if (intercept_allowed) {
    logprintf("{intercept} -> replacement for '%s' is not blocked\n", pathname);
    auto t = settings.programs.find(pathname);
    if (t != settings.programs.end()) {
      std::vector<const char *> args;

      logprintf("[INTERCEPT] execve(\"%s\", /* argv: {", pathname);

      size_t i = 0;
      while (argv[i]) {
        logprintf("'%s' ", argv[i]);
        args.push_back(argv[i]);
        i++;
      }

      if (envp) {
        logprintf("} */ %p,  /* envp: {", argv);
        i = 0;
        while (envp[i]) {
          logprintf("'%s' ", envp[i]);
          i++;
        }
        logprintf("} */ %p);", envp);
      } else {
        logprintf("} */ NULL);");
      }

      std::string prog = pathname;
      prep_prog_argv_env(prog, args, envs);
      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      real_execve(prog.c_str(), const_cast<char *const *>(args.data()),
                  const_cast<char *const *>(envs.data()));
      logprintf("[FATAL] Last call failed\n");
      logflush();
      _exit(7);
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", pathname);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", pathname);
  }

  prep_common_envp(envs);
  logflush();
  real_execve(pathname, argv, const_cast<char *const *>(envs.data()));
  logprintf("[FATAL] Last call failed\n");
  logflush();
  _exit(7);
}

int posix_spawn(pid_t *__restrict pid, const char *__restrict path,
                const posix_spawn_file_actions_t *__restrict file_actions,
                const posix_spawnattr_t *__restrict attrp,
                char *const *__restrict argv, char *const *__restrict envp) {

  initlib();
  logprintf("{intercept} app is calling posix_spawn('%s')\n", path);
  logflush();

  std::vector<const char *> envs;
  if (envp) {
    size_t i = 0;
    while (envp[i]) {
      envs.push_back(envp[i]);
      i++;
    }
  }

  if (intercept_allowed) {
    logprintf("{intercept} -> replacement for '%s' is not blocked\n", path);
    auto t = settings.programs.find(path);
    if (t != settings.programs.end()) {
      std::vector<const char *> args;
      logprintf("[INTERCEPT] posix_spawn( /* pid = */ %p, \"%s\", ... ); ", pid,
                path);

      size_t i = 0;
      while (argv[i]) {
        // logprintf("'%s' ", argv[i]);
        args.push_back(argv[i]);
        i++;
      }

      std::string prog = path;
      prep_prog_argv_env(prog, args, envs);
      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      return real_posix_spawn(pid, prog.c_str(), file_actions, attrp,
                              const_cast<char *const *>(args.data()),
                              const_cast<char *const *>(envs.data()));
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", path);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", path);
  }
  prep_common_envp(envs);
  return real_posix_spawn(pid, path, file_actions, attrp, argv,
                          const_cast<char *const *>(envs.data()));
}

int posix_spawnp(pid_t *__restrict pid, const char *__restrict file,
                 const posix_spawn_file_actions_t *__restrict file_actions,
                 const posix_spawnattr_t *__restrict attrp,
                 char *const *__restrict argv, char *const *__restrict envp) {
  initlib();
  logprintf("{intercept} app is calling posix_spawnp('%s')\n", file);
  logflush();

  std::vector<const char *> envs;
  if (envp) {
    size_t i = 0;
    while (envp[i]) {
      envs.push_back(envp[i]);
      i++;
    }
  }

  if (intercept_allowed) {
    logprintf("{intercept} -> replacement for '%s' is not blocked\n", file);
    auto t = settings.programs.find(file);
    if (t != settings.programs.end()) {
      std::vector<const char *> args;
      logprintf("[INTERCEPT] posix_spawnp( /* pid = */ %p, \"%s\", ... ); ",
                pid, file);

      size_t i = 0;
      while (argv[i]) {
        // logprintf("'%s' ", argv[i]);
        args.push_back(argv[i]);
        i++;
      }

      std::string prog = file;
      prep_prog_argv_env(prog, args, envs);
      logprintf(" // replaced with '%s' \n", prog.c_str());
      logflush();

      return real_posix_spawnp(pid, prog.c_str(), file_actions, attrp,
                               const_cast<char *const *>(args.data()),
                               const_cast<char *const *>(envs.data()));
    } else {
      logprintf("{intercept} -> no replacement found for '%s'\n", file);
    }
  } else {
    logprintf("{intercept} -> not allowed to replace '%s'\n", file);
  }
  prep_common_envp(envs);
  return real_posix_spawnp(pid, file, file_actions, attrp, argv,
                           const_cast<char *const *>(envs.data()));
}

} // extern "C"
