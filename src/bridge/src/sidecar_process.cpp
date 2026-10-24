/**
 * @file sidecar_process.cpp
 * @brief Platform implementation of the sidecar launcher.
 */
#include <srsp/bridge/sidecar_process.hpp>

#include <cstring>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <poll.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace srsp::bridge {

#if defined(_WIN32)

SidecarProcess::~SidecarProcess() { close_handles(); }

bool SidecarProcess::start(const std::string& executable,
                           const std::vector<std::string>& args) {
  if (running()) {
    return false;
  }

  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;

  HANDLE read_end = nullptr;
  HANDLE write_end = nullptr;
  if (!::CreatePipe(&read_end, &write_end, &attributes, 0)) {
    return false;
  }
  ::SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

  std::string command = "\"" + executable + "\"";
  for (const auto& arg : args) {
    command += " \"" + arg + "\"";
  }

  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = write_end;
  startup.hStdError = write_end;
  startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION info{};
  const BOOL ok = ::CreateProcessA(
      nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
      nullptr, nullptr, &startup, &info);
  ::CloseHandle(write_end);
  if (!ok) {
    ::CloseHandle(read_end);
    return false;
  }

  ::CloseHandle(info.hThread);
  process_ = info.hProcess;
  pipe_read_ = read_end;
  pid_ = static_cast<int>(info.dwProcessId);
  return true;
}

bool SidecarProcess::read_byte(char* out, int timeout_ms) {
  const DWORD start = ::GetTickCount();
  for (;;) {
    DWORD available = 0;
    if (!::PeekNamedPipe(static_cast<HANDLE>(pipe_read_), nullptr, 0, nullptr,
                         &available, nullptr)) {
      return false;
    }
    if (available > 0) {
      DWORD read = 0;
      if (!::ReadFile(static_cast<HANDLE>(pipe_read_), out, 1, &read, nullptr) ||
          read != 1) {
        return false;
      }
      return true;
    }
    if (::GetTickCount() - start > static_cast<DWORD>(timeout_ms)) {
      return false;
    }
    ::Sleep(5);
  }
}

bool SidecarProcess::read_line(std::string* line, int timeout_ms) {
  if (!running() || pipe_read_ == nullptr) {
    return false;
  }
  std::string result;
  char c = 0;
  while (read_byte(&c, timeout_ms)) {
    if (c == '\n') {
      if (!result.empty() && result.back() == '\r') {
        result.pop_back();
      }
      if (line != nullptr) {
        *line = result;
      }
      return true;
    }
    result.push_back(c);
  }
  return false;
}

void SidecarProcess::terminate() {
  if (running()) {
    ::TerminateProcess(static_cast<HANDLE>(process_), 0);
  }
}

int SidecarProcess::wait() {
  int code = -1;
  if (process_ != nullptr) {
    ::WaitForSingleObject(static_cast<HANDLE>(process_), 5000);
    DWORD exit_code = 0;
    if (::GetExitCodeProcess(static_cast<HANDLE>(process_), &exit_code)) {
      code = static_cast<int>(exit_code);
    }
  }
  close_handles();
  return code;
}

void SidecarProcess::close_handles() {
  if (pipe_read_ != nullptr) {
    ::CloseHandle(static_cast<HANDLE>(pipe_read_));
    pipe_read_ = nullptr;
  }
  if (process_ != nullptr) {
    ::CloseHandle(static_cast<HANDLE>(process_));
    process_ = nullptr;
  }
  pid_ = 0;
}

#else  // POSIX

SidecarProcess::~SidecarProcess() { close_handles(); }

bool SidecarProcess::start(const std::string& executable,
                           const std::vector<std::string>& args) {
  if (running()) {
    return false;
  }
  int fds[2] = {-1, -1};
  if (::pipe(fds) != 0) {
    return false;
  }
  const pid_t pid = ::fork();
  if (pid < 0) {
    ::close(fds[0]);
    ::close(fds[1]);
    return false;
  }
  if (pid == 0) {
    ::dup2(fds[1], STDOUT_FILENO);
    ::dup2(fds[1], STDERR_FILENO);
    ::close(fds[0]);
    ::close(fds[1]);
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    ::execv(executable.c_str(), argv.data());
    ::_exit(127);
  }
  ::close(fds[1]);
  pipe_read_ = fds[0];
  pipe_write_ = -1;
  pid_ = static_cast<int>(pid);
  return true;
}

bool SidecarProcess::read_byte(char* out, int timeout_ms) {
  struct pollfd pfd{};
  pfd.fd = pipe_read_;
  pfd.events = POLLIN;
  const int ready = ::poll(&pfd, 1, timeout_ms);
  if (ready <= 0) {
    return false;
  }
  return ::read(pipe_read_, out, 1) == 1;
}

bool SidecarProcess::read_line(std::string* line, int timeout_ms) {
  if (!running() || pipe_read_ < 0) {
    return false;
  }
  std::string result;
  char c = 0;
  while (read_byte(&c, timeout_ms)) {
    if (c == '\n') {
      if (line != nullptr) {
        *line = result;
      }
      return true;
    }
    result.push_back(c);
  }
  return false;
}

void SidecarProcess::terminate() {
  if (running()) {
    ::kill(static_cast<pid_t>(pid_), SIGTERM);
  }
}

int SidecarProcess::wait() {
  int code = -1;
  if (running()) {
    int status = 0;
    if (::waitpid(static_cast<pid_t>(pid_), &status, 0) >= 0) {
      code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
  }
  close_handles();
  return code;
}

void SidecarProcess::close_handles() {
  if (pipe_read_ >= 0) {
    ::close(pipe_read_);
    pipe_read_ = -1;
  }
  if (pipe_write_ >= 0) {
    ::close(pipe_write_);
    pipe_write_ = -1;
  }
  pid_ = 0;
}

#endif

}  // namespace srsp::bridge
